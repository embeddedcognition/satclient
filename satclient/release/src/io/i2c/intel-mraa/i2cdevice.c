/*
   Author: James Beasley
   Repo: https://github.com/embeddedcognition/satclient
*/

#include <stdio.h>          //using for "printf" function
#include <stdlib.h>         //using for "malloc" and "free" functions, and "NULL" macro
#include <string.h>         //using "memcpy" function
#include "i2cdevice.h"

//global vars
static const int READ_BYTE_FAILURE = -1;
//** bit mask constants **
/*
    Enables a multi-byte read/write: supplied starting register (byte) is auto incremented (to the next byte)
    to read/write the series of bytes desired see pages 32-33 of the lsm9ds0 data sheet for info -
    http://www.st.com/st-web-ui/static/active/en/resource/technical/document/datasheet/DM00087365.pdf
*/
static const uint8_t ENABLE_ADDRESS_AUTO_INCREMENT = 0x80;

//function definition
//init the i2c device
bool init_i2c_device(I2C_DEVICE* device, const int bus, const uint8_t device_addr)
{
    //check input
    if (device != NULL)
    {
        //create a handle to the i2c bus through the mraa library
        device->i2c_context = mraa_i2c_init(bus);

        //if the handle was successfully created
        if (device->i2c_context != NULL)
        {
            //set the address of the particular board/sensor connecting on the i2c bus and check result status
            if (mraa_i2c_address(device->i2c_context, device_addr) == MRAA_SUCCESS)
            {
                //success
                return true;
            }

            //deallocate i2c context
            mraa_i2c_stop(device->i2c_context);
        }
    }

    //failure
    return false;
}

//function definition
//deinit the i2c device
void shutdown_i2c_device(I2C_DEVICE* device)
{
    //check input
    if (device != NULL)
    {
        //deallocate mraa i2c bus handle
        mraa_i2c_stop(device->i2c_context);
    }
}

//function definition
//read a byte of data (into the supplied data buffer) from the device at a particular register address
bool read_byte(I2C_DEVICE* device, const uint8_t register_addr, uint8_t* data_buffer)
{
    //local vars
    int read_result;    //byte read is returned as int since a signed error code is also a possible result, therefore we check for error code then cast to byte

    //check inputs
    if ((device != NULL) && (data_buffer != NULL))
    {
        //read byte (returned as int)
        read_result = mraa_i2c_read_byte_data(device->i2c_context, register_addr);

        //check result status (if no error)
        if (read_result != READ_BYTE_FAILURE)
        {
            //cast read_result to byte and store in supplied byte buffer
            *data_buffer = (uint8_t)read_result;

            //success
            return true;
        }
    }

    //failure
    return false;
}

//function definition
/*
    Read a series of bytes of data (into the supplied data buffer) from the device at a particular register address,
    in order to enable a multi-byte read the most significant bit of the register address must be 1 therefore the supplied
    register address will be OR'ed with 0x80
*/
bool read_bytes(I2C_DEVICE* device, const uint8_t register_addr, uint8_t* data_buffer, const int count_of_bytes_to_read)
{
    //check inputs
    if ((device != NULL) && (data_buffer != NULL))
    {
        //read bytes and check result status
        if (mraa_i2c_read_bytes_data(device->i2c_context, (register_addr | ENABLE_ADDRESS_AUTO_INCREMENT), data_buffer, count_of_bytes_to_read) == count_of_bytes_to_read)
        {
            //success
            return true;
        }
    }

    //failure
    return false;
}

//function definition
//write a byte of data to the device at a particular register address
bool write_byte(I2C_DEVICE* device, const uint8_t register_addr, const uint8_t data)
{
    //check input
    if (device != NULL)
    {
        //write byte and check result status
        if (mraa_i2c_write_byte_data(device->i2c_context, data, register_addr) == MRAA_SUCCESS)
        {
            //success
            return true;
        }
    }

    //failure
    return false;
}

//function definition
/*
    Write a series of bytes of data to the device at a particular register address,
    in order to enable a multibyte write the most significant bit of the register
    address must be 1 therefore the supplied register address will be OR'ed with 0x80
*/
bool write_bytes(I2C_DEVICE* device, const uint8_t register_addr, const uint8_t* data, const int count_of_bytes_to_write)
{
    //local vars
    mraa_result_t result;
    uint8_t* write_buffer;

    //check input
    if (device != NULL)
    {
        //allocate a buffer that can hold the register and the data to write
        //(no need to zero memory (e.g. calloc) as we're writing to it immediately
        write_buffer = malloc((count_of_bytes_to_write + 1) * (sizeof (uint8_t)));

        //if the buffer was successfully created
        if (write_buffer != NULL)
        {
            //write register addr
            write_buffer[0] = (register_addr | ENABLE_ADDRESS_AUTO_INCREMENT);

            //tag on the rest of the data to be written at the particular register addr
            memcpy(&(write_buffer[1]), data, count_of_bytes_to_write);

            //write bytes
            result = mraa_i2c_write(device->i2c_context, write_buffer, count_of_bytes_to_write);

            //deallocate buffer
            free(write_buffer);

            //check result status
            if (result == MRAA_SUCCESS)
            {
                //success
                return true;
            }
        }
    }

    //failure
    return false;
}
