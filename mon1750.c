/**************************************************************************
 * 
 * Filename:
 * 
 *   mon1750.c 
 *
 * Description:
 *
 *   This is a generic monitor for MAS31750SBC and other 1750A based computer 
 *  
 *   If use with single UART, it receives Intel HEX code from UART 1 ( 8251 @ 0x500/501)
 *   if use with dual UARTs:
 *     for MAS31750SBC , receives HEX code form UART 2 ( 8251 @ 0x520/521)
 *     for my target,    receives HEX code form UART 2 ( 8251 @ 0x600/601)
 *
 *   Auto detects the mechism of expanded memory.
 *     * window page register
 *     * BX1752
 *     * for 31750SBC or w/ expanded memory, use High 32KW memory 
 *     * for P1750, use P1753/1754 MMU
 *
 **************************************************************************/
#include <intrrpt.h>
#include <sys/kernel.h>

#ifdef __DEBUG
#include <stdio.h>
#endif 

/*-----------------------  配置区域 ----------------------------------*/
 
/*  If use with MAS31750 SBC, uncomment follow line */
//  #define __USE_MAS31750SBC  1 

/*  If use with dual UARTs , uncomment follow line */
//  #define __DUAL_UART 1 

//  If use very low speed CPU, as P1750A@10MHz, disable timeout check 
#define __USE_TIMEOUT 1

// UART1
#define UART1_DATA 0x500
#define UART1_CTL  0x501

// UART2 
#ifdef __USE_MAS31750SBC
// for Dynex MAS31750 SBC 
#define UART2_DATA 0x520
#define UART2_CTL  0x521
#else
// for SISE/809 computer
#define UART2_DATA 0x600
#define UART2_CTL  0x601
#endif

#ifndef __DUAL_UART
// Single UART
#undef UART2_DATA
#undef UART2_CTL
#define UART2_DATA UART1_DATA
#define UART2_CTL  UART1_CTL
#endif


/*--------------------------------------------------------------------*/

#define EEPROM_PAGE_BASE   512/4     /* EEPROM的起始页                */
#define PAGE_BASE          0xf000    /* 映射页的起始地址              */

/**********************************************************************/
typedef unsigned long ADDR32;
typedef enum { 
	USE_WINDOW_REG   = 0x2222,
	USE_MMU          = 0x3333,
	USE_P1754        = 0x4444,
	NO_MASS          = 0xffff,
	MMU_EXIST_31750  = 0x9400,
	MMU_EXIST_BX1750 = 0x2000
} MASS_CFG;

static inline ADDR32
addr32 (short *addr16)
{
	return (ADDR32) ((unsigned) addr16) << 1;
}

static unsigned volatile seg           = 0;                     /* code segment  */
static ADDR32            start_address;                         /* user program entry point */
static MASS_CFG          MassMemory;

static unsigned          MaxMemory     = EEPROM_PAGE_BASE - 8;  /* Maxium memory */
static unsigned          page_size     = 4096;

// 1750A XIO read 
#define XIO_READ 0x8000

/*
 * Convert a hex character, '0' to '9', 'a' to 'f', or 'A' to 'F' to an
 * integer in the range 0 .. 15.  Return -1 if the given character is
 * not a hex character.
 */
#if __USE_TIMEOUT
unsigned inline  
#else
unsigned
#endif 
hex (unsigned char ch)
{
	if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 10;
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	if (ch >= 'A' && ch <= 'F')
		return ch - 'A' + 10;
	return -1;
}

//  ---------  STDIO  ------------------
static inline void
uart_putc (char c)
/*
 * Write one character to the console @UART1. We use the 8251 USART.
 */
{
	int reg;
	
	/* Wait until transmit buffer empty */
	do {
		asm volatile ("xio %0,%1" : "=r" (reg) : "i" (UART1_CTL));
	} while (!(reg & 0x04));
	
	/* Transmit one character */
	asm volatile ("xio %0,%1"::"r" (c), "i" (UART1_DATA));
}

#ifndef _STDIO_H_
int puts(char *buf)
{
	int i;
	
	for (i = 0; buf[i]; i++) {
		uart_putc (buf[i]);
		if (buf[i] == '\n')
			uart_putc ('\r');
	}
	return i;
}
#endif


/* Read the char from UART */
int inline 
get_debug_char ()
{
	int c = 0;
	int reg;
#if __USE_TIMEOUT
	unsigned timeout = 0;
#endif

	/* Wait until receive buffer not empty. Keep watchdog happy. */
	do {
		asm volatile ("xio  r0, go");
		asm volatile ("xio  %0, %1":"=r" (reg):"i"(UART2_CTL + XIO_READ));
#ifdef __USE_TIMEOUT
		timeout++; 
	} while ((!(reg & 0x02)) && (timeout > 0));   
 
	if (timeout == 0) {        
		return -1;
	}
#else
        } while (!(reg & 0x02));
