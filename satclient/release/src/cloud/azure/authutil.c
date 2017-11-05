/*
   Author: James Beasley
   Repo: https://github.com/embeddedcognition/satclient
*/

#define _GNU_SOURCE             //enable GNU extensions in stdio.h so we can use "asprintf" function

#include <stdio.h>              //using for "asprintf" function
#include <stdlib.h>             //using for "free" function and "NULL"
#include <stdint.h>             //using for "uint8_t" and "uint32_t" types
#include <stdbool.h>            //using for "bool" type
#include <string.h>             //using for "strlen" function
#include <time.h>               //using for "time" function
#include <curl/curl.h>          //using for cURL web library
#include "cryptoutil.h"         //using for crypto functions
#include "authutil.h"

//global vars
static const char SERVICE_BUS_NAMESPACE[] = "YOUR_VALUE";                                       //azure service bus namespace
static const char CLAIMS_BASED_SECURITY_NODE_NAME[] = "$cbs";                                   //azure service bus claims-based security entity/node name
static const char SERVICE_BUS_ENDPOINT_TEMPLATE[] = "amqp://%s.servicebus.windows.net/%s";      //template for service bus entity endpoint
static const int SECONDS_IN_AN_HOUR = 1 * 60 * 60;                                              //compute the number of seconds in an hour, 1 hour X 60 minutes X 60 seconds
static const int SECONDS_IN_A_DAY = 1 * 24 * 60 * 60;                                           //compute the number of seconds in a day, 1 day X 24 hours X 60 minutes X 60 seconds
static const int SECONDS_IN_A_WEEK = 7 * 24 * 60 * 60;                                          //compute the number of seconds in a week, 7 days X 24 hours X 60 minutes X 60 seconds
static const int EXPECTED_HMAC_DIGEST_SIZE_BYTES = 32;                                          //expected size (in bytes) of the HMAC SHA-256 digest

//function declarations
static bool load_base64_encoded_secret_key(char**);

//function definition
//create fully formatted endpoint to an azure service bus entity/node (via amqp)
char* create_service_bus_endpoint(const char* entity_name)
{
    //local vars
    char* endpoint;                 //formatted endpoint
    int asprintf_return_value;      //return value of the asprintf function

    //check input
    if (entity_name != NULL)
    {
        //construct the endpoint
        asprintf_return_value = asprintf(&endpoint, SERVICE_BUS_ENDPOINT_TEMPLATE, SERVICE_BUS_NAMESPACE, entity_name);

        //if the endpoint was created successfully
        if ((asprintf_return_value > 0) && (endpoint != NULL))
        {
            //return the endpoint
            return endpoint;
        }

        //free endpoint in case a buffer was allocated by asprintf
        free(endpoint);
    }

    //failure
    return NULL;
}

