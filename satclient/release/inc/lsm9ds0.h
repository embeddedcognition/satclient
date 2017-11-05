/*
   Author: James Beasley
   Repo: https://github.com/embeddedcognition/satclient
*/

#ifndef LSM9DS0_H_
#define LSM9DS0_H_

#include <stdint.h>         //using for "uint8_t", "uint16_t", and "int16_t" types
#include <stdbool.h>        //using for "bool" type
#include "i2cdevice.h"      //using to access I2C bus

//enum for use in determining which sensor to work with
typedef enum lsm9ds0_sensor
{
    ACCEL,
    GYRO,
    MAGNETO
}LSM9DS0_SENSOR;

//xyz signal reading object representation
typedef struct lsm9ds0_signal_reading
{
    double x;
    double y;
    double z;
}LSM9DS0_SIGNAL_READING;

//signal reading aggregate representation
typedef struct lsm9ds0_signal_reading_aggregate
{
    LSM9DS0_SIGNAL_READING accel;
    LSM9DS0_SIGNAL_READING magneto;
    LSM9DS0_SIGNAL_READING gyro;
}LSM9DS0_SIGNAL_READING_AGGREGATE;

//lsm9ds0 object representation
typedef struct lsm9ds0
{
    //i2c connections
    I2C_DEVICE gyro_i2c_device;
    I2C_DEVICE accel_magneto_i2c_device;
    //sensor scale factors
    double gyro_scale_factor;
    double accel_scale_factor;
    double magneto_scale_factor;
}LSM9DS0;

//function declarations
bool init_lsm9ds0(LSM9DS0*);
void shutdown_lsm9ds0(LSM9DS0* lsm);
bool get_sensor_id(LSM9DS0*, LSM9DS0_SENSOR, uint8_t*);
bool get_latest_signal_reading(LSM9DS0*, LSM9DS0_SENSOR, LSM9DS0_SIGNAL_READING*);
bool check_signal_reading_availability(LSM9DS0*, LSM9DS0_SENSOR, bool*);

#endif /* LSM9DS0_H_ */
