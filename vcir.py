import sys, json, os

class Node:
    def fanin(self): return len(self.predecessors)
    def fanout(self): return len(self.successors)
    def __init__(self,nodeid,props):
        self.successors = {}
        self.predecessors = {}
        self.nodeid = nodeid
        self.__dict__.update(props)

class Transition(Node):
    def __init__(self,nodeid,props):
        super().__init__(nodeid,props)

class Place(Node):
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
        for arc in pnobj['arcs']:
            src = arc['src']
            tgt = arc['tgt']
            wt = arc['wt']
            srcnode = self.nodes[src]
            tgtnode = self.nodes[tgt]
            srcnode.successors[tgt] = wt
            tgtnode.predecessors[src] = wt

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
        mutexControlled = { self.pn.nodes[s] for m in self.mutexes for s in self.pn.nodes[m].successors }
        for mc in mutexControlled:
            mcpreds = [ self.pn.nodes[p] for p in mc.predecessors ]
            controllingMutexCnt = len(list(p for p in mcpreds if p.nodeid in self.mutexes))
            if controllingMutexCnt > 1 :
                print('Transition may be controlled by at most 1 mutexes',mc.nodeid,mc.label,controllingMutexCnt)
    def highCapacityMustBePassive(self):
        highCapPlaces = [ p for p in self.pn.places.values() if p.capacity > 1 ]
        for p in highCapPlaces:
            if p.nodeid not in self.passive_branches:
                print('Places with capacity > 1 must be passive branches',p.nodeid,p.label)
    def joinCapacity(self):
        highJoinTrns = [ t for t in self.pn.transitions.values() if t.fanin > 4 ]
        for t in highJoinTrns:
            print('Joins up to fanin 4 supported (as of now)',t.nodeid,t.label,t.fanin())
    # Wish list
    # - Successors of a passive branch must be mutually exclusive. Requires
    # analysis to check since they may not directly depend on a mutex.
    def checkFilExists(self,flnm):
        if not os.path.exists(flnm):
            print('Did not find file', flnm)
            sys.exit(1)
    def validate(self):
        self.mutexFanInOuts()
        self.branchPlaceType()
        self.atMostOneMutex()
        self.highCapacityMustBePassive()
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

