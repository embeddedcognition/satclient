/*
   Author: James Beasley
   Repo: https://github.com/embeddedcognition/satclient
*/

#include "lsm9ds0.h"
#include "lsm9ds0_private.h"

//function definition
//init the lsm9ds0 board (the sensors on this specific integrated circuit)
bool init_lsm9ds0(LSM9DS0* lsm)
{
    //check input
    if (lsm != NULL)
    {
        //if the i2c device connections were successfully initialized
        if (init_i2c_device(&(lsm->accel_magneto_i2c_device), I2C_BUS, ACCEL_MAGNETO_ADDR) && init_i2c_device(&(lsm->gyro_i2c_device), I2C_BUS, GYRO_ADDR))
        {
            //if the sensors were successfully initialized
            if (init_gyro(&(lsm->gyro_i2c_device)) && init_accel(&(lsm->accel_magneto_i2c_device)) && init_magneto(&(lsm->accel_magneto_i2c_device)))
            {
                //get scale factor (based on FSR sensitivity) to apply to raw signal reading
                lsm->gyro_scale_factor = get_fsr_scale_factor(GYRO_FSR_245DPS);
                lsm->accel_scale_factor = get_fsr_scale_factor(ACCEL_FSR_2G);
                lsm->magneto_scale_factor = get_fsr_scale_factor(MAGNETO_FSR_2GS);

                //success
                return true;
            }
        }
    }

    //failure
    return false;
}

//shutdown the lsm9ds0 board object
void shutdown_lsm9ds0(LSM9DS0* lsm)
{
    //check input
    if (lsm != NULL)
    {
        //deallocate i2c device handles
        shutdown_i2c_device(&(lsm->accel_magneto_i2c_device));
        shutdown_i2c_device(&(lsm->gyro_i2c_device));
    }
}

//function definition
//init gyro sensor bit fields (using the particular i2c device handler it's associated with)
static bool init_gyro(I2C_DEVICE* device)
{
    //check input
    if (device != NULL)
    {
        //if the bit fields were successfully set
        //CTRL_REG1_G - set output data rate of 95hz & 12.5 cutoff 0000=0, normal mode and xyz enabled 1111=F
        //CTRL_REG2_G - defaults
        //CTRL_REG3_G - defaults
        //CTRL_REG4_G - defaults
        //CTRL_REG5_G - defaults
        if (write_byte(device, CTRL_REG1_G, 0x0F) &&
            write_byte(device, CTRL_REG2_G, 0x00) &&
            write_byte(device, CTRL_REG3_G, 0x00) &&
            write_byte(device, CTRL_REG4_G, 0x00) &&
            write_byte(device, CTRL_REG5_G, 0x00))
        {
            //success
            return true;
        }
    }

    //failure
    return false;
}

//function definition
//init accel sensor bit fields
static bool init_accel(I2C_DEVICE* device)
{
    //check input
    if (device != NULL)
    {
        //if the bit fields were successfully set
        //CTRL_REG0_XM - defaults
        //CTRL_REG1_XM - set output data rate of 100hz 0110=6, continuous update and xyz enabled 0111=7
        //CTRL_REG2_XM - set anti-alias filter to 773hz 00, acceleration full scale to 2g 000, self-test to normal 00, serial interface to 4-wire 0 (default)
        //CTRL_REG3_XM - don't set interrupts
        if (write_byte(device, CTRL_REG0_XM, 0x00) &&
            write_byte(device, CTRL_REG1_XM, 0x67) &&
            write_byte(device, CTRL_REG2_XM, 0x00) &&
            write_byte(device, CTRL_REG3_XM, 0x00))
        {
            //success
            return true;
        }
    }

    //failure
    return false;
}

