/*
   Author: James Beasley
   Repo: https://github.com/embeddedcognition/satclient
*/

#define _GNU_SOURCE             //enable GNU extensions in stdio.h so we can use "getline" function

#include <stdio.h>              //using for "sprintf" function
#include <stdlib.h>             //using for "malloc" function, "size_t", and "ssize_t" types
#include <string.h>             //using for "strlen", "memcpy", and "memset" functions
#include <openssl/hmac.h>       //using for "HMAC" function
#include <openssl/bio.h>        //using for "BIO*" functions
#include <openssl/evp.h>        //using for "EVP_sha256" and "EVP_BytesToKey" functions
#include <openssl/buffer.h>     //using for "BUF_MEM" structure
#include "cryptoutil.h"

//global vars
static const int GET_LINE_FAILED = -1;                              //failure code for "getline" function
static const int OPENSSL_SALT_SIGNATURE_AND_VALUE_SIZE_BYTES = 16;  //size (in bytes) of the openssl salt signature ("Salted__") and value
static const int OPENSSL_SALT_VALUE_SIZE_BYTES = 8;                 //size (in bytes) of the AES-256 salt value
static const int OPENSSL_EVP_CIPHER_SUCCESS = 1;                    //openssl evp cipher function was a success
static const int OPENSSL_EVP_BYTESTOKEY_ITERATION_COUNT = 1;        //algorithm iteration count, "openssl enc" does not currently support an option for iteration count but defaults to 1, therefore we must use only one iteration when deriving the key and iv
static const int EXPECTED_HMAC_SECRET_KEY_SIZE_BYTES = 32;          //expected size (in bytes) of the HMAC SHA-256 secret key
static const int NUL_TERMINATED_STRING = -1;                        //indicates the supplied buffer to BIO_new_mem_buf is nul '\0' terminated
static const int BASE64_BYTE_BLOCK_SIZE = 3;                        //size in bytes of a base64 block (24 bits divided by 8 bits per byte)
static const int BASE64_CHAR_BLOCK_SIZE = 4;                        //size in characters of a base64 block (24 bits divided by 6 bits per character)

//function declarations
static int compute_base64_decode_byte_array_size(const char*);

//function definition
//load the base64 encoded openssl payload (salt header + ciphertext) from the local file
//the file contents are encrypted and base64 encoded beforehand using: openssl enc -e -aes-256-cfb -salt -md sha256 -pass pass:"YOUR_PASSPHRASE_HERE" -base64 -A -in secret_key.plaintext -out secret_key.base64_encoded_ciphertext
bool load_base64_encoded_openssl_payload(char** base64_encoded_openssl_payload)
{
    //local vars
    bool operation_status = false;                  //denotes success or failure of the operation
    FILE* file_handle;                              //file containing encrypted secret key
    int file_content_size;                          //size (in bytes) of the file content
    char* file_location_input = NULL;               //user input denoting location of file containing AES-256 encrypted secret key
    size_t file_location_input_buffer_size = 0;     //size of buffer allocated by getline()
    ssize_t file_location_input_size;               //size of location input

    //check input
    if (base64_encoded_openssl_payload != NULL)
    {
        //read user input
        //enter absolute path to key file (should be of the form "/.../secret_key.base64_encoded_ciphertext")
        printf("Enter the location of the base64 encoded openssl payload file: ");
        file_location_input_size = getline(&file_location_input, &file_location_input_buffer_size, stdin);
        if (file_location_input_size == GET_LINE_FAILED)
        {
            fprintf(stderr, "ERROR: FAILED TO OBTAIN LOCATION OF BASE64 ENCODED OPENSSL PAYLOAD FILE CONTAINING ENCRYPTED DATA!\n");
        }

        //if lines were read in properly - input lengths should be greater than 1 (e.g., since the newline character is present in the line read we need to ensure they didn't just hit enter)
        if ((file_location_input != NULL) && (file_location_input_size > 1))
        {
            //zero out the new line at the end of the file location string (place a nul character to terminate the string there)
            file_location_input[file_location_input_size - 1] = '\0';

            //open file
            file_handle = fopen(file_location_input, "r");

            //if the file stream was opened successfully
            if (file_handle != NULL)
            {
                //determine the size (in bytes) of the file contents
                //advance the file pointer to the end of the file
                fseek(file_handle, 0, SEEK_END);
                //determine the size (in bytes) of the file contents
                file_content_size = ftell(file_handle);
                //move file pointer back to beginning of file
                rewind(file_handle);

                //allocate a byte buffer to store the entire file contents + 1 cell at the end for the nul character
                *base64_encoded_openssl_payload = malloc((file_content_size + 1) * (sizeof (char)));

                //if the byte buffer was created successfully
                if (*base64_encoded_openssl_payload != NULL)
                {
                    //if the entire file contents were read into the allocated byte buffer
                    if (fread(*base64_encoded_openssl_payload, sizeof (char), file_content_size, file_handle) == file_content_size)
                    {
                        //add the nul character in the last array cell to properly terminate the string
                        (*base64_encoded_openssl_payload)[file_content_size] = '\0';

                        //ensure a line feed '\n' character isn't present at the end of the base64 string (could've been appended when the payload was originally saved), if so zero it out
                        if ((*base64_encoded_openssl_payload)[file_content_size - 1] == '\n')
                        {
                            //zero out line feed character
                            (*base64_encoded_openssl_payload)[file_content_size - 1] = '\0';
                        }

                        //success
                        operation_status = true;
                    }
                    else
                    {
                        fprintf(stderr, "ERROR: FAILED TO LOAD BASE64 ENCODED OPENSSL PAYLOAD FILE CONTAINING ENCRYPTED DATA!\n");
                    }
                }

                //close file handle
                fclose(file_handle);
            }
        }
        else
        {
            fprintf(stderr, "ERROR: FAILED TO OBTAIN LOCATION OF BASE64 ENCODED OPENSSL PAYLOAD FILE FROM USER!\n");
        }

        //free user input (even if failure occurred getline may have allocated buffers)
        free(file_location_input);
    }

    return operation_status;
}

