/*
   Author: James Beasley
   Repo: https://github.com/embeddedcognition/satclient
*/

#include <stdio.h>              //used for "printf/sprintf" functions and "NULL" macro
#include <stdint.h>             //using for "uint8_t" type
#include <time.h>               //using for "time" function
#include "lsm9ds0.h"            //using lsm9ds0 board
#include "iotdevicegateway.h"   //using to publish events to aws iot device gateway
#include "lsm9ds0processor.h"

//function declarations
static void display_sensor_info(LSM9DS0*);
static void poll_for_signal_readings(LSM9DS0*);
static bool convert_lsm9ds0_signal_reading_aggregate_to_telemetry_reading(LSM9DS0_SIGNAL_READING_AGGREGATE*, TELEMETRY_READING*, int);

//function definition
//performs signal acquisition and telemetry process until desired limit is reached
bool perform_lsm9ds0_sat(int desired_processing_limit)
{
    //local vars
    bool operation_status = false;          //denotes success or failure of the operation
    int sequence_id = 0;                    //zero indexed - order of signal readings (each telemetry reading is tagged with a sequence number)
    LSM9DS0 lsm;
    IOT_DEVICE_GATEWAY device_gateway;
    LSM9DS0_SIGNAL_READING_AGGREGATE signal_reading_aggregate;
    TELEMETRY_READING telemetry;

    //if the board and gateway were successfully initialized
    if (init_lsm9ds0(&lsm) && init_iot_device_gateway(&device_gateway))
    {
        //display sensor info
        display_sensor_info(lsm);

        //loop forever
        while (true)
        {
            //block until new signal readings are available
            poll_for_signal_readings(lsm);

            //** perform signal acquisition **
            //get the latest accelerometer, magnetometer, and gyroscope readings (will also check for any overruns)
            if (get_latest_signal_reading(lsm, ACCEL, &(signal_reading_aggregate.accel)) &&
                get_latest_signal_reading(lsm, MAGNETO, &(signal_reading_aggregate.magneto)) &&
                get_latest_signal_reading(lsm, GYRO, &(signal_reading_aggregate.gyro)))
            {
                //** perform signal transformation **
                //convert signal reading aggregate to telemetry reading
                //if successful conversion
                if (convert_lsm9ds0_signal_reading_aggregate_to_telemetry_reading(&signal_reading_aggregate, &telemetry, sequence_id))
                {
                    //** perform data transmission **
                    //publish telemetry reading to aws iot device gateway (fire and forget)
                    publish_telemetry_to_device_gateway(&device_gateway, &telemetry);

                    //break out if we've sent our limit of messages for this run of the SAT client
                    if (sequence_id == desired_processing_limit)
                    {
                        //success
                        operation_status = true;
                        break;
                    }

                    //advance the sequence id
                    sequence_id++;
                }
                else
                {
                    fprintf(stderr, "ERROR: FAILED TO CONVERT SIGNAL READING AGGREGATE TO TELEMETRY READING!\n");
                }
            }
            else
            {
                fprintf(stderr, "ERROR: FAILED TO OBTAIN LATEST SIGNAL READINGS!\n");
            }
        }
    }
    else
    {
        fprintf(stderr, "ERROR: FAILED TO INITIALIZE LSM9DS0 AND/OR IOT DEVICE GATEWAY OBJECT(S)!\n");
    }

    //shutdown iot device gateway
    shutdown_iot_device_gateway(device_gateway);

    return operation_status;
}

//function definition
//display the onboard sensor info
static void display_sensor_info(LSM9DS0* lsm)
{
    //local vars
    uint8_t sensor_id;

    //check input
    if (lsm != NULL)
    {
        //print sensor IDs
        printf("IMU - ");
        get_sensor_id(lsm, ACCEL, &sensor_id);
        printf("Accel ID: 0x%X (should equal: 0x49), ", sensor_id);
        get_sensor_id(lsm, MAGNETO, &sensor_id);
        printf("Magneto ID: 0x%X (should equal: 0x49), ", sensor_id);
        get_sensor_id(lsm, GYRO, &sensor_id);
        printf("Gyro ID: 0x%X (should equal: 0xD4)\n", sensor_id);
    }
}

