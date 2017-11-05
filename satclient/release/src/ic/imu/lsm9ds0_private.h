/*
   Author: James Beasley
   Repo: https://github.com/embeddedcognition/satclient
*/

#ifndef LSM9DS0_PRIVATE_H_
#define LSM9DS0_PRIVATE_H_

#include <stdio.h>      //using for "printf" function
#include <stdlib.h>     //using for "malloc" and "free" functions

//global vars
//** register address constants **
//i2c bus the lsm9ds0 board communicates on
static const int I2C_BUS = 1;
//i2c addresses for sensors
static const uint8_t GYRO_ADDR = 0x6B;
static const uint8_t ACCEL_MAGNETO_ADDR = 0x1D;
//lsm9ds0 data sheet - http://www.st.com/st-web-ui/static/active/en/resource/technical/document/datasheet/DM00087365.pdf
//not all registers are utilized in this implementation but nonetheless included
//device ID register - readonly - multiple sensors could reside on a particular device (e.g. accelerometer & magnetometer)
static const uint8_t WHO_AM_I_XM = 0x0F;    //accelerometer & magnetometer
static const uint8_t WHO_AM_I_G = 0x0F;     //gyro
//*** gyro sensor register addresses ***
//control registers - set sensor behavior
static const uint8_t CTRL_REG1_G = 0x20;
static const uint8_t CTRL_REG2_G = 0x21;
static const uint8_t CTRL_REG3_G = 0x22;
static const uint8_t CTRL_REG4_G = 0x23;
static const uint8_t CTRL_REG5_G = 0x24;
//reference / data capture interrupt register
static const uint8_t REFERENCE_DATACAPTURE_G = 0x25;
//status register - query state - readonly
static const uint8_t STATUS_REG_G = 0x27;
//computed xyz gyro sensor reading registers - readonly
static const uint8_t OUT_X_L_G = 0x28;
static const uint8_t OUT_X_H_G = 0x29;
static const uint8_t OUT_Y_L_G = 0x2A;
static const uint8_t OUT_Y_H_G = 0x2B;
static const uint8_t OUT_Z_L_G = 0x2C;
static const uint8_t OUT_Z_H_G = 0x2D;
//fifo registers
//control
static const uint8_t FIFO_CTRL_REG_G = 0x2E;
//status - readonly
static const uint8_t FIFO_SRC_REG_G = 0x2F;
//interrupt registers
//config
static const uint8_t INT1_CFG_G = 0x30;
//status - readonly
static const uint8_t INT1_SRC_G = 0x31;
//threshold & duration
static const uint8_t INT1_THS_XH_G = 0x32;
static const uint8_t INT1_THS_XL_G = 0x33;
static const uint8_t INT1_THS_YH_G = 0x34;
static const uint8_t INT1_THS_YL_G = 0x35;
static const uint8_t INT1_THS_ZH_G = 0x36;
static const uint8_t INT1_THS_ZL_G = 0x37;
static const uint8_t INT1_DURATION_G = 0x38;
//*** accel & magneto sensor register addresses ***
//computed temperature sensor reading registers - readonly
static const uint8_t OUT_TEMP_L_XM = 0x05;
static const uint8_t OUT_TEMP_H_XM = 0x06;
//status registers - query state - readonly
static const uint8_t STATUS_REG_M  = 0x07;  //magneto
static const uint8_t STATUS_REG_A  = 0x27;  //accel
//computed xyz magnetometer sensor reading registers - readonly
static const uint8_t OUT_X_L_M = 0x08;
static const uint8_t OUT_X_H_M = 0x09;
static const uint8_t OUT_Y_L_M = 0x0A;
static const uint8_t OUT_Y_H_M = 0x0B;
static const uint8_t OUT_Z_L_M = 0x0C;
static const uint8_t OUT_Z_H_M = 0x0D;
//computed xyz accelerometer sensor reading registers - readonly
static const uint8_t OUT_X_L_A = 0x28;
static const uint8_t OUT_X_H_A = 0x29;
static const uint8_t OUT_Y_L_A = 0x2A;
static const uint8_t OUT_Y_H_A = 0x2B;
static const uint8_t OUT_Z_L_A = 0x2C;
static const uint8_t OUT_Z_H_A = 0x2D;
//magneto interrupt registers
//control
static const uint8_t INT_CTRL_REG_M = 0x12;
//status - readonly
static const uint8_t INT_SRC_REG_M = 0x13;
//threshold
static const uint8_t INT_THS_L_M = 0x14;
static const uint8_t INT_THS_H_M = 0x15;
//offset for magneto
static const uint8_t OFFSET_X_L_M = 0x16;
static const uint8_t OFFSET_X_H_M = 0x17;
static const uint8_t OFFSET_Y_L_M = 0x18;
static const uint8_t OFFSET_Y_H_M = 0x19;
static const uint8_t OFFSET_Z_L_M = 0x1A;
static const uint8_t OFFSET_Z_H_M = 0x1B;
//high pass filter reference for accel
static const uint8_t REFERENCE_X = 0x1C;
static const uint8_t REFERENCE_Y = 0x1D;
static const uint8_t REFERENCE_Z = 0x1E;
//control registers - set sensor behavior
//accel
static const uint8_t CTRL_REG0_XM = 0x1F;
static const uint8_t CTRL_REG1_XM = 0x20;
static const uint8_t CTRL_REG2_XM = 0x21;
static const uint8_t CTRL_REG3_XM = 0x22;
//magneto
static const uint8_t CTRL_REG4_XM = 0x23;
static const uint8_t CTRL_REG5_XM = 0x24;
static const uint8_t CTRL_REG6_XM = 0x25;
static const uint8_t CTRL_REG7_XM = 0x26;
//fifo registers
//control
static const uint8_t FIFO_CTRL_REG = 0x2E;
//status - readonly
static const uint8_t FIFO_SRC_REG = 0x2F;
//inertial interrupt generator registers
static const uint8_t INT_GEN_1_REG = 0x30;
static const uint8_t INT_GEN_2_REG = 0x34;
//status - readonly
static const uint8_t INT_GEN_1_SRC = 0x31;
static const uint8_t INT_GEN_2_SRC = 0x35;
//threshold & duration
static const uint8_t INT_GEN_1_THS = 0x32;
static const uint8_t INT_GEN_1_DURATION = 0x33;
static const uint8_t INT_GEN_2_THS = 0x36;
static const uint8_t INT_GEN_2_DURATION = 0x37;
//misc registers
static const uint8_t CLICK_CFG = 0x38;
static const uint8_t CLICK_SRC = 0x39;
static const uint8_t CLICK_THS = 0x3A;
static const uint8_t TIME_LIMIT = 0x3B;
static const uint8_t TIME_LATENCY = 0x3C;
static const uint8_t TIME_WINDOW = 0x3D;
static const uint8_t ACT_THS = 0x3E;
static const uint8_t ACT_DUR = 0x3F;

