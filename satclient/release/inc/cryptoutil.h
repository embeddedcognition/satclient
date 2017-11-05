/*
   Author: James Beasley
   Repo: https://github.com/embeddedcognition/satclient
*/

#ifndef CRYPTOUTIL_H_
#define CRYPTOUTIL_H_

#include <stdbool.h>    //using for "bool" type
#include <stdint.h>     //using for "uint8_t" and "uint32_t" types

//enum for use in setting cipher mode (decryption=0, encryption=1)
typedef enum cipher_mode
{
    DECRYPT,
    ENCRYPT
}CIPHER_MODE;

//function declarations
bool load_base64_encoded_openssl_payload(char**);
bool decrypt_base64_encoded_openssl_payload(const char*, uint8_t**, uint32_t*);
bool extract_salt_and_ciphertext_from_openssl_payload(const uint8_t*, const uint32_t, uint8_t**, uint32_t*, uint8_t**, uint32_t*);
bool derive_aes256cfb_cipher_key_and_iv_from_passphrase_and_salt(const uint8_t*, const uint32_t, const uint8_t*, const uint32_t, uint8_t**, uint32_t*, uint8_t**, uint32_t*);
bool compute_aes256cfb_cipher(CIPHER_MODE, const uint8_t*, const uint32_t, const uint8_t*, const uint32_t, const uint8_t*, const uint32_t, uint8_t**, uint32_t*);
bool compute_sha256_hmac(const uint8_t*, const uint32_t, const char*, uint8_t**, uint32_t*);
bool compute_sha256_hmac_2(const uint8_t*, const uint32_t, const char*, uint8_t**, uint32_t*);
char* compute_base16_string(const uint8_t*, const uint32_t);
char* compute_text_string(const uint8_t*, const uint32_t);
bool compute_base64_encode(const uint8_t*, const uint32_t, char**);
bool compute_base64_decode(const char*, uint8_t**, uint32_t*);

#endif /* CRYPTOUTIL_H_ */
