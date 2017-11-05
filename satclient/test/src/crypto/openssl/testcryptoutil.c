/*
   Author: James Beasley
   Repo: https://github.com/embeddedcognition/satclient

   Tests in this suite are of the form:
   Test Name: test_[Name of function being tested]_[condition tested]_renders_[expected result]
   Behavior Tested: The [Name of function being tested] function should provide [expected result] when [condition tested] is applied.

 * Assuming your SSH'd into your embedded linux system...
 *
 * The contents of plaintext.txt are: "Plaintext for compute_aes256cfb_cipher test."
 *
 * To encrypt the plaintext located in plaintext.txt and output it to ciphertext.base2, execute the following:
 *
 * openssl enc -e -aes-256-cfb -salt -md sha256 -pass pass:"The sparrow flies at sunset." -p -in plaintext.txt -out ciphertext.base2
 *
 * You'll receive similar to the following below (since the 64-bit salt is randomly generated each time, the key and iv will of course be different)
 *
 * salt=CCD3729694A02D65
 * key=FB5927F22EAA9B2C8C17379D83E37FE70E4E37F79B44373C3B51FC47A8BDC27F
 * iv =372C1EDFC6233322F27F6FE7D7AF3456
 *
 * The contents of ciphertext.base2 are binary, to view them in hex (base16) format, execute the following:
 *
 * xxd ciphertext.base2
 *
 * You'll receive similar output to the following:
 *
 * 0000000: 5361 6c74 6564 5f5f ccd3 7296 94a0 2d65  Salted__..r...-e
 * 0000010: 2631 76db 5807 d0de 6125 70f6 ca20 3047  &1v.X...a%p.. 0G
 * 0000020: e138 2b39 bb37 cc64 313b 090e 84ce 834c  .8+9.7.d1;.....L
 * 0000030: 1a78 1064 be17 7838 60b9 2073 82         .x.d..x8`. s.
 *
 * The first line (16 bytes) of the output is the salt header. Specifically the "Salted__" label/signature takes up the first 8 bytes, and the remaining 8 bytes are the salt value itself.
 *
 * Alternatively seen by executing the following:
 *
 * xxd -p ciphertext.base2  (The salt header has been manually highlighted via parens below)
 *
 * (53616c7465645f5fccd3729694a02d65)263176db5807d0de612570f6ca203047e1382b39bb37cc64313b090e84ce834c1a781064be17783860b9207382
 *
 * The remaining lines of the output are the actual ciphertext (should equal the size of the plaintext since we're using cfb mode for encryption). This can be verified by executing the following:
 *
 * xxd plaintext.txt
 *
 * 0000000: 506c 6169 6e74 6578 7420 666f 7220 636f  Plaintext for co
 * 0000010: 6d70 7574 655f 6165 7332 3536 6366 625f  mpute_aes256cfb_
 * 0000020: 6369 7068 6572 2074 6573 742e 0a         cipher test..
 *
 * The contents of ciphertext.base2 can now be converted to base64 so they are easier to view and transport:
 *
 * openssl enc -base64 -A -in ciphertext.base2 -out ciphertext.base64
 *
 * The contents of ciphertext.base64 should be similar to: U2FsdGVkX1/M03KWlKAtZSYxdttYB9DeYSVw9sogMEfhOCs5uzfMZDE7CQ6EzoNMGngQZL4XeDhguSBzgg==
 *
 * Next execute the following to test your ability to reproduce the same key and iv as above, based on your knowledge of the passphrase and salt value
 *
 * openssl enc -e -aes-256-cfb -S CCD3729694A02D65 -md sha256 -pass pass:"The sparrow flies at sunset." -p -in plaintext.txt -out ciphertext.base2
 *
 * You should receive the same output as above:
 *
 * salt=CCD3729694A02D65
 * key=FB5927F22EAA9B2C8C17379D83E37FE70E4E37F79B44373C3B51FC47A8BDC27F
 * iv =372C1EDFC6233322F27F6FE7D7AF3456
 *
 * We can now use this feature to extract the salt from the ciphertext and use it along with the passphrase to automatically derive the key and iv for the user instead of needing to ask them
 * to enter these values each time the satclient is executed.
 *
 * The contents of the ciphertext.base2 file can be manually decrypted by executing the following:
 *
 * openssl enc -d -aes-256-cfb -salt -md sha256 -pass pass:"The sparrow flies at sunset." -in ciphertext.base2 -out plaintext.txt
 *
 * The contents of the ciphertext.base64 file can be manually decrypted by executing the following:
 *
 * openssl enc -d -aes-256-cfb -salt -md sha256 -pass pass:"The sparrow flies at sunset." -base64 -A -in ciphertext.base64 -out plaintext.txt
 */

