//
// Created by cp723 on 3/14/2019.
//

#include <stdio.h>
#include <memory.h>

#include "crypto/aes256-ni.h"

void print_hex(const unsigned char *array, size_t count) {
    for(size_t i = 0; i < count; i++) {
        printf("%02x", array[i]);
    }
}

int main() {
    const unsigned char key[] = {
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
            0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
            0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
    };
    // A message that's exactly 16 bytes long.
    const char msg[] = "Hello world x2!\n";
    const unsigned char expected_cipher[] = {
            0x00, 0x80, 0xb5, 0xcd, 0x7d, 0x63, 0x1b, 0x04,
            0x25, 0x8a, 0xa4, 0x38, 0x55, 0x33, 0x1b, 0x3e
    };

    unsigned char cipher[16];
    char decrypted_msg[sizeof(msg)];

    aes256_ecb_encrypt(cipher, key, (const unsigned char*)msg, strlen(msg));

    printf("Encryption: Test ");
    if(!memcmp(cipher, expected_cipher, sizeof(cipher))) {
        printf("Passed\n");
    }
    else {
        printf("Failed\n");
    }

    print_hex(cipher, sizeof(cipher));
    printf("\n");

    print_hex(expected_cipher, sizeof(expected_cipher));
    printf("\n\n");

    aes256_ecb_decrypt((unsigned char*)decrypted_msg, key, expected_cipher, sizeof(expected_cipher));
    decrypted_msg[strlen(msg)] = '\0';

    printf("Decryption: Test ");
    if(!strcmp(msg, decrypted_msg)) {
        printf("Passed\n");
    }
    else {
        printf("Failed\n");
    }

    print_hex(msg, strlen(msg));
    printf("\n");

    print_hex(decrypted_msg, strlen(msg));
    printf("\n");

    return EXIT_SUCCESS;
}