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
VER   = 
XGC   = /opt/m1750-ada$(VER)/bin/m1750-coff-
CC    = $(XGC)gcc
RUN    = $(XGC)run
LDFLAGS = -T mon1750.M -Wl,-Map=mon1750.map
RUNFLAGS = -bcyprsES -a "-bt -cpu mas31750 -uart1 1750 -freq 16"

# if use with MAS31750SBC, add -D __USE_MAS31750SBC=1 
# if use 2 UARTs, add -D __DUAL_UART=1
#CFLAGS = -O3 -g -D __DUAL_UART=1 -D __USE_MAS31750SBC=1
# -D __USE_TIMEOUT
CFLAGS = -g -Wall 

TARGET = mon1750

all: hex 

hex:  $(TARGET) prom 
# PROM->SRAM loader at head
	cp prom.hex mon1750_prom.hex
# Move mon1750 obj code to PROM space @0x200 
	$(XGC)objcopy -O ihex --change-address=512 mon1750 mon1750_tmp.hex
# Remove Start Address line
	sed "/:040*30*200F7/d" mon1750_tmp.hex >> mon1750_prom.hex
	-rm -f prom mon1750_tmp.hex   
# Split to two 8bit PROM images, High and Low 
	$(XGC)objcopy -O ihex -i2 -b0 --gap-fill 0x74 --pad-to 0x2000 mon1750_prom.hex  mon1750_h.hex
	$(XGC)objcopy -O ihex -i2 -b1 --gap-fill 0x00 --pad-to 0x2000 mon1750_prom.hex  mon1750_l.hex

ram: $(TARGET)
	$(XGC)objcopy -O ihex $< $<_ram.hex

downld: ram
	cp $(TAREGT)_ram.hex /dev/ttyS0	

asm:    $(TARGET)
	$(XGC)objdump -Sa $< > $<.asm

mon1750: mon1750.o 

prom:	prom.o
	$(CC) -o $@ -T prom.M $<
	$(XGC)objcopy -O ihex prom prom_tmp.hex
	sed "/^:0*1FF/d"  prom_tmp.hex > prom.hex         # remove the end flag	
	-rm -f prom prom_tmp.hex

run:	$(TARGET)
	$(RUN) $(RUNFLAGS) $< 

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
