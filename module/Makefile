obj-m := w25q64.o

KERNEL_SRC := /home/jaggu/workspace/udooSumo/kernel_4.17.x+fslc/linux-fslc/
SRC := $(shell pwd)
all:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC)

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) moduues_install

clean:
	rm -f *.o *~ core .depend .*.cmd *.ko *.mod.c
	rm -f Module.makers Module.symvers modules.order
	rm -rf .tmp_versions Modules.symvers
