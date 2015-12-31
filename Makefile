###########################################################################
#
# Filename:
#
#   Makefile
#
# Description:
#
#   Makefile to build the MAS31750SBC/xTU monitor
#
#   There are four targets in this Makefile:
#     all (hex) 	- Monitor running in StartUp PROM
#     ram          	- Monitor running in SRAM or Load
#     downld            - Download the moitor into Target 
#     clean  		- delete derived files
#
#   Yao Fei (feiyao@me.com)
#
###########################################################################
VER   = -1.7.17b1
XGC   = /opt/m1750-ada$(VER)/bin/m1750-coff-
CC    = $(XGC)gcc
LDFLAGS = -T mon1750.M -Wl,-Map=mon1750.map

# if use with MAS31750SBC, add -D __USE_MAS31750SBC=1 
# if use 2 UARTs, add -D __TWO_UART=1
#CFLAGS = -O3 -g -D __TWO_UART=1 -D __USE_MAS31750SBC=1
# -D __USE_TIMEOUT
CFLAGS = -g -Wall 

all: hex 

hex:  prom mon1750
	cp prom.hex mon1750_prom.hex
	$(XGC)objcopy -O ihex --change-address=512 mon1750 mon1750_tmp.hex
	sed "/:040*30*200F7/d" mon1750_tmp.hex >> mon1750_prom.hex
	-rm -f prom mon1750_tmp.hex   
	$(XGC)objcopy -O ihex -i2 -b0 --gap-fill 0x74 --pad-to 0x2000 mon1750_prom.hex  mon1750_h.hex
	$(XGC)objcopy -O ihex -i2 -b1 --gap-fill 0x00 --pad-to 0x2000 mon1750_prom.hex  mon1750_l.hex


ram: mon1750
	$(XGC)objcopy -O ihex mon1750  mon1750_ram.hex

downld: ram
	cp mon1750_ram.hex /dev/ttyS0	

asm:    mon1750
	$(XGC)objdump -Sa $< > $<.asm

mon1750: mon1750.o 

prom:	prom.o
	$(CC) -o $@ -T prom.M $<
	$(XGC)objcopy -O ihex prom prom_tmp.hex
	sed "/^:0*1FF/d"  prom_tmp.hex > prom.hex         # remove the end flag	
	-rm -f prom_tmp.hex

log:
	git pull
	git log > ChangeLog

clean:
	rm -rf *.o *.sre *.map mon1750*.hex *.bin *.asm prom*.hex
	rm -rf mon1750  mon1750_ram
	rm -rf *~ 
#
mon1750.o:  mon1750.c
prom.o: prom.S
