 /*
  * UAE - The Un*x Amiga Emulator
  *
  *  Serial Line Emulation
  *
  * (c) 1996, 1997 Stefan Reinauer <stepan@linux.de>
  * (c) 1997 Christian Schmitt <schmitt@freiburg.linux.de>
  *
  */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "uae.h"
#include "memory.h"
#include "custom.h"
#include "newcpu.h"
#include "cia.h"
#include "serial.h"

#ifdef SERIAL_PORT
# ifndef WIN32
#  include "osdep/serial.h"
# else
#  include "od-win32/parser.h"
# endif
#endif

#define SERIALDEBUG	0 /* 0, 1, 2 3 */
#define SERIALHSDEBUG	0
#define		TIOCM_LE	0001		/* line enable */
#define		TIOCM_DTR	0002		/* data terminal ready */
#define		TIOCM_RTS	0004		/* request to send */
#define		TIOCM_ST	0010		/* secondary transmit */
#define		TIOCM_SR	0020		/* secondary receive */
#define		TIOCM_CTS	0040		/* clear to send */
#define		TIOCM_CAR	0100		/* carrier detect */
#define		TIOCM_CD	TIOCM_CAR
#define		TIOCM_RNG	0200		/* ring */
#define		TIOCM_RI	TIOCM_RNG
#define		TIOCM_DSR	0400		/* data set ready */
#define MODEMTEST	0 /* 0 or 1 */
#define BUFLEN 8192
#define PORT 5556

void diep(char *s) {
    perror(s);
    exit(1);
}

static int doinit = 0;
static int offset = 0;
static int data_in_serdat;	/* new data written to SERDAT */
static int data_in_serdatr;	/* new data received */
static int data_in_sershift;	/* data transferred from SERDAT to shift register */
static uae_u16 serdatshift;	/* serial shift register */
static int ovrun;
static int dtr;
static int serial_period_hsyncs, serial_period_hsync_counter;
static int ninebit;
int serdev;
struct sockaddr_in si_me, si_other;
static int s, d, slen=sizeof(si_other), setup=0;
char buf[BUFLEN];

void serial_open(void);
void serial_close(void);

uae_u16 serper, serdat, serdatr;

static const int allowed_baudrates[] = {
    0, 110, 300, 600, 1200, 2400, 4800, 9600, 14400,
    19200, 31400, 38400, 57600, 115200, 128000, 256000, -1
};

void SERPER (uae_u16 w)
{
    int baud = 0, i, per;
    static int warned;

    if (serper == w)  /* don't set baudrate if it's already ok */
	return;
    ninebit = 0;
    serper = w;

    if (w & 0x8000) {
	if (!warned) {
	    write_log ( "SERIAL: program uses 9bit mode PC=%x\n", m68k_getpc (&regs) );
	    warned++;
	}
	ninebit = 1;
    }
    w &= 0x7fff;

    if (w < 13)
	w = 13;

    per = w;
    if (per == 0) per = 1;
    per = 3546895 / (per + 1);
    if (per <= 0) per = 1;
    i = 0;
    /*while (allowed_baudrates[i] >= 0 && per > allowed_baudrates[i] * 100 / 97)
	i++;
    baud = allowed_baudrates[i];*/
    baud = 2400;

    serial_period_hsyncs = (((serper & 0x7fff) + 1) * 10) / maxhpos;
    if (serial_period_hsyncs <= 0)
	serial_period_hsyncs = 1;
    serial_period_hsync_counter = 0;

    write_log ("SERIAL: period=%d, baud=%d, hsyncs=%d PC=%x\n", w, baud, serial_period_hsyncs, m68k_getpc (&regs));

    if (ninebit)
	baud *= 2;
    if (currprefs.serial_direct) {
	if (baud != 31400 && baud < 115200)
	    baud = 115200;
	serial_period_hsyncs = 1;
    }
#ifdef SERIAL_PORTS
    setbaud (baud);
#endif
}

static char dochar (int v)
{
    v &= 0xff;
    if (v >= 32 && v < 127) return (char)v;
    return '.';
}

