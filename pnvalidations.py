#!/usr/bin/env python3

import sys
from vcir import Vcir
from vcirbase import NodeClass

if len(sys.argv) != 2 :
    print('Usage:',sys.argv[0],'<vc-stem>')
    sys.exit(1)

stem = sys.argv[1]
if stem == 'show' :
    NodeClass.printProps()
else:
    vcir = Vcir(stem)
