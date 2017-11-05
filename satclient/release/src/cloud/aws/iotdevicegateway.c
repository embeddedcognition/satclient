/*
   Author: James Beasley
   Repo: https://github.com/embeddedcognition/satclient
*/

#include <stdio.h>              //using for "printf" functions
#include <stdint.h>             //using for "uint8_t" type
#include <stdlib.h>             //using for "malloc" and "free" functions, and "NULL"
#include <string.h>             //using for "strlen" function
#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "iotdevicegateway.h"

//global vars
static const char MQTT_PUBLISH_TOPIC[] = "YOUR_VALUE";  //mqtt topic to publish telemetry to

//function definition
//init the iot device gateway
bool init_iot_device_gateway(IOT_DEVICE_GATEWAY* device_gateway)
{
    //local vars
    bool operation_status = false;              //denotes success or failure of the operation
    IoT_Error_t result_code = FAILURE;          //result code from iot operation
    IoT_Client_Init_Params client_parameters = iotClientInitParamsDefault;
    IoT_Client_Connect_Params connection_parameters = iotClientConnectParamsDefault;

    //check input
    if (device_gateway != NULL)
    {
        //set client parameters
        client_parameters.enableAutoReconnect = false;
        client_parameters.pHostURL = AWS_IOT_MQTT_HOST;
        client_parameters.port = AWS_IOT_MQTT_PORT;
        client_parameters.pRootCALocation = AWS_IOT_ROOT_CA_FILENAME;
        client_parameters.pDeviceCertLocation = AWS_IOT_CERTIFICATE_FILENAME;
        client_parameters.pDevicePrivateKeyLocation = AWS_IOT_PRIVATE_KEY_FILENAME;
        client_parameters.mqttCommandTimeout_ms = 20000;
        client_parameters.tlsHandshakeTimeout_ms = 5000;
        client_parameters.isSSLHostnameVerify = true;
        client_parameters.disconnectHandler = NULL;
        client_parameters.disconnectHandlerData = NULL;

        //attempt to initialize the client context
        result_code = aws_iot_mqtt_init(&(device_gateway->client_context), &client_parameters);

        //if initialization was successful
        if (result_code == SUCCESS)
        {
            //set connection parameters
            connection_parameters.keepAliveIntervalInSec = 10;
            connection_parameters.isCleanSession = true;
            connection_parameters.MQTTVersion = MQTT_3_1_1;
            connection_parameters.pClientID = AWS_IOT_MQTT_CLIENT_ID;
            connection_parameters.clientIDLen = (uint16_t)strlen(AWS_IOT_MQTT_CLIENT_ID);
            connection_parameters.isWillMsgPresent = false;

            //attempt to connect to the aws device gateway
            result_code = aws_iot_mqtt_connect(&(device_gateway->client_context), &connection_parameters);

            //if we successfully connected
            if (result_code == SUCCESS)
            {
                //success
                operation_status = true;
            }
            else
            {
                fprintf(stderr, "ERROR: AWS IOT CONNECT FAILED! - %d - CONNECTING TO: %s:%d\n", result_code, client_parameters.pHostURL, client_parameters.port);
            }
        }
        else
        {
            fprintf(stderr, "ERROR: AWS IOT INIT FAILED! - %d\n", result_code);
        }
    }

    return operation_status;
}

//function definition
//deinit the iot device gateway
void shutdown_iot_device_gateway(IOT_DEVICE_GATEWAY* device_gateway)
{
    //local vars
    IoT_Error_t result_code = FAILURE;  //result code from iot operation

    //check input
    if (device_gateway != NULL)
    {
        //attempt to disconnect from the aws device gateway
        result_code = aws_iot_mqtt_disconnect(&(device_gateway->client_context));

        //if disconnect failed
        if (result_code != SUCCESS)
        {
            fprintf(stderr, "ERROR: AWS IOT DISCONNECT FAILED! - %d\n", result_code);
        }
    }
}

//function definition
//publish a telemetry reading to the aws iot device gateway
bool publish_telemetry_to_device_gateway(IOT_DEVICE_GATEWAY* device_gateway, TELEMETRY_READING* reading)
{
    //local vars
    bool operation_status = false;              //denotes success or failure of the operation
    IoT_Error_t result_code = FAILURE;          //result code from iot operation
    IoT_Publish_Message_Params msg_parameters;  //parameters of the message to publish

    //check inputs
    if ((device_gateway != NULL) && (reading != NULL))
    {
        //set parameters
        msg_parameters.qos = QOS0;	//fire and forget - it may or may not get there
        msg_parameters.payload = (void*)reading->json;
        msg_parameters.isRetained = 0;
        msg_parameters.payloadLen = strlen(reading->json);

        //attempt to publish message
        result_code = aws_iot_mqtt_publish(&(device_gateway->client_context), MQTT_PUBLISH_TOPIC, strlen(MQTT_PUBLISH_TOPIC), &msg_parameters);

        //if the message was successfully published
        if (result_code == SUCCESS)
        {
            //success
            operation_status = true;
        }
        else
        {
            fprintf(stderr, "ERROR: AWS IOT PUBLISH FAILED! - %d\n", result_code);
        }
    }

    return operation_status;
}
