/*********************************************************************
 * pwm.c - PWM HW program
 * sudo bash -c 'make clean; make all;'
 * ./pwm 1 or ./pwm 3 or ./pwm 20 100 5200  
 * DEPLOY:
 *		if different machine copy and
 *		sudo bash -c 'sudo chown root ./pwm; sudo chmod +x ./pwm; sudo chmod u+s ./pwm'
 *
 *********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <bcm_host.h>

#define debug_print(fmt, ...) \
            do { if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while (0)
#define DEBUG 1

#define PWM_CLK_PASSWORD 0x5a000000
#define BCM2835_PWM_CONTROL 0
#define BCM2835_PWM_STATUS  1
#define BCM2835_PWM0_RANGE  4
#define BCM2835_PWM0_DATA   5

#define	PWMCLK_CNTL 40
#define	PWMCLK_DIV  41
#define BLOCK_SIZE 	(4*1024)

#define RPI3_PB		0x3F000000
#define RPI4_PB		0xFE000000

static volatile unsigned BCM2708_PERI_BASE, GPIO_BASE, PWM_BASE, CLK_BASE;
static volatile double clock_rate;

static volatile unsigned *ugpio = 0;
static volatile unsigned *ugpwm = 0;
static volatile unsigned *ugclk = 0;

static struct S_PWM_CTL {
	unsigned	PWEN1 : 1;
	unsigned	MODE1 : 1;
	unsigned	RPTL1 : 1;
	unsigned	SBIT1 : 1;
	unsigned	POLA1 : 1;
	unsigned	USEF1 : 1;
	unsigned	CLRF1 : 1;
	unsigned	MSEN1 : 1;
} volatile *pwm_ctl = 0;

static struct S_PWM_STA {
	unsigned	FULL1 : 1;
	unsigned	EMPT1 : 1;
	unsigned	WERR1 : 1;
	unsigned	RERR1 : 1;
	unsigned	GAP01 : 1;
	unsigned	GAP02 : 1;
	unsigned	GAP03 : 1;
	unsigned	GAP04 : 1;
	unsigned	BERR : 1;
	unsigned	STA1 : 1;
} volatile *pwm_sta = 0;

static volatile unsigned *pwm_rng1 = 0;
static volatile unsigned *pwm_dat1 = 0;

#define INP_GPIO(g) *(ugpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) \
    *(ugpio+(((g)/10))) |= (((a)<=3?(a)+4:((a)==4?3:2))<<(((g)%10)*3))

/*
 * Initialize GPIO/PWM/CLK Access
 */
static void
pwm_init() {
	int fd;	
	char *map;

	fd = open("/dev/mem",O_RDWR|O_SYNC);  /* Needs root access */
	if ( fd < 0 ) {
		perror("Opening /dev/mem");
		exit(1);
	}

	map = (char *) mmap(
		NULL,             /* Any address */
		BLOCK_SIZE,       /* # of bytes */
		PROT_READ|PROT_WRITE,
		MAP_SHARED,       /* Shared */
		fd,               /* /dev/mem */
		PWM_BASE          /* Offset to GPIO */
	);

	if ( (long)map == -1L ) {
		perror("mmap(/dev/mem)");    
		exit(1);
	}

	/* Access to PWM */
	ugpwm = (volatile unsigned *)map;
	pwm_ctl  = (struct S_PWM_CTL *) &ugpwm[BCM2835_PWM_CONTROL];
	pwm_sta  = (struct S_PWM_STA *) &ugpwm[BCM2835_PWM_STATUS];
	pwm_rng1 = &ugpwm[BCM2835_PWM0_RANGE];
	pwm_dat1 = &ugpwm[BCM2835_PWM0_DATA];

	map = (char *) mmap(
		NULL,             /* Any address */
		BLOCK_SIZE,       /* # of bytes */
		PROT_READ|PROT_WRITE,
		MAP_SHARED,       /* Shared */
		fd,               /* /dev/mem */
		CLK_BASE          /* Offset to GPIO */
	);

	if ( (long)map == -1L ) {
		perror("mmap(/dev/mem)");    
		exit(1);
	}

	/* Access to CLK */
	ugclk = (volatile unsigned *)map;

	map = (char *) mmap(
		NULL,             /* Any address */
		BLOCK_SIZE,       /* # of bytes */
		PROT_READ|PROT_WRITE,
		MAP_SHARED,       /* Shared */
		fd,               /* /dev/mem */
		GPIO_BASE         /* Offset to GPIO */
	);

	if ( (long)map == -1L ) {
		perror("mmap(/dev/mem)");    
		exit(1);
	}
	
	/* Access to GPIO */
	ugpio = (volatile unsigned *)map;

	close(fd);
}