//function definition
//loop until new signal readings are available
static void poll_for_signal_readings(LSM9DS0* lsm)
{
    //local vars
    bool accel_reading_availability = false;
    bool magneto_reading_availability = false;
    bool gyro_reading_availability = false;

    //check input
    if (lsm != NULL)
    {
        //loop until a new signal reading is available from the accelerometer, magnetometer, and gyroscope
        while ((!accel_reading_availability) && (!magneto_reading_availability) && (!gyro_reading_availability))
        {
            //check to see if a new accelerometer reading is available
            if (!check_signal_reading_availability(lsm, ACCEL, &accel_reading_availability))
            {
                fprintf(stderr, "ERROR: FAILED TO CHECK FOR ACCEL SIGNAL READING AVAILABILITY!\n");
            }
            //check to see if a new magnetometer reading is available
            if (!check_signal_reading_availability(lsm, MAGNETO, &magneto_reading_availability))
            {
                fprintf(stderr, "ERROR: FAILED TO CHECK FOR MAGNETO SIGNAL READING AVAILABILITY!\n");
            }
            //check to see if a new gyroscope reading is available
            if (!check_signal_reading_availability(lsm, GYRO, &gyro_reading_availability))
            {
                fprintf(stderr, "ERROR: FAILED TO CHECK FOR GYRO SIGNAL READING AVAILABILITY!\n");
            }
        }
    }
}

//function definition
//convert a lsm9ds0 signal reading aggregate object to a telemetry reading object
static bool convert_lsm9ds0_signal_reading_aggregate_to_telemetry_reading(LSM9DS0_SIGNAL_READING_AGGREGATE* signal_reading_aggregate, TELEMETRY_READING* telemetry, int sequence_id)
{
    //local vars
    time_t timestamp;                       //number of seconds since unix epoch
    struct tm* decomposed_timestamp;        //timestamp value broken up into a tm structure
    char timestamp_string[25];              //formatted string version of the timestamp

    //check inputs
    if ((signal_reading_aggregate != NULL) && (telemetry != NULL))
    {
        //generate & format timestamp
        time(&timestamp);                                                   //get current time (number of seconds since unix epoch)
        decomposed_timestamp = gmtime(&timestamp);                          //broken down time into tm structure
        strftime(timestamp_string, 25, "%F %T", decomposed_timestamp);      //format broken down time as 'yyyy-mm-dd hh:mm:ss'

        //generate json formatted payload
        sprintf(telemetry->json,
                "{"
                "\"device_id\":\"edison_alva1\","
                "\"sequence_id\":%d,"
                "\"timestamp\":\"%s\","
                "\"accel\":"
                    "{"
                      "\"x\":%f,"
                      "\"y\":%f,"
                      "\"z\":%f"
                    "},"
                "\"magneto\":"
                    "{"
                      "\"x\":%f,"
                      "\"y\":%f,"
                      "\"z\":%f"
                    "},"
                "\"gyro\":"
                    "{"
                      "\"x\":%f,"
                      "\"y\":%f,"
                      "\"z\":%f"
                    "}"
                "}",
                sequence_id,
                timestamp_string,
                signal_reading_aggregate->accel.x,
                signal_reading_aggregate->accel.y,
                signal_reading_aggregate->accel.z,
                signal_reading_aggregate->magneto.x,
                signal_reading_aggregate->magneto.y,
                signal_reading_aggregate->magneto.z,
                signal_reading_aggregate->gyro.x,
                signal_reading_aggregate->gyro.y,
                signal_reading_aggregate->gyro.z
        );

        //success
        return true;
    }

    //failure
    return false;
}