//function definition
//init magneto sensor bit fields
static bool init_magneto(I2C_DEVICE* device)
{
    //check input
    if (device != NULL)
    {
        //if the bit fields were successfully set
        //CTRL_REG4_XM - don't set interrupts
        //CTRL_REG5_XM - set temperature sensor to off 0, resolution to high 11, output data rate to 100hz 101, don't set interrupts 00 -> 0111=7, 0100=4
        //CTRL_REG6_XM - set to +-2 gauss
        //CTRL_REG7_XM - defaults
        if (write_byte(device, CTRL_REG4_XM, 0x00) &&
            write_byte(device, CTRL_REG5_XM, 0x74) &&
            write_byte(device, CTRL_REG6_XM, 0x00) &&
            write_byte(device, CTRL_REG7_XM, 0x00))
        {
            //success
            return true;
        }
    }

    //failure
    return false;
}

//function definition
//get whoami id associated with a particular sensor
bool get_sensor_id(LSM9DS0* lsm, LSM9DS0_SENSOR sensor, uint8_t* sensor_id)
{
    //local vars
    uint8_t register_addr;
    I2C_DEVICE* device;

    //check inputs
    if ((lsm != NULL) && (sensor_id != NULL))
    {
        //set the register address & device based on the sensor type
        switch (sensor)
        {
            case ACCEL:
                register_addr = WHO_AM_I_XM;                    //set who am i address
                device = &(lsm->accel_magneto_i2c_device);      //set accel_magneto device
                break;
            case GYRO:
                register_addr = WHO_AM_I_G;                     //set who am i address
                device = &(lsm->gyro_i2c_device);               //set gyro device
                break;
            case MAGNETO:
                register_addr = WHO_AM_I_XM;                    //set who am i address
                device = &(lsm->accel_magneto_i2c_device);      //set accel_magneto device
                break;                                          //unnecessary but added for consistency
        }

        //if we successfully read a byte
        if (read_byte(device, register_addr, sensor_id))
        {
            //success
            return true;
        }
    }

    //failure
    return false;
}

