/* app_stm32l0.c
 *
 * Test bare-metal boot-led-on application
 *
 * Copyright (C) 2021 wolfSSL Inc.
 *
 * This file is part of wolfBoot.
 *
 * wolfBoot is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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

#include <stdint.h>
#include "led.h"
#include "target.h"
#include "image.h"
#include "wolfboot/wolfboot.h"

#ifdef TARGET_stm32l0

#define UART2 (0x40004400)
#define UART2_CR1      (*(volatile uint32_t *)(UART2 + 0x00))
#define UART2_CR2      (*(volatile uint32_t *)(UART2 + 0x04))
#define UART2_CR3      (*(volatile uint32_t *)(UART2 + 0x08))
#define UART2_BRR      (*(volatile uint32_t *)(UART2 + 0x0c))
#define UART2_ISR      (*(volatile uint32_t *)(UART2 + 0x1c))
#define UART2_ICR      (*(volatile uint32_t *)(UART2 + 0x20))
#define UART2_RDR      (*(volatile uint32_t *)(UART2 + 0x24))
#define UART2_TDR      (*(volatile uint32_t *)(UART2 + 0x28))

#define UART_CR1_UART_ENABLE    (1 << 0)
#define UART_CR1_SYMBOL_LEN     (1 << 12)
#define UART_CR1_PARITY_ENABLED (1 << 10)
#define UART_CR1_OVER8          (1 << 15)
#define UART_CR1_PARITY_ODD     (1 << 9)
#define UART_CR1_TX_ENABLE      (1 << 3)
#define UART_CR1_RX_ENABLE      (1 << 2)
#define UART_CR2_STOPBITS       (3 << 12)
#define UART_CR2_LINEN          (1 << 14)
#define UART_CR2_CLKEN          (1 << 11)
#define UART_CR3_HDSEL          (1 << 3)
#define UART_CR3_SCEN           (1 << 5)
#define UART_CR3_IREN           (1 << 1)
#define UART_ISR_TX_EMPTY       (1 << 7)
#define UART_ISR_RX_NOTEMPTY    (1 << 5)

#define RCC_IOPENR              (*(volatile uint32_t *)(0x4002102C))
#define APB1_CLOCK_ER           (*(volatile uint32_t *)(0x40021038))
#define IOPAEN (1 << 0)
#define IOPCEN (1 << 2)

#define UART2_APB1_CLOCK_ER_VAL 	(1 << 17)

#define GPIOA_BASE 0x50000000
#define GPIOA_MODE  (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_OTYPE (*(volatile uint32_t *)(GPIOA_BASE + 0x04))
#define GPIOA_OSPD  (*(volatile uint32_t *)(GPIOA_BASE + 0x08))
#define GPIOA_PUPD  (*(volatile uint32_t *)(GPIOA_BASE + 0x0c))
#define GPIOA_ODR   (*(volatile uint32_t *)(GPIOA_BASE + 0x14))
#define GPIOA_BSRR  (*(volatile uint32_t *)(GPIOA_BASE + 0x18))
#define GPIOA_AFL   (*(volatile uint32_t *)(GPIOA_BASE + 0x20))
#define GPIOA_AFH   (*(volatile uint32_t *)(GPIOA_BASE + 0x24))

#define GPIO_MODE_AF (2)
#define UART2_PIN_AF 4
#define UART2_RX_PIN 2
#define UART2_TX_PIN 3

#ifndef CLOCK_SPEED
#define CLOCK_SPEED (24000000)
#endif

/* Cortex-M0+ VTOR register */
static inline uint32_t get_vtor(void) {
    return *(volatile uint32_t*)0xE000ED08u;
}

/* Running image header is immediately before the vector table */
static inline uint8_t* running_hdr_ptr(void) {
    return (uint8_t*)(get_vtor() - IMAGE_HEADER_SIZE);
}

static void uart2_pins_setup(void)
{
    uint32_t reg;
    RCC_IOPENR |= IOPAEN;
    /* Set mode = AF */
    reg = GPIOA_MODE & ~ (0x03 << (UART2_RX_PIN * 2));
    GPIOA_MODE = reg | (2 << (UART2_RX_PIN * 2));
    reg = GPIOA_MODE & ~ (0x03 << (UART2_TX_PIN * 2));
    GPIOA_MODE = reg | (2 << (UART2_TX_PIN * 2));

    /* Alternate function: use low pins (2 and 3) */
    reg = GPIOA_AFL & ~(0xf << (UART2_TX_PIN * 4));
    GPIOA_AFL = reg | (UART2_PIN_AF << (UART2_TX_PIN * 4));
    reg = GPIOA_AFL & ~(0xf << (UART2_RX_PIN * 4));
    GPIOA_AFL = reg | (UART2_PIN_AF << (UART2_RX_PIN * 4));

}

