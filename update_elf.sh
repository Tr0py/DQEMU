aarch64-linux-gnu-gcc -pthread --static -o elfs/64-elf -g -Wall elfs/thread.c
arm-linux-gnueabihf-gcc --static -pthread -o elfs/32-elf -g -Wall elfs/thread.c
