/*
   Author: James Beasley
   Repo: https://github.com/embeddedcognition/satclient
*/

#ifndef I2CDEVICE_H_
#define I2CDEVICE_H_

#include <stdint.h>         //using for "uint8_t" type
#include <stdbool.h>        //using for "bool" type
#include <mraa/i2c.h>       //using for Intel MRAA low-level library

//i2c device object representation
typedef struct i2c_device
{
    mraa_i2c_context i2c_context;  //mraa i2c bus handle
}I2C_DEVICE;

//function declarations
bool init_i2c_device(I2C_DEVICE*, const int, const uint8_t);
void shutdown_i2c_device(I2C_DEVICE*);
bool read_byte(I2C_DEVICE*, const uint8_t, uint8_t*);
bool read_bytes(I2C_DEVICE*, const uint8_t, uint8_t*, const int);
bool write_byte(I2C_DEVICE*, const uint8_t, const uint8_t);
bool write_bytes(I2C_DEVICE*, const uint8_t, const uint8_t*, const int);

#endif /* I2CDEVICE_H_ */
