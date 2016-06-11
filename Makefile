MODULE=driver_SO_Final
 
KERNEL_SRC=/lib/modules/3.18.0-trunk-rpi2/build
 
obj-m += ${MODULE}.o
 
compile:
	make -C ${KERNEL_SRC} M=${CURDIR} modules

install: 
	sudo insmod ${MODULE}.ko 
	dmesg | tail 
	sudo chmod go+rw /dev/leds
	sudo chmod go+rw /dev/speaker
	sudo chmod go+rw /dev/buttons
	
uninstall:
	sudo rmmod ${MODULE} 
	dmesg | tail
