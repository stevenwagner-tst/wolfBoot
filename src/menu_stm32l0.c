#include "menu.h"
#include "wolfboot/wolfboot.h"
#include "image.h"
#include "printf.h"
#include "hal.h"
#include <stdint.h>

/* ========== Board/SoC specifics (STM32L0, USART2 on PA2/PA3) ========== */
#ifndef CLOCK_SPEED
#define CLOCK_SPEED (24000000u)
#endif

#define RCC_IOPENR      (*(volatile uint32_t *)(0x4002102C))
#define IOPAEN          (1u << 0)

#define USART2_BASE     (0x40004400u)
#define USART2_ISR      (*(volatile uint32_t *)(USART2_BASE + 0x1C))
#define USART2_RDR      (*(volatile uint32_t *)(USART2_BASE + 0x24))
#define USART2_TDR      (*(volatile uint32_t *)(USART2_BASE + 0x28))
#define USART_ISR_RXNE  (1u << 5)
#define USART_ISR_TXE   (1u << 7)
#define USART2_ICR      (*(volatile uint32_t *)(USART2_BASE + 0x20)) /* interrupt clear */
#define USART_ISR_PE    (1u << 0)
#define USART_ISR_FE    (1u << 1)
#define USART_ISR_NF    (1u << 2)
#define USART_ISR_ORE   (1u << 3)
#define USART_ISR_RXNE  (1u << 5)

/* ICR bits to clear the corresponding ISR flags (same positions on L0) */
#define USART_ICR_PECF   (1u << 0)
#define USART_ICR_FECF   (1u << 1)
#define USART_ICR_NCF    (1u << 2)
#define USART_ICR_ORECF  (1u << 3)

/* ========== UI timing/helpers ========== */
static void delay_cycles(uint32_t n){ while(n--) __asm__ volatile("nop"); }
static void delay_ms(uint32_t ms){
    const uint32_t per_ms = CLOCK_SPEED / 8000u;
    for (uint32_t i = 0; i < ms; ++i) delay_cycles(per_ms);
}

/* Non-blocking getchar on USART2 (console already initialized by wolfBoot) */
static int uart_nb_getc(void)
{
    uint32_t isr = USART2_ISR;

    /* Clear line/errors proactively so RX keeps working */
    if (isr & (USART_ISR_FE | USART_ISR_NF | USART_ISR_ORE | USART_ISR_PE)) {
        /* If a byte is pending, read it once to advance the state machine */
        if (isr & USART_ISR_RXNE) (void)USART2_RDR;
        /* Clear error conditions */
        USART2_ICR = (USART_ICR_FECF | USART_ICR_NCF | USART_ICR_ORECF | USART_ICR_PECF);
        /* Re-read ISR after clearing */
        isr = USART2_ISR;
    }

    if (isr & USART_ISR_RXNE) {
        return (int)(USART2_RDR & 0xFF);
    }
    return -1;
}

static inline unsigned ui_strlen(const char* s) {
    unsigned n = 0;
    while (s[n] != '\0') n++;
    return n;
}

static inline void ui_write(const char* s) {
    uart_write(s, ui_strlen(s));
}

static void ui_print_hex_u32(uint32_t v) {
    static const char hexd[] = "0123456789ABCDEF";
    char buf[10] = "0x00000000";
    for (int i = 9; i >= 2; --i) {
        buf[i] = hexd[v & 0xF];
        v >>= 4;
    }
    ui_write(buf);
}

/* ========== Persist preferred slot in SWAP sector (tiny record) ========== */
#define PREF_MAGIC   0x534C4F54u /* "SLOT" */
#define PREF_ADDR    ((uintptr_t)WOLFBOOT_PARTITION_SWAP_ADDRESS) /* first bytes */
#define PREF_ALIGN   (4u)

typedef struct {
    uint32_t magic;
    uint32_t choice;   /* 0=AUTO, 1=A, 2=B */
    uint32_t crc;
} pref_rec_t;

static uint32_t crc32_sw(const uint8_t* p, uint32_t len){
    uint32_t c = 0xFFFFFFFFu;
    for (uint32_t i=0;i<len;i++){
        c ^= p[i];
        for (int b=0;b<8;b++)
            c = (c>>1) ^ (0xEDB88320u & (-(int)(c & 1u)));
    }
    return ~c;
}

static int read_pref(pref_rec_t* out){
    const pref_rec_t* pr = (const pref_rec_t*)PREF_ADDR;
    if (pr->magic != PREF_MAGIC) return -1;
    uint32_t calc = crc32_sw((const uint8_t*)pr, sizeof(pref_rec_t)-4u);
    if (calc != pr->crc) return -1;
    *out = *pr;
    return 0;
}