//function definition
//create a shared access signature (token) from the shared access policy secret-key to use for authentication/authorization to azure
char* create_shared_access_token(const char* endpoint, const char* shared_access_policy_name)
{
    //local vars
    time_t cur_time_since_epoch;                            //seconds since unix epoch
    time_t token_expiry;                                    //time-to-live (TTL) of the token
    int asprintf_return_value;                              //return value of the asprintf function
    CURL* curl_context;                                     //curl handle
    char* message;                                          //a message (of the form: endpoint + CRLF + expiry_time) that we'll compute a message authentication code for using the primary secret-key associated with the shared access policy
    char* url_encoded_endpoint;                             //a url-encoded version of the endpoint
    uint8_t* hmac;                                          //keyed-hash message authentication code (HMAC) of the message (formatted as a 32-byte binary digest)
    uint32_t hmac_size;                                     //hmac size (in bytes)
    char* base64_encoded_hmac;                              //a base64-encoded version of the HMAC
    char* url_encoded_base64_encoded_hmac;                  //a url-encoded version of the base64-encoded HMAC
    char* base64_encoded_shared_access_policy_secret_key;   //shared access policy secret key (base64 encoded) - primary secret key for a shared access policy associated with the entity (e.g., event hub), a hash (signature) is computed from this secret key by both the sender and receiver, if the hashes match, the receiver grants the sender the rights to the entity that are specified in the particular shared access policy ("send" in this case)
    uint8_t* shared_access_policy_secret_key;               //shared access policy secret key (binary)
    uint32_t shared_access_policy_secret_key_size;          //shared access policy secret key size (in bytes)
    char* token = NULL;                                     //formatted shared access signature (token)

    //check inputs
    if ((endpoint != NULL) && (shared_access_policy_name != NULL))
    {
        //initialize curl library for url encoding
        curl_context = curl_easy_init();

        //if the curl library was initialized successfully
        if (curl_context != NULL)
        {
            //get the current time, in seconds since unix epoch (01/01/1970)
            time(&cur_time_since_epoch);
            //set the time to live on this token to be 1 hour from now
            token_expiry = cur_time_since_epoch + SECONDS_IN_AN_HOUR;

            //url encode the endpoint
            url_encoded_endpoint = curl_easy_escape(curl_context, endpoint, strlen(endpoint));

            //if the url encoded endpoint and token expiry were successfully created
            if ((url_encoded_endpoint != NULL) && (token_expiry > 0))
            {
                //load the locally stored and AES-256 encrypted secret key associated with the shared access policy
                //primary secret key for a shared access policy associated with the event hub, a hash (signature) is computed from this secret key by both the sender and receiver,
                //if the hashes match, the receiver grants the sender the rights to the event hub that are specified in the particular shared access policy ("send" in this case)
                //if the secret key was successfully loaded
                if (load_base64_encoded_secret_key(&base64_encoded_shared_access_policy_secret_key))
                {
                    printf("Base64 Encoded Key: %s\n", base64_encoded_shared_access_policy_secret_key);

                    //construct a message that we can generate a message authentication code (MAC) from so that the receiver (e.g., Azure) can test the sender's knowledge of the shared access policy secret-key
                    asprintf_return_value = asprintf(&message, "%s\n%ld", url_encoded_endpoint, token_expiry);

                    printf("Message: %s\n", message);

                    //if the key and message were successfully loaded/created
                    if ((base64_encoded_shared_access_policy_secret_key != NULL) && (asprintf_return_value > 0) && (message != NULL))
                    {
                        //if the base64 encoding was successfully stripped off to get the binary secret key
                        if (compute_base64_decode(base64_encoded_shared_access_policy_secret_key, &shared_access_policy_secret_key, &shared_access_policy_secret_key_size))
                        {
                            //immediately zero-out the memory that stored the base64 encoded secret key
                            memset(base64_encoded_shared_access_policy_secret_key, 0, strlen(base64_encoded_shared_access_policy_secret_key));

                            //if the secret key was base64 decoded to its raw binary form
                            if ((shared_access_policy_secret_key != NULL) && (shared_access_policy_secret_key_size > 0))
                            {
                                //if a keyed-hash (SHA-256) MAC was successfully computed for the message
                                if (compute_sha256_hmac(shared_access_policy_secret_key, shared_access_policy_secret_key_size, message, &hmac, &hmac_size))
                                {
                                    //immediately zero-out the memory that stored the binary secret key
                                    memset(shared_access_policy_secret_key, 0, shared_access_policy_secret_key_size);

                                    //if the hmac was created successfully
                                    if ((hmac != NULL) && (hmac_size == EXPECTED_HMAC_DIGEST_SIZE_BYTES))
                                    {
                                        //if the computed HMAC was successfully base64 encoded
                                        if (compute_base64_encode(hmac, hmac_size, &base64_encoded_hmac))
                                        {
                                            //if the base64 encoded hmac was created successfully
                                            if (base64_encoded_hmac != NULL)
                                            {
                                                printf("Base64 Encoded HMAC: %s\n", base64_encoded_hmac);

                                                //url encode the base64-encoded HMAC
                                                url_encoded_base64_encoded_hmac = curl_easy_escape(curl_context, base64_encoded_hmac, strlen(base64_encoded_hmac));

                                                //if the url encoded base64 encoded hmac successfully created
                                                if (url_encoded_base64_encoded_hmac != NULL)
                                                {
                                                    //construct the token
                                                    /*
                                                     * signature (sig) = base64-encoded keyed-hash message authentication code (HMAC) generated (by the sender) from the "endpoint + CRLF + expiry_time" message
                                                     * expiry (se) = identifies when the token's time to live
                                                     * key name (skn) = identifies the shared access policy (and relating secret-key) that the receiver (e.g., Azure) should use to test the authenticity of the sender (i.e., compare HMAC's)
                                                     * resource (sr) = the endpoint
                                                     */
                                                    asprintf_return_value = asprintf(&token, "SharedAccessSignature sig=%s&se=%ld&skn=%s&sr=%s", url_encoded_base64_encoded_hmac, token_expiry, shared_access_policy_name, url_encoded_endpoint);

                                                    //if token creation failed
                                                    if (asprintf_return_value < 0)
                                                    {
                                                        //just in case a buffer was allocated by asprintf
                                                        free(token);
                                                        token = NULL;
                                                    }

                                                    //free buffer
                                                    curl_free(url_encoded_base64_encoded_hmac);
                                                }

                                                //free buffer
                                                free(base64_encoded_hmac);
                                            }
                                        }

                                        //free buffer
                                        free(hmac);
                                    }
                                }

                                //free buffer
                                free(shared_access_policy_secret_key);
                            }
                        }
                        else
                        {
                            fprintf(stderr, "ERROR: FAILED TO BASE64 DECODE SHARED ACCESS POLICY SECRET KEY!\n");
                        }
                    }

                    //free buffer
                    free(base64_encoded_shared_access_policy_secret_key);
                    free(message);
                }
            }

            //free buffer
            curl_free(url_encoded_endpoint);
            //deinit curl library resources
            curl_easy_cleanup(curl_context);
        }
    }

    //return fully formatted shared access token (or NULL if failure)
    return token;
}

