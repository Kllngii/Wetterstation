#include <msp430.h> 
#include "msp430g2553_i2c/i2c.h"


/**
 * main.c
 */
int main(void)
{
	WDTCTL = WDTPW | WDTHOLD;	// stop watchdog timer
	// Init
	I2C_init(0x76);

	P2IN = 0x06;
	P2OUT = 0x00;
	
	return 0;
}
