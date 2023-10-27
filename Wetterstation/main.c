/**
 * Name: Lasse Kelling, Fabian Schmalenbach
 * Datum: 24.10.2023
 *
 * Unter (Properties -> Resource) encoding UTF-8 und line delimitor Unix auswählen!
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "inc/hw_i2c.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"

#include "driverlib/gpio.h"
#include "driverlib/i2c.h"
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"

#define I2C_BUFFER_LEN 4

void main(), setup();

void main() {
    uint32_t i = 0;
    uint32_t i2cTx[I2C_BUFFER_LEN];

    setup();

    while(1) {
        for(i = 0; i < I2C_BUFFER_LEN; i++) {
            I2CMasterDataPut(I2C0_BASE, i2cTx[i]);
            I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_SINGLE_SEND);
        }
        while(I2CMasterBusy(I2C0_BASE)) {}

        //I2CMasterDataGet(I2C0_BASE);
    }
}
/*
 * I2C0-SCL <-> PB2
 * I2C0-SDA <-> PB3
 */
void setup() {
    SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_OSC | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ); //16MHz Clock anschalten
    SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C0); //I2C0 mit 16MHz versorgen

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    GPIOPinConfigure(GPIO_PB2_I2C0SCL);
    GPIOPinConfigure(GPIO_PB3_I2C0SDA);
    GPIOPinTypeI2CSCL(GPIO_PORTB_BASE, GPIO_PIN_2);
    GPIOPinTypeI2C(GPIO_PORTB_BASE, GPIO_PIN_3);

    I2CMasterInitExpClk(I2C0_BASE, SysCtlClockGet(), false);
    printf("Setup erfolgreich!\n");
}