//function definition
//decrypt the supplied base64 encoded openssl payload (salt header + ciphertext)
bool decrypt_base64_encoded_openssl_payload(const char* base64_encoded_openssl_payload, uint8_t** output_plaintext, uint32_t* output_plaintext_size)
{
    //local vars
    bool operation_status = false;                  //denotes success or failure of the operation
    char* passphrase_input = NULL;                  //user input denoting passphrase used to encrypt the secret key in the file
    size_t passphrase_input_buffer_size = 0;        //size of buffer allocated by getline()
    ssize_t passphrase_input_size;                  //size of passphrase input
    uint8_t* openssl_payload;                       //handle to openssl payload (i.e., salt header + ciphertext) loaded from file
    uint32_t openssl_payload_size;                  //size (in bytes) of openssl_payload
    uint8_t* openssl_payload_salt;                  //handle to salt extracted from openssl_payload, 8-byte (64-bit) salt (random data) that is combined with the passphrase (to discourage a dictionary attack) to generate the key and iv
    uint32_t openssl_payload_salt_size;             //size (in bytes) of openssl_payload_salt
    uint8_t* openssl_payload_ciphertext;            //handle to ciphertext extracted from openssl_payload
    uint32_t openssl_payload_ciphertext_size;       //size (in bytes) of openssl_payload_ciphertext
    uint8_t* cipher_key = NULL;                     //handle to cipher key (used to decrypt payload)
    uint32_t cipher_key_size;                       //size (in bytes) of cipher key
    uint8_t* cipher_iv = NULL;                      //handle to cipher iv (used to decrypt payload)
    uint32_t cipher_iv_size;                        //size (in bytes) of cipher iv

    //check inputs
    if ((base64_encoded_openssl_payload != NULL) && (output_plaintext != NULL) && (output_plaintext_size != NULL))
    {
        //read user input
        //enter decryption passphrase
        printf("Enter the passphrase used to encrypt the openssl payload the file: ");
        passphrase_input_size = getline(&passphrase_input, &passphrase_input_buffer_size, stdin);
        if (passphrase_input_size == GET_LINE_FAILED)
        {
            fprintf(stderr, "ERROR: FAILED TO OBTAIN PASSPHRASE USED TO ENCRYPT OPENSSL PAYLOAD!\n");
        }

        //if lines were read in properly - input lengths should be greater than 1 (e.g., since the newline character is present in the line read we need to ensure they didn't just hit enter)
        if ((passphrase_input != NULL) && (passphrase_input_size > 1))
        {
            //zero out the new line at the end of the passphrase string (place a nul character to terminate the string there)
            passphrase_input[passphrase_input_size - 1] = '\0';

            //if base64 decoding was successful
            //the openssl payload (salt header + ciphertext) is base64 encoded for easy handling, so we must first strip off the encoding to get the raw (binary) payload
            if (compute_base64_decode(base64_encoded_openssl_payload, &openssl_payload, &openssl_payload_size))
            {
                //double check if the openssl payload was successfully assigned
                if ((openssl_payload != NULL) && (openssl_payload_size > 0))
                {
                    //if the salt and ciphertext were successfully extracted from the openssl_payload
                    if (extract_salt_and_ciphertext_from_openssl_payload(openssl_payload, openssl_payload_size, &openssl_payload_salt, &openssl_payload_salt_size, &openssl_payload_ciphertext, &openssl_payload_ciphertext_size))
                    {
                        //double check if the salt and ciphertext were successfully assigned
                        if ((openssl_payload_salt != NULL) && (openssl_payload_salt_size > 0) && (openssl_payload_ciphertext != NULL) && (openssl_payload_ciphertext_size > 0))
                        {
                            //derive the cipher key and iv from the supplied passphrase and salt
                            //passphrase_input_length is factoring in '\n' in the last cell, so we subtract that out
                            if (derive_aes256cfb_cipher_key_and_iv_from_passphrase_and_salt((uint8_t*)passphrase_input, (passphrase_input_size - 1), openssl_payload_salt, openssl_payload_salt_size, &cipher_key, &cipher_key_size, &cipher_iv, &cipher_iv_size))
                            {
                                //double check if the key and iv were successfully assigned
                                if ((cipher_key != NULL) && (cipher_key_size > 0) && (cipher_iv != NULL) && (cipher_iv_size > 0))
                                {
                                    //if decryption was successful
                                    if (compute_aes256cfb_cipher(DECRYPT, cipher_key, cipher_key_size, cipher_iv, cipher_iv_size, openssl_payload_ciphertext, openssl_payload_ciphertext_size, output_plaintext, output_plaintext_size))
                                    {
                                        //double check if the plaintext was successfully assigned
                                        if ((*output_plaintext != NULL) && (*output_plaintext_size > 0))
                                        {
                                            //ensure a line feed '\n' character (hex = "0x0a") isn't present at the end of the byte string (could've been appended when the plaintext was originally saved), if so zero it out
                                            if ((*output_plaintext)[*output_plaintext_size - 1] == 0x0a)
                                            {
                                                //zero out line feed character and decrement the size of the buffer so it's not used
                                                (*output_plaintext)[*output_plaintext_size - 1] = 0;
                                                (*output_plaintext_size)--;
                                            }

                                            //success
                                            operation_status = true;
                                        }
                                    }
                                    else
                                    {
                                        fprintf(stderr, "ERROR: FAILED TO DECRYPT OPENSSL PAYLOAD!\n");
                                    }

                                    //free buffers
                                    free(cipher_key);
                                    free(cipher_iv);
                                }
                            }
                            else
                            {
                                fprintf(stderr, "ERROR: FAILED TO DERIVE CIPHER KEY & IV FROM ENCRYPTION PASSPHRASE & SALT!\n");
                            }

                            //free buffers
                            free(openssl_payload_salt);
                            free(openssl_payload_ciphertext);
                        }
                    }

                    //free buffers
                    free(openssl_payload);
                }
            }
            else
            {
                fprintf(stderr, "ERROR: FAILED TO DECODE BASE64 STRING!\n");
            }
        }
        else
        {
            fprintf(stderr, "ERROR: FAILED TO OBTAIN ENCRYPTION PASSPHRASE FROM USER!\n");
        }

        //free user input (even if failure occurred getline may have allocated buffers)
        free(passphrase_input);
    }

    return operation_status;
}

