/* taken from arch/ppc/kernel/ppc-stub.c */

/****************************************************************************

		THIS SOFTWARE IS NOT COPYRIGHTED

   HP offers the following for use in the public domain.  HP makes no
   warranty with regard to the software or its performance and the
   user accepts the software "AS IS" with all faults.

   HP DISCLAIMS ANY WARRANTIES, EXPRESS OR IMPLIED, WITH REGARD
   TO THIS SOFTWARE INCLUDING BUT NOT LIMITED TO THE WARRANTIES
   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

****************************************************************************/

/****************************************************************************
 *  Header: remcom.c,v 1.34 91/03/09 12:29:49 glenne Exp $
 *
 *  Module name: remcom.c $
 *  Revision: 1.34 $
 *  Date: 91/03/09 12:29:49 $
 *  Contributor:     Lake Stevens Instrument Division$
 *
 *  Description:     low level support for gdb debugger. $
 *
 *  Considerations:  only works on target hardware $
 *
 *  Written by:      Glenn Engel $
 *  ModuleState:     Experimental $
 *
 *  NOTES:           See Below $
 *
 *  Modified for SPARC by Stu Grossman, Cygnus Support.
 *
 *  This code has been extensively tested on the Fujitsu SPARClite demo board.
 *
 *  To enable debugger support, two things need to happen.  One, a
 *  call to set_debug_traps() is necessary in order to allow any breakpoints
 *  or error conditions to be properly intercepted and reported to gdb.
 *  Two, a breakpoint needs to be generated to begin communication.  This
 *  is most easily accomplished by a call to breakpoint().  Breakpoint()
 *  simulates a breakpoint by executing a trap #1.
 *
 *************
 *
 *    The following gdb commands are supported:
 *
 * command          function                               Return value
 *
 *    g             return the value of the CPU registers  hex data or ENN
 *    G             set the value of the CPU registers     OK or ENN
 *    qOffsets      Get section offsets.  Reply is Text=xxx;Data=yyy;Bss=zzz
 *
 *    mAA..AA,LLLL  Read LLLL bytes at address AA..AA      hex data or ENN
 *    MAA..AA,LLLL: Write LLLL bytes at address AA.AA      OK or ENN
 *
 *    c             Resume at current address              SNN   ( signal NN)
 *    cAA..AA       Continue at address AA..AA             SNN
 *
 *    s             Step one instruction                   SNN
 *    sAA..AA       Step one instruction from AA..AA       SNN
 *
 *    k             kill
 *
 *    ?             What was the last sigval ?             SNN   (signal NN)
 *
 *    bBB..BB	    Set baud rate to BB..BB		   OK or BNN, then sets
 *							   baud rate
 *
 * All commands and responses are sent with a packet which includes a
 * checksum.  A packet consists of
 *
 * $<packet info>#<checksum>.
 *
 * where
 * <packet info> :: <characters representing the command or response>
 * <checksum>    :: <two hex digits computed as modulo 256 sum of <packetinfo>>
 *
 * When a packet is received, it is first acknowledged with either '+' or '-'.
 * '+' indicates a successful transfer.  '-' indicates a failed transfer.
 *
 * Example:
 *
 * Host:                  Reply:
 * $m0,10#2a               +$00010203040506070809101112131415#42
 *
 ****************************************************************************/

#include <ppcboot.h>

#include <kgdb.h>
#include <command.h>

#if (CONFIG_COMMANDS & CFG_CMD_KGDB)

/*
 * BUFMAX defines the maximum number of characters in inbound/outbound buffers
 */
#define BUFMAX 1024
static char remcomInBuffer[BUFMAX];
static char remcomOutBuffer[BUFMAX];
static char remcomRegBuffer[BUFMAX];

static int initialized = 0;
static int kgdb_active = 0;
static u_int error_jmp_buf[BUFMAX/2];
static int longjmp_on_fault = 0;
#ifdef KGDB_DEBUG
static int kdebug = 1;
#endif

