/*
   Author: James Beasley
   Repo: https://github.com/embeddedcognition/satclient
*/

#ifndef EVENTHUB_H_
#define EVENTHUB_H_

#include <stdbool.h>            //using for "bool" type
#include "messagingclient.h"    //using for messaging client library

//of the format: "amqps://{shared access key name}:{shared access token}@{service bus namespace}.servicebus.windows.net/{event hub name}"
//event hub object representation
typedef struct event_hub
{
    MESSAGING_CLIENT* mclient;  //messaging client handle
    char* event_hub_endpoint;   //endpoint for the event hub entity
    char* shared_access_token;  //shared access signature (token)
}EVENT_HUB;

//telemetry reading object representation
typedef struct telemetry_reading
{
    char json[256];  //reading formatted as json
}TELEMETRY_READING;

//function declarations
EVENT_HUB* new_event_hub(void);
void free_event_hub(EVENT_HUB*);
bool publish_telemetry_to_event_hub(EVENT_HUB*, TELEMETRY_READING*);

#endif /* EVENTHUB_H_ */
