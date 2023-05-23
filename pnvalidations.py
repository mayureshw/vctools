#!/usr/bin/env python3

import sys
import json
import os
from vcpetrinet import VcPetriNet, Vcir

def checkFilExists(flnm):
    if not os.path.exists(flnm):
        print('Did not find file', flnm)
        sys.exit(1)
    
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
