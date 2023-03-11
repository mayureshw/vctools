VCFILE=$1
VCNAME=`basename $VCFILE .vc`
IRFILE=$VCNAME.vcir

if [ ! -f $IRFILE ]
then
    echo vcir file not found: $IRFILE
    exit 1
fi

xsb -e "[pipeusage], pipeusage('$VCNAME'), halt." | tab2tree.sh