static int readser(int *returndata) {
    //char *buf = "55AA412A009455AA4B0404130a0f341c00008a";
    if (!setup) {
        if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) diep("Socket error.");
        int flags = fcntl(s, F_GETFL);
        flags |= O_NONBLOCK;
        fcntl(s, F_SETFL, flags);
	
        memset((char *) &si_me, 0, sizeof(si_me));
        si_me.sin_family = AF_INET;
        si_me.sin_port = htons(PORT);
        si_me.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(s, &si_me, sizeof(si_me)) == -1) diep("bind");
    }
    setup=1;

    int recvdd = recvfrom(s, buf, BUFLEN, 0, &si_other, &slen);
    if (recvdd == -1) return 0;
    //if (recvdd == -1 && !doinit) return 0;
    doinit = 1;
    
    //write_log("%s", buf);
    //char *buf = "55AA412A009455AA463243303130383038474E414530314E4E4E4E4E4E4C32393036595959323333363036303135313030594E59438E384E4E4E4E4E311E";
    int i=0, j;
    char *hexPtr = buf;
    unsigned int *result = calloc(strlen(buf)/2 + 1, sizeof *result);
    
    while (sscanf(hexPtr, "%02x", &result[i++])) {
        hexPtr += 2;
        if (hexPtr >= buf + strlen(buf)) break;
    }
    /*if (offset > (i-1)) {
        offset = 0;
        doinit = 0;
        return 0;
    }*/
        
    //for (j = 0; j < i; j++) write_log("%c\n", result[j]);
    int lens = strlen(buf)/2;
    
    memcpy(returndata, result, 1);
    //memcpy(returndata, result+offset, 1);
    //offset += 1;
    return 1;
}

static void checkreceive (int mode)
{
#ifdef SERIAL_PORT
    static uae_u32 lastchartime;
    static int ninebitdata;
    struct timeval tv;
    char *buf = "55AA412A009455AA4B0404130a0f341c00008a";
    int recdata;

    //if (!readseravail ())
	//return;

    if (data_in_serdatr) {
	/* probably not needed but there may be programs that expect OVRUNs.. */
	gettimeofday (&tv, NULL);
	if (tv.tv_sec > lastchartime) {
	    ovrun = 1;
	    INTREQ (0x8000 | 0x0800);
	    while (readser (&recdata));
	    write_log ("SERIAL: overrun\n");
	}
	return;
    }

    if (ninebit) {
	for (;;) {
	    if (!readser (&recdata))
		return;
    /*int i=0, j;
        char *hexPtr = buf;
    
        while (sscanf(hexPtr, "%02x", &recdata[i++])) {
            hexPtr += 2;
            if (hexPtr >= buf + strlen(buf)) break;
        }
        for (j = 0; j < i; j++) printf("%c", recdata[j]);*/
	    if (ninebitdata) {
		serdatr = (ninebitdata & 1) << 8;
		serdatr |= recdata;
		serdatr |= 0x200;
		ninebitdata = 0;
		break;
	    } else {
		ninebitdata = recdata;
		if ((ninebitdata & ~1) != 0xa8) {
		    write_log ("SERIAL: 9-bit serial emulation sync lost, %02.2X != %02.2X\n", ninebitdata & ~1, 0xa8);
		    ninebitdata = 0;
		    return;
		}
		continue;
	    }
	}
    } else {
	int returnvalue = readser(&recdata);
	    if (!returnvalue) return;
   /* int i=0, j;
    char *hexPtr = buf;

    while (sscanf(hexPtr, "%02x", &recdata[i++])) {
        hexPtr += 2;
        if (hexPtr >= buf + strlen(buf)) break;
    }
    for (j = 0; j < i; j++) printf("%c", recdata[j]);*/
    serdatr = recdata;
	serdatr |= 0x100;
    }
    gettimeofday (&tv, NULL);
    lastchartime = tv.tv_sec + 5;
    data_in_serdatr = 1;
    serial_check_irq ();
    write_log ("SERIAL: received %02.2X (%c)\n", serdatr & 0xff, dochar (serdatr));
#endif

}

static void checksend (int mode)
{
    int bufstate;

#ifdef SERIAL_PORTS
    bufstate = checkserwrite ();
#else
    bufstate = 1;
#endif

    if (!data_in_serdat && !data_in_sershift)
	return;

    if (data_in_sershift && mode == 0 && bufstate)
	data_in_sershift = 0;

    if (data_in_serdat && !data_in_sershift) {
	data_in_sershift = 1;
	serdatshift = serdat;
#ifdef SERIAL_PORTS
	if (ninebit)
	    writeser (((serdatshift >> 8) & 1) | 0xa8);
	writeser (serdatshift);
#endif
	data_in_serdat = 0;
	INTREQ (0x8000 | 0x0001);
#if SERIALDEBUG > 2
	write_log ("SERIAL: send %04.4X (%c)\n", serdatshift, dochar (serdatshift));
#endif
    }
}

