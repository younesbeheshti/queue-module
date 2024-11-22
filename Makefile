CC      = gcc
BLOCKING = 1
MKFLAGS = -C /lib/modules/$(KVERSION)/build M=$(PWD)
obj-m += queue_module.o
KVERSION = $(shell uname -r)

all:
	make $(MKFLAGS) modules
clean:
	make $(MKFLAGS) clean
	rm user

user:
	$(CC) main.c shmutil.c -lm -o user

permission:
	chmod 777 /dev/myQueue

install:
	insmod queue_module.ko blocking=$(BLOCKING)
uninstall:
	rmmod queue_module