static const char hexchars[]="0123456789abcdef";

/* Convert ch from a hex digit to an int */
static int
hex(unsigned char ch)
{
	if (ch >= 'a' && ch <= 'f')
		return ch-'a'+10;
	if (ch >= '0' && ch <= '9')
		return ch-'0';
	if (ch >= 'A' && ch <= 'F')
		return ch-'A'+10;
	return -1;
}

/* Convert the memory pointed to by mem into hex, placing result in buf.
 * Return a pointer to the last char put in buf (null).
 */
static unsigned char *
mem2hex(char *mem, char *buf, int count)
{
	unsigned char ch;

	longjmp_on_fault = 1;
	while (count-- > 0) {
		ch = *mem++;
		*buf++ = hexchars[ch >> 4];
		*buf++ = hexchars[ch & 0xf];
	}
	*buf = 0;
	longjmp_on_fault = 0;
	return buf;
}

/* convert the hex array pointed to by buf into binary to be placed in mem
 * return a pointer to the character AFTER the last byte fetched from buf.
*/
static char *
hex2mem(char *buf, char *mem, int count)
{
	int i, hexValue;
	unsigned char ch;
	char *mem_start = mem;

	longjmp_on_fault = 1;
	for (i=0; i<count; i++) {
		if ((hexValue = hex(*buf++)) < 0)
			kgdb_error(KGDBERR_NOTHEXDIG);
		ch = hexValue << 4;
		if ((hexValue = hex(*buf++)) < 0)
			kgdb_error(KGDBERR_NOTHEXDIG);
		ch |= hexValue;
		*mem++ = ch;
	}
	kgdb_flush_cache_range((void *)mem_start, (void *)(mem - 1));
	longjmp_on_fault = 0;

	return buf;
}

/*
 * While we find nice hex chars, build an int.
 * Return number of chars processed.
 */
static int
hexToInt(char **ptr, int *intValue)
{
	int numChars = 0;
	int hexValue;

	*intValue = 0;

	longjmp_on_fault = 1;
	while (**ptr) {
		hexValue = hex(**ptr);
		if (hexValue < 0)
			break;

		*intValue = (*intValue << 4) | hexValue;
		numChars ++;

		(*ptr)++;
	}
	longjmp_on_fault = 0;

	return (numChars);
}

/* scan for the sequence $<data>#<checksum>     */
static void
getpacket(char *buffer)
{
	unsigned char checksum;
	unsigned char xmitcsum;
	int i;
	int count;
	unsigned char ch;

	do {
		/* wait around for the start character, ignore all other
		 * characters */
		while ((ch = (getDebugChar() & 0x7f)) != '$') {
#ifdef KGDB_DEBUG
			serial_putc(ch);
#endif
			;
		}

		checksum = 0;
		xmitcsum = -1;

		count = 0;

		/* now, read until a # or end of buffer is found */
		while (count < BUFMAX) {
			ch = getDebugChar() & 0x7f;
			if (ch == '#')
				break;
			checksum = checksum + ch;
			buffer[count] = ch;
			count = count + 1;
		}

		if (count >= BUFMAX)
			continue;

		buffer[count] = 0;

		if (ch == '#') {
			xmitcsum = hex(getDebugChar() & 0x7f) << 4;
			xmitcsum |= hex(getDebugChar() & 0x7f);
			if (checksum != xmitcsum)
				putDebugChar('-');	/* failed checksum */
			else {
				putDebugChar('+'); /* successful transfer */
				/* if a sequence char is present, reply the ID */
				if (buffer[2] == ':') {
					putDebugChar(buffer[0]);
					putDebugChar(buffer[1]);
					/* remove sequence chars from buffer */
					count = strlen(buffer);
					for (i=3; i <= count; i++)
						buffer[i-3] = buffer[i];
				}
			}
		}
	} while (checksum != xmitcsum);
}

