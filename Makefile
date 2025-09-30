## wolfBoot Makefile
#
# Configure by passing alternate values
# via environment variables.
#
# Configuration values: see tools/config.mk
-include .config
include tools/config.mk

## Initializers
WOLFBOOT_ROOT?=$(PWD)
CFLAGS:=-D__WOLFBOOT -DWOLFBOOT_VERSION=$(WOLFBOOT_VERSION)UL -ffunction-sections -fdata-sections
LSCRIPT:=config/target.ld
LSCRIPT_FLAGS:=
LDFLAGS :=
SECURE_LDFLAGS:=
LD_START_GROUP:=-Wl,--start-group
LD_END_GROUP:=-Wl,--end-group
LSCRIPT_IN:=hal/$(TARGET).ld
V?=0
DEBUG?=1
DEBUG_UART?=1
LIBS=
SIGN_ALG=
OBJCOPY_FLAGS=
BIG_ENDIAN?=0
USE_GCC?=1
USE_GCC_HEADLESS?=1
FLASH_OTP_KEYSTORE?=0
BOOTLOADER_PARTITION_SIZE?=$$(( $(WOLFBOOT_PARTITION_BOOT_ADDRESS) - $(ARCH_FLASH_OFFSET)))

OBJS:= \
	./src/string.o \
	./src/image.o \
	./src/libwolfboot.o \
	./hal/hal.o \
	./src/menu_stm32l0.o

ifneq ($(filter 1,$(DEBUG_UART) $(UART_FLASH)),)
  OBJS += src/uart_flash.o
  OBJS += hal/uart/uart_drv_$(UART_TARGET).o
  CFLAGS += -DUART_FLASH=$(UART_FLASH)
  CFLAGS += -DUART_TARGET=$(UART_TARGET)
endif

ifneq ($(TARGET),library)
	OBJS+=./hal/$(TARGET).o
endif

ifeq ($(SIGN),NONE)
  PRIVATE_KEY=
else
  PRIVATE_KEY=wolfboot_signing_private_key.der
  ifeq ($(FLASH_OTP_KEYSTORE),1)
    OBJS+=./src/flash_otp_keystore.o
  else
    OBJS+=./src/keystore.o
  endif
endif

WOLFCRYPT_OBJS:=
PUBLIC_KEY_OBJS:=
UPDATE_OBJS:=

## Architecture/CPU configuration
include arch.mk

# Parse config options
include options.mk

CFLAGS+=-Wall -Wextra -Wno-main -ffreestanding -Wno-unused \
  -I. -Iinclude/ -Ilib/wolfssl -nostartfiles \
  -DWOLFSSL_USER_SETTINGS \
  -DWOLFTPM_USER_SETTINGS \
  -DPLATFORM_$(TARGET)

MAIN_TARGET=factory.bin

ifeq ($(TARGET),stm32l5)
    # Don't build a contiguous image
	MAIN_TARGET:=wolfboot.bin test-app/image_v1_signed.bin
endif

ASFLAGS:=$(CFLAGS)

all: $(MAIN_TARGET)

wolfboot.bin: wolfboot.elf
	@echo "\t[BIN] $@"
	$(Q)$(OBJCOPY) -O binary $^ $@

align: wolfboot-align.bin

.bootloader-partition-size:
	$(Q)printf "%d" $(WOLFBOOT_PARTITION_BOOT_ADDRESS) > .wolfboot-offset
	$(Q)printf "%d" $(ARCH_FLASH_OFFSET) > .wolfboot-arch-offset
	$(Q)expr `cat .wolfboot-offset` - `cat .wolfboot-arch-offset` > .bootloader-partition-size
	$(Q)rm -f .wolfboot-offset .wolfboot-arch-offset

wolfboot-align.bin: .bootloader-partition-size wolfboot.bin
	$(Q)dd if=/dev/zero bs=`cat .bootloader-partition-size` count=1 2>/dev/null | tr "\000" "\377" > $(@)
	$(Q)dd if=wolfboot.bin of=$(@) conv=notrunc 2>/dev/null
	@echo
	@echo "\t[SIZE]"
	$(Q)$(SIZE) wolfboot.elf
	@echo