//function definition
//get latest xyz reading from a particular sensor
bool get_latest_signal_reading(LSM9DS0* lsm, LSM9DS0_SENSOR sensor, LSM9DS0_SIGNAL_READING* signal_reading)
{
    //local vars
    uint8_t data_buffer[READ_BYTES_BLOCK_SIZE];
    uint8_t register_addr;
    double scale_factor;
    I2C_DEVICE* device;
    bool signal_reading_overrun_occurrence;

    //check inputs
    if ((lsm != NULL) && (signal_reading != NULL))
    {
        //set the register address, scale factor, and device based on the sensor type
        switch (sensor)
        {
            case ACCEL:
                register_addr = OUT_X_L_A;                      //read from address OUT_X_L_A to OUT_Z_H_A
                scale_factor = lsm->accel_scale_factor;         //get scale factor for this sensor
                device = &(lsm->accel_magneto_i2c_device);      //set accel_magneto device
                break;
            case GYRO:
                register_addr = OUT_X_L_G;                      //read from address OUT_X_L_G to OUT_Z_H_G
                scale_factor = lsm->gyro_scale_factor;          //set calculated resolution for this sensor
                device = &(lsm->gyro_i2c_device);               //set gyro device
                break;
            case MAGNETO:
                register_addr = OUT_X_L_M;                      //read from address OUT_X_L_M to OUT_Z_H_M
                scale_factor = lsm->magneto_scale_factor;       //set calculated resolution for this sensor
                device = &(lsm->accel_magneto_i2c_device);      //set accel_magneto device
                break;                                          //unnecessary but added for consistency
        }

        //check to see if a reading overrun occurred (did not read the latest signal sample in time) for the particular sensor
        if (check_signal_reading_overrun_occurrence(lsm, sensor, &signal_reading_overrun_occurrence))
        {
            //if an overrun occurred
            if (signal_reading_overrun_occurrence)
            {
                fprintf(stderr, "WARNING: SIGNAL OVERRUN OCCURRED!\n");
            }
        }
        else
        {
            fprintf(stderr, "ERROR: FAILED TO CHECK FOR SIGNAL OVERRUN OCCURRENCE!\n");
        }

        //read 6 bytes of data (3 words - x,y,z) - each word is in two's compliment (little endian)
        //if we successfully read 6 bytes
        if (read_bytes(device, register_addr, data_buffer, READ_BYTES_BLOCK_SIZE))
        {
            /*
                For example, 0x1EF8 might be read as the current x-axis sample (data_buffer[0] = 1E, data_buffer[1] = F8),
                since this is stored in little-endian representation, the bytes will need to be swapped, so now we should
                have 0xF81E. This 16-bit value is stored in two's compliment representation, so it will need to be converted
                to decimal before use. In this case, the binary representation of 0xF81E is 1111100000011110. If the sign bit
                was 0, (e.g., 0111100000011110) no additional steps would be necessary, simply convert to decimal. In our case, the
                sign bit is 1, so a few steps need to be followed. First, take the one's compliment of the binary representation
                (e.g., invert all bits), so we should now have 0000011111100001. Next we need to add 1, resulting in 0000011111100010.
                Next we convert to decimal, resulting in 2018. Finally, applying a negative gives us the actual number (e.g., -2018).
                In practice, this conversion operation is not necessary as casting to a signed decimal type (int16_t) automatically
                does the conversion. We aren't done yet, as the number is still a raw value and must be scaled before actual use
                (e.g., -2018 * 0.000061 = -0.123098 g).
            */

            //bytes 1 (MSB) & 0 (LSB) form a word representing the x axis value
            signal_reading->x = (int16_t)((((uint16_t)data_buffer[1]) << 8) | ((uint16_t)data_buffer[0]));  //bytes are little endian, so swap them and combine into a uint16_t, then casting to int16_t auto converts from two's compliment to decimal
            signal_reading->x *= scale_factor;                                                              //apply scale factor to raw reading

            //bytes 3 (MSB) & 2 (LSB) form a word representing the y axis value
            signal_reading->y = (int16_t)((((uint16_t)data_buffer[3]) << 8) | ((uint16_t)data_buffer[2]));  //bytes are little endian, so swap them and combine into a uint16_t, then casting to int16_t auto converts from two's compliment to decimal
            signal_reading->y *= scale_factor;                                                              //apply scale factor to raw reading

            //bytes 5 (MSB) & 4 (LSB) form a word representing the z axis value
            signal_reading->z = (int16_t)((((uint16_t)data_buffer[5]) << 8) | ((uint16_t)data_buffer[4]));  //bytes are little endian, so swap them and combine into a uint16_t, then casting to int16_t auto converts from two's compliment to decimal
            signal_reading->z *= scale_factor;                                                              //apply scale factor to raw reading

            //success
            return true;
        }
    }

    //failure
    return false;
}

//function definition
//get scale factor (sensitivity) based on supplied sensor FSR (full scale range)
//from table 3, page 13 of data sheet comment in lsm9ds0_private header file
static double get_fsr_scale_factor(SENSOR_FSR fsr)
{
    switch (fsr)
    {
        case GYRO_FSR_245DPS:
            //8.75 * 10^-3 = 0.00875
            return 0.00875;
        case GYRO_FSR_500DPS:
            //17.50 * 10^-3 = 0.01750
            return 0.01750;
        case GYRO_FSR_2000DPS:
            //70 * 10^-3 = 0.070
            return 0.070;
        case ACCEL_FSR_2G:
            //0.061 * 10^-3 = 0.000061
            return 0.000061;
        case ACCEL_FSR_4G:
            //0.122 * 10^-3 = 0.000122
            return 0.000122;
        case ACCEL_FSR_6G:
            //0.183 * 10^-3 = 0.000183
            return 0.000183;
        case ACCEL_FSR_8G:
            //0.244 * 10^-3 = 0.000244
            return 0.000244;
        case ACCEL_FSR_16G:
            //0.732 * 10^-3 = 0.000732
            return 0.000732;
        case MAGNETO_FSR_2GS:
            //0.08 * 10^-3 = 0.00008
            return 0.00008;
        case MAGNETO_FSR_4GS:
            //0.16 * 10^-3 = 0.00016
            return 0.00016;
        case MAGNETO_FSR_8GS:
            //0.32 * 10^-3 = 0.00032
            return 0.00032;
        case MAGNETO_FSR_12GS:
            //0.48 * 10^-3 = 0.00048
            return 0.00048;
    }

    //no match
    return 0;
}

