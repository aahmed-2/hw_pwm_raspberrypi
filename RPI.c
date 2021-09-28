#include "RPI.h"

#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>

//struct bcm2835_address_access gpio = {GPIO_BASE};
//struct bcm2835_address_access pwm_clk = {CM_PWM_CTL_BASE};
//struct bcm2835_address_access pwm = {PWM_BASE};
static char* gpio    = NULL;
static char* pwm_clk = NULL;
static char* pwm     = NULL;

void printHexToBinary(char byte)
{
    unsigned int val = (unsigned int) byte;
    unsigned short int div = 128;
    for(int tmp = 1 ; tmp <= 8; tmp++)
    {
        if( val/div )
        {
            printf("1");
            val = val - div;
        }
        else
        {
            printf("0");
        }
        div = div/2;
    }
}

void pointerCharDebug(const char* pointer)
{
    unsigned int* tmp_pointer = (unsigned int*) pointer;

    printf("At location 0x%08X:", pointer);
    for(int iy = 0; iy < 4; iy++)
    {
        printf("%3u ", pointer[iy]);
        printHexToBinary( pointer[iy]);
        printf(" ");
    }
    printf("\n");
}

void pointerIntDebug(const char* pointer)
{
    unsigned int* tmp_pointer = (unsigned int*) pointer;
    printf("At location 0x%08x:\n", tmp_pointer);
    printf("Value in decimal: %u\n", *tmp_pointer);
    bool val = 0;

    putc(' ', stdout);
    for(int ix = 0; ix <= 9; ix++)
    {

        printf("|  Pin %1u  ", ix);
    }

    printf("|\n |");

    for(int ix = 0; ix < 32; ix++)
    {
        val = *tmp_pointer & (1<<ix);
        if(val)
        {
            printf(" 1 ");
        }
        else
        {
            printf(" 0 ");
        }
        if(ix % 3 == 2)
        {
           printf("|");
        }
    }
    putc('\n', stdout);
}

/*
    Check if the desired Address is a valid offset
    mmap requires offsets to be aligned to PAGE SIZE (i.e. 4096)
 */
unsigned long checkValid_Offset(const unsigned long desiredAddress)
{
    if( (desiredAddress / PAGE_SIZE) == 0)
    {
        return 0;
    }

    // How far into the page desiredAddress is (i.e. page offset)
    return (unsigned long) desiredAddress % PAGE_SIZE;
}


// Exposes the physical address defined in the passed structure using mmap on /dev/mem
int map_peripheral(char** pointer, const unsigned long registerAddress)
{
    unsigned long page_offset = checkValid_Offset(registerAddress);

    // Open /dev/mem
    int mem_fd = -1;
    if( (mem_fd = open("/dev/mem", O_RDWR|O_SYNC)) < 0 )
    {
        printf("Failed to open /dev/mem. Please check permissions.\n");
        exit(1);
    }

    *pointer = (char*) mmap(
                NULL,
                BLOCK_SIZE,
                PROT_READ|PROT_WRITE,
                MAP_SHARED,
                mem_fd,                          // File descriptor to physical memory virtual file '/dev/mem' TODO - clarify this comment
                (registerAddress - page_offset)  // Address in physical map that we want this memory block to expose
                );
    close(mem_fd);

    if ( *pointer == MAP_FAILED)
    {
        printf("Failed Mapping attempt\n");
        exit(1);
    }
    else
    {
        printf("Mapping Succeeded.\n");
        *pointer = *pointer + page_offset;
    }
    return 0;
}

void unmap_peripheral(char* pointer)
{
    munmap(pointer, BLOCK_SIZE);
}

