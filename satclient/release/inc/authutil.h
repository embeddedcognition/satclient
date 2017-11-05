/*
   Author: James Beasley
   Repo: https://github.com/embeddedcognition/satclient
*/

#ifndef AUTHUTIL_H_
#define AUTHUTIL_H_

#include "messagingclient.h"    //using for messaging client interface

//function declarations
char* create_service_bus_endpoint(const char*);
char* create_shared_access_token(const char*, const char*);
bool authenticate_claim(MESSAGING_CLIENT*, const char*);

#endif /* AUTHUTIL_H_ */