/* send the packet in buffer.  */
static void
putpacket(unsigned char *buffer)
{
	unsigned char checksum;
	int count;
	unsigned char ch, recv;

	/*  $<packet info>#<checksum>. */
	do {
		putDebugChar('$');
		checksum = 0;
		count = 0;

		while ((ch = buffer[count])) {
			putDebugChar(ch);
			checksum += ch;
			count += 1;
		}

		putDebugChar('#');
		putDebugChar(hexchars[checksum >> 4]);
		putDebugChar(hexchars[checksum & 0xf]);
		recv = getDebugChar();
	} while ((recv & 0x7f) != '+');
}

/*
 * This function does all command processing for interfacing to gdb.
 */
static int
handle_exception (struct pt_regs *regs)
{
	int addr;
	int length;
	char *ptr;
	kgdb_data kd;
	int i;

	if (!initialized) {
		printf("kgdb: exception before kgdb is initialized! huh?\n");
		return (0);
	}

	if (longjmp_on_fault) {
		longjmp_on_fault = 0;
		kgdb_longjmp((long*)error_jmp_buf, KGDBERR_MEMFAULT);
		panic("kgdb longjump failed!\n");
	}

	if (kgdb_active) {
		printf("kgdb: unexpected exception from within kgdb\n");
		return (0);
	}
	kgdb_active = 1;

	kgdb_interruptible(0);

	printf("kgdb: entering handle_exception; trap [0x%x]\n",
	       kgdb_trap(regs));

	if (kgdb_setjmp((long*)error_jmp_buf) != 0)
		panic("kgdb: error or fault in entry init!\n");

	kgdb_enter(regs, &kd);

	ptr = remcomOutBuffer;

	*ptr++ = 'T';

	*ptr++ = hexchars[kd.sigval >> 4];
	*ptr++ = hexchars[kd.sigval & 0xf];

	for (i = 0; i < kd.nregs; i++) {
		kgdb_reg *rp = &kd.regs[i];

		*ptr++ = hexchars[rp->num >> 4];
		*ptr++ = hexchars[rp->num & 0xf];
		*ptr++ = ':';
		ptr = mem2hex((char *)&rp->val, ptr, 4);
		*ptr++ = ';';
	}

	*ptr = 0;

#ifdef KGDB_DEBUG
	if (kdebug)
		printf("kgdb: remcomOutBuffer: %s\n", remcomOutBuffer);
#endif

	putpacket(remcomOutBuffer);

	while (1) {
		volatile int errnum;

		remcomOutBuffer[0] = 0;

		getpacket(remcomInBuffer);

		errnum = kgdb_setjmp((long*)error_jmp_buf);

		if (errnum == 0) switch (remcomInBuffer[0]) {

		case '?':               /* report most recent signal */
			remcomOutBuffer[0] = 'S';
			remcomOutBuffer[1] = hexchars[kd.sigval >> 4];
			remcomOutBuffer[2] = hexchars[kd.sigval & 0xf];
			remcomOutBuffer[3] = 0;
			break;

#ifdef KGDB_DEBUG
		case 'd':
			/* toggle debug flag */
			kdebug ^= 1;
			break;
#endif

		case 'g':	/* return the value of the CPU registers. */
			length = kgdb_getregs(regs, remcomRegBuffer, BUFMAX);
			mem2hex(remcomRegBuffer, remcomOutBuffer, length);
			break;

		case 'G':   /* set the value of the CPU registers */
			length = strlen(&remcomInBuffer[1]);
			if ((length & 1) != 0) kgdb_error(KGDBERR_BADPARAMS);
			hex2mem(&remcomInBuffer[1], remcomRegBuffer, length);
			kgdb_putregs(regs, remcomRegBuffer, length);
			strcpy(remcomOutBuffer,"OK");
			break;

		case 'H':
			/* don't do anything, yet, just acknowledge */
			if (hexToInt(&ptr, &addr))
				strcpy(remcomOutBuffer,"OK");
			else
				kgdb_error(KGDBERR_BADPARAMS);
			break;

		case 'm':	/* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
				/* Try to read %x,%x.  */

			ptr = &remcomInBuffer[1];

			if (hexToInt(&ptr, &addr)
			    && *ptr++ == ','
			    && hexToInt(&ptr, &length))	{
				mem2hex((char *)addr, remcomOutBuffer, length);
			} else {
				kgdb_error(KGDBERR_BADPARAMS);
			}
			break;

		case 'M': /* MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK */
			/* Try to read '%x,%x:'.  */

			ptr = &remcomInBuffer[1];

			if (hexToInt(&ptr, &addr)
			    && *ptr++ == ','
			    && hexToInt(&ptr, &length)
			    && *ptr++ == ':') {
				hex2mem(ptr, (char *)addr, length);
				strcpy(remcomOutBuffer, "OK");
			} else {
				kgdb_error(KGDBERR_BADPARAMS);
			}
			break;


		case 'k':    /* kill the program, actually just continue */
			kd.extype = KGDBEXIT_KILL;
			goto doexit;

		case 'c':    /* cAA..AA  Continue; address AA..AA optional */
			/* try to read optional parameter, pc unchanged if no parm */
			kd.extype = KGDBEXIT_CONTINUE;

			ptr = &remcomInBuffer[1];
			if (hexToInt(&ptr, &addr)) {
				kd.exaddr = addr;
				kd.extype |= KGDBEXIT_WITHADDR;
			}

			goto doexit;

		case 's':
			kd.extype = KGDBEXIT_SINGLE;

		doexit:
/* Need to flush the instruction cache here, as we may have deposited a
 * breakpoint, and the icache probably has no way of knowing that a data ref to
 * some location may have changed something that is in the instruction cache.
 */
			kgdb_flush_cache_all();
			kgdb_exit(regs, &kd);
			kgdb_active = 0;
			kgdb_interruptible(1);
			return (1);

		case 'r':		/* Reset (if user process..exit ???)*/
			panic("kgdb reset.");
			break;
		}			/* switch */

		if (errnum != 0)
			sprintf(remcomOutBuffer, "E%02d", errnum);

#ifdef KGDB_DEBUG
		if (remcomOutBuffer[0] && kdebug) {
			printf("kgdb:  remcomInBuffer: %s\n", remcomInBuffer);
			printf("kgdb: remcomOutBuffer: %s\n", remcomOutBuffer);
		}
#endif

		/* reply to the request */
		putpacket(remcomOutBuffer);

	} /* while(1) */
}

