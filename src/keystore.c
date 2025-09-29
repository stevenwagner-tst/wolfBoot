/* Keystore file for wolfBoot, automatically generated. Do not edit.  */
/*
 * This file has been generated and contains the public keys
 * used by wolfBoot to verify the updates.
 */
#include <stdint.h>
#include "wolfboot/wolfboot.h"
#include "keystore.h"

#ifdef WOLFBOOT_NO_SIGN
    #define NUM_PUBKEYS 0
#else

#if !defined(KEYSTORE_ANY) && (KEYSTORE_PUBKEY_SIZE != KEYSTORE_PUBKEY_SIZE_ECC256)
    #error Key algorithm mismatch. Remove old keys via 'make keysclean'
#else

#if defined(__APPLE__) && defined(__MACH__)
#define KEYSTORE_SECTION __attribute__((section ("__KEYSTORE,__keystore")))
#elif defined(__CCRX__) || defined(WOLFBOOT_RENESAS_RSIP) || defined(WOLFBOOT_RENESAS_TSIP) || defined(WOLFBOOT_RENESAS_SCEPROTECT)
#define KEYSTORE_SECTION /* Renesas RX */
#elif defined(TARGET_x86_64_efi)
#define KEYSTORE_SECTION
#else
#define KEYSTORE_SECTION __attribute__((section (".keystore")))
#endif

#define NUM_PUBKEYS 1
const KEYSTORE_SECTION struct keystore_slot PubKeys[NUM_PUBKEYS] = {

    /* Key associated to file 'wolfboot_signing_private_key.der' */
    {
        .slot_id = 0,
        .key_type = AUTH_KEY_ECC256,
        .part_id_mask = 0xFFFFFFFF,
        .pubkey_size = 64,
        .pubkey = {
            
            0xb2, 0x78, 0x4e, 0xc4, 0xf9, 0x85, 0xb5, 0x1e,
            0xa9, 0x61, 0x93, 0x1f, 0xb5, 0x9d, 0x7b, 0x68,
            0x1c, 0xc2, 0xf7, 0x92, 0xa4, 0x86, 0x68, 0xca,
            0x78, 0x7c, 0xc2, 0x4b, 0x6b, 0xe8, 0xaa, 0xc8,
            0xc0, 0x65, 0x08, 0x22, 0xb5, 0x3d, 0xcc, 0x82,
            0x16, 0xcd, 0xc8, 0x55, 0xa8, 0x3d, 0x7a, 0x52,
            0x90, 0x59, 0x11, 0x6e, 0xea, 0x12, 0xc7, 0xa0,
            0xac, 0xe5, 0x74, 0xfc, 0x4a, 0x8a, 0x3e, 0xd2


        },
    },


};

int keystore_num_pubkeys(void)
{
    return NUM_PUBKEYS;
}

uint8_t *keystore_get_buffer(int id)
{
    (void)id;
    if (id >= keystore_num_pubkeys())
        return (uint8_t *)0;
    return (uint8_t *)PubKeys[id].pubkey;
}

int keystore_get_size(int id)
{
    (void)id;
    if (id >= keystore_num_pubkeys())
        return -1;
    return (int)PubKeys[id].pubkey_size;
}

uint32_t keystore_get_mask(int id)
{
    if (id >= keystore_num_pubkeys())
        return 0;
    return PubKeys[id].part_id_mask;
}

uint32_t keystore_get_key_type(int id)
{
    return PubKeys[id].key_type;
}

#endif /* Keystore public key size check */
#endif /* WOLFBOOT_NO_SIGN */