//function definition
//load/decrypt the base64 encoded secret key from the local file
static bool load_base64_encoded_secret_key(char** base64_encoded_secret_key)
{
    //local vars
    bool operation_status = false;          //denotes success or failure of the operation
    char* base64_encoded_openssl_payload;   //handle to base64 encoded openssl payload (i.e., salt header + ciphertext)
    uint8_t* plaintext = NULL;              //handle to decrypted data from file
    uint32_t plaintext_size;                //size (in bytes) of decrypted data from file

    //check input
    if (base64_encoded_secret_key != NULL)
    {
        //if the base64 encoded openssl payload (salt header + ciphertext) was successfully loaded
        if (load_base64_encoded_openssl_payload(&base64_encoded_openssl_payload))
        {
            //double check if the base64 encoded openssl payload was successfully assigned
            if (base64_encoded_openssl_payload != NULL)
            {
                //if the base64 encoded openssl payload was successfully decrypted
                if (decrypt_base64_encoded_openssl_payload(base64_encoded_openssl_payload, &plaintext, &plaintext_size))
                {
                    //double check if the plaintext was successfully assigned
                    if ((plaintext != NULL) && (plaintext_size > 0))
                    {
                        //convert the plaintext byte array into a string (since its a base64 encoded secret key)
                        *base64_encoded_secret_key = compute_text_string(plaintext, plaintext_size);

                        //if the buffer was created successfully
                        if (*base64_encoded_secret_key != NULL)
                        {
                            //if the string length matches the size of the byte array is was constructed from
                            if (strlen(*base64_encoded_secret_key) == plaintext_size)
                            {
                                //success
                                operation_status = true;
                            }
                        }

                        //free buffer
                        free(plaintext);
                    }
                }
            }
        }
    }

    return operation_status;
}

//construct an amqp message containing the shared access token and send to the "$cbs" (claim-based security) sevice bus node so that the token can be validated by Azure, once this successfully occurs, telemetry can be sent to the event hub

//specify "ReplyTo" field in the message for the $cbs node to send the result back

//function definition
/*
 * The receiver (e.g., $cbs) extracts the shared access token from the message body and recomputes the base64-encoded HMAC based on the other attributes in the shared access token (sr=event_hub_endpoint and se=expiry),
 * if the the base64-encoded HMAC's match, then the receiver grants the access specified in the shared access policy.
*/
bool authenticate_claim(MESSAGING_CLIENT* mclient, const char* shared_access_token)
{
    //local vars
    char* claims_based_security_endpoint;

    //check inputs
    if ((mclient != NULL) && (shared_access_token != NULL))
    {
        //create a formatted endpoint to the azure service bus claims-based security entity
        claims_based_security_endpoint = create_service_bus_endpoint(CLAIMS_BASED_SECURITY_NODE_NAME);

        //if the endpoint was successfully created
        if (claims_based_security_endpoint != NULL)
        {
            //success
            return true;
        }
    }

    //failure
    return false;
}
