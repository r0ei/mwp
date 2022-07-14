obj-m := mwp.o
EXTRA_CFLAGS += -I$(M)/include -Werror -Wall -c -D__KERNEL__ -DMODULE

all:
	$(MAKE) -C /lib/modules/$$(uname -r)/build M=$(CURDIR) modules

clean:
	$(MAKE) -C /lib/modules/$$(uname -r)/build M=$(CURDIR) clean