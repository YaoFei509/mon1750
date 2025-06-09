# mon1750
A generic ROM HEX loader for MIL-STD-1750A based computers

Universal monitor for MIL-STD-1750A based computer.

  Support various 1750A based computer, such as GEC/Plessy (Dynex) SBC31750. Receive Intel HEX format object code, and run it.

  This monitor will copy itself from PROM to RAM after power-up or RESET, so the computer must support StartUp ROM mode, starting from 0, and at least 48K SRAM from 0.  

  while uploading user application code from UART, monitor use EXPAND memory to store it temporily, so the computer must have enough EXPAND memory.
 
