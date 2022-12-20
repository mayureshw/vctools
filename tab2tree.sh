sort |
awk '
function tabs(n)
{
    for(j=0;j<n;j++) printf("\t")
}

{
    PRT=0
    for(i=1;i<=NF;i++)
    {
        if ( $i != LAST[i] ) PRT=1
        if ( PRT )
        {
            tabs(i-1)
            print($i)
            LAST[i]=$i
        }
    }
}'
