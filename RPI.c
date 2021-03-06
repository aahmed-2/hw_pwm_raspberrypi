#include "RPI.h"

#include <fcntl.h>

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

// Exposes the physical address defined in the passed structure using mmap on /dev/mem
int map_peripheral(char* pointer, unsigned long registerAddress)
{
    // Open /dev/mem
    int mem_fd = -1;
    if( (mem_fd = open("/dev/mem", O_RDWR|O_SYNC)) < 0 )
    {
        printf("Failed to open /dev/mem. Please check permissions.\n");
        return -1;
    }

    pointer = (char*) mmap(
                NULL,
                BLOCK_SIZE,
                PROT_READ|PROT_WRITE,
                MAP_SHARED,
                mem_fd,             // File descriptor to physical memory virtual file '/dev/mem' TODO - clarify this comment
                0x20200000          // Address in physical map that we want this memory block to expose
                );
    close(mem_fd);
    if ( pointer == MAP_FAILED)
    {
        printf("Failed Mapping attempt\n");
    }
    else
    {
        printf("Mapping Succeeded.\n");
        unsigned int* tmp_pointer = (unsigned int*) pointer;

            printf("                        00  01  02  03\n");
            printf("--------------------------------------\n\n");
        for(int ix = 0; ix < 6; ix++)
        {
            //printf("At location 0x%X: %u, %u\n", (pointer + 4*ix), &pointer[ix], &tmp_pointer[ix] );
            printf("At location 0x%08X:", (pointer + 4*ix));
            for(int iy = 0; iy < 4; iy++)
            {
                printf("%3u ", pointer[ix + iy]);
                printHexToBinary( pointer[ix + iy]);
                printf(" ");
            }
            printf("\n");
        }
        //printf("Pointer value %X\n",(unsigned*)(pointer + 0x48));
        //for(int ix = 0; ix < 64; ix++)
        //{
        //    printf("Value: %d %x\n", ix, *(pointer + ix));
        //}
    }
    return 0;
}

void unmap_peripheral(char* pointer)
{
    munmap(pointer, BLOCK_SIZE);
}

int main(int argc, char* argv [])
{
    map_peripheral(gpio, GPIO_BASE);//AUX_MU_IER_REG);
    unmap_peripheral(gpio);
    return 0;

}