//function definition
//extracts the salt and ciphertext from the supplied openssl payload
bool extract_salt_and_ciphertext_from_openssl_payload(const uint8_t* openssl_payload, const uint32_t openssl_payload_size, uint8_t** output_openssl_payload_salt, uint32_t* output_openssl_payload_salt_size, uint8_t** output_openssl_payload_ciphertext, uint32_t* output_openssl_payload_ciphertext_size)
{
    //local vars
    bool operation_status = false;      //denotes success or failure of the operation

    //check inputs
    if ((openssl_payload != NULL) && (openssl_payload_size > OPENSSL_SALT_SIGNATURE_AND_VALUE_SIZE_BYTES) && (output_openssl_payload_salt != NULL) && (output_openssl_payload_salt_size != NULL) && (output_openssl_payload_ciphertext != NULL) && (output_openssl_payload_ciphertext_size != NULL))
    {
        ///allocate a buffers to hold extracted salt and ciphertext
        *output_openssl_payload_salt = malloc(OPENSSL_SALT_VALUE_SIZE_BYTES * (sizeof (uint8_t)));
        *output_openssl_payload_ciphertext = malloc((openssl_payload_size - OPENSSL_SALT_SIGNATURE_AND_VALUE_SIZE_BYTES) * (sizeof (uint8_t)));

        //if the buffers were successfully created
        if ((*output_openssl_payload_salt != NULL) && (*output_openssl_payload_ciphertext != NULL))
        {
            //the salt value is embedded in the openssl_payload at bytes 8 - 15 (zero indexed)
            //we need to extract it so we can use it in combination with the supplied passphrase to derived the key and iv used in the encryption process
            memcpy(*output_openssl_payload_salt, &(openssl_payload[OPENSSL_SALT_SIGNATURE_AND_VALUE_SIZE_BYTES - OPENSSL_SALT_VALUE_SIZE_BYTES]), OPENSSL_SALT_VALUE_SIZE_BYTES);

            //the ciphertext is embedded in the openssl_payload at bytes 16 to (openssl_payload_size - 1) - zero indexed
            //we need to extract so it can be decrypted and the secret key used to communicate with cloud services
            memcpy(*output_openssl_payload_ciphertext, &(openssl_payload[OPENSSL_SALT_SIGNATURE_AND_VALUE_SIZE_BYTES]), (openssl_payload_size - OPENSSL_SALT_SIGNATURE_AND_VALUE_SIZE_BYTES));

            //set lengths
            *output_openssl_payload_salt_size = OPENSSL_SALT_VALUE_SIZE_BYTES;
            *output_openssl_payload_ciphertext_size = openssl_payload_size - OPENSSL_SALT_SIGNATURE_AND_VALUE_SIZE_BYTES;

            //success
            operation_status = true;
        }

        //if failure
        if (!operation_status)
        {
            //free buffers
            free(*output_openssl_payload_salt);
            free(*output_openssl_payload_ciphertext);
            //zero size values
            *output_openssl_payload_salt_size = 0;
            *output_openssl_payload_ciphertext_size = 0;
        }
    }

    return operation_status;
}