void initGPIO(char* pointer)
{
    unsigned int* tmp_pointer = (unsigned int*) pointer;
    printf("Pointer starting at: %X\n", pointer);
    printf("Incremented pointer: %X\n", (pointer+4));
    printf("Incremented pointer: %X\n", (tmp_pointer+1));

    pointerIntDebug(pointer+4);
    *(tmp_pointer+1) &=  ~( 7<<(((18)%10)*3) );
    pointerIntDebug(pointer+4);
    *(tmp_pointer+1) |=  (((5)<=3?(5) + 4:(5)==4?3:2)<<(((18)%10)*3));
    pointerIntDebug(pointer+4);
    *(tmp_pointer+1) &=  ~( 7<<(((18)%10)*3) );
    pointerIntDebug(pointer+4);
}

// cat /sys/kernel/debug/clk/clk_summary
void initPWM_Clk(char* pointer)
{
    unsigned int* tmp_pointer = (unsigned int*) pointer;
    struct PWM_CLK_CTL_REG *pwm_clk_ctl_reg = (struct PWM_CLK_CTL_REG*) tmp_pointer;

    //0xAB CD EF GH
    unsigned int zero_mask = 0x00FFFFFF;
    unsigned int pass_mask = 0x5A000000;
    *tmp_pointer = *tmp_pointer &= zero_mask;
    *tmp_pointer = *tmp_pointer |= pass_mask;


    while (*tmp_pointer & 0x08)
    {
        printf("Waiting for PWM CLK busy bit\n");
    }

    //Set Clock Divisor (Integer and Factional)
    if ( !(*tmp_pointer & 0x08) )
    {
        unsigned int* pwm_clk_div_reg_pointer = tmp_pointer + 1;
        *pwm_clk_div_reg_pointer = *pwm_clk_div_reg_pointer &= zero_mask;
        *pwm_clk_div_reg_pointer = *pwm_clk_div_reg_pointer |= pass_mask;

        *pwm_clk_div_reg_pointer &= 0xFFFA0000;
    }
    else
    {
        printf("Failure to set clock divisor\n");
    }

    if ( !(*tmp_pointer & 0x08) )
    {
        //Set Clock Source
        //Setting to PLLD (Clock source #6)
        *tmp_pointer &= 0xFFFFFFF0;
        *tmp_pointer |= 0x00000006;

        //Enable Clock Generator
        *tmp_pointer |= 0x00000010;
    }
    else
    {
        printf("Failure to Set PWM CLK Source and Enable PWM CLK\n");
    }

}

void initPWM(char* pointer)
{
    unsigned int* tmp_pointer = (unsigned int*) pointer;
    unsigned int* pwm_rng1_pointer = (unsigned int*) pointer + 0x10;
    unsigned int* pwm_dat1_pointer = (unsigned int*) pointer + 0x14;

    *tmp_pointer &= 0x00000000;
    *tmp_pointer |= 0x00000081;

    *pwm_rng1_pointer = 0x00002700;
    *pwm_dat1_pointer = 0x000003E0; //10% duty cycle
}

int main(int argc, char* argv [])
{
    map_peripheral(&gpio, GPIO_BASE);
    printf("gpio pointer: %X\n", gpio);
    map_peripheral(&pwm_clk, CM_PWM_CTL_BASE);
    printf("gpio pointer: %X\n", pwm_clk);
    map_peripheral(&pwm, PWM_BASE);
    printf("gpio pointer: %X\n", pwm);

    initGPIO(gpio);
    initPWM_Clk(pwm_clk);
    initPWM(pwm);

    unsigned int* pwm_dat1_pointer = (unsigned int*) (pwm + 0x14);
    sleep(4);
    *pwm_dat1_pointer = 0x000005DC; //15% duty cycle
    sleep(4);
    *pwm_dat1_pointer = 0x000001F4; // 5% duty cycle
    sleep(5);

    *pwm_dat1_pointer = 0x00000000; // 0% duty cycle
    printf("Disabling PWM Channel 1...\n");

    unsigned int* pwm_pointer = (unsigned int*) pwm;
    *pwm_pointer = 0x00;

    printf("Unmapping peripherals...\n");

    unmap_peripheral(gpio);
    unmap_peripheral(pwm_clk);
    unmap_peripheral(pwm);
    return 0;

}
