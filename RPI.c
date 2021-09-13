#include "RPI.h"

#include <fcntl.h>

struct bcm2835_address_access gpio = {GPIO_BASE};
struct bcm2835_address_access pwm_clk = {CM_PWM_CTL_BASE};
struct bcm2835_address_access pwm = {PWM_BASE};

// Exposes the physical address defined in the passed structure using mmap on /dev/mem
int map_peripheral(struct bcm2835_address_access *p)
{
    // Open /dev/mem
    if( (p->mem_fd = open("/dev/mem", O_RDWR|O_SYNC)) < 0 )
    {
        printf("Failed to open /dev/mem. Please check permissions.\n");
        return -1;
    }

    p->map = mmap(
                NULL,
                BLOCK_SIZE,
                PROT_READ|PROT_WRITE,
                MAP_SHARED,
                p->mem_fd,      // File descriptor to physical memory virtual file '/dev/mem' TODO - clarify this comment
                p->addr_p       // Address in physical map that we want this memory block to expose TODO - clarify address names
                );

    p->addr = (volatile unsigned int *)p->map;

    return 0;
}

int map_pwm_clk(struct bcm2835_address_access *p)
{
    //Open /dev/mem
    if( (p->mem_fd = open("/dev/mem", O_RDWR|O_SYNC)) < 0 )
    {
        printf("Failed to open /dev/mem. Please check permissions.\n");
        return -1; 
    }

    p->map = mmap(
                NULL,
                BLOCK_SIZE,
                PROT_READ|PROT_WRITE,
                MAP_SHARED,
                p->mem_fd,
                p->addr_p
                );
    p->addr = (volatile unsigned int *)p->map;

}

int map_pwm()
{}

void unmap_peripheral(struct bcm2835_address_access *p)
{
    munmap(p->map, BLOCK_SIZE);
    close(p->mem_fd);
}