//function definition
//derive the secret key and iv used for encryption from the supplied passphrase and salt
bool derive_aes256cfb_cipher_key_and_iv_from_passphrase_and_salt(const uint8_t* passphrase, const uint32_t passphrase_size, const uint8_t* salt, const uint32_t salt_size, uint8_t** output_cipher_key, uint32_t* output_cipher_key_size, uint8_t** output_cipher_iv, uint32_t* output_cipher_iv_size)
{
    //local vars
    bool operation_status = false;    //denotes success or failure of the operation

    //check inputs
    if ((passphrase != NULL) && (passphrase_size > 0) && (salt != NULL) && (salt_size == OPENSSL_SALT_VALUE_SIZE_BYTES) && (output_cipher_key != NULL) && (output_cipher_key_size != NULL) && (output_cipher_iv != NULL) && (output_cipher_iv_size != NULL))
    {
        ///allocate a buffer to hold the key and iv
        *output_cipher_key = malloc(EVP_CIPHER_key_length(EVP_aes_256_cfb()) * (sizeof (uint8_t))); //should be 32 bytes
        *output_cipher_iv = malloc(EVP_CIPHER_iv_length(EVP_aes_256_cfb()) * (sizeof (uint8_t))); //should be 16 bytes

        //if the buffers were successfully created
        if ((*output_cipher_key != NULL) && (*output_cipher_iv != NULL))
        {
            //derive the key and iv based on the supplied passphrase and salt
            *output_cipher_key_size = EVP_BytesToKey(EVP_aes_256_cfb(), EVP_sha256(), salt, passphrase, passphrase_size, OPENSSL_EVP_BYTESTOKEY_ITERATION_COUNT, *output_cipher_key, *output_cipher_iv);

            //if a valid secret key was outputted (size-wise)
            if (*output_cipher_key_size == EVP_CIPHER_key_length(EVP_aes_256_cfb()))
            {
                //if key was successfully created then it's likely the iv was too, so set its length
                *output_cipher_iv_size = EVP_CIPHER_iv_length(EVP_aes_256_cfb());
                //success
                operation_status = true;
            }
        }

        //if failure
        if (!operation_status)
        {
            //free buffers
            free(*output_cipher_key);
            free(*output_cipher_iv);
            //zero size values
            *output_cipher_key_size = 0;
            *output_cipher_iv_size = 0;
        }
    }

    return operation_status;
}

