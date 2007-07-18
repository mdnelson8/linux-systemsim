puts "To debug mambo with gdb: attach [pid]"
#gets stdin
#simdebug set UART 1

#  get the general procedures defined for mambo
source $env(LIB_DIR)/ppc/util.tcl


#
#   First create a simulator called mysim
#
define dup gpul myconf
myconf config cpus 1
myconf config memory_size 128M
# set to 1/8 for Book 4
# set to 1/60 for G5
# set to 1/512 for jimi
myconf config processor timebase_frequency 1/60
source ../maple_devices.tcl

# turn off the freakin uarts
myconf config /UART/0/enabled off
myconf config /UART/1/enabled off

define machine myconf mysim

source ../maple_devtree.tcl

set mamborc /dev/null
if { [ file exists $env(HOME)/.mamborc.prefix ] } {
        set mamborc $env(HOME)/.mamborc.prefix
}
source $mamborc

source $env(LIB_DIR)/common/mambo_init.tcl
# Start the GUI if -g option was given
MamboInit::gui $env(LIB_DIR)/common/gui/default_gui.tcl

#
#   Setup bogus stuff
#
mysim bogushalt delay 0xffffff
mysim bogus disk init 0 $IMAGES/gpul/linux/ubuntu.img newcow simple.cow 1024
# bogusnet netap version
#mysim bogus net init 0 de:ed:ca:fe:ba:be /tmp/serversocketh0 0 0
# bogusnet tun/tap version
#mysim bogus net init 0 de:ed:ca:fe:ba:be tap 9 0
#exec /usr/bin/sudo /sbin/ifconfig tun0 172.20.0.1


#
#   Now load the boot image
#
mysim load vmlinux $IMAGES/gpul/vmlinux-2.6.16.img 0

set mamborc /dev/null
if { [ file exists $env(HOME)/.mamborc ] } {
   set mamborc $env(HOME)/.mamborc
}
source $mamborc

# fix up the bootparms (for new 2.6.16-rc1-systemsim kernels)
set root [mysim of find_device /]
set of_chosen [mysim of find_device /chosen]
mysim of setprop $of_chosen bootargs "rw root=/dev/mambobd0 console=hvc0"

# add systemsim device node  (for new 2.6.16-rc1-systemsim kernels)
# mysim of addchild $root "systemsim" ""

#
#   maybe set some triggers
#
#mysim trigger set pc 0x0   "break_point mysim pc_at_zero"
#mysim trigger set pc 0x300 "dpage_fault mysim"
#mysim trigger set pc 0x700 "break_point mysim pc_at_0x700"
#mysim trigger set pc 0x900 "break_point mysim pc_at_0x900"

#enable fast mode
mysim set loose on

# Run verbose
#vgo mysim

# Run quiet
mysim go

