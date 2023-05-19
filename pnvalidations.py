#!/usr/bin/env python3

import sys
import json
import os

class Node:
    def __init__(self,nodeid,props):
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
            for (nodeid,props) in pnobj['places'].items()
            }

class VcPetriNet(PetriNet):
    def __init__(self,pnobj):
        super().__init__(pnobj)

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
