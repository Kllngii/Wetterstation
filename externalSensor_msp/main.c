#include <msp430.h> 
#include "bme280/bme280.h"
#include <stdbool.h>
#include "drivers/board/board.h"

/**
 * main.c
 */
int main(void)
{
	WDTCTL = WDTPW | WDTHOLD;	// stop watchdog timer

	board_init();
    volatile float temperature, humidity, pressure;

	if (bme280_init() == 0xff)
	{
		return 0;
	}

	temperature = bme280_readTemperature();
	humidity = bme280_readHumidity();
	pressure = bme280_readPressure();

	return 0;
}
