//
//  bme280.c
//  i2c
//
//  Created by Michael Köhler on 09.10.17.
//
//

#include "bme280.h"
#include "../drivers/i2c/i2c.h"
#include <math.h>       // for NAN & pow()

struct i2c_data i2cData;

volatile uint32_t t_fine;
volatile bme280_calib_data _bme280_calib;

/**********************************************
 Public Function: bme280_init
 
 Purpose: Initialise sensor
 
 Input Parameter: sensor
 
 Return Value: uint8_t
 - Value 0x00 means BME280 detected
 - Value 0x01 means BMP280 detected
 - Value 0xff means sensor unknown or argue out of range
 **********************************************/
uint8_t bme280_init(){
    
    uint8_t returnValue = 0xff;
    uint8_t txBuf[2] = {0};
    uint8_t rxBuf = 0;
    txBuf[0] = BME280_REGISTER_CHIPID;
    i2cData.tx_buf = txBuf;
    i2cData.rx_buf = &rxBuf;
    i2cData.tx_len = 1;
    i2cData.rx_len = 1;
    i2c_transfer(&i2cData);
    switch (rxBuf){
        case 0x60:
            // BME280 connected
            returnValue = 0x00;
            
            // init softreset of sensor
            txBuf[0] = BME280_REGISTER_SOFTRESET;
            txBuf[1] = 0xB6;
            i2cData.tx_buf = txBuf;
            i2cData.tx_len = sizeof(txBuf);
            i2cData.rx_len = 0;
            i2c_transfer(&i2cData);
            
            // write config for humidity
            txBuf[0] = BME280_REGISTER_CONTROLHUMID;
            txBuf[1] = BME280_HUM_CONFIG;
            i2c_transfer(&i2cData);
        break;
        case 0x58:
            // BMP280 connected
            returnValue = 0x01;
            
            // init softreset of sensor
            txBuf[0] = BME280_REGISTER_SOFTRESET;
            txBuf[1] = 0xB6;
            i2cData.tx_buf = txBuf;
            i2cData.tx_len = sizeof(txBuf);
            i2cData.rx_len = 0;
            i2c_transfer(&i2cData);
            
        break;
        default:
            // wrong chip-id, abort init
            return 0xff;
    }
    
    // write config for filter, standby-time and SPI-Mode (SPI off)
    txBuf[0] = BME280_REGISTER_CONFIG;
    txBuf[1] = BME280_CONFIG;
    i2cData.rx_len = 0;
    i2cData.tx_len = 2;
    i2c_transfer(&i2cData);
    
    // write config for pressure, temperture and sensor-mode
    txBuf[0] = BME280_REGISTER_CONTROL;
    txBuf[1] = (BME280_TEMP_CONFIG << 5)|(BME280_PRESS_CONFIG << 2)|(BME280_MODE_CONFIG);
    i2c_transfer(&i2cData);
    
    // read coefficients
    bme280_readCoefficients();
    return returnValue;
}

/**********************************************
 Public Function: bme280_readTemperature
 
 Purpose: Read temperature
 
 Input Parameter: uint8_t sensor: choose sensor on I2C
 
 Return Value: float
 - temperature in celsius
 - Value NAN means measurement disable or argue out of range
 **********************************************/
float bme280_readTemperature(){

    uint8_t txBuf = BME280_REGISTER_TEMPDATA;
    uint8_t rxBuf[3] = {0};
    i2cData.tx_buf = &txBuf;
    i2cData.rx_buf = rxBuf;
    i2cData.tx_len = 1;
    i2cData.rx_len = 3;
    i2c_transfer(&i2cData);

    uint32_t adc_T = (rxBuf[0] << 16) | (rxBuf[1] << 8) | rxBuf[2];
        
    if (adc_T == 0x800000) // value in case temperature measurement was disabled
        return NAN;
    int32_t var1, var2;
    
    adc_T >>= 4;
    
    var1  = ((((adc_T>>3) - ((int32_t)_bme280_calib.dig_T1 <<1))) *
             ((int32_t)_bme280_calib.dig_T2)) >> 11;
    
    var2  = (((((adc_T>>4) - ((int32_t)_bme280_calib.dig_T1)) *
               ((adc_T>>4) - ((int32_t)_bme280_calib.dig_T1))) >> 12) *
             ((int32_t)_bme280_calib.dig_T3)) >> 14;
    
    t_fine = var1 + var2;
    
    float T  = (t_fine * 5 + 128) >> 8;
    
    return T/100;
}