/*
 * Establish the PWM frequency:
 */
static int
pwm_frequency(float freq) {
	debug_print(" Call pwm_frequency(%.lf) %s:%d\r\n",freq, __BASE_FILE__, __LINE__);
	long idiv;
	int rc = 0;

	/*
	 * Stop the clock:
	 */
	// ugclk[PWMCLK_CNTL] = 0x5A000020;	/* Kill clock */
	ugclk[PWMCLK_CNTL] = (ugclk[PWMCLK_CNTL]&~0x10)|PWM_CLK_PASSWORD;	/* Turn OFF enable flag */
	debug_print(" Turn OFF enable flag %s:%d\r\n",__BASE_FILE__, __LINE__);
	while( ugclk[PWMCLK_CNTL]&0x80 ) {
		debug_print(" Wait for Busy flag to turn OFF %s:%d\r\n",__BASE_FILE__, __LINE__);
	};															/* Wait for Busy flag */
	// usleep(10);
	pwm_ctl->PWEN1 = 0;     		/* Disable PWM */
	debug_print(" Disable PWM %s:%d\r\n",__BASE_FILE__, __LINE__);
	// usleep(10);  

	/*
	 * Compute and set the divisor :
	 */
	idiv = (long) ( clock_rate / (double) freq );
	if ( idiv < 1 ) {
		idiv = 1;			/* Lowest divisor */
		rc = -1;
	} else if ( idiv >= 0x1000 ) {
		idiv = 0xFFF;			/* Highest divisor */
		rc = +1;
	}

	ugclk[PWMCLK_DIV] = PWM_CLK_PASSWORD | ( idiv << 12 );
    
	/*
	 * Set source to oscillator and enable clock:
	 */
    ugclk[PWMCLK_CNTL] = 0x5A000011;
	while( !(ugclk[PWMCLK_CNTL]&0x80) ) {
		debug_print(" Wait for Busy flag to turn ON %s:%d\r\n",__BASE_FILE__, __LINE__);
	};
	pwm_ctl->PWEN1 = 1;     		/* Enable PWM */
	debug_print(" Enable PWM %s:%d\r\n",__BASE_FILE__, __LINE__);

	/*
 	 * GPIO 18 is PWM, when set to Alt Func 5 :
	 */
	INP_GPIO(18);		/* Set ALT = 0 */
	SET_GPIO_ALT(18,5);	/* Or in '5' */

	pwm_ctl->MODE1 = 0;     /* PWM mode */
	pwm_ctl->RPTL1 = 0;
	pwm_ctl->SBIT1 = 0;
	pwm_ctl->POLA1 = 0;
	pwm_ctl->USEF1 = 0;
	pwm_ctl->MSEN1 = 0;     /* PWM mode */
	pwm_ctl->CLRF1 = 1;
	return rc;
}

/*
 * Set PWM to ratio N/M, and enable it:
 */
static void
pwm_ratio(unsigned n,unsigned m) {

	debug_print(" Call pwm_ratio(%d, %d) %s:%d\n\r",n, m, __BASE_FILE__, __LINE__);
	// pwm_ctl->PWEN1 = 0;					/* Disable */
	// debug_print(" Disable PWM %s:%d\r\n",__BASE_FILE__, __LINE__);

	*pwm_rng1 = m;
	*pwm_dat1 = n;

	if ( !pwm_sta->STA1 ) {
		if ( pwm_sta->RERR1 )
			pwm_sta->RERR1 = 1;
		if ( pwm_sta->WERR1 )
			pwm_sta->WERR1 = 1;
		if ( pwm_sta->BERR )
			pwm_sta->BERR = 1;
	}

	pwm_ctl->PWEN1 = 1;					/* Enable */
	debug_print(" Enable PWM %s:%d\r\n",__BASE_FILE__, __LINE__);
}