static int write_pref(slot_choice_t ch){
    pref_rec_t pr;
    pr.magic  = PREF_MAGIC;
    pr.choice = (uint32_t)ch;
    pr.crc    = crc32_sw((const uint8_t*)&pr, sizeof(pref_rec_t)-4u);

    hal_flash_unlock();
#ifndef WOLFBOOT_FLASH_MULTI_SECTOR_ERASE
    /* erase just first small chunk; sector granularity is handled by HAL */
    hal_flash_erase(PREF_ADDR, sizeof(pref_rec_t));
#else
    /* if you mass-erase elsewhere, you can just write */
#endif
    int rc = hal_flash_write(PREF_ADDR, (const uint8_t*)&pr, sizeof(pr));
    hal_flash_lock();
    return rc;
}

void persist_preferred_slot(slot_choice_t choice){ (void)write_pref(choice); }
slot_choice_t read_preferred_slot(void){
    pref_rec_t pr;
    return (read_pref(&pr) == 0) ? (slot_choice_t)pr.choice : SLOT_AUTO;
}
void clear_preferred_slot(void){
    hal_flash_unlock();
#ifndef WOLFBOOT_FLASH_MULTI_SECTOR_ERASE
    hal_flash_erase(PREF_ADDR, sizeof(pref_rec_t));
#endif
    /* optionally write zeros */
    hal_flash_lock();
}

// /* --- fwd decls for functions provided elsewhere --- */
// extern int  uart_nb_getc(void);
// extern void delay_ms(uint32_t ms);
// extern void ui_write(const char *s);
// extern void ui_print_hex_u32(uint32_t v);
// extern slot_choice_t read_preferred_slot(void);
// extern void persist_preferred_slot(slot_choice_t choice);
extern uint32_t wolfBoot_get_blob_version(uint8_t *blob);

static int get_key(uint32_t timeout_ms){
    uint32_t waited = 0;
    for (;;) {
        int c = uart_nb_getc();     /* -1 if no char */
        if (c >= 0) return (c & 0xFF);
        if (timeout_ms && waited >= timeout_ms) return -1;
        delay_ms(2);
        waited += 2;
    }
}

/* ========== Image helpers (quiet & fast) ========== */
#ifndef ARCH_FLASH_OFFSET
#define ARCH_FLASH_OFFSET FLASHMEM_ADDRESS_SPACE   /* 0x08000000 on L0 */
#endif

static int header_valid(uintptr_t hdr_off){
    volatile uint32_t magic = *(volatile uint32_t*)(hdr_off + ARCH_FLASH_OFFSET);
    return (magic == WOLFBOOT_MAGIC);
}

static uint32_t header_version(uintptr_t hdr_off){
    return wolfBoot_get_blob_version((uint8_t*)(hdr_off + ARCH_FLASH_OFFSET));
}

static void ui_read_slots(int* a_valid, uint32_t* a_ver, int* b_valid, uint32_t* b_ver){
    uintptr_t a_hdr = (uintptr_t)WOLFBOOT_PARTITION_BOOT_ADDRESS;
    uintptr_t b_hdr = (uintptr_t)WOLFBOOT_PARTITION_UPDATE_ADDRESS;
    *a_valid = header_valid(a_hdr);
    *b_valid = header_valid(b_hdr);
    *a_ver   = *a_valid ? header_version(a_hdr) : 0;
    *b_ver   = *b_valid ? header_version(b_hdr) : 0;
}

/* Eat bytes until we've had 'quiet_ms' of silence */
static void drain_uart_quick(uint32_t quiet_ms)
{
    uint32_t quiet = 0;
    for (;;) {
        int d = uart_nb_getc();
        if (d >= 0) { quiet = 0; continue; }
        if (quiet >= quiet_ms) break;
        delay_ms(1);
        quiet++;
    }
}