/**********************************************
 Public Function: bme280_readPressure
 
 Purpose: Read pressure
 
 Input Parameter: uint8_t sensor: choose sensor on I2C
 
 Return Value: float
 - pressure in hPa
 - Value NAN means measurement disable or argue out of range
 **********************************************/
float bme280_readPressure(){
    
    int64_t var1, var2, p;
    
    bme280_readTemperature(); // must be done first to get t_fine
    
    uint8_t txBuf = BME280_REGISTER_PRESSUREDATA;
    uint8_t rxBuf[3] = {0};
    i2cData.tx_buf = &txBuf;
    i2cData.rx_buf = rxBuf;
    i2cData.tx_len = 1;
    i2cData.rx_len = 3;
    i2c_transfer(&i2cData);

    uint32_t adc_P = (rxBuf[0] << 16) | (rxBuf[1] << 8) | rxBuf[2];
    if (adc_P == 0x800000) // value in case pressure measurement was disabled
        return NAN;
    adc_P >>= 4;
    
    var1 = ((int64_t)t_fine) - 128000ul;
    var2 = var1 * var1 * (int64_t)_bme280_calib.dig_P6;
    var2 = var2 + ((var1*(int64_t)_bme280_calib.dig_P5)<<17);
    var2 = var2 + (((int64_t)_bme280_calib.dig_P4)<<35);
    var1 = ((var1 * var1 * (int64_t)_bme280_calib.dig_P3)>>8) +
    ((var1 * (int64_t)_bme280_calib.dig_P2)<<12);
    var1 = (((((int64_t)1)<<47)+var1))*((int64_t)_bme280_calib.dig_P1)>>33;
    
    if (var1 == 0) {
        return 0; // avoid exception caused by division by zero
    }
    p = 1048576ul - adc_P;
    p = (((p<<31) - var2)*3125ul) / var1;
    var1 = (((int64_t)_bme280_calib.dig_P9) * (p>>13) * (p>>13)) >> 25;
    var2 = (((int64_t)_bme280_calib.dig_P8) * p) >> 19;
    
    p = ((p + var1 + var2) >> 8) + (((int64_t)_bme280_calib.dig_P7)<<4);
    return (float)p/256ul;
}

/**********************************************
 Public Function: bme280_readHumidity
 
 Purpose: Read humidity
 
 Input Parameter: uint8_t sensor: choose sensor on I2C
 
 Return Value: float
 - humidity in %
 - Value NAN means measurement disable or argue out of range
 **********************************************/
float bme280_readHumidity(){
    
    uint8_t txBuf = BME280_REGISTER_CHIPID;
    uint8_t rxBuf[3] = {0};
    i2cData.tx_buf = &txBuf;
    i2cData.rx_buf = rxBuf;
    i2cData.tx_len = 1;
    i2cData.rx_len = 1;
    i2c_transfer(&i2cData);

    if(rxBuf[0]!=0x60) // sensor isn't a BME280 with humidity unit
        return NAN;
    
    bme280_readTemperature(); // must be done first to get t_fine
    
    txBuf = BME280_REGISTER_HUMIDDATA;
    i2cData.tx_buf = &txBuf;
    i2cData.rx_buf = rxBuf;
    i2cData.tx_len = 1;
    i2cData.rx_len = 2;
    i2c_transfer(&i2cData);

    int32_t adc_H = (rxBuf[0] << 8) | rxBuf[1];

    if (adc_H == 0x8000) // value in case humidity measurement was disabled
        return NAN;
    int32_t v_x1_u32r;
    
    v_x1_u32r = (t_fine - ((int32_t)76800));
    
    v_x1_u32r = (((((adc_H << 14) - (((int32_t)_bme280_calib.dig_H4) << 20) -
                    (((int32_t)_bme280_calib.dig_H5) * v_x1_u32r)) + ((int32_t)16384)) >> 15) *
                 (((((((v_x1_u32r * ((int32_t)_bme280_calib.dig_H6)) >> 10) *
                      (((v_x1_u32r * ((int32_t)_bme280_calib.dig_H3)) >> 11) + ((int32_t)32768))) >> 10) +
                    ((int32_t)2097152)) * ((int32_t)_bme280_calib.dig_H2) + 8192) >> 14));
    
    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *
                               ((int32_t)_bme280_calib.dig_H1)) >> 4));
    
    v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
    v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;
    float h = (v_x1_u32r>>12);
    return  h / 1024;
}

/**********************************************
 Public Function: bme280_readHumidity
 
 Purpose: Read humidity
 
 Input Parameter: uint8_t sensor: choose sensor on I2C
                  float seaLevel: pressure at sealevel
 
 Return Value: float
 - level of sensor over sealevel in meter
 - Value NAN means measurement disable or argue out of range
 **********************************************/
