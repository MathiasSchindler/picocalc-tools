#include "crypto/ed25519.h"

int crypto_ed25519_public_key_from_seed(unsigned char public_key[32], const unsigned char seed[32]) {
    (void)public_key;
    (void)seed;
    return -1;
}

int crypto_ed25519_sign(
    unsigned char signature[64],
    const unsigned char *message,
    size_t message_len,
    const unsigned char seed[32],
    const unsigned char public_key[32]
) {
    (void)signature;
    (void)message;
    (void)message_len;
    (void)seed;
    (void)public_key;
    return -1;
}

int crypto_ed25519_verify(
    const unsigned char signature[64],
    const unsigned char *message,
    size_t message_len,
    const unsigned char public_key[32]
) {
    (void)signature;
    (void)message;
    (void)message_len;
    (void)public_key;
    return -1;
}