#include <stdlib.h>         //using for "free" function and "NULL"
#include <string.h>         //using for "strlen" function
#include "unity.h"          //using unity unit testing framework/harness
#include "cryptoutil.h"     //testing functions in the crypto module

//global vars
//plaintext (in hex format)
//plaintext (in text format) = "Plaintext for compute_aes256cfb_cipher test."
//the last byte "0x0a" is a line feed character '\n' (which is likely to be appended at the end of the string (to terminate the line) when saving a file in an editor (such as vi))
static const uint8_t expected_plaintext[] = {0x50, 0x6c, 0x61, 0x69, 0x6e, 0x74, 0x65, 0x78, 0x74, 0x20, 0x66, 0x6f, 0x72, 0x20, 0x63, 0x6f, \
                                             0x6d, 0x70, 0x75, 0x74, 0x65, 0x5f, 0x61, 0x65, 0x73, 0x32, 0x35, 0x36, 0x63, 0x66, 0x62, 0x5f, \
                                             0x63, 0x69, 0x70, 0x68, 0x65, 0x72, 0x20, 0x74, 0x65, 0x73, 0x74, 0x2e, 0x0a};
//passphrase (in hex format)
//passphrase (in text format) = "The sparrow flies at sunset."
static const uint8_t expected_passphrase[] = {0x54, 0x68, 0x65, 0x20, 0x73, 0x70, 0x61, 0x72, 0x72, 0x6f, 0x77, 0x20, 0x66, 0x6c, 0x69, 0x65, \
                                              0x73, 0x20, 0x61, 0x74, 0x20, 0x73, 0x75, 0x6e, 0x73, 0x65, 0x74, 0x2e};
//256-bit secret key generated using the passphrase and the salt
static const uint8_t expected_key[] = {0xfb, 0x59, 0x27, 0xf2, 0x2e, 0xaa, 0x9b, 0x2c, 0x8c, 0x17, 0x37, 0x9d, 0x83, 0xe3, 0x7f, 0xe7, \
                                       0x0e, 0x4e, 0x37, 0xf7, 0x9b, 0x44, 0x37, 0x3c, 0x3b, 0x51, 0xfc, 0x47, 0xa8, 0xbd, 0xc2, 0x7f};
//128-bit initialization vector generated using the passphrase and the salt
static const uint8_t expected_iv[] = {0x37, 0x2c, 0x1e, 0xdf, 0xc6, 0x23, 0x33, 0x22, 0xf2, 0x7f, 0x6f, 0xe7, 0xd7, 0xaf, 0x34, 0x56};
//salt header + ciphertext, (processed version of plaintext) generated by openssl command line utility - first 16 bytes are the salt header, remaining bytes are the ciphertext
static const uint8_t expected_openssl_payload[] = {0x53, 0x61, 0x6c, 0x74, 0x65, 0x64, 0x5f, 0x5f, 0xcc, 0xd3, 0x72, 0x96, 0x94, 0xa0, 0x2d, 0x65, \
                                                   0x26, 0x31, 0x76, 0xdb, 0x58, 0x07, 0xd0, 0xde, 0x61, 0x25, 0x70, 0xf6, 0xca, 0x20, 0x30, 0x47, \
                                                   0xe1, 0x38, 0x2b, 0x39, 0xbb, 0x37, 0xcc, 0x64, 0x31, 0x3b, 0x09, 0x0e, 0x84, 0xce, 0x83, 0x4c, \
                                                   0x1a, 0x78, 0x10, 0x64, 0xbe, 0x17, 0x78, 0x38, 0x60, 0xb9, 0x20, 0x73, 0x82};
