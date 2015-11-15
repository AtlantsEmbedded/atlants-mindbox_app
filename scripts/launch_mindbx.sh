#!/bin/sh /etc/rc.common
# MindBx startup script
 
START=99
#STOP=100
 
start() {  

	while ! ls /sys/class/bluetooth/hci0 1> /dev/null 2>&1; do
		echo "waiting for hci0..."
		sleep 1
	done

	sleep 10

	echo "hci0 is present"
	
	hciconfig hci0 sspmode 1

	/atom/data/data_interface /atom/data/config/mindbox_datainterface.xml &
	sleep 2
	/atom/data/data_preprocessing /atom/data/config/mindbox_preprocessing.xml &
	sleep 2
	/atom/app/mindbx_app /atom/app/config/mindbox_application.xml &
}                 
 
stop() {          
        killall data_interface
        killall data_preprocessing
        killall mindbx_app
}

boot() {

	while ! ls /sys/class/bluetooth/hci0 1> /dev/null 2>&1; do
		echo "waiting for hci0..."
		sleep 1
	done

	sleep 10
	
	echo "hci0 is present"
	
	hciconfig hci0 sspmode 1

	/atom/data/data_interface /atom/data/config/mindbox_datainterface.xml &
	sleep 2
	/atom/data/data_preprocessing /atom/data/config/mindbox_preprocessing.xml &
	sleep 2
	/atom/app/mindbx_app /atom/app/config/mindbox_application.xml &
}
