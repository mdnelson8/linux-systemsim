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

# we prefer a maple right now
myconf config pic/start 0xf8040000
myconf config pic/end 0xf807ffff
myconf config pic/little_endian 0

set isa_base 0xf4000000

myconf config rtc/start [add64 $isa_base 0x900]
myconf config rtc/end [add64 $isa_base 0x90f]
myconf config uart0 on
myconf config uart0/start [add64 $isa_base 0x3f8]
myconf config uart0/end [add64 $isa_base 0x3ff]
myconf config uart1/start [add64 $isa_base 0x2f8]
myconf config uart1/end [add64 $isa_base 0x2ff]

define machine myconf mysim

set root [ mysim of find_device / ]

set ht  [ mysim of addchild $root ht 0 ]
set range [list 0x0 0x0 ]
mysim of addprop $ht array "bus-range" range
set ranges [list 0x81000000 0x00000000 0x00000000 0x00000000	         0xf4000000 0x00000000 0x00400000 0x82000000	         0x00000000 0x80000000 0x00000000 0x80000000	         0x00000000 0x70000000 ]
mysim of addprop $ht array "ranges" ranges
set reg [list 0x0 0xf2000000 0x03000000 ]
mysim of addprop $ht array "reg" reg
mysim of addprop $ht string "compatible" "u3-ht"
mysim of addprop $ht int "#size-cells" 2
mysim of addprop $ht int "#address-cells" 3
mysim of addprop $ht string "device_type" "ht"
mysim of addprop $ht string "name" "ht"

set isa  [ mysim of addchild $ht isa 4 ]
mysim of addprop $isa string "name" "isa"
mysim of addprop $isa string "device_type" "isa"
set reg [list 0x2000 0x0 0x0 0x0 0x0]
mysim of addprop $isa array "reg" reg
set ranges [list 0x1 $isa_base 0x10000 ]
mysim of addprop $isa array "ranges" ranges

set rtc [ mysim of addchild $isa rtc 0x900 ]
mysim of addprop $rtc string "compatible" "pnpPNP,B00"
mysim of addprop $rtc string "name" "rtc"
mysim of addprop $rtc string "device_type" "rtc"
set reg  [list 0x1 0x900 0x1 0x1 0x901 0x1]
mysim of addprop $rtc array "reg" reg

set uart1 [ mysim of addchild $isa serial 0x3f8 ]
set reg [ list 0x1 0x3f8 0x8 ]
mysim of addprop $uart1 array "reg" reg
mysim of addprop $uart1 string "device_type" "serial"
mysim of package_to_path $uart1

if { [ info exists GUI_HOOK ] } then { eval $GUI_HOOK }

#
#   Now load the boot image
#

mysim bogus disk init 0 rootdisk.img newcow simple.cow 1024
mysim bogus net init 0 de:ad:ca:fe:ba:be /tmp/serversocketh0 0 0
mysim load vmlinux vmlinux 0

mysim modify loose on

set usertcl /dev/null
if { [ file exists ./user.tcl ] } {
   set usertcl ./user.tcl
}
source $usertcl

set of_root [mysim of find_device /]
mysim of setprop $of_root compatible "Momentum,Maple"
set of_chosen [mysim of find_device /chosen]
mysim of setprop $of_chosen bootargs "rw root=/dev/mambobd0 console=bogus0"
simdebug set PCI 1
set dart [ mysim of addchild $root dart 0 ]
mysim of addprop $dart string "device_type" "dart"
mysim of addprop $dart string "compatible" "u3-dart"
mysim go
