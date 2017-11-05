/*
   Author: James Beasley
   Repo: https://github.com/embeddedcognition/satclient
*/

#include <stdio.h>              //using for "printf" function
#include <stdlib.h>             //using for "malloc" and "free" functions, and "NULL"
#include <string.h>             //using for "strlen" function
#include <proton/message.h>     //using for qpid proton amqp client
#include <messagingclient.h>

//https://github.com/Azure/azure-service-bus-samples/blob/master/proton-c-queues-and-topics/sender.c
//https://blogs.msdn.microsoft.com/servicebus/2014/12/16/using-service-bus-with-the-proton-c-client/

#define check(messenger)                                                     \
  {                                                                          \
    if(pn_messenger_errno(messenger))                                        \
    {                                                                        \
      die(__FILE__, __LINE__, pn_error_text(pn_messenger_error(messenger))); \
    }                                                                        \
  }

//global vars
static const int MESSENGER_START_SUCCESS = 0;		//success code for "pn_messenger_start" function

//function declarations
static bool init_messaging_client(MESSAGING_CLIENT*);
static void die(const char*, int, const char*);

//function definition
//create messaging client object
MESSAGING_CLIENT* new_messaging_client(void)
{
    //local vars
    MESSAGING_CLIENT* mclient;

    //allocate messaging client object
    mclient = malloc(sizeof (MESSAGING_CLIENT));

    //if the object was successfully created
    if (mclient != NULL)
    {
        //if the messaging client was successfully initialized
        if (init_messaging_client(mclient))
        {
            //return handle to new object
            return mclient;
        }

        //deallocate messaging client
        free(mclient);
    }

    //failure
    return NULL;
}

//function definition
//init the messaging client
static bool init_messaging_client(MESSAGING_CLIENT* mclient)
{
    //check input
    if (mclient != NULL)
    {
        //create a handle to the logical queue infrastructure (in & out queues)
        mclient->messenger_context = pn_messenger(NULL);

        //if the handle was successfully created
        if (mclient->messenger_context != NULL)
        {
            //set asynchronous behavior (non-blocking)
            pn_messenger_set_blocking(mclient->messenger_context, false);
            //set outgoing queue window size
            pn_messenger_set_outgoing_window(mclient->messenger_context, 1024);

            //starts up messaging infrastructure
            //if the messenger started successfully
            if (pn_messenger_start(mclient->messenger_context) == MESSENGER_START_SUCCESS)
            {
                //each time a message is "put/pulled" on/from the queue it is copied, so since we're only sending one message at a time
                //we'll create it once here instead of each call to the "publish_message" function
                mclient->reusable_message = pn_message();

                //if the handle was successfully created
                if (mclient->reusable_message != NULL)
                {
                    //success
                    return true;
                }
            }

            //free messenger context
            pn_messenger_free(mclient->messenger_context);
        }
    }

    //failure
    return false;
}

//function definition
//deinit the messaging client
void free_messaging_client(MESSAGING_CLIENT* mclient)
{
    //check input
    if (mclient != NULL)
    {
        //shuts down messaging infrastructure
        pn_messenger_stop(mclient->messenger_context);
        //deallocate messaging infrastructure resources
        pn_messenger_free(mclient->messenger_context);
        //deallocate message object
        pn_message_free(mclient->reusable_message);

        //deallocate event hub object
        free(mclient);
    }
}

//function definition
//publish a message to an amqp endpoint
bool publish_message(MESSAGING_CLIENT* mclient, const char* endpoint, const char* json_formatted_message)
{
    //local vars
    pn_data_t* message_body;    //message body handle
    pn_tracker_t tracker;       //message tracker
    pn_status_t status;         //message status

    //check inputs

    //clear the contents of the message (reuse existing so we don't need to recreate each time)
    pn_message_clear(mclient->reusable_message);

    //get a handle to the message body object
    message_body = pn_message_body(mclient->reusable_message);

    //set the destination address for the message
    pn_message_set_address(mclient->reusable_message, endpoint);

    //set the content type of the message data
    //application/json;type=entry;charset=utf-8
    //application/octet-stream
    pn_message_set_content_type(mclient->reusable_message, (char*)"application/json");

    //pn_message_is_inferred();
    //pn_message_set_inferred(message, true);

    //set the body content of the message
    pn_data_put_string(message_body, pn_bytes(strlen(json_formatted_message), json_formatted_message));

    //put a message in the outgoing queue
    pn_messenger_put(mclient->messenger_context, mclient->reusable_message);

    check(mclient->messenger_context);

    //get a tracker for the message we just put on the outgoing queue
    tracker = pn_messenger_outgoing_tracker(mclient->messenger_context);

    //get the current status of a message in the outgoing queue
    status = pn_messenger_status(mclient->messenger_context, tracker);
    printf("Status after put: %d\n", status);

    //pn_messenger_work(ehub->messenger_context, 0); //do not block
    pn_messenger_send(mclient->messenger_context, 1);

    check(mclient->messenger_context);

    //get the current status of a message in the outgoing queue
    status = pn_messenger_status(mclient->messenger_context, tracker);
    printf("Status after send: %d\n", status);

    return true;
}

static void die(const char* file, int line, const char* message)
{
    //check inputs

    fprintf(stderr, "%s:%i: %s\n", file, line, message);
}