//the salt value occupies bytes 8 thru 15 of the openssl_payload
static const uint8_t* expected_openssl_payload_salt = &(expected_openssl_payload[8]);
//advance openssl_payload pointer past the salt signature and value - first 16 bytes - to point at the ciphertext (remaining bytes in array)
static const uint8_t* expected_openssl_payload_ciphertext = &(expected_openssl_payload[16]);
//HMAC produced using sha-256, expected_key, and the following message: http%3A%2F%2Fazure.com%2Fml\n1466231493
//the hmac can be generated on demand by executing the following: echo -e -n "http%3A%2F%2Fazure.com%2Fml\n1466231493" | openssl dgst -sha256 -mac HMAC -macopt hexkey:fb5927f22eaa9b2c8c17379d83e37fe70e4e37f79b44373c3b51fc47a8bdc27f -hex
//the hexkey used is the "expected_key" defined above
//Note on "echo" command line options, "-e" option honors the newline character in the string and "-n" removes the newline character that would otherwise be appended to the end of the string
static const uint8_t expected_hmac[] = {0x8d, 0x17, 0x4d, 0x6a, 0x20, 0xa3, 0x72, 0xc7, 0x3a, 0xed, 0x34, 0x47, 0x31, 0xa7, 0x66, 0xc7, \
                                        0x46, 0x6b, 0xcc, 0x7e, 0xa6, 0xe1, 0xba, 0xe0, 0x7a, 0x51, 0xdb, 0xbb, 0xaf, 0x24, 0x91, 0x5e};
//array sizes
static const uint32_t expected_plaintext_size = 45;
static const uint32_t expected_passphrase_size = 28;
static const uint32_t expected_key_size = 32;
static const uint32_t expected_iv_size = 16;
static const uint32_t expected_openssl_payload_size = 61;
static const uint32_t expected_openssl_payload_salt_size = 8;
//in cfb mode the ciphertext is the same size as the plaintext
static const uint32_t expected_openssl_payload_ciphertext_size = 45;
static const uint32_t expected_hmac_size = 32;

//function declarations
static void test_extract_salt_and_ciphertext_from_openssl_payload_if_valid_inputs_renders_valid_salt_and_ciphertext(void);
static void test_derive_aes256cfb_cipher_key_and_iv_from_passphrase_and_salt_if_valid_inputs_renders_valid_key_and_iv(void);
static void test_compute_aes256cfb_cipher_if_decrypt_mode_and_valid_inputs_renders_valid_plaintext(void);
static void test_compute_sha256_hmac_if_valid_inputs_renders_valid_hmac(void);
static void test_compute_base64_encode_if_valid_inputs_renders_valid_encoded_text_string(void);
static void test_compute_base64_decode_if_valid_inputs_renders_valid_decoded_byte_data(void);
int main(void);

//function definition
/*
 * This function contains initialization logic run before each test function is executed.
 * It sets up the preconditions/environment necessary for each test to run.
 */
void setUp(void){}

//function definition
/*
 * This function contains cleanup logic run after each test function is executed.
 * It cleanly removes the preconditions/environment at the end of each test.
 */
void tearDown(void){}

//function definition
/*
 *   Behavior Tested: The extract_salt_and_ciphertext_from_openssl_payload function should provide the expected salt and ciphertext values when:
 *   - a valid openssl_payload is supplied
 */
