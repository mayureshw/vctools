import sys, json, os

class Arc:
    def __init__(self,d):
        self.__dict__.update(d)
class Node:
    def fanin(self): return len(self.iarcs)
    def fanout(self): return len(self.oarcs)
    def successors(self): return [ a.tgtnode for a in self.oarcs ]
    def predecessors(self): return [ a.srcnode for a in self.iarcs ]
    def __init__(self,nodeid,props):
        self.oarcs = []
        self.iarcs = []
        self.nodeid = nodeid
        self.__dict__.update(props)

class Transition(Node):
    def __init__(self,nodeid,props):
        super().__init__(nodeid,props)

class Place(Node):
    def isHighCapacity(self): return self.capacity > 1 or self.capacity == 0
    def __init__(self,nodeid,props):
        super().__init__(nodeid,props)

class PetriNet:
    def __init__(self,pnobj):
        self.places = {
            int(nodeid) : Place(int(nodeid),props)
            for (nodeid,props) in pnobj['places'].items()
            }
        self.transitions = {
            int(nodeid) : Transition(int(nodeid),props)
            for (nodeid,props) in pnobj['transitions'].items()
            }
        self.nodes = {**self.places,**self.transitions}
        self.arcs = []
        for arc in pnobj['arcs']:
            src = arc['src']
            tgt = arc['tgt']
            wt = arc['wt']
            srcnode = self.nodes[src]
            tgtnode = self.nodes[tgt]
            srcpos = srcnode.fanout()
            tgtpos = tgtnode.fanin()
            arcobj = Arc({
                'srcnode' : srcnode,
                'tgtnode' : tgtnode,
                'srcpos'  : srcpos,
                'tgtpos'  : tgtpos,
                'wt'      : wt,
                })
            srcnode.oarcs += [ arcobj ]
            tgtnode.iarcs += [ arcobj ]
            self.arcs += [ arcobj ]

class VcPetriNet(PetriNet):
    def __init__(self,pnobj):
        super().__init__(pnobj)

class Vcir:
    def mutexFanInOuts(self):
        mutexplaces = [ self.pn.nodes[mutex] for mutex in self.mutexes ]
        for p in mutexplaces:
            if p.fanin() != p.fanout():
                print('fanin',p.fanin(),'fanout',p.fanout(),'mismatch for mutex place',p.nodeid,p.label)
    def branchPlaceType(self):
        resolved_branches = self.mutexes.union(self.passive_branches).union(self.branches)
        branchplaces = [ p for p in self.pn.places.values() if p.fanout() > 1 ]
        for p in branchplaces:
            if p.nodeid not in resolved_branches:
                print('unresolved branch place:',p.nodeid,p.label)
    def atMostOneMutex(self):
        mutexControlled = { s for m in self.mutexes for s in self.pn.nodes[m].successors() }
        for mc in mutexControlled:
            controllingMutexCnt = len(list(p for p in mc.predecessors() if p.nodeid in self.mutexes))
            if controllingMutexCnt > 1 :
                print('Transition may be controlled by at most 1 mutexes',mc.nodeid,mc.label,controllingMutexCnt)
    def highCapacityMustBePassive(self):
        highCapPlaces = [ p for p in self.pn.places.values() if p.isHighCapacity() ]
        for p in highCapPlaces:
            if p.nodeid not in self.passive_branches:
                print('Places with capacity > 1 must be passive branches',p.nodeid,p.label)
    def joinCapacity(self):
        highJoinTrns = [ t for t in self.pn.transitions.values() if t.fanin > 4 ]
        for t in highJoinTrns:
            print('Joins up to fanin 4 supported (as of now)',t.nodeid,t.label,t.fanin())
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
        self.pn = VcPetriNet(pnobj)
        jsonobj = json.load(open(jsonflnm))
        self.mutexes = set(jsonobj['mutexes'])
        self.passive_branches = set(jsonobj['passive_branches'])
        self.branches = set(jsonobj['branches'])

