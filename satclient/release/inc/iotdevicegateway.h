/*
   Author: James Beasley
   Repo: https://github.com/embeddedcognition/satclient
*/

#ifndef IOTDEVICEGATEWAY_H_
#define IOTDEVICEGATEWAY_H_

#include <stdbool.h>                            //using for "bool" type
#include "aws_iot_mqtt_client_interface.h"      //using for AWS IoT device gateway connection

//iot device gatway object representation
typedef struct iot_device_gateway
{
    AWS_IoT_Client client_context;  //aws iot client handle
}IOT_DEVICE_GATEWAY;

//telemetry reading object representation
typedef struct telemetry_reading
{
    char json[256];  //reading formatted as json
}TELEMETRY_READING;

//function declarations
bool init_iot_device_gateway(IOT_DEVICE_GATEWAY*);
bool shutdown_iot_device_gateway(IOT_DEVICE_GATEWAY*);
bool publish_telemetry_to_device_gateway(IOT_DEVICE_GATEWAY*, TELEMETRY_READING*);

#endif /* IOTDEVICEGATEWAY_H_ */
