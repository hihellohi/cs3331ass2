#!/usr/bin/bash

./Lsr A 2000 configA.txt > A &
./Lsr B 2001 configB.txt > B &
./Lsr C 2002 configC.txt > C &
./Lsr D 2003 configD.txt > D &
./Lsr E 2004 configE.txt > E &
./Lsr F 2005 configF.txt > F &

sleep $1
kill %"$3"
sleep $2
kill $(jobs -p)