//function definition
//performs AES-256-CFB encryption/decryption on the supplied byte array (could be ciphertext (for decryption mode) or plaintext (for encryption mode)
bool compute_aes256cfb_cipher(CIPHER_MODE cmode, const uint8_t* key, const uint32_t key_size, const uint8_t* iv, const uint32_t iv_size, const uint8_t* input_byte_array, const uint32_t input_byte_array_size, uint8_t** output_byte_array, uint32_t* output_byte_array_size)
{
    //local vars
    bool operation_status = false;      //denotes success or failure of the operation
    EVP_CIPHER_CTX* cipher_context;     //cipher context handle
    int bytes_output_size;              //# of bytes outputted by the cipher operation (encryption or decryption)

    //check inputs
    if ((key != NULL) && (key_size > 0) && (iv != NULL) && (iv_size > 0) && (input_byte_array != NULL) && (input_byte_array_size > 0) && (output_byte_array != NULL) && (output_byte_array_size != NULL))
    {
        //ensure that the appropriate key/iv length has been supplied to this cipher
        if ((EVP_CIPHER_key_length(EVP_aes_256_cfb()) == key_size) && (EVP_CIPHER_iv_length(EVP_aes_256_cfb()) == iv_size))
        {
            //create cipher context
            cipher_context = EVP_CIPHER_CTX_new();

            //if the context was successfully created
            if (cipher_context != NULL)
            {
                //allocate a buffer to hold the output bytes
                *output_byte_array = malloc(input_byte_array_size * (sizeof (uint8_t)));

                //if the buffer was successfully created
                if (*output_byte_array != NULL)
                {
                    //using CFB to ensure we produce a predictable size of output (for easier handling), in CFB mode the size of plaintext and ciphertext are equal (CFB acts as a stream cipher encrypting byte per byte vs. block per block where padding may be necessary)
                    //if cipher context initialization was successful
                    if (EVP_CipherInit_ex(cipher_context, EVP_aes_256_cfb(), NULL, key, iv, cmode) == OPENSSL_EVP_CIPHER_SUCCESS)
                    {
                        //if performing the cipher operation (encryption or decryption) was successful
                        if (EVP_CipherUpdate(cipher_context, *output_byte_array, &bytes_output_size, input_byte_array, input_byte_array_size) == OPENSSL_EVP_CIPHER_SUCCESS)
                        {
                            //store the number of bytes outputted
                            *output_byte_array_size = bytes_output_size;

                            //NOTE: a call to EVP_CipherFinal_ex is unnecessary given we're using cfb mode (no padding to worry about, acts as stream cipher operating on bytes not blocks)
                        }
                        else
                        {
                            fprintf(stderr, "ERROR: FAILED TO PERFORM CIPHER OPERATION!\n");
                        }
                    }
                    else
                    {
                        fprintf(stderr, "ERROR: FAILED TO INITIALIZE CIPHER CONTEXT!\n");
                    }

                    //if the size of the output equals the size of our input, declare success
                    if (*output_byte_array_size == input_byte_array_size)
                    {
                        //success
                        operation_status = true;
                    }
                    else //we have a problem
                    {
                        fprintf(stderr, "ERROR: CIPHER OPERATION FAILED, INPUT/OUTPUT DATA BUFFER SIZES DON'T MATCH!\n");
                        //free buffer
                        free(*output_byte_array);
                        *output_byte_array_size = 0;
                    }
                }

                //free context
                EVP_CIPHER_CTX_free(cipher_context);
            }
        }
        else
        {
            fprintf(stderr, "ERROR: LENGTH OF KEY AND/OR IV SUPPLIED IS UNSUPPORTED BY AES-256-CFB ALGORITHM!\n");
        }
    }

    return operation_status;
}

//function definition
//computes a keyed-hash (SHA-256) message authentication code (HMAC) for a particular message (formatted as a 32-byte binary digest)
bool compute_sha256_hmac(const uint8_t* secret_key, const uint32_t secret_key_size, const char* message, uint8_t** output_hmac, uint32_t* output_hmac_size)
{
    //local vars
    bool operation_status = false;      //denotes success or failure of the operation
    HMAC_CTX hmac_context;              //hmac context handle

    //check inputs (key size should be 32-byte / 256-bit since we're using SHA-256)
    if ((secret_key != NULL) && (secret_key_size == EXPECTED_HMAC_SECRET_KEY_SIZE_BYTES) && (message != NULL) && (output_hmac != NULL) && (output_hmac_size != NULL))
    {
        //create a buffer to hold the outputted 32-byte hmac (digest size should be 32-byte since we're using a 256-bit secret key)
        *output_hmac = malloc(EVP_MD_size(EVP_sha256()) * (sizeof (uint8_t)));

        //if the buffer was successfully created
        if (*output_hmac != NULL)
        {
            //init hmac context before first use
            HMAC_CTX_init(&hmac_context);

            //initialize the context to use the supplied secret key and digest
            //if successful
            if (HMAC_Init_ex(&hmac_context, secret_key, secret_key_size, EVP_sha256(), NULL))
            {
                //generate the hmac for the supplied message, based on previously supplied secret key and digest
                if (HMAC_Update(&hmac_context, (const uint8_t*)message, strlen(message)))
                {
                    //move the generated hmac into the buffer created
                    if (HMAC_Final(&hmac_context, *output_hmac, output_hmac_size))
                    {
                        //if a valid hmac was outputted (size-wise)
                        if (*output_hmac_size == EVP_MD_size(EVP_sha256()))
                        {
                            //success
                            operation_status = true;
                        }
                    }
                }
            }

            //clear context
            HMAC_CTX_cleanup(&hmac_context);
        }

        //if failure
        if (!operation_status)
        {
            //free buffer
            free(*output_hmac);
            //set to NULL & zero size buffer
            *output_hmac = NULL;
            *output_hmac_size = 0;
        }
    }

    return operation_status;
}

