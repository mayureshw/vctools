#!/usr/bin/env python3

import sys
from vcir import Vcir

if len(sys.argv) != 2 :
    print('Usage:',sys.argv[0],'<vc-stem>')
    sys.exit(1)

stem = sys.argv[1]
vcir = Vcir(stem)