#endif


	/* Get one 8-bit character.  */
	asm volatile ("xio  %0, %1":"=r" (c): "i"(UART2_DATA + XIO_READ));

        return (c & 0xff);
}

/* Clear the UART, Tartan code need it! */
inline void quiet()
{
#ifdef __USE_TIMEOUT
	while (get_debug_char() != -1 ) {};
#endif
}

// get byte
unsigned  inline get_byte() 
{
	unsigned a;

	a = hex(get_debug_char()) << 4;
	a += hex(get_debug_char());

	a &= 0xff;
	return a;
} 

//--------------------------------------------------------------------------------
// check if the mass memory exist

MASS_CFG check_mass()
{
	MASS_CFG mass = NO_MASS;
	int i;
	unsigned reg;

	puts("Check MMU ....");

	reg = 14;
	asm volatile("xio %0, wopr+14"::"r"(reg));  // write
	asm volatile("xio %0, ropr+14":"=r"(reg));  // read 

	if (14 == reg)  {          // make sure about it
		unsigned volatile *p = (unsigned *)(PAGE_BASE + 4095);
		reg = 31;
		do {
			asm volatile ("xio %0, wopr+15"::"r"(reg));
			reg+=16;
			*p = reg;
		} while ( (*p == reg) && (reg < (EEPROM_PAGE_BASE + 16)) );
		MaxMemory = (reg - 15 - 16 - 8);

		// 恢复F000
		reg = 15;
		asm volatile ("xio  %0, wopr+15"::"r"(reg));
		
		mass = USE_MMU;
		puts("Found : ");

		if (MaxMemory == (EEPROM_PAGE_BASE - 8))
			puts("512K ");
		
		// check P1754
		asm volatile ("xio %0, 0x9f41":"=r"(i));
		if (i != 0xffff ) { 
			mass = USE_P1754;
		} 
	} else {
		puts("Not found.\nCheck in-house page window register...");

		for (i=0; i<32767; i++)  // wait xTU mass memory FPGA loading
			asm volatile ("xio %0, 0x0110"::"r"(i));
		
		asm volatile ("xio %0, 0x8110":"=r"(reg));

		if (-1 != reg)  {
		        puts("Found!\n");
			mass = USE_WINDOW_REG;
	        } else { 
			puts("Not Found!\n");
		}
	}

	return mass;
}

#if __USE_TIMEOUT 
void  inline 
#else
void
#endif
SetPage(unsigned  page)
{
	switch (MassMemory) {
	case USE_MMU: 
	case USE_P1754:
		asm volatile ("xio %0, wopr+15"::"r"(page));  //写页面寄存器520F
		break;
		
	case USE_WINDOW_REG:
		asm volatile ("xio %0,  0x0110"::"r"(page));
		break;

	default:
		break;
	}
}