//function definition
//computes a keyed-hash (SHA-256) message authentication code (HMAC) for a particular message (formatted as a 32-byte binary digest)
bool compute_sha256_hmac_2(const uint8_t* secret_key, const uint32_t secret_key_size, const char* message, uint8_t** output_hmac, uint32_t* output_hmac_size)
{
    //local vars
    bool operation_status = false;      //denotes success or failure of the operation

    //check inputs (key size should be 32-byte / 256-bit since we're using SHA-256)
    if ((secret_key != NULL) && (secret_key_size == EXPECTED_HMAC_SECRET_KEY_SIZE_BYTES) && (message != NULL) && (output_hmac != NULL) && (output_hmac_size != NULL))
    {
        //create a buffer to hold the outputted 32-byte hmac (digest size should be 32-byte since we're using a 256-bit secret key)
        *output_hmac = malloc(EVP_MD_size(EVP_sha256()) * (sizeof (uint8_t)));

        //if the buffer was successfully created
        if (*output_hmac != NULL)
        {
            //compute the 32-byte hmac and store it in the output_hmac buffer
            //sha-256 produces a 32-byte (256-bit) keyed-hash message authentication code (hmac) - binary digest
            //if the hmac was successfully generated
            if (HMAC(EVP_sha256(), secret_key, secret_key_size, (const uint8_t*)message, strlen(message), *output_hmac, output_hmac_size) != NULL)
            {
                //if a valid hmac was outputted (size-wise)
                if (*output_hmac_size == EVP_MD_size(EVP_sha256()))
                {
                    //success
                    operation_status = true;
                }
            }
        }

        //if failure
        if (!operation_status)
        {
            //free buffer
            free(*output_hmac);
            //set to NULL & zero size buffer
            *output_hmac = NULL;
            *output_hmac_size = 0;
        }
    }

    return operation_status;
}

//function definition
//returns a hex (base16) formatted string from the supplied byte array
char* compute_base16_string(const uint8_t* byte_array, const uint32_t byte_array_size)
{
    //local vars
    int index;          //index for the loop
    char* hex_string;   //the character hexadecimal string, each of the byte array cells contain two 4-bit hexadecimal characters, totaling (byte_array_size * 2) in all

    //check inputs
    if ((byte_array != NULL) && (byte_array_size > 0))
    {
        //create a 2n-character buffer to hold the hexadecimal string value of the n-byte data (adding 1 cell at the end for the nul character)
        hex_string = malloc(((byte_array_size * 2) + 1) * (sizeof (char)));

        //if the buffer was successfully created
        if (hex_string != NULL)
        {
            //iterate through the n-byte array, for each byte, print the two 4-bit hex characters to the hex array
            for (index = 0; index < byte_array_size; index++)
            {
                /*
                    For each byte in the array, two 4-bit hex characters are written to the hex array, the starting pointer (i.e., "&(hex_string[index * 2])")
                    for sprintf advances two cells ahead (in the hex string) for each iteration of the loop so that the next two hex characters can be written
                    to the hex string. "%02x" formatting is used to ensure (at least) two hex characters are always written (since these are bytes
                    there won't be more than two), x = print integer (supplied byte that we cast to "unsigned int", as the "x" format option is expecting it)
                    in lower-case hexadecimal format, 2 = require field width of at least 2 digits, 0 = even if the most significant 4 bits are zero, display
                    them anyway.
                */
                sprintf(&(hex_string[index * 2]), "%02x", (unsigned int)(byte_array[index]));
            }

            //add the nul character in the last array cell to properly terminate the string
            hex_string[byte_array_size * 2] = '\0';

            //return the n-byte array as a 2n-character hexadecimal string
            return hex_string;
        }
    }

    //failure
    return NULL;
}