//** bit mask constants **
static const uint8_t NEW_SIGNAL_READING_AVAILABLE = 0x08;   //binary: 00001000 <- bit 5 is set to 1 signifying a new xyz reading is available
static const uint8_t XYZ_SIGNAL_OVERRUN_OCCURRED = 0x80;    //binary: 10000000 <- LSB is set to 1 signifying an xyz overrun has occurred

static const int READ_BYTES_BLOCK_SIZE = 6;     //the number of bytes to read in a single read_bytes() call

//enum for sensor full scale range (+ or -)
typedef enum sensor_fsr
{
    //gyro (degrees per second - dps) full scale range (+ or -)
    GYRO_FSR_245DPS,
    GYRO_FSR_500DPS,
    GYRO_FSR_2000DPS,
    //accel (g's) full scale range (+ or -)
    ACCEL_FSR_2G,
    ACCEL_FSR_4G,
    ACCEL_FSR_6G,
    ACCEL_FSR_8G,
    ACCEL_FSR_16G,
    //magnetometer (gauss') full scale range (+ or -)
    MAGNETO_FSR_2GS,
    MAGNETO_FSR_4GS,
    MAGNETO_FSR_8GS,
    MAGNETO_FSR_12GS
}SENSOR_FSR;

//function declarations
bool init_lsm9ds0(LSM9DS0*);
static bool init_gyro(I2C_DEVICE*);
static bool init_accel(I2C_DEVICE*);
static bool init_magneto(I2C_DEVICE*);
static double get_fsr_scale_factor(SENSOR_FSR);
static bool check_signal_reading_overrun_occurrence(LSM9DS0*, LSM9DS0_SENSOR, bool*);

#endif /* LSM9DS0_PRIVATE_H_ */
