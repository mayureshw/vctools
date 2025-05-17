import sys, json, os
from itertools import chain
from operator import *
from vcpnnodes import *
from vcdpnodes import *
from vcsysnodes import *
from vcnodeprops import *

class Vcir:
    def branchPlaceType(self):
        resolved_branches = self.mutexes.union(self.passive_branches).union(self.branches)
        branchplaces = [ p for p in self.pn.places.values() if p.fanout('petri') > 1 ]
        for p in branchplaces:
            if p.nodeid not in resolved_branches:
                print('ERROR: unresolved branch place:',p.nodeid,p.label)
    def highCapacityMustBePassive(self):
        highCapPlaces = [ p for p in self.pn.places.values() if p.isHighCapacity() ]
        for p in highCapPlaces:
            if not p.isPassiveBranch():
                print('ERROR: Places with capacity > 1 must be passive branches',p.nodeid,p.label)
    def arcWtNotSupported(self):
        highWtArcs = [ a for a in self.pn.arcs if a.wt > 1 ]
        for a in highWtArcs:
            print('ERROR: High arc wt not supported in asyncvhdl as of now',a.srcnode.nodeid,'->',a.tgtnode.nodeid,a.wt)
    def confusionNotSupported(self):
        jointrns = [ t for t in self.pn.transitions.values() if t.fanin('petri') > 1 ]
        confpairs = [ (p,t) for t in jointrns for p in t.predecessors('petri') if p.isPlace() and p.fanout('petri') > 1 ]
        for p,t in confpairs:
            print('ERROR: Confusion scenario not supported',p.nodeid,p.label,t.nodeid,t.label)
    def checksNotAutomated(self):
        print('ERROR: Do run simulator with PN_PLACE_CAPACITY_EXCEPTION enabled. It is not checked by this tool.')
        print('ERROR: Do ensure, successors of passive branch are mutually exclusive. It is not checked, as it requires analysis such as unfoldings.')
    def validate(self):
        self.branchPlaceType()
        self.highCapacityMustBePassive()
        self.arcWtNotSupported()
        self.confusionNotSupported()
    def checkFilExists(self,flnm):
        if not os.path.exists(flnm):
            print('Did not find file', flnm)
            sys.exit(1)
    def nodes(self): return [ n for n in chain(
        [ n for n in self.pn.nodes.values() if n.fanin('total') > 0 or n.fanout('total') > 0 ],
        self.dp.nodes.values(),
        self.sysdp.nodes.values()
        )]
    def nonCalledNonDaemonEns(self): return [ en for en in self.module_entries
        if self.module_entries[en]['nCallers'] == 0 and self.module_entries[en]['isDaemon'] == 0 ]
    def validateModules(self):
        for moddict in self.module_entries.values():
            if moddict['isVolatile'] and moddict['nCallers'] !=1 :
                print('Volatile module with > 1 callers is not supported',moddict['name'])
                sys.exit(1)
    def __init__(self,stem):
        pnflnm = stem + '_petri.json'
        jsonflnm = stem + '.json'
        self.checkFilExists(pnflnm)
        self.checkFilExists(jsonflnm)
        pnobj = json.load(open(pnflnm))
        jsonobj = json.load(open(jsonflnm))
        self.mutexes = set(jsonobj['mutexes'])
        self.passive_branches = set(jsonobj['passive_branches'])
        self.branches = set(jsonobj['branches'])
        self.simu_only = set(jsonobj['simu_only'])
        self.module_entries = { int(en) : d for en,d in jsonobj['modules'].items() }
        self.validateModules()
        self.module_exits = { v['exit'] for v in jsonobj['modules'].values() }
        self.pipes = jsonobj.get('pipes',{})
        self.storage = jsonobj.get('storage',{})
        self.ncnode = NCNode(0,self,{})
        self.pn = VcPetriNet(pnobj,self)
        self.dp = VcDP(jsonobj['dpes'],self)
        for en,moddescr in jsonobj['modules'].items():
            entryplace = self.pn.nodes[ int(en) ]
            exitplace = self.pn.nodes[ moddescr['exit'] ]
            # entry place uses module iwidths as its owidths to supply fp to its users
            entryplace.owidths = moddescr['iwidths']
            # exit place uses module owidths as its iwidths to collect out parms for return
            exitplace.iwidths = moddescr['owidths']

            for tgtpos,srcinfo in moddescr['dpinps'].items():
                srcnode = self.dp.nodes[srcinfo['id']]
                DPArc(srcnode,exitplace,{
                    'srcpos' : srcinfo['oppos'],
                    'tgtpos' : tgtpos,
                    'rel' : 'data'
                    })
            for tgtpos,srcinfo in moddescr['fpinps'].items():
                DPArc(entryplace,exitplace,{
                    'srcpos' : srcinfo['oppos'],
                    'tgtpos' : tgtpos,
                    'rel' : 'data'
                    })
        self.dp.createArcs() # Needs to be done after pn is in place
        self.sysdp = VCSysDP(self)
        self.pn.classify() # Needs to be called after pn,dp,sys - all are in place
        self.validate()
