#include <stdio.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>

/*******************************************************************************
*                              Unix Related Macros                             *
*******************************************************************************/

//TODO Check pi block size
#define BLOCK_SIZE              (4*1024)
#define PAGE_SIZE               4096

/*******************************************************************************
*                   Raspberry PI Peripheral Address and Offsets                *
*******************************************************************************/

#define BCM2835_PERI_BASE           0x20000000
#define BCM2835_GPIO_OFFSET         0x00200000
#define BCM2835_PWM_OFFSET          0x0020C000
#define BCM2835_PWM_CLK_OFFSET      0x001010a0
#define AUX_MU_IER_REG              0x20215048


/***************************************
*            GPIO Functionality        * 
***************************************/

#define GPIO_BASE               (BCM2835_PERI_BASE + BCM2835_GPIO_OFFSET) //GPIO Base Register

//GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) TODO - discover why this is a 'rule' --> might be to zero out 3-bit values in the GPIO select register 
#define INP_GPIO(p,g)         *(p + (g)/10) &= ~( 7<<(((g)%10)*3) )
#define OUT_GPIO(p,g)         *(p + (g)/10) |=  ( 1<<(((g)%10)*3) )
#define SET_GPIO_ALT(p,g,a)   *(p + (g)/10) |=  (((a)<=3?(a) + 4:(a)==4?3:2)<<(((g)%10)*3))

#define SET_GPIO_1(p,g)            *(p + 7) |= (1 << (g))
#define SET_GPIO_2(p,g)            *(p + 8) |= (1 << ((g)%32))

#define CLR_GPIO_1(p,g)            *(p + 10) |= (1 << (g))
#define CLR_GPIO_2(p,g)            *(p + 11) |= (1 << ((g)%32))

/***************************************
*         PWM Clock Functionality      * 
***************************************/

#define  CM_PWM_CTL_BASE        (BCM2835_PERI_BASE + BCM2835_PWM_CLK_OFFSET)
#define  PWM_CLK_CTL            (CM_PWM_CTL_BASE + 0x0)
#define  PWM_CLK_DIV            (CM_PWM_CTL_BASE + 0x4)

struct PWM_CLK_CTL_REG
{
    unsigned int PASSWD:8;  //Bits 24:31 Clock Manager password "5a"
    unsigned int  RSRV0:8;  //Bits 16:23 Reserved
    unsigned int  RSRV1:5;  //Bits 11:15 Reserved
    unsigned int   MASH:2;  //Bit   9:10 MASH Control
    unsigned int   FLIP:1;  //Bit   8    FLIP Invert the clock generator output
    unsigned int   BUSY:1;  //Bit   7    Clock generator is running
    unsigned int  RSRV2:1;  //Bit   6    Reserved
    unsigned int   KILL:1;  //Bit   5    Kill the clock generator
    unsigned int   ENAB:1;  //Bit   4    Enable the clock generator
    unsigned int    SRC:1;  //Bit   0:3  Clock Source
                                         /*
                                         0 = GND
                                         1 = oscillator
                                         2 = testdebug0
                                         3 = testdebug1
                                         4 = PLLA per
                                         5 = PLLC per
                                         6 = PLLD per
                                         7 = HDMI auxiliary
                                      8-15 = GND
                                         */
};

struct PWM_CLK_DIV_REG
{
    unsigned int PASSWD:8;   //Bits 24:31 Clock Manager password "5a"
    unsigned int   DIVI:12;  //Bits 23:12 Integer part of divisor
    unsigned int   DIVF:12;  //Bits  0:11 Fractional part of divisor
};

/***************************************
*            PWM Functionality         * 
***************************************/

#define  PWM_BASE               (BCM2835_PERI_BASE + BCM2835_PWM_OFFSET)  //PWM  Base Register
#define  PWM_CTL                (PWM_BASE + 0x0)                          //PWM CTL Register
#define  PWM_STA                (PWM_BASE + 0x1)                          //PWM STA Register
#define  PWM_RNG1               (PWM_BASE + 0x10)                         //PWM Channel 1 Range
#define  PWM_DAT1               (PWM_BASE + 0x14)                         //PWM Channel 1 Data
#define  PWM_RNG2               (PWM_BASE + 0x20)                         //PWM Channel 2 Range
#define  PWM_DAT2               (PWM_BASE + 0x24)                         //PWM Channel 2 Data

struct PWM_CTL_REG
{
    unsigned int RSRV0:8; //Bits 24:31
    unsigned int RSRV1:8; //Bits 16:23
    unsigned int MSEN2:1; //Bit  15
    unsigned int RSRV2:1; //Bit  14
    unsigned int USEF2:1; //Bit  13
    unsigned int POLA2:1; //Bit  12
    unsigned int SBIT2:1; //Bit  11
    unsigned int RPTL2:1; //Bit  10
    unsigned int MODE2:1; //Bit   9
    unsigned int PWEN2:1; //Bit   8
    unsigned int MSEN1:1; //Bit   7
    unsigned int CLRF1:1; //Bit   6
    unsigned int USEF1:1; //Bit   5
    unsigned int POLA1:1; //Bit   4
    unsigned int SBIT1:1; //Bit   3
    unsigned int RPTL1:1; //Bit   2
    unsigned int MODE1:1; //Bit   1
    unsigned int PWEN1:1; //Bit   0
};

struct PWM_STA_REG
{
    unsigned int RSRV0:8; //Bits 24:31 Reserved 
    unsigned int RSRV1:8; //Bits 16:23 Reserved
    unsigned int RSRV2:4; //Bits 13:16 Reserved
    unsigned int  STA4:1; //Bit  12    Channel 4 State
    unsigned int  STA3:1; //Bit  11    Channel 3 State
    unsigned int  STA2:1; //Bit  10    Channel 2 State
    unsigned int  STA1:1; //Bit   9    Channel 1 State
    unsigned int  BERR:1; //Bit   8    Bus Error Flag
    unsigned int GAPO4:1; //Bit   7    Channel 4 Gap Occurred Flag
    unsigned int GAPO3:1; //Bit   6    Channel 3 Gap Occurred Flag FIFO Read Error Flag
    unsigned int GAPO2:1; //Bit   5    Channel 2 Gap Occurred Flag FIFO Write Error Flag
    unsigned int GAPO1:1; //Bit   4    Channel 1 Gap Occurred Flag
    unsigned int RERR1:1; //Bit   3    FIFO Read Error Flag
    unsigned int WERR1:1; //Bit   2    FIFO Write Error Flag
    unsigned int EMPT1:1; //Bit   1    FIFO Empty Flag
    unsigned int FULL1:1; //Bit   0    FIFO Full Flag
};

struct PWM_RNG1_REG
{
    unsigned int PWM_RANGE1;    //Bits 0:31 Channel 1 Range
};

struct PWM_DAT1_REG
{
    unsigned int PWM_DATA1;     //Bits 0:31 Channel 1 Data
};

struct PWM_RNG2_REG
{
    unsigned int PWM_RANGE2;    //Bits 0:31 Channel 2 Range
};

struct PWM_DAT2_REG
{
    unsigned int PWM_DATA2;     //Bits 0:31 Channel 2 Data
};


/*******************************************************************************
*                   Register Struct and Address Initialization                 *
*******************************************************************************/

//IO Access
//TODO Change the name of this struct to something like bmc2835_address_access
struct bcm2835_address_access 
{
    unsigned long addr_p;
    int mem_fd;
    void *map;
    volatile unsigned int *addr;
};

//extern struct bcm2835_address_access gpio;
//extern struct bcm2835_address_access pwm_clk;
//extern struct bcm2835_address_access pwm;