/* Minimal nav: A/D (or a/d) + Enter. (Optional: arrows via ESC [ C/D) */
static int read_nav_key(uint32_t poll_ms_per_call)
{
    int c = get_key(poll_ms_per_call);
    if (c < 0) return -1;

    /* Enter */
    if (c == '\r' || c == '\n') return c;

    /* WASD */
    if (c == 'A' || c == 'a') return 'L';
    if (c == 'D' || c == 'd') return 'R';
    if (c == 'S' || c == 's' ||
        c == 'U' || c == 'u')   return c;

    /* ESC-based sequences: arrows, F-keys, Home/End/Del, etc. */
    if (c == 0x1B) {
        int b1 = get_key(30);                 /* was 20 */
        if (b1 < 0) return -2;

        if (b1 == '[' || b1 == 'O') {
            int final = -1;
            for (int i = 0; i < 8; i++) {
                int b = get_key(20);          /* was 10 */
                if (b < 0) break;
                if ((b >= 'A' && b <= 'D') || b == '~') { final = b; break; }
            }
            drain_uart_quick(3);              /* keep this small drain */
            if (final == 'C') return 'R';
            if (final == 'D') return 'L';
            return -2;
        }
        drain_uart_quick(3);
        return -2;
    }

    //     /* Unknown second byte after ESC: drain a tad and ignore */
    //     drain_uart_quick(2);
    //     return -2;
    // }

    /* Ignore everything else (function keys you didn’t parse, etc.) */
    return -1;
}

static unsigned my_strlen(const char* s)
{ 
    unsigned n=0;
    while (s[n]) n++;
    return n; 
}

/* Writes one 24-char cell like: ">> SLOT A (ACTIVE) <<" or "   SLOT B (BACKUP)   " */
static void write_cell(const char* label, const char* role, int selected)
{
    int used = 0;

    /* left chevrons or spaces (3 chars) */
    if (selected) { ui_write("  >> "); used += 5; }
    else          { ui_write("     "); used += 5; }

    /* label, " (", role, ")" */
    ui_write(label);                     used += (int)my_strlen(label);
    ui_write(" (");                      used += 2;
    if (role && *role) { ui_write(role); used += (int)my_strlen(role); }
    ui_write(")");                       used += 1;

    /* right chevrons only if selected (3 chars) */
    if (selected) { ui_write(" <<"); used += 3; }

    /* pad the rest with spaces up to 24 total */
    while (used < 24) { ui_write(" "); used++; }
}

/* ========== Simple text UI ========== */
static void draw_ui(int sel, int a_valid, uint32_t a_ver, int b_valid, uint32_t b_ver, slot_choice_t saved_choice)
{
    /* derive ACTIVE/BACKUP badges from saved default; blank when AUTO */
    const char* a_role = "ACTIVE";
    const char* b_role = "BACKUP";
    if (saved_choice == SLOT_A) { a_role = "ACTIVE"; b_role = "BACKUP"; }
    else if (saved_choice == SLOT_B){ a_role = "BACKUP"; b_role = "ACTIVE"; }
    /* else (AUTO): leave both empty to avoid implying a default */

    /* clear + title */
    ui_write("\x1B[2J\x1B[H");
    ui_write("=== BOOT Partition Selector Menu ===\r\n\r\n");

    /* top borders */
    ui_write("+------------------------+  +------------------------+\r\n");

    /* headline cells (exact 24 chars each) */
    ui_write("|");
    write_cell("SLOT A", a_role, (sel == 0));
    ui_write("|  |");
    write_cell("SLOT B", b_role, (sel == 1));
    ui_write("|\r\n");

    /* bottom border of headline row */
    ui_write("+------------------------+  +------------------------+\r\n");

    /* validity row */
    ui_write("  ");
    ui_write(a_valid ? "Image: Valid  " : "Image: Invalid");
    ui_write("              ");
    ui_write(b_valid ? "Image: Valid" : "Image: Invalid");
    ui_write("\r\n");

    /* version row (hex) */
    ui_write("  FW Version: ");
    ui_print_hex_u32(a_ver);
    ui_write("      FW Version: ");
    ui_print_hex_u32(b_ver);
    ui_write("\r\n\r\n");

    ui_write("Controls:\r\n");
    ui_write("A = Move Selection Left\r\n");
    ui_write("D = Move Selection Right\r\n");
    ui_write("S = Save Current Selection as default BOOT Partition\r\n");
    ui_write("U = Update Firmware\r\n");
    ui_write("Enter = Boot to Selected BOOT Partition\r\n\r\n");
}

/* Global choice (default AUTO) */
volatile slot_choice_t g_menu_choice = SLOT_AUTO;

