#!/usr/bin/env python3

import sys
import json
import os

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
            int(nodeid) : Place(nodeid,props)
            for (nodeid,props) in pnobj['places'].items()
            }
        self.transitions = {
            int(nodeid) : Transition(nodeid,props)
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
                print('fanin',p.fanin(),'fanout',p.fanout(),'mismatch for mutex place',p.nodeid)
    def branchPlaceType(self):
        branchplaces = [ p for p in self.pn.places.values() if p.fanout() > 1 ]
        for p in branchplaces:
            if p not in self.mutexes and p not in self.passive_branches and p not in self.branches:
                print('unresolved branch place:',p.nodeid)
    def validate(self):
        self.mutexFanInOuts()
        self.branchPlaceType()
    def __init__(self,jsonobj,pn):
        self.pn = pn
        self.mutexes = set(jsonobj['mutexes'])
        self.passive_branches = set(jsonobj['passive_branches'])
        self.branches = set(jsonobj['branches'])

def checkFilExists(flnm):
    if not os.path.exists(flnm):
        print('Did not find file', flnm)
        sys.exit(1)
    
if __name__ == '__main__':
    if len(sys.argv) != 2 :
        print('Usage:',sys.argv[0],'<vc-stem>')
        sys.exit(1)
    stem = sys.argv[1]
    pnflnm = stem + '_petri.json'
    jsonflnm = stem + '.json'
    checkFilExists(pnflnm)
    checkFilExists(jsonflnm)
    pnobj = json.load(open(pnflnm))
    pn = VcPetriNet(pnobj)
    jsonobj = json.load(open(jsonflnm))
    vcir = Vcir(jsonobj,pn)
    vcir.validate()
