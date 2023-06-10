import sys, json, os
from operator import *
from vcpnnodes import *
from vcdpnodes import *
from vcnodeprops import *

class VcPetriNet:
    def isDPArc(self,a): return \
        self.vcir.dp.isPNDPTrans( a['src'] ) or \
        self.vcir.dp.isDPPNTrans( a['tgt'] )
    def __init__(self,pnobj,vcir):
        self.vcir = vcir
        self.places = {
            int(nodeid) : Place(int(nodeid),vcir,props)
            for (nodeid,props) in pnobj['places'].items()
            }
        self.transitions = {
            int(nodeid) : Transition(int(nodeid),vcir,props)
            for (nodeid,props) in pnobj['transitions'].items()
            }
        self.nodes = {**self.places,**self.transitions}
        self.arcs = []
        nondparcs = [ a for a in pnobj['arcs'] if not self.isDPArc(a) ]
        for arc in nondparcs:
            srcnode = self.nodes[ arc['src'] ]
            tgtnode = self.nodes[ arc['tgt'] ]
            arcobj = PNArc({
                'srcnode'   : srcnode,
                'tgtnode'   : tgtnode,
                'wt'        : arc['wt'],
                })
            srcnode.addOarc(arcobj)
            tgtnode.addIarc(arcobj)
            if srcnode.isPlace() and arcobj.rel in {'mutex','passivebranch'} :
                revarc = arcobj.reversedArc()
                srcnode.addIarc(revarc)
                tgtnode.addOarc(revarc)
        for node in self.nodes.values(): node.classify()

class VcDP:
    def isPNDPTrans(self,tid): return tid in self.pndpTrans
    def isDPPNTrans(self,tid): return tid in self.dppnTrans
    def getTransSet(self,dpes,keys):
        return { t for dpe in dpes.values() for tk in keys for t in dpe[tk] }
    def __init__(self,dpes,vcir):
        self.nodes = { int(id):DPNode(int(id),vcir,dpe) for id,dpe in dpes.items()}
        pndpkeys = [ 'reqs', 'greqs' ]
        dppnkeys = [ 'acks', 'gacks' ]
        # Note: ftreqs remain connected in the PN
        self.pndpTrans = self.getTransSet( dpes, pndpkeys )
        self.dppnTrans = self.getTransSet( dpes, dppnkeys )

class Vcir:
    def branchPlaceType(self):
        resolved_branches = self.mutexes.union(self.passive_branches).union(self.branches)
        branchplaces = [ p for p in self.pn.places.values() if p.fanout('total') > 1 ]
        for p in branchplaces:
            if p.nodeid not in resolved_branches:
                print('ERROR: unresolved branch place:',p.nodeid,p.label)
    # TODO: When supported, we might make HighCapacityPlace a class and move this validation there
    def highCapacityMustBePassive(self):
        highCapPlaces = [ p for p in self.pn.places.values() if p.isHighCapacity() ]
        for p in highCapPlaces:
            if not p.isPassiveBranch():
                print('ERROR: Places with capacity > 1 must be passive branches',p.nodeid,p.label)
    def highCapacityNotSupported(self):
        highCapPlaces = [ p for p in self.pn.places.values() if p.isHighCapacity() ]
        for p in highCapPlaces:
            print('ERROR: High capacity places not supported in asyncvhdl as of now',p.nodeid,p.label,p.capacity)
    def arcWtNotSupported(self):
        highWtArcs = [ a for a in self.pn.arcs if a.wt > 1 ]
        for a in highWtArcs:
            print('ERROR: High arc wt not supported in asyncvhdl as of now',a.srcnode.nodeid,'->',a.tgtnode.nodeid,a.wt)
    def confusionNotSupported(self):
        jointrns = [ t for t in self.pn.transitions.values() if t.fanin('petri') > 1 ]
        confpairs = [ (p,t) for t in jointrns for p in t.predecessors('petri') if p.fanout('petri') > 1 ]
        for p,t in confpairs:
            print('ERROR: Cofusion scenarion not supported',p.nodeid,p.label,t.nodeid,t.label)
    def checksNotAutomated(self):
        print('ERROR: Do run simulator with PN_PLACE_CAPACITY_EXCEPTION enabled. It is not checked by this tool.')
        print('ERROR: Do ensure, successors of passive branch are mutually exclusive. It is not checked, as it requires analysis such as unfoldings.')
    def validate(self):
        self.branchPlaceType()
        self.highCapacityMustBePassive()
        self.highCapacityNotSupported()
        self.arcWtNotSupported()
        self.confusionNotSupported()
    def checkFilExists(self,flnm):
        if not os.path.exists(flnm):
            print('Did not find file', flnm)
            sys.exit(1)
    def nodes(self): return list(self.pn.nodes.values()) + list(self.dp.nodes.values())
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
        self.dp = VcDP(jsonobj['dpes'],self)
        self.pn = VcPetriNet(pnobj,self)
        self.validate()