static void test_extract_salt_and_ciphertext_from_openssl_payload_if_valid_inputs_renders_valid_salt_and_ciphertext(void)
{
    //local vars
    bool operation_status;
    uint8_t* openssl_payload_salt;
    uint32_t openssl_payload_salt_size;
    uint8_t* openssl_payload_ciphertext;
    uint32_t openssl_payload_ciphertext_size;

    //test the specific behavior
    operation_status = extract_salt_and_ciphertext_from_openssl_payload(expected_openssl_payload, expected_openssl_payload_size, &openssl_payload_salt, &openssl_payload_salt_size, &openssl_payload_ciphertext, &openssl_payload_ciphertext_size);

    //assert the expected results
    //the function should return "true" denoting operation success
    TEST_ASSERT_TRUE(operation_status);
    //ensure the size of the outputted byte arrays match the expected sizes
    TEST_ASSERT_EQUAL_INT(expected_openssl_payload_salt_size, openssl_payload_salt_size);
    TEST_ASSERT_EQUAL_INT(expected_openssl_payload_ciphertext_size, openssl_payload_ciphertext_size);
    //sample multiple portions of the salt to ensure it matches the expected salt
    TEST_ASSERT_EQUAL_HEX8(expected_openssl_payload_salt[0], openssl_payload_salt[0]); //beginning
    TEST_ASSERT_EQUAL_HEX8(expected_openssl_payload_salt[(int)(expected_openssl_payload_salt_size / 2)], openssl_payload_salt[(int)(openssl_payload_salt_size / 2)]); //middle
    TEST_ASSERT_EQUAL_HEX8(expected_openssl_payload_salt[expected_openssl_payload_salt_size - 1], openssl_payload_salt[openssl_payload_salt_size - 1]); //end
    //sample multiple portions of the ciphertext to ensure it matches the expected ciphertext
    TEST_ASSERT_EQUAL_HEX8(expected_openssl_payload_ciphertext[0], openssl_payload_ciphertext[0]); //beginning
    TEST_ASSERT_EQUAL_HEX8(expected_openssl_payload_ciphertext[(int)(expected_openssl_payload_ciphertext_size / 2)], openssl_payload_ciphertext[(int)(openssl_payload_ciphertext_size / 2)]); //middle
    TEST_ASSERT_EQUAL_HEX8(expected_openssl_payload_ciphertext[expected_openssl_payload_ciphertext_size - 1], openssl_payload_ciphertext[openssl_payload_ciphertext_size - 1]); //end

    //free buffers
    free(openssl_payload_salt);
    free(openssl_payload_ciphertext);
}

//function definition
/*
 *   Behavior Tested: The derive_aes256cfb_cipher_key_and_iv_from_passphrase_and_salt function should provide the expected key and iv values when:
 *   - a valid passphrase and salt are supplied
 */
static void test_derive_aes256cfb_cipher_key_and_iv_from_passphrase_and_salt_if_valid_inputs_renders_valid_key_and_iv(void)
{
    //local vars
    bool operation_status;
    uint8_t* key;
    uint32_t key_size;
    uint8_t* iv;
    uint32_t iv_size;

    //test the specific behavior
    operation_status = derive_aes256cfb_cipher_key_and_iv_from_passphrase_and_salt(expected_passphrase, expected_passphrase_size, expected_openssl_payload_salt, expected_openssl_payload_salt_size, &key, &key_size, &iv, &iv_size);

    //assert the expected results
    //the function should return "true" denoting operation success
    TEST_ASSERT_TRUE(operation_status);
    //ensure the size of the outputted byte arrays match the expected sizes
    TEST_ASSERT_EQUAL_INT(expected_key_size, key_size);
    TEST_ASSERT_EQUAL_INT(expected_iv_size, iv_size);
    //sample multiple portions of the key to ensure it matches the expected key
    TEST_ASSERT_EQUAL_HEX8(expected_key[0], key[0]); //beginning
    TEST_ASSERT_EQUAL_HEX8(expected_key[(int)(expected_key_size / 2)], key[(int)(key_size / 2)]); //middle
    TEST_ASSERT_EQUAL_HEX8(expected_key[expected_key_size - 1], key[key_size - 1]); //end
    //sample multiple portions of the iv to ensure it matches the expected iv
    TEST_ASSERT_EQUAL_HEX8(expected_iv[0], iv[0]); //beginning
    TEST_ASSERT_EQUAL_HEX8(expected_iv[(int)(expected_iv_size / 2)], iv[(int)(iv_size / 2)]); //middle
    TEST_ASSERT_EQUAL_HEX8(expected_iv[expected_iv_size - 1], iv[iv_size - 1]); //end

    //free buffers
    free(key);
    free(iv);
}