//function definition
//returns a text string version of the supplied byte array
char* compute_text_string(const uint8_t* byte_array, const uint32_t byte_array_size)
{
    //local vars
    int index;              //index for the loop
    char* text_string;      //text string representation of the byte array

    //check inputs
    if ((byte_array != NULL) && (byte_array_size > 0))
    {
        //create a character buffer to hold the string representation of the n-byte data (adding 1 cell at the end for the nul character)
        text_string = malloc(((byte_array_size) + 1) * (sizeof (char)));

        //if the buffer was successfully created
        if (text_string != NULL)
        {
            //iterate through the n-byte array, for each byte, print a character to the string
            for (index = 0; index < byte_array_size; index++)
            {
                text_string[index] = (char)(byte_array[index]);
            }

            //add the nul character in the last array cell to properly terminate the string
            text_string[byte_array_size] = '\0';

            //return the n-byte array as a string
            return text_string;
        }
    }

    //failure
    return NULL;
}

//function definition
//base64 encodes the supplied byte data - binary data contains printable and non-printable characters, so it's common to convert them to base64 to make them easier to transport and print
bool compute_base64_encode(const uint8_t* byte_array, const uint32_t byte_array_size, char** output_base64_encoded_string)
{
    //local vars
    BIO* memory_sink;                   //sink containing encoded data
    BIO* base64_encoding_filter;        //filter that base64 encodes data written through it
    BUF_MEM* memory_buffer_handle;      //handle to underlying memory structure of the sink BIO
    int write_return_value;             //bio_write function return value
    int flush_return_value;             //bio_flush function return value
    bool operation_status = false;      //denotes success or failure of the operation

    //check inputs
    if ((byte_array != NULL) && (byte_array_size > 0) && (output_base64_encoded_string != NULL))
    {
        //create an i/o filter that base64 encodes data written through it
        base64_encoding_filter = BIO_new(BIO_f_base64());
        //create a memory sink for encoded data to land in
        memory_sink = BIO_new(BIO_s_mem());
        //encode the data all on one line
        BIO_set_flags(base64_encoding_filter, BIO_FLAGS_BASE64_NO_NL);

        //if the filter and sink were created successfully
        if ((base64_encoding_filter != NULL) && (memory_sink != NULL))
        {
            //link (append) the sink to the filter (e.g., create of a BIO chain)
            //essentially a linked-list pipeline where data is sent through the filter BIO and then directed to the memory sink BIO for storage
            BIO_push(base64_encoding_filter, memory_sink);

            //base64 encode the supplied binary data
            write_return_value = BIO_write(base64_encoding_filter, byte_array, byte_array_size);

            //signal that no more data is to be encoded (flush the last block into the linked memory sink)
            flush_return_value = BIO_flush(base64_encoding_filter);

            //if the write/flush was successful
            if ((write_return_value > 0) && (flush_return_value > 0))
            {
                //get a handle to the underlying memory structure of the sink BIO
                BIO_get_mem_ptr(memory_sink, &memory_buffer_handle);

                //create a buffer to hold the base64 encoded string (adding 1 cell at the end for the nil character)
                *output_base64_encoded_string = malloc(((memory_buffer_handle->length) + 1) * (sizeof (char)));

                //if the buffer was successfully created
                if (*output_base64_encoded_string != NULL)
                {
                    //copy the encoded data from the sink to a durable memory location under our control
                    memcpy(*output_base64_encoded_string, memory_buffer_handle->data, memory_buffer_handle->length);

                    //add the nul character to the end of the encoded string to properly terminate it
                    (*output_base64_encoded_string)[memory_buffer_handle->length] = '\0';

                    //success
                    operation_status = true;
                }
            }
        }

        //free all BIO's
        BIO_free(base64_encoding_filter);
        BIO_free(memory_sink);
    }

    return operation_status;
}

