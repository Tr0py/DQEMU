for i in $(seq 1 10000);
do
DQEMU --offloadmode client --node 4 --group 1 32-elf
done