static uint32_t running_firmware_version(void) {
    return wolfBoot_get_blob_version(running_hdr_ptr());
}

static int running_is_slot_b(void) {
    /* Slot B VTOR is slot B header + IMAGE_HEADER_SIZE */
    uintptr_t slotB_vtor =
        (uintptr_t)TO_ABS_ADDR(WOLFBOOT_PARTITION_UPDATE_ADDRESS) + IMAGE_HEADER_SIZE;
    return get_vtor() == slotB_vtor;
}

extern int uart_init(uint32_t bitrate, uint8_t data, char parity, uint8_t stop);
extern int uart_tx(uint8_t c);

static inline void uart_write(const char* buf, int len) {
    for (int i = 0; i < len; i++) uart_tx((uint8_t)buf[i]);
}

// int uart_read(uint8_t *c, int len)
// {
//     volatile uint32_t reg;
//     int i = 0;
//     reg = UART2_ISR;
//     if (reg & UART_ISR_RX_NOTEMPTY) {
//         *c = (uint8_t)UART2_RDR;
//         return 1;
//     }
//     return 0;
// }

static inline int my_strlen(const char* s) {
    int n = 0;
    while (s[n] != '\0') n++;
    return n;
}

void uart_print(const char *s)
{
    uart_write(s, my_strlen(s));
}

static void uart_print_u32(uint32_t v) {    // print unsigned in decimal
    char buf[11];                            // 10 digits + NUL
    int i = 10;
    buf[i] = '\0';
    do {
        buf[--i] = (char)('0' + (v % 10u));
        v /= 10u;
    } while (v && i > 0);
    uart_print(&buf[i]);
}

static void uart_print_hex_u32(uint32_t v) { // optional hex helper
    const char hexd[] = "0123456789ABCDEF";
    char buf[10] = "0x00000000";
    for (int i = 9; i >= 2; --i) { buf[i] = hexd[v & 0xF]; v >>= 4; }
    uart_print(buf);
}

/* ---------- Tiny delay (no SysTick / CMSIS needed) ---------- */
static void delay_cycles(uint32_t n) { while (n--) __asm__ volatile ("nop"); }

/* Very rough ~1ms using CPU busy-wait (tune divider if needed) */
static void delay_ms(uint32_t ms)
{
    /* Divider 3000 -> ~8 cycles per loop body * 3000 ≈ 24k cycles ~ 1 ms @ 24 MHz */
    const uint32_t per_ms = CLOCK_SPEED / 8000u;
    for (uint32_t i = 0; i < ms; i++) delay_cycles(per_ms);
}

/* Longest key possible: AES256 (32 key + 16 IV = 48) */
char enc_key[] = "0123456789abcdef0123456789abcdef"
                 "0123456789abcdef";

void main(void)
{
    uint32_t version;
    uint32_t update_version;

    /* Bring up UART first so early logs are visible */
    // uart_setup(115200);
    uart_init(115200, 8, 'N', 1);
    uart_print("\n\rSTM32L0 Test Application\n\r");

    uint32_t boot_ver   = wolfBoot_current_firmware_version();  // Slot A (BOOT)
    uint32_t update_ver = wolfBoot_update_firmware_version();   // Slot B (UPDATE)
    uint32_t run_ver    = running_firmware_version();           // what’s actually running

    uart_print("Partition A version = ");   uart_print_u32(boot_ver);   uart_print("\r\n");
    uart_print("Partition B version = "); uart_print_u32(update_ver); uart_print("\r\n");
    uart_print("Firmware is Currently Running Version = ");   uart_print_u32(run_ver);    uart_print("\r\n");

    uint32_t blinks_per_second = run_ver ? run_ver : 1;

// #if EXT_ENCRYPTED
//         wolfBoot_set_encrypt_key((uint8_t *)enc_key, (uint8_t *)(enc_key + 32));
// #endif
        // wolfBoot_update_trigger();

    wolfBoot_success();
    /* Blink 'version' times per second indefinitely */
    while (1) {
        uint32_t cycle_ms = 1000u / blinks_per_second;  /* full on+off time */
        uint32_t half_ms  = cycle_ms / 2u;              /* on or off time   */

        for (uint32_t i = 0; i < blinks_per_second; i++) {
            boot_led_on();
            delay_ms(half_ms);
            boot_led_off();
            delay_ms(half_ms);
        }

        /* Keep period at ~1s, burn remainder if needed */
        uint32_t spent = (cycle_ms * blinks_per_second);
        if (spent < 1000u) delay_ms(1000u - spent);
    }
}

#endif /* TARGET_stm32l0 */