//function definition
//decodes the supplied base64 encoded text data to its binary version
bool compute_base64_decode(const char* base64_encoded_string, uint8_t** output_byte_array, uint32_t* output_byte_array_size)
{
    //local vars
    BIO* memory_source;                     //source containing data to encode
    BIO* base64_decoding_filter;            //filter that base64 decodes data read through it
    int read_return_value;                  //bio_read function return value
    int computed_output_byte_array_size;    //necessary size of byte buffer to hold base64 decoded data
    bool operation_status = false;          //denotes success or failure of the operation

    //check inputs
    if ((base64_encoded_string != NULL) && (output_byte_array != NULL) && (output_byte_array_size != NULL))
    {
        //create an i/o filter that base64 encodes data written through it
        base64_decoding_filter = BIO_new(BIO_f_base64());
        //create a memory source for encoded data to be read from
        memory_source = BIO_new_mem_buf((char*)base64_encoded_string, NUL_TERMINATED_STRING);
        //expect the base64 encoded string contained in memory_source to be all on one line
        BIO_set_flags(base64_decoding_filter, BIO_FLAGS_BASE64_NO_NL);

        //if the filter and source were created successfully
        if ((base64_decoding_filter != NULL) && (memory_source != NULL))
        {
            //link (append) the source to the filter (e.g., create of a BIO chain)
            //essentially a linked-list pipeline where data is read through the filter BIO from the memory source BIO (thus base64 decoding it)
            BIO_push(base64_decoding_filter, memory_source);

            //compute the necessary size of the output byte array based on the supplied base64 string we aim to decode
            computed_output_byte_array_size = compute_base64_decode_byte_array_size(base64_encoded_string);

            //if we successfully computed the output byte array size
            if (computed_output_byte_array_size > 0)
            {
                //create byte buffer to hold decoded data
                *output_byte_array = malloc(computed_output_byte_array_size * (sizeof (uint8_t)));

                //if the buffer was successfully created
                if (*output_byte_array != NULL)
                {
                    //base64 decode the supplied data string
                    read_return_value = BIO_read(base64_decoding_filter, *output_byte_array, computed_output_byte_array_size);

                    //if the read was successful
                    if (read_return_value == computed_output_byte_array_size)
                    {
                        //set output byte array size
                        *output_byte_array_size = computed_output_byte_array_size;
                        //success
                        operation_status = true;
                    }
                    else //failure
                    {
                        //free buffer
                        free(*output_byte_array);
                        //set to null to indicate failure
                        *output_byte_array = NULL;
                        *output_byte_array_size = 0;
                    }
                }
            }
        }

        //free all BIO's
        BIO_free(base64_decoding_filter);
        BIO_free(memory_source);
    }

    return operation_status;
}

//function definition
//computes the size (in bytes) of the buffer needed to store the binary data derived from a base64 decoding
/*
 * Each base64 character (e.g, A-Z, a-z, 0-9, +, /) represents 6-bits of binary information (i.e., log2(64) = 6),
 * to simplify this (and allow us to think in terms of bytes) we'll think of 4 base64 characters representing
 * 3 bytes of data (i.e., 4 chars * 6 bits = 24 bits = 3 bytes). 3 bytes (or 24 bits is the  minimum block size).
 * This means that 4 * (n / 3) base64 characters are required to represent n bytes of data. If n is not divisible by 3,
 * 0 value bytes will need to be applied to the end (as padding) to get to the minimum block size. These 0 value bytes
 * are represented by the '=' character in base64 (the padding character). A '==' sequence at the end of a base64 encoded
 * string indicates that the last 24 bit block in the n byte binary buffer it was derived from (the n bytes being divided
 * into 3 byte blocks) only had 1 byte, therefore 2 zero vale bytes needed to be added to achieve the minimum block size.
 * A '=' sequence at the end indicates only 1 zero value bytes needed to be added to achieve the minimum block size. Obviously,
 * if n was divisible by 3 (zero remainder), no padding ('=' characters) would be applied.
*/
static int compute_base64_decode_byte_array_size(const char* base64_encoded_string)
{
    //local vars
    int computed_byte_array_size = 0;   //return value
    int base64_encoded_string_size;     //size of the supplied base64 encoded string
    int number_of_padding_bytes = 0;    //denotes how many padding characters ('=') are present in the supplied base64 encoded string

    //check input
    if (base64_encoded_string != NULL)
    {
        //compute the base64 string size
        base64_encoded_string_size = strlen(base64_encoded_string);

        //verify minimum block size so we don't step out of memory bounds
        if (base64_encoded_string_size >= BASE64_CHAR_BLOCK_SIZE)
        {
            //if a padding character exists in the last cell of the base64 encoded string
            if (base64_encoded_string[base64_encoded_string_size - 1] == '=')
            {
                //denode that a padding character was identified
                number_of_padding_bytes++;

                //if another padding character exists in the second to the last cell of the base64 encoded string
                if (base64_encoded_string[base64_encoded_string_size - 2] == '=')
                {
                    //denode that a padding character was identified
                    number_of_padding_bytes++;
                }
            }
        }

        //compute the byte array size, subtracting off any padding characters used during the original encoding process
        computed_byte_array_size = ((base64_encoded_string_size * BASE64_BYTE_BLOCK_SIZE) / BASE64_CHAR_BLOCK_SIZE) - number_of_padding_bytes;
    }

    //return the computed length
    return computed_byte_array_size;
}
