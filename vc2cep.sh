#!/bin/sh

VCFILE=$1
DEPFILE=depdat
VCIRFILE=vcirdat

if [ ! -f "$VCFILE" ]
then
    echo "Usage: $0 <vcfile>"
    exit 1
fi

vc2p.out $VCFILE $VCIRFILE
echo [depgen]. | xsb > $DEPFILE
dep2cep depdat cepdat
