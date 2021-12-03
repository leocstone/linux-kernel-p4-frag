MODULE = frag

obj-m += $(MODULE).o

KERNELDIR ?= /lib/modules/$(shell uname -r)/build

PWD := $(shell pwd)

all: $(MODULE) forceFrag

%.o: %.c
	@echo "  CC      $<"
	@$(CC) -c $< -o $@

# This program will force fragmentation of the memory
forceFrag: ./forced-frag/forceFrag.c
	$(CC) $^ -o ./forced-frag/$@

$(MODULE):
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	rm ./forced-frag/forceFrag
