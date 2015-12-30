# mon1750
A generic ROM HEX loader for MIL-STD-1750A based computers

SISE Universal monitor for 1750 computer.

  support various 1750A based computer. Receive Intel HEX format object code , and run it or store into EEPROM.

  Support patch handling in P1750 computer.

  This monitor will copy itself from PROM to RAM after power-up or RESET, so the computer must support StartUp ROM mode, starting from 0, and at least 48K SRAM from 0.  

  while uploading user application code from UART, monitor use EXPAND memory to store it temporily, so the computer must have enough EXPAND memory.
 