/*
 * MicroSeconds sleep
 */ 
int msleep(long msec) {
	
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}
/*
 * Main program:
 */
int
main(int argc,char **argv) {
	
	debug_print("< PWM start bcm_host_get_peripheral_address >%s\n\r",  __BASE_FILE__);
	/* Get peripheral addres via <bcm_host.h>
	* see: https://www.raspberrypi.org/documentation/hardware/raspberrypi/peripheral_addresses.md
	*/
	BCM2708_PERI_BASE	= bcm_host_get_peripheral_address();
	GPIO_BASE			= (BCM2708_PERI_BASE + 0x200000);
	PWM_BASE			= (BCM2708_PERI_BASE + 0x20C000);
	CLK_BASE			= (BCM2708_PERI_BASE + 0x101000);
	static volatile int n, m = 0;
	static volatile float f, delay = 0.0;

	if ( BCM2708_PERI_BASE == RPI4_PB ) {
		/* RPI4 = 54000000.0 */
		clock_rate = 54000000.0;
		n = 20;
		m = 100;
		f = 5200.0;
		delay = 100.0;
	} else if ( BCM2708_PERI_BASE == RPI3_PB ) {
		/* RPI3 = 19200000.0 */
		clock_rate = 19200000.0;
		n = 50;
		m = 100;
		f = 5200.0;
		delay = 100.0;
	}
	debug_print(" BCM2708_PERI_BASE:\t0x%X\r\n GPIO_BASE:\t\t0x%X\r\n PWM_BASE:\t\t0x%X\r\n CLK_BASE:\t\t0x%X\n\r clock_rate:\t\t%.lf\n\r",BCM2708_PERI_BASE,GPIO_BASE,PWM_BASE,CLK_BASE,clock_rate);

	pwm_init();

	if ( argc == 2 ) {
		int sw = atoi(argv[1]);
		/* Configure and enable PWM */
		pwm_frequency(f);
		pwm_ratio(n,m);
		
		/* Cycle for 1..3 */
		debug_print("Cycles: %d\n",sw);
		for (unsigned char z = 1; z <= sw; z += 1 ) {
			pwm_ctl->PWEN1 = 1;					/* Enable PWM */
			debug_print(" Enable PWM %s:%d\r\n",__BASE_FILE__, __LINE__);
			debug_print("%d - PWM set for %d/%d, frequency %.1f, delay %.1f\n",z,n,m,f,delay);
			msleep(delay);
			pwm_ctl->PWEN1 = 0;					/* Disable PWM */
			debug_print(" Disable PWM %s:%d\r\n",__BASE_FILE__, __LINE__);
			/* InterBeep delay */
			msleep(delay-25);	
		}
	} else	{    
		if ( argc > 1 ) {
			n = atoi(argv[1]); debug_print("argc: %d, argv[1]: %d\n",argc,n);
		}
		if ( argc > 2 ) {
			m = atoi(argv[2]); debug_print("argc: %d, argv[2]: %d\n",argc,m);
		}
		if ( argc > 3 ) {
			f = atof(argv[3]); debug_print("argc: %d, argv[3]: %.1f\n",argc,f);
		}
		if ( argc > 4 ) {
			delay = atof(argv[4]); debug_print("argc: %d, argv[4]: %.1f\n",argc,delay);
		}
		if ( argc > 1 ) {
			if ( n > m || n < 1 || m < 1 || f < 586.0 || f > clock_rate ) {
				fprintf(stderr,"Value error: N=%d, M=%d, F=%.1f\n",n,m,f);
				return 1;
			}
		}
		debug_print(" Start PWM set for %d/%d, frequency %.1f, delay %.1f\n",n,m,f,delay);
		pwm_frequency(f);
		pwm_ratio(n,m);
		msleep(delay);
		pwm_ctl->PWEN1 = 0;					/* Disable PWM */
		debug_print(" Disable PWM %s:%d\r\n",__BASE_FILE__, __LINE__);
	}
	return 0;
}

/*********************************************************************
 * End pwm.c
 *********************************************************************/