/* Init the UART, and check MMU or MassMemory config */
// init the 8251 of SBC31750 
// for xTU, UART needn't Init
MASS_CFG inline sbc_init()
{
//对于UART是用FPGA实现的，不需要初始化
#ifdef __USE_MAS31750SBC
	asm (" 
         xorr r0,r0
         xio r0, dsbl
         xio r0, 0x521
         xio r0, 0x501
         xio r0, 0x521
         xio r0, 0x501
         xio r0, 0x521
         xio r0, 0x501

         sbr 9,  r0
         xio r0, 0x521
         xio r0, 0x501

         lim r0, 0x00ce
         xio r0, 0x521
         xio r0, 0x501

         lim r0, 0x0027
         xio r0, 0x521
         xio r0, 0x501
         xio r0, enbl
        ");
#endif 
	return check_mass(); // for xTU, check mass memory 
}

/*
 * Reset computer
 * 把暂存在高端内存或者数据缓冲区的代码拷贝到0开始
 */
void reset_stub() 
{
	register int i, j = MaxMemory;

	asm (" 
         xorr R0, R0
         xio  R0, WSW
         xio  R0, SMK  
         xio  R0, TAH
         xio  R0, TBH 
         xio  R0, RPI
         xio  R0, GO
         xio  R0, RCFR
         ");

	switch (MassMemory) {
	case USE_WINDOW_REG: 
		// for xTU , 52K RAM is valid , 0xe000~0xefff is used by device.
		for (i=0; i<64-12; i++) {
			asm("xio %0, 0x0110"::"r"(i+350));
			memcpy((void *)(i*1024), (void *)PAGE_BASE, 1024);
		}

		i = 511; 
		asm("xio %0, 0x0110"::"r"(i));

		break;

	case USE_MMU:
	case USE_P1754:
		for (i=0; i<8; i++) {
			asm("xio %0, wopr+15"::"r"(i+j));
			memcpy((void *)(i*4096), (void *)PAGE_BASE, 4096);  //把120-127页的内容拷贝到0-7页
		}

		i = 15; 
		asm volatile ("xio  %0, wopr+15"::"r"(i));  //恢复页缓冲区即F000开始的一页
		
		break;
		
	case NO_MASS:
	default:
		memcpy((void *)0, (void *)0x8000, 0x5800);
	}

	/* enable EDAC and jump to user code */
	asm("xio  R0,  CLIR
             sr   R15, R15
             sr   R14, R14
             jci  uc,  _sistack    
        ");   //无条件跳转到用户代码
}


// EDAC 0-767K
#define EDAC_DISABLE_WORD 0xe0c6
#define EDAC_ENABLE_WORD  0xe6c6

// 禁止读操作的EDAC检查, 但是写操作会产生校验码
void inline disable_edac()
{
	int i = EDAC_DISABLE_WORD;          // c0c6 or e0c6 ?

	asm ("xio  %0, 0x1f50"::"r"(i));
}

// 容许读操作的EDAC
void inline enable_edac()
{
	int i = EDAC_ENABLE_WORD; 

	asm ("xio  %0, 0x1f50"::"r"(i));
}


/*
 *  把复位代码转移到中断堆栈  move the reset stub to start of istack 
 */
extern unsigned _sistack;
void yfreset()
{
	unsigned *p, *rs;
	unsigned sa = (unsigned)(start_address & 0xffff);

	// for GCC-1750, we need right shift, 
	// but for TADS, we dont't need ... Oops..
	// 0x001eW for Tartan w/o expanded memory
	// 0x0240W for Tartan w/  expanded memory. fucking.
	if ((start_address != 0x0000001eL) 
	    && ( start_address != 0x00000240L)) {
		start_address  >>= 1;  // byte addr to word addr
		puts("Reseting.    ");
	}
	else 
		puts("Tartan code? ");

	quiet();

	p = &_sistack;  //中断栈 字地址为0XDE00  》32K的地方
	*p = sa; 
	p++;

	rs = (unsigned *)(&reset_stub); 
	(unsigned)rs >>=1 ;      // byte address to word address
	memcpy(p, rs, 256);      //将reset_stub搬到_sistack开始的地方

	 //关中断
	//在_sistack开始的地方执行 reset_stub代码

	uart_putc('x');
	asm(" xio  R0, DSBL                  
              jc   uc, 0, %0"::"r"(p));   
}

// 下载HEX代码文件 Download the Intel HEX file 
// 2002年1月10日修改
// 2002年2月1日修改

enum {
	DATA        = 0, 
	EOF         = 1, 
	SEGADDR     = 2,
	STARTADDR   = 3,
	EXLINEADDR  = 4,
	STRLINEADDR = 5,
} HEXFILE;

int downldhex()
{
	unsigned data, *addr;
	unsigned len, i, clas, sum;
	int      page = 0, flag =0;

	start_address = (ADDR32)0L;

	while (1) {
		while (get_debug_char() != ':') {};  //Intel HEX file 的每一行都是从字符:开始的
		
		len = get_byte(); //有效数据的字节数，不包括和校验
		sum = len;
		len >>= 1;  // byte count to word count

		addr = (unsigned *)get_byte();
		sum += (unsigned)addr;
		(unsigned)addr *= 256;
		i = get_byte();
		sum += i;
		addr += i;
		(unsigned)addr /= 2;   // Byte address to Word address

		clas = get_byte();
		sum += clas;

		switch (clas) {
		case DATA: // Data 
			switch (MassMemory) {
			
			case USE_MMU:
				/*
				 * for seg 0x0x00 ( 0~ 32K code), tempory use the highest eight pages
				 * for seg 0x1x00, 0x2x00 and other, use its normal page
				 */
				
				page = seg/512 + (((unsigned)addr)/4096);  // seg/4096/2*16 gcc can't optimi
				//seg=0???
				// fucking tartan, It can send me a 0x800 segment address !
				if (page < 8)
					page += MaxMemory;

				SetPage(page);
				(unsigned)addr %= 4096;   //取页内偏移地址
				(unsigned)addr |= PAGE_BASE; //加上映射页的起始地址
				break;


			case USE_WINDOW_REG:
				// store it in Mass memory
				// 64 避开最低64K字空间
				page =  350 + (unsigned)seg/128 + (unsigned)addr/1024;  //seg/1024/2*16
				SetPage(page);
				(unsigned)addr %= 1024;
				(unsigned)addr |= PAGE_BASE; 
				break;

			default:
			case NO_MASS: 
				(unsigned)addr |= 0x8000;   // store into upper 32K word RAM

				if ((unsigned)addr > 0xd7f0) 
					puts("Too large, no enough memory\r");
				break;
			}

			for (;len;len--, addr++) {
				i = get_byte();
				sum += i;
				data = i << 8;
				i = get_byte();
				sum += i;
				data += i;
				*addr = data;  //addr为字地址，所以存放时按照字存放
			
				if ((MassMemory != NO_MASS) && 
				    (unsigned)addr == (PAGE_BASE + (page_size-1))) { 
					SetPage(page+1);                    //换页
					addr = (unsigned *)(PAGE_BASE - 1);
				}
			}

			sum += get_byte();  //和校验
			sum &= 0xff;
			if (sum) {
				puts("Recv: Checksum error, aborted...\n");
			}

			if (USE_MMU == MassMemory)
				SetPage(15);
			
			break;

		case EOF: // EOF flag
			if (get_byte() ==  0xff)  {
				puts("Receive success.\n");
				if (0 == flag)
					seg = 0;
				
				yfreset();				
			}
			break;

		case SEGADDR: // Segment addr
			i = get_byte();
			sum += i;
			seg = i << 8;
			i = get_byte();
			sum += i;
			seg += i;

			sum += get_byte();
			sum &= 0xff;
			
			if (sum) {
				puts("Seg addr: Checksum error , aborted ...\n");
			}

			if (seg > 0x2000) {
				puts("exceed 64K, too large.\n");
			}; 

			break;
			
		case STARTADDR: // start address    seg:offset   8086 format.
			i     = get_byte();
			sum  += i;
			data  = i << 8;
			i     = get_byte();
			sum  += i;
			data += i;

			seg = data;
			start_address = (ADDR32)data;  // seg addr shift 4 bits
			start_address <<= 4;
			flag = 1;

			i     = get_byte();
			sum  += i;
			data  = i << 8;
			i     = get_byte();
			sum  += i;
			data += i;

			start_address += (ADDR32)data;

	
			sum += get_byte(); 
			sum &= 0xff;
			
			if (sum) {
				puts("Checksum error , aborted ...\n\n");
			}

			break;
		case EXLINEADDR:
		case STRLINEADDR:
			puts("Unknow data. \n");
			break;

		default:
			puts("Error, aborted...\n");
			break;
		} // switch
	}
}

void
machine_error_handler (int signum, struct _iframe *ifp)
{
	char *fault_names[16] = {
		"CPU memory protection",
		"DMA memory protection",
		"Memory parity",
		"PIO channel parity",
		"DMA channel parity",
		"Illegal IO command",
		"PIO transmission",
		"Watchdog",
		"Illegal address",
		"Illegal instruction",
		"Privileged instruction",
		"Address state",
		"Bit 12",
		"Built-in test",
		"Bit 14",
		"Bit 15"
	};

	unsigned ft;

	puts ("\r\nMachine Error ");

	asm volatile ("xio    %0,rcfr":"=r" (ft));
	if (ft != 0x0000) {
		int i;

		puts("Faults are: \n");

		for (i = 0; i <= 15; i++) {
			unsigned mask = 0x0001 << (15 - i);

			if (ft & mask) {
				puts (": ");
				puts (fault_names[i]);
				puts (" fault\n");
			}
		}
	}
}

// copy each page to its self, init EDAC code 
void init_edac()
{
	int page; 
	void *p = (void *)PAGE_BASE; 
	
	disable_edac();

	for (page = 0 ; page < 512/4; page ++) {
		SetPage(page);
		memcpy(p, p, 4096); 
	}

	SetPage(15);
	enable_edac();
}


int main ()
{
	int i, n;

#ifdef _STDIO_H_
	// for default art0.S 
	__xgc_attach_sch();
#endif
	/*
	 * Print the subversion identifier on the console. 
	 */
	puts("\nUniversal monitor for MAS31750SBC/xTU (c)2000,2011 SISE\n");

	MassMemory = sbc_init();

	sys_handler (INTBUS, machine_error_handler);

	switch(MassMemory) {
	case USE_WINDOW_REG:
		page_size = 1024;
		puts("Use xTU mass memory.\n");
		break;

	case USE_P1754:
		puts("Use P1754 MMU.\n");
		init_edac();

#if __USE_TIMEOUT
		// wait user to press any key
		puts("Press <ENTER> to upload HEX code.\n");

		for (i=0; i<10; i++) {                 //about 8s
			n = get_debug_char();          //规定的时间内，串口有数据，则get_debug_char()返回，否则返回-1
			if (n != -1) 
				break;
		}
#endif

		break;
		
	case USE_MMU:
		puts("Use MAS31751/BX1752 MMU.\n");
		break;

	case NO_MASS:
	default:
		puts("Mass memory not found.\n");
		puts("Only support 22K word program. \n");
	}


#if __DUAL_UART
	puts("Support download Intel HEX file on USART2.\n");
#else
	puts("Support download Intel HEX file on USART. \n");
#endif	

        //从串口下载数据到扩展内存，
	downldhex();   
}