void serial_hsynchandler (void)
{
    if (serial_period_hsyncs == 0)
	return;
    serial_period_hsync_counter++;
    if (serial_period_hsyncs == 1 || (serial_period_hsync_counter % (serial_period_hsyncs - 1)) == 0)
	checkreceive (0);
    if ((serial_period_hsync_counter % serial_period_hsyncs) == 0)
	checksend (0);
}

/* Not (fully) implemented yet: (on windows this work)
 *
 *  -  Something's wrong with the Interrupts.
 *     (NComm works, TERM does not. TERM switches to a
 *     blind mode after a connect and wait's for the end
 *     of an asynchronous read before switching blind
 *     mode off again. It never gets there on UAE :-< )
 *
 */

void SERDAT (uae_u16 w)
{
    serdat = w;

    if (!(w & 0x3ff)) {
#if SERIALDEBUG > 1
	write_log ("SERIAL: zero serial word written?! PC=%x\n", m68k_getpc (&regs));
#endif
	return;
    }

#if SERIALDEBUG > 1
    if (data_in_serdat) {
	write_log ("SERIAL: program wrote to SERDAT but old byte wasn't fetched yet\n");
    }
#endif

    data_in_serdat = 1;
    if (!data_in_sershift)
	checksend (1);

#if SERIALDEBUG > 2
    write_log ("SERIAL: wrote 0x%04x (%c) PC=%x\n", w, dochar (w), m68k_getpc (&regs));
#endif

    return;
}

uae_u16 SERDATR (void)
{
    serdatr &= 0x03ff;
    if (!data_in_serdat)
	serdatr |= 0x2000;
    if (!data_in_sershift)
	serdatr |= 0x1000;
    if (data_in_serdatr)
	serdatr |= 0x4000;
    if (ovrun)
	serdatr |= 0x8000;
#if SERIALDEBUG > 2
    write_log ( "SERIAL: read 0x%04.4x (%c) %x\n", serdatr, dochar (serdatr), m68k_getpc (&regs));
#endif
    ovrun = 0;
    data_in_serdatr = 0;
    return serdatr;
}

void serial_check_irq (void)
{
    if (data_in_serdatr)
	INTREQ_0 (0x8000 | 0x0800);
}

void serial_dtr_on(void)
{
#if SERIALHSDEBUG > 0
    write_log ("SERIAL: DTR on\n");
#endif
    dtr = 1;
    if (currprefs.serial_demand)
	serial_open ();
#ifdef SERIAL_PORTS
    setserstat (TIOCM_DTR, dtr);
#endif
}

void serial_dtr_off(void)
{
#if SERIALHSDEBUG > 0
    write_log ("SERIAL: DTR off\n");
#endif
    dtr = 0;
#ifdef SERIAL_PORTS
    if (currprefs.serial_demand)
	serial_close ();
    setserstat(TIOCM_DTR, dtr);
#endif
}

void serial_flush_buffer (void)
{
}

static uae_u8 oldserbits;

static void serial_status_debug (const char *s)
{
#if SERIALHSDEBUG > 1
    write_log ("%s: DTR=%d RTS=%d CD=%d CTS=%d DSR=%d\n", s,
	(oldserbits & 0x80) ? 0 : 1, (oldserbits & 0x40) ? 0 : 1,
	(oldserbits & 0x20) ? 0 : 1, (oldserbits & 0x10) ? 0 : 1, (oldserbits & 0x08) ? 0 : 1);
#endif
}

