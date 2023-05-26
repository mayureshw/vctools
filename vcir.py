import sys, json, os

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

class NodeClass :
    sign  = lambda : False
    props = []

class MutexPlace(NodeClass):
    sign  = lambda n : n.isPlace() and n.isMutex()
    props = [
('rev_mutex fanin = mutex fanout', lambda n: n.fanin('rev_mutex') == n.fanout('mutex')),
('petri fanin = mutex fanout'    , lambda n: n.fanin('petri') == n.fanout('mutex')    ),
('all fanin = mutex fanout * 2'  , lambda n: n.fanin('all') == n.fanout('mutex') * 2  ),
('all fanout = mutex fanout'     , lambda n: n.fanout('all') == n.fanout('mutex')     ),
        ]

class PassiveBranchPlace(NodeClass):
    sign  = lambda n : n.isPlace() and n.isPassiveBranch()
    props = [
('rev_passivebranch fanin = passivebranch fanout', lambda n: n.fanin('rev_passivebranch') == n.fanout('passivebranch')     ),
('all fanin = passivebranch fanout + petri fanin', lambda n: n.fanin('all') == n.fanout('passivebranch') + n.fanin('petri')),
('all fanout = passivebranch fanout'             , lambda n: n.fanout('all') == n.fanout('passivebranch')                  ),
        ]

class BranchPlace(NodeClass):
    sign  = lambda n : n.isPlace() and n.isBranch()
    props = [
('branch fanout = 2', lambda n: n.fanout('branch') == 2),
('petri fanin = 1'  , lambda n: n.fanin('petri') == 1  ),
('all fanin = 1'    , lambda n: n.fanin('all') == 1    ),
('all fanout = 2'   , lambda n: n.fanout('all') == 2   ),
        ]

class MergePlace(NodeClass):
    sign  = lambda n : n.isPlace() and n.fanin('petri') == 2 and n.fanout('petri') == 1
    props = [
('all fanin = 2' , lambda n: n.fanin('all') == 2 ),
('all fanout = 1', lambda n: n.fanout('all') == 1),
        ]

class PassThrough(NodeClass):
    sign  = lambda n : n.fanin('all') == 1 and n.fanout('all') == 1 and n.fanin('petri') == 1 and n.fanout('petri') == 1

class Node:
    arcrels = [ 'petri', 'mutex', 'passivebranch', 'branch', 'all', 'rev_mutex', 'rev_passivebranch' ]
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
        self.oarcs['all'] += [arc]
    def addIarc(self,arc):
        arc.tgtpos = len(self.iarcs[arc.rel])
        self.iarcs[arc.rel] += [arc]
        self.iarcs['all'] += [arc]
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
        clss = list( nc for nc in NodeClass.__subclasses__() if nc.sign(self) )
        if len(clss) > 1:
            print('MultiClassification',clss,self.nodeinfo())
            return
        elif len(clss) == 0:
            print('NoClassification',self.nodeinfo())
            return
        nodeclass = clss[0]
        self.classname = nodeclass.__name__
        propvios = [ msg for msg,propfn in nodeclass.props if not propfn(self) ]
        if len(propvios) > 0 :
            for p in propvios:
                print('PROPVIO',self.classname,p,self.nodeinfo())
        else:
            print('CLASSOK',self.classname,self.nodeinfo())
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

class Vcir:
    def mutexFanInOuts(self):
        mutexplaces = [ self.pn.nodes[mutex] for mutex in self.mutexes ]
        for p in mutexplaces:
            if p.fanin('all') != p.fanout('all'):
                print('fanin',p.fanin('all'),'fanout',p.fanout('all'),'mismatch for mutex place',p.nodeid,p.label)
    def branchPlaceType(self):
        resolved_branches = self.mutexes.union(self.passive_branches).union(self.branches)
        branchplaces = [ p for p in self.pn.places.values() if p.fanout('all') > 1 ]
        for p in branchplaces:
            if p.nodeid not in resolved_branches:
                print('unresolved branch place:',p.nodeid,p.label)
    def atMostOneMutex(self):
        mutexControlled = { s for m in self.mutexes for s in self.pn.nodes[m].successors('mutex') }
        for mc in mutexControlled:
            controllingMutexCnt = len(list(p for p in mc.predecessors('mutex')))
            if controllingMutexCnt > 1 :
                print('Transition may be controlled by at most 1 mutexes',mc.nodeid,mc.label,controllingMutexCnt)
    def highCapacityMustBePassive(self):
        highCapPlaces = [ p for p in self.pn.places.values() if p.isHighCapacity() ]
        for p in highCapPlaces:
            if not p.isPassiveBranch():
                print('Places with capacity > 1 must be passive branches',p.nodeid,p.label)
    def joinCapacity(self):
        highJoinTrns = [ t for t in self.pn.transitions.values() if t.fanin('petri') > 4 ]
        for t in highJoinTrns:
            print('Joins up to fanin 4 supported (as of now)',t.nodeid,t.label,t.fanin('petri'))
    def branchFanout(self):
        branchplaces = [ self.pn.places[p] for p in self.branches ]
        for p in branchplaces:
            if p.fanout('branch') != 2 :
                print('Branch fanout must be 2',p.nodeid,p.label,p.fanout('branch'))
    def highCapacityNotSupported(self):
        highCapPlaces = [ p for p in self.pn.places.values() if p.isHighCapacity() ]
        for p in highCapPlaces:
            print('High capacity places not supported in asyncvhdl as of now',p.nodeid,p.label,p.capacity)
    def arcWtNotSupported(self):
        highWtArcs = [ a for a in self.pn.arcs if a.wt > 1 ]
        for a in highWtArcs:
            print('High arc wt not supported in asyncvhdl as of now',a.srcnode.nodeid,'->',a.tgtnode.nodeid,a.wt)
    def checksNotAutomated(self):
        print('Do run simulator with PN_PLACE_CAPACITY_EXCEPTION enabled. It is not checked by this tool.')
        print('Do ensure, successors of passive branch are mutually exclusive. It is not checked, as it requires analysis such as unfoldings.')
    def validate(self):
        self.mutexFanInOuts()
        self.branchPlaceType()
        self.atMostOneMutex()
        self.highCapacityMustBePassive()
        self.branchFanout()
        self.highCapacityNotSupported()
        self.arcWtNotSupported()
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
