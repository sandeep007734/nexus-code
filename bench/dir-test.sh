#!/bin/bash
num=1

while [ $num -lt 10 ]; do
  sudo bash -c "echo 3 > /proc/sys/vm/drop_caches"
  ~/iozone-current/iozone3 -aU -g -i 0,1,2 -e -c -r 4k 32m
  let num+=1
done