uae_u8 serial_readstatus (uae_u8 dir)
{
    int status = 0;
    uae_u8 serbits = oldserbits;

#ifdef SERIAL_PORTS
    getserstat (&status);

    if (!(status & TIOCM_CAR)) {
	if (!(serbits & 0x20)) {
	    serbits |= 0x20;
#if SERIALHSDEBUG > 0
	    write_log ("SERIAL: CD off\n");
#endif
	}
    } else {
	if (serbits & 0x20) {
	    serbits &= ~0x20;
#if SERIALHSDEBUG > 0
	    write_log ("SERIAL: CD on\n");
#endif
	}
    }

    if (!(status & TIOCM_DSR)) {
	if (!(serbits & 0x08)) {
	    serbits |= 0x08;
#if SERIALHSDEBUG > 0
	    write_log ("SERIAL: DSR off\n");
#endif
	}
    } else {
	if (serbits & 0x08) {
	    serbits &= ~0x08;
#if SERIALHSDEBUG > 0
	    write_log ("SERIAL: DSR on\n");
#endif
	}
    }

    if (!(status & TIOCM_CTS)) {
	if (!(serbits & 0x10)) {
	    serbits |= 0x10;
#if SERIALHSDEBUG > 0
	    write_log ("SERIAL: CTS off\n");
#endif
	}
    } else {
	if (serbits & 0x10) {
	    serbits &= ~0x10;
#if SERIALHSDEBUG > 0
	    write_log ("SERIAL: CTS on\n");
#endif
	}
    }

    serbits &= 0x08 | 0x10 | 0x20;
    oldserbits &= ~(0x08 | 0x10 | 0x20);
    oldserbits |= serbits;
#endif

    serial_status_debug ("read");

    return oldserbits;
}

uae_u8 serial_writestatus (uae_u8 newstate, uae_u8 dir)
{
#ifdef SERIAL_PORTS
    if (((oldserbits ^ newstate) & 0x80) && (dir & 0x80)) {
	if (newstate & 0x80)
	    serial_dtr_off();
	else
	    serial_dtr_on();
    }

    if (!currprefs.serial_hwctsrts && (dir & 0x40)) {
	if ((oldserbits ^ newstate) & 0x40) {
	    if (newstate & 0x40) {
		setserstat (TIOCM_RTS, 0);
#if SERIALHSDEBUG > 0
		write_log ("SERIAL: RTS cleared\n");
#endif
	    } else {
		setserstat (TIOCM_RTS, 1);
#if SERIALHSDEBUG > 0
		write_log ("SERIAL: RTS set\n");
#endif
	    }
	}
    }

#if 0 /* CIA io-pins can be read even when set to output.. */
    if ((newstate & 0x20) != (oldserbits & 0x20) && (dir & 0x20))
	write_log ("SERIAL: warning, program tries to use CD as an output!\n");
    if ((newstate & 0x10) != (oldserbits & 0x10) && (dir & 0x10))
	write_log ("SERIAL: warning, program tries to use CTS as an output!\n");
    if ((newstate & 0x08) != (oldserbits & 0x08) && (dir & 0x08))
	write_log ("SERIAL: warning, program tries to use DSR as an output!\n");
#endif

    if (((newstate ^ oldserbits) & 0x40) && !(dir & 0x40))
	write_log ("SERIAL: warning, program tries to use RTS as an input!\n");
    if (((newstate ^ oldserbits) & 0x80) && !(dir & 0x80))
	write_log ("SERIAL: warning, program tries to use DTR as an input!\n");

#endif

    oldserbits &= ~(0x80 | 0x40);
    newstate &= 0x80 | 0x40;
    oldserbits |= newstate;
    serial_status_debug ("write");

    return oldserbits;
}

void serial_open(void)
{
#ifdef SERIAL_PORTS
    if (serdev)
	return;

    if (!openser ("/dev/prevue")) {
	write_log ("SERIAL: Could not open device %s\n", currprefs.sername);
	return;
    }
    serdev = 1;
#endif
}

void serial_close (void)
{
#ifdef SERIAL_PORTS
    closeser ();
    serdev = 0;
#endif
}

void serial_init (void)
{
#ifdef SERIAL_PORTS
    if (!currprefs.use_serial)
	return;

    if (!currprefs.serial_demand)
	serial_open ();

#endif
}

void serial_exit (void)
{
#ifdef SERIAL_PORTS
    serial_close ();	/* serial_close can always be called because it	*/
#endif
    dtr = 0;		/* just closes *opened* filehandles which is ok	*/
    oldserbits = 0;	/* when exiting.				*/
}

void serial_uartbreak (int v)
{
#ifdef SERIAL_PORTS
    serialuartbreak (v);
#endif
}
