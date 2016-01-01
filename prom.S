/*********************************************************
 * 1750 PROM Loader
 *
 * Yao Fei 	
 *
 * 从0x100处加载PROM程序
 *********************************************************/

/*************************************************************************
 *
 * start
 *
 * This is address 0x00000, the location of the instruction executed on
 * power up. See MIL-STD-1750A Section 4.4.4. Note: we have only 16#40#
 * bytes of space before the interrupt vector.
 * 
 *************************************************************************/


	.section .init,"x"
	.global start

        /* 上电入口, 0地址 */
start:
	xio  r0, dsbl           // CLI	
        lim  r6, 256
__bpu_wt:
        soj r6, __bpu_wt	// wait BPU

	j __rst

/*************************************************************************
 *
 * __ivec
 *
 * The interrupt vector and interrupt handlers. The interrupt mechanism is
 * described in MIL-STD-1750A Section 4.6. The linker script file should
 * locate the .init section at zero, so that the label start is at address
 * 0x00000 and the block of pointers is at address 0x00040. 
 *
 * We declare the interrupt vectors in a read-only section so that the
 * objdump program and the simulator's coverage analysis feature do not
 * mistake the contents for executable instructions.
 *
 *************************************************************************/

        /* Word addresses 20 to 3f are reserved for the interrupt vectors.
	* See MIL-STD-1750A Section 4.6.1.4.  */

        .section .ivec,"r"

	/* in PROM Loader, disable all interrupts. */
_ivec:
	.word 0,0
	.word 0,0
	.word 0,0
	.word 0,0
	.word 0,0
	.word 0,0
	.word 0,0
	.word 0,0
	.word 0,0
	.word 0,0
	.word 0,0
	.word 0,0
	.word 0,0
	.word 0,0
	.word 0,0
	.word 0,0

	 .section .text0,"x"

        /* 真正的入口  */
__rst:	
	/* For P1750 */
        /* set configure register of P1754 */
	// Read the PIC status reg 
	xio   r0, PI+0x1F41
	cim   r0, 0xffff
	bez   __no_p1754

	/* control register 	*/
        lim   r0, 0x0740       // EST, EAD, EXP, CNF 
        xio   r0, 0x1f40

        /* memory ready program register */
        lim   r0, 0x2222
        xio   r0, 0x1f42

	/* I/O ready program register */
        lim   r0, 0x4040
        xio   r0, 0x1f43

	/* program register */
        lim   r0, 0x5040
        xio   r0, 0x1f44

	/* unimplemented memory register */
        lim   r0, 0xffff      /* No use now */
        xio   r0, 0x1F46

	/* first unimplemented output command */
        xorr  r0, r0 
        xio   r0, 0x1F47
        /* first unimplemented input command */
        xio   r0, 0x1F48


__no_p1754:
        /* set configure register of P1753 */
        /* control register 1*/
        lim   r0, 0xC3FF      /* C3FF is default value */
        xio   r0, 0x1f51

	/* FIX ME */
	/* Add init code for your devices */

	/* ... */
__move:
        /* 1st, copy 0@PROM to 0@SRAM , only prom.S */ 
        /* In StartUp ROM mode, read from PROM, write to SRAM */
        xorr r2, r2
        xorr r0, r0
        lim  r1, 0x100          // length of PROM.o 
        mov  r0, r2             // PROM 0 -> SRAM0

        /* 2nd, copy whole 8K PROM to 32K @ SRAM    */
	xorr r2, r2             // from PROM 0
	lim  r0, 0x8000		// target
	lim  r1, 0x2000		// length of 8K PROM chip
	mov  r0, r2		// move it

        xio  r0, dsur           // Disable SUR
				// what happen now? 
	l    r0, __start        // Check memory	
	lim  r1, 0x8000 
	c    r0, __start, r1
	bnz  __mem_err
	jc   uc, __start, r1	// Jump to skip ivec
	
__mem_err:	                // If not found SRAM @ 32K, display 'S' for ever
	xio  r0, pi+0x501
	tbr  13, r0
       	bez   __mem_err
	lim  r0, 'S'
	xio  r0, 0x500          // UART
	br   __mem_err
	
	/* 
	 * Flowing should running at 0x8000  + __start
	 */
__start:
	xorr r0, r0
        lim  r2, 0x8100		// User code stored begin at 0x100 (sise_mon.o)
        lim  r1, 0x1f00		// Lenght 8Kword-256
        mov  r0, r2		// Move it
	
	lim  r0, 0xa000
	lim  r1, 0x5000
	lim  r2, 0x8000
	mov  r0, r2
	
_quit_code:	                // Quit code 
	sr   r15,r15
	sr   r0, r0
	xio  R0, WSW
        xio  R0, SMK  
        xio  R0, TAH
        xio  R0, TBH 
        xio  R0, RPI
        xio  R0, GO
        xio  R0, RCFR
	xio  R0, CLIR
	jc   uc, 0x0		// start user program