//function definition
/*
 *   Behavior Tested: The compute_aes256cfb_cipher function should provide the expected plaintext when:
 *   - "DECRYPT" mode is selected
 *   - the secret key used to aes-256 encrypt the expected plaintext is supplied
 *   - the input data supplied is the aes-256 encrypted form (ciphertext) of the expected plaintext
 */
static void test_compute_aes256cfb_cipher_if_decrypt_mode_and_valid_inputs_renders_valid_plaintext(void)
{
    //local vars
    bool operation_status;
    uint8_t* plaintext;
    uint32_t plaintext_size;

    //test the specific behavior
    //test function in "DECRYPT" mode with valid inputs
    operation_status = compute_aes256cfb_cipher(DECRYPT, expected_key, expected_key_size, expected_iv, expected_iv_size, expected_openssl_payload_ciphertext, expected_openssl_payload_ciphertext_size, &plaintext, &plaintext_size);

    //assert the expected results
    //the function should return "true" denoting operation success
    TEST_ASSERT_TRUE(operation_status);
    //ensure the size of the outputted byte array matches the expected plaintext size
    TEST_ASSERT_EQUAL_INT(expected_plaintext_size, plaintext_size);
    //sample multiple portions of the outputted plaintext to ensure it matches the expected plaintext
    TEST_ASSERT_EQUAL_HEX8(expected_plaintext[0], plaintext[0]); //beginning
    TEST_ASSERT_EQUAL_HEX8(expected_plaintext[(int)(expected_plaintext_size / 2)], plaintext[(int)(plaintext_size / 2)]); //middle
    TEST_ASSERT_EQUAL_HEX8(expected_plaintext[expected_plaintext_size - 1], plaintext[plaintext_size - 1]); //end

    //free buffer
    free(plaintext);
}

//function definition
/*
 *   Behavior Tested: The compute_sha256_hmac function should provide the expected hmac when:
 *   - a valid secret key and message are supplied
 */
static void test_compute_sha256_hmac_if_valid_inputs_renders_valid_hmac(void)
{
    //local vars
    bool operation_status;
    uint8_t* hmac;
    uint32_t hmac_size;
    const char message[] = "http%3A%2F%2Fazure.com%2Fml\n1466231493";

    //test the specific behavior
    operation_status = compute_sha256_hmac(expected_key, expected_key_size, message, &hmac, &hmac_size);

    //assert the expected results
    //the function should return "true" denoting operation success
    TEST_ASSERT_TRUE(operation_status);
    //ensure the size of the outputted hmac matches the expected hmac size
    TEST_ASSERT_EQUAL_INT(expected_hmac_size, hmac_size);
    //sample multiple portions of the outputted hmac to ensure it matches the expected hmac
    TEST_ASSERT_EQUAL_HEX8(expected_hmac[0], hmac[0]); //beginning
    TEST_ASSERT_EQUAL_HEX8(expected_hmac[(int)(expected_hmac_size / 2)], hmac[(int)(hmac_size / 2)]); //middle
    TEST_ASSERT_EQUAL_HEX8(expected_hmac[expected_hmac_size - 1], hmac[hmac_size - 1]); //end

    //free buffer
    free(hmac);
}

//function definition
/*
 *   Behavior Tested: The compute_base64_encode function should provide the expected encoded data (in text string form) when:
 *   - valid byte data is supplied
 */
static void test_compute_base64_encode_if_valid_inputs_renders_valid_encoded_text_string(void)
{
    //local vars
    bool operation_status;
    char* base64_encoded_string;
    const char expected_base64_encoded_string[] = "VGhlIHNwYXJyb3cgZmxpZXMgYXQgc3Vuc2V0Lg=="; //base64 encoded version of "The sparrow flies at sunset."
    const uint8_t byte_data[] = {0x54, 0x68, 0x65, 0x20, 0x73, 0x70, 0x61, 0x72, 0x72, 0x6f, 0x77, 0x20, 0x66, 0x6c, 0x69, 0x65, 0x73, 0x20, 0x61, 0x74, 0x20, 0x73, 0x75, 0x6e, 0x73, 0x65, 0x74, 0x2e}; //byte version of "The sparrow flies at sunset."
    const uint32_t byte_data_size = 28;

    //test the specific behavior
    operation_status = compute_base64_encode(byte_data, byte_data_size, &base64_encoded_string);

    //assert the expected results
    //the function should return "true" denoting operation success
    TEST_ASSERT_TRUE(operation_status);
    //ensure the size of the outputted string matches the expected string size
    TEST_ASSERT_EQUAL_INT(strlen(expected_base64_encoded_string), strlen(base64_encoded_string));
    //ensure strings match
    TEST_ASSERT_EQUAL_STRING(expected_base64_encoded_string, base64_encoded_string);

    //free buffer
    free(base64_encoded_string);
}

