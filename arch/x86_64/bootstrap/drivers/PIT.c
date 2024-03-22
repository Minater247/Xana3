#include <stdint.h>

#include <stdint.h>
#include <io.h>

//PIT
// Technically this is in hundreds of hz (1 -> 100hz, 2 -> 200hz, etc)
void timer_phase(int hz)
{
    int divisor = 1193180 / hz;
    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, divisor >> 8);
}