/*
 * kgdb_init must be called *after* the
 * monitor is relocated into ram
 */
void
kgdb_init(void)
{

	kgdb_serial_init();
	debugger_exception_handler = handle_exception;
	initialized = 1;

	putDebugStr("kgdb ready\n");
	printf("[handler = 0x%08lx] ready\n",
		(unsigned long)debugger_exception_handler);
}

void
kgdb_error(int errnum)
{
	longjmp_on_fault = 0;
	kgdb_longjmp((long*)error_jmp_buf, errnum);
	panic("kgdb_error: longjmp failed!\n");
}

/* Output string in GDB O-packet format if GDB has connected. If nothing
   output, returns 0 (caller must then handle output). */
int
kgdb_output_string (const char* s, unsigned int count)
{
	char buffer[512];

	count = (count <= (sizeof(buffer) / 2 - 2)) 
		? count : (sizeof(buffer) / 2 - 2);

	buffer[0] = 'O';
	mem2hex ((char *)s, &buffer[1], count);
	putpacket(buffer);

	return 1;
}

void
breakpoint(void)
{
	if (!initialized) {
		printf("breakpoint() called b4 kgdb init\n");
		return;
	}

	kgdb_breakpoint();
}

void
do_kgdb(cmd_tbl_t *cmdtp, bd_t *bd, int flag, int argc, char *argv[])
{
    printf("Entering KGDB mode via exception handler...\n\n");
    breakpoint();
    printf("\nReturned from KGDB mode\n");
}

#else

int kgdb_not_configured = 1;

#endif /* CFG_CMD_KGDB */
