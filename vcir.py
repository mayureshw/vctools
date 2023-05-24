import sys, json, os

class Arc:
    def reversedArc(self): return Arc({
        'srcnode'   : self.tgtnode,
        'tgtnode'   : self.srcnode,
        'wt'        : self.wt,
        'isOrigArc' : False,
        })
    def __init__(self,d): self.__dict__.update(d)

class Node:
    def isPlace(self): return False
    def isTransition(self): return False
    def fanin(self,rel): return len(self.iarcs[rel])
    def fanout(self,rel): return len(self.oarcs[rel])
    def successors(self,rel): return [ a.tgtnode for a in self.oarcs[rel] ]
    def predecessors(self,rel): return [ a.srcnode for a in self.iarcs[rel] ]
    def _addOarc(self,rel,arc):
        arc.srcpos = len(self.oarcs[rel])
        self.oarcs[rel] += [arc]
        self.oarcs['all'] += [arc]
    def _addIarc(self,rel,arc):
        arc.tgtpos = len(self.iarcs[rel])
        self.iarcs[rel] += [arc]
        self.iarcs['all'] += [arc]
        arc.rel = rel # enough to do in any one of _add functions
    def __init__(self,nodeid,vcir,props):
        self.vcir = vcir
        arcrels = [ 'petri', 'mutex', 'passivebranch', 'branch', 'all' ]
        self.oarcs = { r:[] for r in arcrels }
        self.iarcs = { r:[] for r in arcrels }
        self.nodeid = nodeid
        self.__dict__.update(props)

class Transition(Node):
    def isTransition(self): return True
    def nodeType(self): return 'Transition'
    def isFork(self): return self.fanout('petri') > 1
    def isJoin(self): return self.fanin('petri') > 1
    def addIarc(self,arc):
        # For transitions srcplace type governs the iarc type
        arcrel = arc.srcnode.arcRel()
        self._addIarc(arcrel,arc)
    def addOarc(self,arc):
        # For transitions tgtplace mutex has arctype mutex
        # for all others it's a petri arc
        arcrel = arc.tgtnode.arcRel() if arc.tgtnode.isMutex() else 'petri'
        self._addOarc(arcrel,arc)
    def __init__(self,nodeid,vcir,props): super().__init__(nodeid,vcir,props)

class Place(Node):
    def isPlace(self): return True
    def nodeType(self): return 'Place'
    def arcRel(self): return 'mutex' if self.isMutex() else \
            'passivebranch' if self.isPassiveBranch() else \
            'branch' if self.isBranch() else \
            'petri'
    def isMutex(self): return self.nodeid in self.vcir.mutexes
    def isBranch(self): return self.nodeid in self.vcir.branches
    def isPassiveBranch(self): return self.nodeid in self.vcir.passive_branches
    def isMerge(self): return self.fanin('petri') > 1
    def addIarc(self,arc):
        # For mutex, all arcs are mutex, for others i/c arcs are petri
        arcrel = self.arcRel() if self.isMutex() else 'petri'
        self._addIarc(arcrel,arc)
    def addOarc(self,arc):
        # For places own type governs the oarc type
        arcrel = self.arcRel()
        self._addOarc(arcrel,arc)
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
                'isOrigArc' : True,
                })
            srcnode.addOarc(arcobj)
            tgtnode.addIarc(arcobj)
            if srcnode.isPlace() and arcobj.rel in {'mutex','passivebranch'} :
                revarc = arcobj.reversedArc()
                srcnode._addIarc(arcobj.rel,revarc)
                tgtnode._addOarc(arcobj.rel,revarc)

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
