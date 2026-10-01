obj-m := khello.o
KDIR := ../../output/build/linux-custom

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR)
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules_install INSTALL_MOD_PATH=../../target

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
