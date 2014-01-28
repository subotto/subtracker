#!/bin/bash

if [[ "x$1" == "x" ]]
then
    echo "Inserire una sorgente!"
    exit 0
fi

ulimit -v 2097152 # 2GB di memoria virtuale

i=0

while true
do
    let i=$i+1
    echo "Avvio numero $i..."
    date
    ./subtracker2014 $1 subotto/subotto.png subotto/subotto_feature_mask.png | pv -l | (cd web; ./insert.py)
done