float bme280_readAltitude(float seaLevel){
    
    // seaLevel at hPa (mBar), equation from datasheet BMP180, page 16
    
    float atmospheric = bme280_readPressure() / 100.0F;
    return 44330.0 * (1.0 - pow(atmospheric / seaLevel, 0.1903));
}

uint16_t read16_LE(uint8_t reg)
{
    uint8_t rxBuf[2] = {0};
    i2cData.tx_buf = &reg;
    i2cData.rx_buf = rxBuf;
    i2cData.tx_len = 1;
    i2cData.rx_len = 2;
    i2c_transfer(&i2cData);

    uint16_t temp = (rxBuf[0] << 8) | rxBuf[1];
    return (temp >> 8) | (temp << 8);
    
}

int16_t readS16(uint8_t reg)
{
    uint8_t rxBuf[2] = {0};
    i2cData.tx_buf = &reg;
    i2cData.rx_buf = rxBuf;
    i2cData.tx_len = 1;
    i2cData.rx_len = 2;
    i2c_transfer(&i2cData);
    return (int16_t)((rxBuf[0] << 8) | rxBuf[1]);
    
}

int16_t readS16_LE(uint8_t reg)
{
    return (int16_t)read16_LE(reg);
    
}


void bme280_readCoefficients()
{
    uint8_t txBuf = 0;
    uint8_t rxBuf[2] = {0};
    i2cData.tx_buf = &txBuf;
    i2cData.rx_buf = rxBuf;
    i2cData.tx_len = 1;
    i2cData.rx_len = 1;

    _bme280_calib.dig_T1 = read16_LE(BME280_REGISTER_DIG_T1);
    _bme280_calib.dig_T2 = readS16_LE(BME280_REGISTER_DIG_T2);
    _bme280_calib.dig_T3 = readS16_LE(BME280_REGISTER_DIG_T3);
    
    _bme280_calib.dig_P1 = read16_LE(BME280_REGISTER_DIG_P1);
    _bme280_calib.dig_P2 = readS16_LE(BME280_REGISTER_DIG_P2);
    _bme280_calib.dig_P3 = readS16_LE(BME280_REGISTER_DIG_P3);
    _bme280_calib.dig_P4 = readS16_LE(BME280_REGISTER_DIG_P4);
    _bme280_calib.dig_P5 = readS16_LE(BME280_REGISTER_DIG_P5);
    _bme280_calib.dig_P6 = readS16_LE(BME280_REGISTER_DIG_P6);
    _bme280_calib.dig_P7 = readS16_LE(BME280_REGISTER_DIG_P7);
    _bme280_calib.dig_P8 = readS16_LE(BME280_REGISTER_DIG_P8);
    _bme280_calib.dig_P9 = readS16_LE(BME280_REGISTER_DIG_P9);

    i2cData.tx_buf = &txBuf;
    i2cData.rx_buf = rxBuf;
    i2cData.tx_len = 1;
    i2cData.rx_len = 1;
    txBuf = BME280_REGISTER_CHIPID;
    i2c_transfer(&i2cData);

    if(rxBuf[0]==0x60){
        // sensor is a BME280 with humidity unit
        txBuf = BME280_REGISTER_DIG_H1;
        i2c_transfer(&i2cData);
        _bme280_calib.dig_H1 = rxBuf[0];

        _bme280_calib.dig_H2 = readS16_LE(BME280_REGISTER_DIG_H2);

        txBuf = BME280_REGISTER_DIG_H3;
        i2c_transfer(&i2cData);
        _bme280_calib.dig_H3 = rxBuf[0];
        txBuf = BME280_REGISTER_DIG_H4;
        i2c_transfer(&i2cData);
        txBuf = BME280_REGISTER_DIG_H4+1;
        i2cData.rx_buf = &rxBuf[1];
        i2c_transfer(&i2cData);
        _bme280_calib.dig_H4 = (rxBuf[0] << 4) | (rxBuf[1] & 0xF);

        txBuf = BME280_REGISTER_DIG_H5 + 1;
        i2c_transfer(&i2cData);
        txBuf = BME280_REGISTER_DIG_H5;
        i2cData.rx_buf = &rxBuf[1];
        i2c_transfer(&i2cData);
        _bme280_calib.dig_H5 = (rxBuf[0] << 4) | (rxBuf[1] >> 4);

        txBuf = BME280_REGISTER_DIG_H6;
        i2c_transfer(&i2cData);
        _bme280_calib.dig_H6 = (int8_t)rxBuf[0];
    }
}
