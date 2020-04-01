#!/bin/bash
echo "do for 10000 times:\n.qemu-aarch64 --offloadmode server --offloadindex $1 ../workload/a.out"
for i in `seq 1 10000`;do
time ./aarch64-linux-user/qemu-aarch64 -d mmu --offloadmode server --offloadindex $1 ./elfs/64-elf;
echo $i
done

