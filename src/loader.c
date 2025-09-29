/* loader.c
 *
 * Copyright (C) 2020 wolfSSL Inc.
 *
 * This file is part of wolfBoot.
 *
 * wolfBoot is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * wolfBoot is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */

#include "loader.h"
#include "image.h"
#include "hal.h"
#include "spi_flash.h"
#include "uart_flash.h"
#include "wolfboot/wolfboot.h"
#include "menu.h"

#ifdef RAM_CODE
extern unsigned int _start_text;
static volatile const uint32_t __attribute__((used)) wolfboot_version = WOLFBOOT_VERSION;
extern void (** const IV_RAM)(void);
#endif


int main(void)
{
    hal_init();
    wolfBoot_printf("\r\nwolfBoot starting...\r\n");

#ifdef TEST_FLASH
    hal_flash_test();
#endif
#ifdef WOLFBOOT_ENABLE_WOLFHSM_CLIENT
    if (0 != hal_hsm_init_connect()) {
        wolfBoot_panic();
    }
#elif defined(WOLFBOOT_ENABLE_WOLFHSM_SERVER)
    if (0 != hal_hsm_server_init()) {
        wolfBoot_panic();
    }
#endif
    spi_flash_probe();
#ifdef UART_FLASH
    uart_init(UART_FLASH_BITRATE, 8, 'N', 1);
    // wolfBoot_printf("UART flash server ready @ %d\n", UART_FLASH_BITRATE);
    uart_send_current_version();
#endif

g_menu_choice = menu_preboot_run();

#ifdef WOLFBOOT_TPM
    wolfBoot_tpm2_init();
#endif
#ifdef WOLFCRYPT_SECURE_MODE
    wcs_Init();
#endif
    wolfBoot_printf("\r\nWolfBoot Bootloader starting...\r\n");
    wolfBoot_start();

    /* wolfBoot_start should never return. */
    wolfBoot_printf("wolfBoot_start returned! Panic.\n");
    wolfBoot_panic();

    return 0;
}
