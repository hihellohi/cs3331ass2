#!/usr/bin/bash

./Lsr A 2000 configA.txt &
./Lsr B 2001 configB.txt &

sleep $1
kill $(jobs -p)