slot_choice_t menu_preboot_run(void)
{
    int a_valid, b_valid; 
    uint32_t a_ver, b_ver;
    int sel = 0;

    /* Apply saved default (user can still override) */
    slot_choice_t saved = read_preferred_slot();
    if (saved == SLOT_A || saved == SLOT_B) g_menu_choice = saved;
    if (g_menu_choice == SLOT_B) sel = 1;

    const char* slot = (g_menu_choice == SLOT_B) ? "B" : "A";
    int enter_menu = 0;

    ui_write("\r\nPress any key to enter boot menu, otherwise booting from slot ");
    ui_write(slot);
    ui_write(" in ");
    ui_write("\x1B[s");  /* save cursor at start of digits */

    for (int sec = 3; sec > 0; --sec) {
        /* rebuild "3 ..." / "3 2 ..." / "3 2 1 ..." */
        ui_write("\x1B[u");      /* restore cursor to digits */
        char buf[32];
        int n = 0;
        for (int s = 3; s >= sec; --s) {   /* cumulative from 3 down to current */
            if (n) buf[n++] = ' ';
            buf[n++] = (char)('0' + s);
        }
        buf[n++] = ' ';
        buf[n++] = '.';
        buf[n++] = '.';
        buf[n++] = '.';
        buf[n]   = '\0';
        ui_write(buf);
        ui_write("\x1B[K");      /* clear rest of line */

        /* Poll 1s in 10ms slices; ANY byte triggers menu */
        for (int t = 0; t < 100; ++t) {
            if (uart_nb_getc() >= 0) { enter_menu = 1; break; }
            delay_ms(10);
        }
        if (enter_menu) break;
    }
    
    ui_read_slots(&a_valid, &a_ver, &b_valid, &b_ver);

    if (!enter_menu) {
        /* No key: skip menu and persist current choice (if set) */
        if (g_menu_choice != SLOT_AUTO) persist_preferred_slot(g_menu_choice);
        ui_write("\r\n");
        if (g_menu_choice == SLOT_A) {
            ui_write("Booting verion ");
            ui_print_hex_u32(a_ver);
            ui_write(" from Slot A...\r\n");
        }
        else if (g_menu_choice == SLOT_B) {
            ui_write("Booting verion ");
            ui_print_hex_u32(b_ver);
            ui_write(" from Slot B...\r\n");
        }
        else {
            ui_write("No override\r\n");
        }
        return g_menu_choice;
    }

    /* initial slot info + UI */
    ui_write("\r\n");
    
    draw_ui(sel, a_valid, a_ver, b_valid, b_ver, g_menu_choice);

    // /* If user presses ANY key, we stop auto-timeout */
    // // int interactive = 0;
    // int have_saved  = (g_menu_choice == SLOT_A || g_menu_choice == SLOT_B);
    // uint32_t idle = 0;
    // uint32_t idle_limit_ms = 3000;   /* 3s before auto-continue */

    for (;;) {
        int k = read_nav_key(10);     /* poll every 10 ms */
        if (k < 0) {                  /* -1: no byte this tick */
            // idle += 10;
            // if (idle > idle_limit_ms && g_menu_choice != SLOT_AUTO) break;
            continue;
        }

        /* got SOME byte -> user is interacting: disable further timeout */
        // idle = 0;

        // /* We saw some input: disable the timeout from now on */
        // interactive = 1;

        if (k == -2) {
            /* ESC sequence consumed but not relevant: just ignore and keep waiting */
            continue;
        }

        if (k == '\r' || k == '\n') {
            /* ONLY Enter commits & exits */
            g_menu_choice = (sel == 0) ? SLOT_A : SLOT_B;
            persist_preferred_slot(g_menu_choice);
            break;
        } else if (k == 'L') {
            sel = 0;
            draw_ui(sel, a_valid, a_ver, b_valid, b_ver, g_menu_choice);
        } else if (k == 'R') {
            sel = 1;
            draw_ui(sel, a_valid, a_ver, b_valid, b_ver, g_menu_choice);
        } else if (k == 'S' || k == 's') {
            slot_choice_t choice = (sel == 0) ? SLOT_A : SLOT_B;
            if ((choice == SLOT_A && a_valid) || (choice == SLOT_B && b_valid)) {
                persist_preferred_slot(choice);
                ui_write("Saved default.\r\n");
            } else {
                ui_write("Cannot save: chosen slot not valid.\r\n");
            }
            /* stay in menu */
        } else if (k == 'U' || k == 'u') {
            ui_write("Entering UART flash/update server...\r\n");
            /* if you actually jump to a server, do it here. Otherwise just stay. */
            /* continue; */
        } else if (k == 'Q' || k == 'q') {
            /* explicit cancel: no override; continue normal policy */
            g_menu_choice = SLOT_AUTO;
            break;
        }
    }

    /* Final banner without printf */
    if (g_menu_choice == SLOT_A) {
        ui_write("Booting verion ");
        ui_print_hex_u32(a_ver);
        ui_write(" from Slot A...\r\n");
    }
    else if (g_menu_choice == SLOT_B) {
        ui_write("Booting verion ");
        ui_print_hex_u32(b_ver);
        ui_write(" from Slot B...\r\n");
    }
    else {
        ui_write("No override\r\n");
    }

    return g_menu_choice;
}