//function definition
/*
 *   Behavior Tested: The compute_base64_decode function should provide the expected decoded data (in byte form) when:
 *   - a valid base64 encoded string is supplied
 */
static void test_compute_base64_decode_if_valid_inputs_renders_valid_decoded_byte_data(void)
{
    //local vars
    bool operation_status;
    uint8_t* base64_decoded_byte_data;
    uint32_t base64_decoded_byte_data_size;
    const char base64_encoded_string[] = "VGhlIHNwYXJyb3cgZmxpZXMgYXQgc3Vuc2V0Lg=="; //base64 encoded version of "The sparrow flies at sunset."
    const uint8_t expected_base64_decoded_byte_data[] = {0x54, 0x68, 0x65, 0x20, 0x73, 0x70, 0x61, 0x72, 0x72, 0x6f, 0x77, 0x20, 0x66, 0x6c, 0x69, 0x65, 0x73, 0x20, 0x61, 0x74, 0x20, 0x73, 0x75, 0x6e, 0x73, 0x65, 0x74, 0x2e}; //byte version of "The sparrow flies at sunset."
    const uint32_t expected_base64_decoded_byte_data_size = 28;

    //test the specific behavior
    operation_status = compute_base64_decode(base64_encoded_string, &base64_decoded_byte_data, &base64_decoded_byte_data_size);

    //assert the expected results
    //the function should return "true" denoting operation success
    TEST_ASSERT_TRUE(operation_status);
    //ensure the size of the outputted data matches the expected data size
    TEST_ASSERT_EQUAL_INT(expected_base64_decoded_byte_data_size, base64_decoded_byte_data_size);
    //sample multiple portions of the outputted data to ensure it matches the expected data
    TEST_ASSERT_EQUAL_HEX8(expected_base64_decoded_byte_data[0], base64_decoded_byte_data[0]); //beginning
    TEST_ASSERT_EQUAL_HEX8(expected_base64_decoded_byte_data[(int)(expected_base64_decoded_byte_data_size / 2)], base64_decoded_byte_data[(int)(base64_decoded_byte_data_size / 2)]); //middle
    TEST_ASSERT_EQUAL_HEX8(expected_base64_decoded_byte_data[expected_base64_decoded_byte_data_size - 1], base64_decoded_byte_data[base64_decoded_byte_data_size - 1]); //end

    //free buffer
    free(base64_decoded_byte_data);
}

//function definition
//main thread of execution
int main(void)
{
    //setup
    UNITY_BEGIN();

    //run tests
    RUN_TEST(test_extract_salt_and_ciphertext_from_openssl_payload_if_valid_inputs_renders_valid_salt_and_ciphertext);
    RUN_TEST(test_derive_aes256cfb_cipher_key_and_iv_from_passphrase_and_salt_if_valid_inputs_renders_valid_key_and_iv);
    RUN_TEST(test_compute_aes256cfb_cipher_if_decrypt_mode_and_valid_inputs_renders_valid_plaintext);
    RUN_TEST(test_compute_sha256_hmac_if_valid_inputs_renders_valid_hmac);
    RUN_TEST(test_compute_base64_encode_if_valid_inputs_renders_valid_encoded_text_string);
    RUN_TEST(test_compute_base64_decode_if_valid_inputs_renders_valid_decoded_byte_data);

    //tear down & display test results, returns the number of tests that failed
    return UNITY_END();
}
