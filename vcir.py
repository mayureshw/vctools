import sys, json, os
from operator import gt, le, eq, ne, and_, or_, mul, add
from vcnodeclasses import *
from vcnodeprops import *

# Note:
#  - nodeType: Broad classification into just Place and Transition
#  - nodeClass: Further classification based on fanin-fanout structure of a node

class Arc:
    def reversedArc(self): return Arc({
        'srcnode'   : self.tgtnode,
        'tgtnode'   : self.srcnode,
        'wt'        : self.wt,
        'rel'       : 'rev_' + self.rel,
        })
    def inferRel(self): return (
        'mutex' if self.srcnode.isMutex() else \
        'passivebranch' if self.srcnode.isPassiveBranch() else \
        'branch' if self.srcnode.isBranch() else \
        'petri'
        ) if self.srcnode.isPlace() else \
        'petri'
    def __init__(self,d):
        self.__dict__.update(d)
        if 'rel' not in self.__dict__: self.rel = self.inferRel()

class Node:
    arcrels = [ 'petri', 'mutex', 'passivebranch', 'branch', 'total', 'rev_mutex', 'rev_passivebranch' ]
    def nodeClass(self): return self.classname
    def isPlace(self): return False
    def isTransition(self): return False
    def fanin(self,rel): return len(self.iarcs[rel])
    def fanout(self,rel): return len(self.oarcs[rel])
    def successors(self,rel): return [ a.tgtnode for a in self.oarcs[rel] ]
    def predecessors(self,rel): return [ a.srcnode for a in self.iarcs[rel] ]
    def addOarc(self,arc):
        arc.srcpos = len(self.oarcs[arc.rel])
        self.oarcs[arc.rel] += [arc]
        self.oarcs['total'] += [arc]
    def addIarc(self,arc):
        arc.tgtpos = len(self.iarcs[arc.rel])
        self.iarcs[arc.rel] += [arc]
        self.iarcs['total'] += [arc]
    def nodeinfo(self): return str({
        **{
        rel : ( self.fanin(rel), self.fanout(rel) )
        for rel in self.arcrels if self.fanin(rel) > 0 or self.fanout(rel) > 0
        },
        **{'nodeid' : self.nodeid},
        **{ 'typ' : self.nodeType() },
        })
    def processValidations(self,vs):
        for msg,valf in vs:
            if valf(): print(msg, self.nodeinfo(), '\n')
    def classify(self):
        clss = list( nc for nc in NodeClass.__subclasses__() if nc.checkSign(self) )
        if len(clss) > 1:
            print('MultiClassification',clss,self.nodeinfo())
            return
        elif len(clss) == 0:
            print('NoClassification',self.nodeinfo())
            return
        nodeclass = clss[0]
        self.classname = nodeclass.__name__
        propvios = [ str(propfn) for prop in nodeclass.props for propfn in [prop(self)] if not propfn.eval() ]
        if len(propvios) > 0 :
            for p in propvios:
                print('PROPVIO',self.classname,p,self.nodeinfo())
        # else: print('CLASSOK',self.classname,self.nodeinfo())
    def __init__(self,nodeid,vcir,props):
        self.vcir = vcir
        self.oarcs = { r:[] for r in self.arcrels }
        self.iarcs = { r:[] for r in self.arcrels }
        self.nodeid = nodeid
        self.classname = None # set by classify
        self.__dict__.update(props)

class Transition(Node):
    def isTransition(self): return True
    def nodeType(self): return 'Transition'
    def isFork(self): return self.fanout('petri') > 1
    def isJoin(self): return self.fanin('petri') > 1
    def __init__(self,nodeid,vcir,props): super().__init__(nodeid,vcir,props)

class Place(Node):
    def isPlace(self): return True
    def nodeType(self): return 'Place'
    def isMutex(self): return self.nodeid in self.vcir.mutexes
    def isBranch(self): return self.nodeid in self.vcir.branches
    def isPassiveBranch(self): return self.nodeid in self.vcir.passive_branches
    def isMerge(self): return self.fanin('petri') > 1
    def isHighCapacity(self): return self.capacity > 1 or self.capacity == 0
    def __init__(self,nodeid,vcir,props): super().__init__(nodeid,vcir,props)

class VcPetriNet:
    def __init__(self,pnobj,vcir):
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
        for arc in pnobj['arcs']:
            srcnode = self.nodes[ arc['src'] ]
            tgtnode = self.nodes[ arc['tgt'] ]
            arcobj = Arc({
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
        self.pn = VcPetriNet(pnobj,self)
        self.validate()