//function definition
//check if a new signal reading is available for a particular sensor
bool check_signal_reading_availability(LSM9DS0* lsm, LSM9DS0_SENSOR sensor, bool* signal_reading_availability)
{
    //local vars
    uint8_t status_bit_field;
    uint8_t register_addr;
    I2C_DEVICE* device;

    //check inputs
    if ((lsm != NULL) && (signal_reading_availability != NULL))
    {
        //set the register address & device based on the sensor type
        switch (sensor)
        {
            case ACCEL:
                register_addr = STATUS_REG_A;                   //set status register address
                device = &(lsm->accel_magneto_i2c_device);      //set accel_magneto device
                break;
            case GYRO:
                register_addr = STATUS_REG_G;                   //set status register address
                device = &(lsm->gyro_i2c_device);               //set gyro device
                break;
            case MAGNETO:
                register_addr = STATUS_REG_M;                   //set status register address
                device = &(lsm->accel_magneto_i2c_device);      //set accel_magneto device
                break;                                          //unnecessary but added for consistency
        }

        //if we successfully read a byte (the status bit field from the particular sensor)
        if (read_byte(device, register_addr, &status_bit_field))
        {
            //determine if a new reading is available for x,y,z by anding with appropriate mask
            if ((status_bit_field & NEW_SIGNAL_READING_AVAILABLE) != 0)
            {
                *signal_reading_availability = true; //new xyz reading available
            }
            else
            {
                *signal_reading_availability = false; //no new xyz reading available
            }

            //success
            return true;
        }
    }

    //failure
    return false;
}

//function definition
//check if an overrun occurred for a particular sensor (sensor has overwritten x,y,z signal reading before it could be read by client)
static bool check_signal_reading_overrun_occurrence(LSM9DS0* lsm, LSM9DS0_SENSOR sensor, bool* signal_reading_overrun_occurrence)
{
    //local vars
    uint8_t status_bit_field;
    uint8_t register_addr;
    I2C_DEVICE* device;

    //check inputs
    if ((lsm != NULL) && (signal_reading_overrun_occurrence != NULL))
    {
        //set the register address & device based on the sensor type
        switch (sensor)
        {
            case ACCEL:
                register_addr = STATUS_REG_A;                   //set status register address
                device = &(lsm->accel_magneto_i2c_device);      //set accel_magneto device
                break;
            case GYRO:
                register_addr = STATUS_REG_G;                   //set status register address
                device = &(lsm->gyro_i2c_device);               //set gyro device
                break;
            case MAGNETO:
                register_addr = STATUS_REG_M;                   //set status register address
                device = &(lsm->accel_magneto_i2c_device);      //set accel_magneto device
                break;                                          //unnecessary but added for consistency
        }

        //if we successfully read a byte (the status bit field from the particular sensor)
        if (read_byte(device, register_addr, &status_bit_field))
        {
            //determine if the last x,y,z signal was overwritten (before we could read it) by anding with appropriate mask
            if ((status_bit_field & XYZ_SIGNAL_OVERRUN_OCCURRED) != 0)
            {
                *signal_reading_overrun_occurrence = true; //overrun occurred
            }
            else
            {
                *signal_reading_overrun_occurrence = false; //no overrun occurred
            }

            //success
            return true;
        }
    }

    //failure
    return false;
}
