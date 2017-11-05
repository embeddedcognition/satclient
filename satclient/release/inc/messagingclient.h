/*
   Author: James Beasley
   Repo: https://github.com/embeddedcognition/satclient
*/

#ifndef MESSAGINGCLIENT_H_
#define MESSAGINGCLIENT_H_

#include <stdbool.h>            //using for "bool" type
#include <proton/messenger.h>   //using for qpid-proton amqp client library

//messaging client object representation
typedef struct messaging_client
{
    pn_messenger_t* messenger_context;      //in and out message queue handler
    pn_message_t* reusable_message;         //message handle (created once and reused throughout)
}MESSAGING_CLIENT;

//function declarations
MESSAGING_CLIENT* new_messaging_client(void);
void free_messaging_client(MESSAGING_CLIENT*);
bool publish_message(MESSAGING_CLIENT*, const char*, const char*);

#endif /* MESSAGINGCLIENT_H_ */
