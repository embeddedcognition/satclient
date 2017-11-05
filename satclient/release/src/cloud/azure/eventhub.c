/*
   Author: James Beasley
   Repo: https://github.com/embeddedcognition/satclient
*/

#include <stdio.h>              //using for "printf" functions
#include <stdint.h>             //using for "uint8_t" type
#include <stdlib.h>             //using for "malloc" and "free" functions, and "NULL"
#include <string.h>             //using for "strlen" function
#include "authutil.h"           //using for azure service bus authentication/authorization functions
#include "eventhub.h"

//global vars
static const char EVENT_HUB_NODE_NAME[] = "YOUR_VALUE";             //azure service bus event hub entity/node name
static const char SHARED_ACCESS_POLICY_NAME[] = "YOUR_VALUE";       //shared access policy that specifies particular rights to the event hub (in this case "send")

//function declarations
static bool init_event_hub(EVENT_HUB*);

//function definition
//create event hub object
EVENT_HUB* new_event_hub(void)
{
    //local vars
    EVENT_HUB* ehub;

    //allocate event hub object
    ehub = malloc(sizeof (EVENT_HUB));

    //if the object was successfully created
    if (ehub != NULL)
    {
        //if the event hub was successfully initialized
        if (init_event_hub(ehub))
        {
            //return handle to new object
            return ehub;
        }

        //deallocate event hub
        free(ehub);
    }

    //failure
    return NULL;
}

//function definition
//init the event hub
static bool init_event_hub(EVENT_HUB* ehub)
{
    //check input
    if (ehub != NULL)
    {
        //create a handle to the messaging client (interface to service bus)
        ehub->mclient = new_messaging_client();

        //if the handle was successfully created
        if (ehub->mclient != NULL)
        {
            //create a formatted endpoint to the azure service bus event hub entity
            ehub->event_hub_endpoint = create_service_bus_endpoint(EVENT_HUB_NODE_NAME);
            //create shared access (shared secret) token
            ehub->shared_access_token = create_shared_access_token(ehub->event_hub_endpoint, SHARED_ACCESS_POLICY_NAME);

            //if the endpoint and token were successfully created
            if ((ehub->event_hub_endpoint != NULL) && (ehub->shared_access_token != NULL))
            {
                printf("SAS: %s\n", ehub->shared_access_token);

                //before events can be sent to the event hub, our shared access token must first be validated
                //by the special claims-based security ($cbs) service bus node
                if (authenticate_claim(ehub->mclient, ehub->shared_access_token))
                {
                    //success
                    return true;
                }
            }

            //free all buffers (as we don't know which failed)
            free(ehub->event_hub_endpoint);
            free(ehub->shared_access_token);
        }
    }

    //failure
    return false;
}

//function definition
//deinit the event hub
void free_event_hub(EVENT_HUB* ehub)
{
    //check input
    if (ehub != NULL)
    {
        //deallocate messaging client object
        free_messaging_client(ehub->mclient);

        //deallocate endpoints/tokens
        free(ehub->event_hub_endpoint);
        free(ehub->shared_access_token);
        //deallocate event hub object
        free(ehub);
    }
}

//function definition
//publish a telemetry reading to an existing azure event hub
bool publish_telemetry_to_event_hub(EVENT_HUB* ehub, TELEMETRY_READING* reading)
{
    //check inputs

    //publish the message
    return publish_message(ehub->mclient, ehub->event_hub_endpoint, reading->json);
}