test-app/image.bin: wolfboot.bin
	$(Q)make -C test-app WOLFBOOT_ROOT=$(WOLFBOOT_ROOT)
	$(Q)rm -f src/*.o hal/*.o
	$(Q)$(SIZE) test-app/image.elf

standalone:
	$(Q)make -C test-app TARGET=$(TARGET) EXT_FLASH=$(EXT_FLASH) SPI_FLASH=$(SPI_FLASH) ARCH=$(ARCH) \
    NO_XIP=$(NO_XIP) V=$(V) RAM_CODE=$(RAM_CODE) WOLFBOOT_VERSION=$(WOLFBOOT_VERSION)\
	MCUXPRESSO=$(MCUXPRESSO) MCUXPRESSO_CPU=$(MCUXPRESSO_CPU) MCUXPRESSO_DRIVERS=$(MCUXPRESSO_DRIVERS) \
	MCUXPRESSO_CMSIS=$(MCUXPRESSO_CMSIS) NVM_FLASH_WRITEONCE=$(NVM_FLASH_WRITEONCE) \
	FREEDOM_E_SDK=$(FREEDOM_E_SDK) standalone
	$(Q)$(OBJCOPY) -O binary test-app/image.elf standalone.bin
	$(Q)$(SIZE) test-app/image.elf

include tools/test.mk
include tools/test-enc.mk

ed25519.der:
	$(Q)$(KEYGEN_TOOL) $(KEYGEN_OPTIONS) src/ed25519_pub_key.c
ecc256.der:
	$(Q)$(KEYGEN_TOOL) $(KEYGEN_OPTIONS) src/ecc256_pub_key.c
rsa2048.der:
	$(Q)$(KEYGEN_TOOL) $(KEYGEN_OPTIONS) src/rsa2048_pub_key.c
rsa4096.der:
	$(Q)$(KEYGEN_TOOL) $(KEYGEN_OPTIONS) src/rsa4096_pub_key.c

keytools:
	@make -C tools/keytools

test-app/image_v1_signed.bin: test-app/image.bin
	@echo "\t[SIGN] $(BOOT_IMG)"
	$(Q)$(SIGN_TOOL) $(SIGN_OPTIONS) $(BOOT_IMG) $(PRIVATE_KEY) 1

	$(Q)(test $(SIGN) = NONE) || $(SIGN_ENV) $(SIGN_TOOL) $(SIGN_OPTIONS) \
		$(SECONDARY_SIGN_OPTIONS) $(BOOT_IMG) $(PRIVATE_KEY) \
		$(SECONDARY_PRIVATE_KEY) 1 || true
	$(Q)(test $(SIGN) = NONE) && $(SIGN_ENV) $(SIGN_TOOL) $(SIGN_OPTIONS) $(BOOT_IMG) 1 || true

test-app/image_v2_signed.bin: $(BOOT_IMG)
	$(Q)(test $(SIGN) = NONE) || $(SIGN_ENV) $(SIGN_TOOL) $(SIGN_OPTIONS) \
		$(SECONDARY_SIGN_OPTIONS) $(BOOT_IMG) $(PRIVATE_KEY) \
		$(SECONDARY_PRIVATE_KEY) 2 || true
	$(Q)(test $(SIGN) = NONE) && $(SIGN_ENV) $(SIGN_TOOL) $(SIGN_OPTIONS) \
		$(BOOT_IMG) 2  || true

test-app/image_v3_signed.bin: $(BOOT_IMG)
	$(Q)(test $(SIGN) = NONE) || $(SIGN_ENV) $(SIGN_TOOL) $(SIGN_OPTIONS) \
		$(SECONDARY_SIGN_OPTIONS) $(BOOT_IMG) $(PRIVATE_KEY) \
		$(SECONDARY_PRIVATE_KEY) 3 || true
	$(Q)(test $(SIGN) = NONE) && $(SIGN_ENV) $(SIGN_TOOL) $(SIGN_OPTIONS) \
		$(BOOT_IMG) 3  || true

test-app/image.elf: wolfboot.elf
	$(Q)$(MAKE) -C test-app WOLFBOOT_ROOT="$(WOLFBOOT_ROOT)" image.elf
	$(Q)$(SIZE) test-app/image.elf

ifeq ($(ELF_FLASH_SCATTER),1)
test-app/image.elf: squashelf
endif

assemble_internal_flash.dd: FORCE
	$(Q)$(BINASSEMBLE) internal_flash.dd \
		0 wolfboot.bin \
		$$(($(WOLFBOOT_PARTITION_BOOT_ADDRESS) - $(ARCH_FLASH_OFFSET))) test-app/image_v1_signed.bin \
		$$(($(WOLFBOOT_PARTITION_UPDATE_ADDRESS)-$(ARCH_FLASH_OFFSET))) /tmp/swap \
		$$(($(WOLFBOOT_PARTITION_SWAP_ADDRESS)-$(ARCH_FLASH_OFFSET))) /tmp/swap

internal_flash.dd: $(BINASSEMBLE) wolfboot.bin $(BOOT_IMG) $(PRIVATE_KEY) test-app/image_v1_signed.bin
	@echo "\t[MERGE] internal_flash.dd"
	$(Q)dd if=/dev/zero bs=1 count=$$(($(WOLFBOOT_SECTOR_SIZE))) > /tmp/swap
	make assemble_internal_flash.dd

factory.bin: $(BINASSEMBLE) wolfboot.bin $(BOOT_IMG) $(PRIVATE_KEY) test-app/image_v1_signed.bin
	@echo "\t[MERGE] $@"
	$(Q)cat wolfboot-align.bin test-app/image_v1_signed.bin > $@

wolfboot.elf: include/target.h $(OBJS) $(LSCRIPT) FORCE
	@echo "\t[LD] $@"
	$(Q)$(LD) $(LDFLAGS) -Wl,--start-group $(OBJS) -Wl,--end-group -o $@

$(LSCRIPT): hal/$(TARGET).ld .bootloader-partition-size FORCE
	@cat hal/$(TARGET).ld | \
		sed -e "s/##WOLFBOOT_PARTITION_BOOT_ADDRESS##/`cat .bootloader-partition-size`/g" | \
		sed -e "s/##WOLFBOOT_ORIGIN##/$(WOLFBOOT_ORIGIN)/g" \
		> $@

hex: wolfboot.hex

%.hex:%.elf
	@echo "\t[ELF2HEX] $@"
	@$(OBJCOPY) -O ihex $^ $@

src/ed25519_pub_key.c: ed25519.der

src/ecc256_pub_key.c: ecc256.der

src/rsa2048_pub_key.c: rsa2048.der

src/rsa4096_pub_key.c: rsa4096.der

keys: $(PRIVATE_KEY)

clean:
	@find . -type f -name "*.o" | xargs rm -f
	@rm -f *.bin *.elf wolfboot.map *.bin  *.hex config/target.ld .bootloader-partition-size
	@make -C test-app clean
	@make -C tools/check_config clean

distclean: clean
	@rm -f *.pem *.der tags ./src/*_pub_key.c include/target.h
	@make -C tools/keytools clean

include/target.h: include/target.h.in FORCE
	@cat include/target.h.in | \
	sed -e "s/##WOLFBOOT_PARTITION_SIZE##/$(WOLFBOOT_PARTITION_SIZE)/g" | \
	sed -e "s/##WOLFBOOT_SECTOR_SIZE##/$(WOLFBOOT_SECTOR_SIZE)/g" | \
	sed -e "s/##WOLFBOOT_PARTITION_BOOT_ADDRESS##/$(WOLFBOOT_PARTITION_BOOT_ADDRESS)/g" | \
	sed -e "s/##WOLFBOOT_PARTITION_UPDATE_ADDRESS##/$(WOLFBOOT_PARTITION_UPDATE_ADDRESS)/g" | \
	sed -e "s/##WOLFBOOT_PARTITION_SWAP_ADDRESS##/$(WOLFBOOT_PARTITION_SWAP_ADDRESS)/g" | \
	sed -e "s/##WOLFBOOT_DTS_BOOT_ADDRESS##/$(WOLFBOOT_DTS_BOOT_ADDRESS)/g" | \
	sed -e "s/##WOLFBOOT_DTS_UPDATE_ADDRESS##/$(WOLFBOOT_DTS_UPDATE_ADDRESS)/g" | \
	sed -e "s/##WOLFBOOT_LOAD_ADDRESS##/$(WOLFBOOT_LOAD_ADDRESS)/g" | \
	sed -e "s/##WOLFBOOT_LOAD_DTS_ADDRESS##/$(WOLFBOOT_LOAD_DTS_ADDRESS)/g" \
		> $@

config: FORCE
	make -C config

check_config:
	make -C tools/check_config


../src/libwolfboot.o: ../src/libwolfboot.c FORCE
	@echo "\t[CC-$(ARCH)] $@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ ../src/libwolfboot.c

%.o:%.c
	@echo "\t[CC-$(ARCH)] $@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $^

%.o:%.S
	@echo "\t[AS-$(ARCH)] $@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $^

FORCE:

.PHONY: FORCE clean
