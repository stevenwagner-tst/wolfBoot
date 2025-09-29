## Measured boot requires TPM to be present
ifeq ($(MEASURED_BOOT),1)
  WOLFTPM:=1
  CFLAGS+=-DWOLFBOOT_MEASURED_BOOT
  CFLAGS+=-DWOLFBOOT_MEASURED_PCR_A=$(MEASURED_PCR_A)
endif

## DSA Settings
ifeq ($(SIGN),ECC256)
  KEYGEN_OPTIONS+=--ecc256
  SIGN_OPTIONS+=--ecc256
  PRIVATE_KEY=ecc256.der
  WOLFCRYPT_OBJS+= \
    $(MATH_OBJS) \
    ./lib/wolfssl/wolfcrypt/src/ecc.o \
    ./lib/wolfssl/wolfcrypt/src/memory.o \
    ./lib/wolfssl/wolfcrypt/src/wc_port.o \
    ./lib/wolfssl/wolfcrypt/src/hash.o
  CFLAGS+=-DWOLFBOOT_SIGN_ECC256 -DXMALLOC_USER
  ifeq ($(WOLFTPM),0)
    CFLAGS+=-Wstack-usage=3888
  else
    CFLAGS+=-Wstack-usage=6680
  endif
  PUBLIC_KEY_OBJS=./src/ecc256_pub_key.o
endif

ifeq ($(SIGN),ED25519)
  KEYGEN_OPTIONS+=--ed25519
  SIGN_OPTIONS+=--ed25519
  PRIVATE_KEY=ed25519.der
  WOLFCRYPT_OBJS+= ./lib/wolfssl/wolfcrypt/src/sha512.o \
    ./lib/wolfssl/wolfcrypt/src/ed25519.o \
    ./lib/wolfssl/wolfcrypt/src/ge_low_mem.o \
    ./lib/wolfssl/wolfcrypt/src/hash.o \
    ./lib/wolfssl/wolfcrypt/src/wolfmath.o \
    ./lib/wolfssl/wolfcrypt/src/wc_port.o \
    ./lib/wolfssl/wolfcrypt/src/fe_low_mem.o
  PUBLIC_KEY_OBJS=./src/ed25519_pub_key.o
  CFLAGS+=-DWOLFBOOT_SIGN_ED25519 -Wstack-usage=1024
endif

ifeq ($(SIGN),RSA2048)
  KEYGEN_OPTIONS+=--rsa2048
  SIGN_OPTIONS+=--rsa2048
  PRIVATE_KEY=rsa2048.der
  IMAGE_HEADER_SIZE=512
  WOLFCRYPT_OBJS+= \
    $(RSA_EXTRA_OBJS) \
    $(MATH_OBJS) \
    ./lib/wolfssl/wolfcrypt/src/rsa.o \
    ./lib/wolfssl/wolfcrypt/src/asn.o \
    ./lib/wolfssl/wolfcrypt/src/hash.o \
    ./lib/wolfssl/wolfcrypt/src/wc_port.o
  PUBLIC_KEY_OBJS=./src/rsa2048_pub_key.o
  CFLAGS+=-DWOLFBOOT_SIGN_RSA2048 -DXMALLOC_USER $(RSA_EXTRA_CFLAGS) \
		  -DIMAGE_HEADER_SIZE=512
  ifeq ($(WOLFTPM),0)
    CFLAGS+=-Wstack-usage=12288
  else
    CFLAGS+=-Wstack-usage=8320
  endif
endif

ifeq ($(SIGN),RSA4096)
  KEYGEN_OPTIONS+=--rsa4096
  SIGN_OPTIONS+=--rsa4096
  PRIVATE_KEY=rsa4096.der
  IMAGE_HEADER_SIZE=1024
  WOLFCRYPT_OBJS+= \
    $(RSA_EXTRA_OBJS) \
    $(MATH_OBJS) \
    ./lib/wolfssl/wolfcrypt/src/rsa.o \
    ./lib/wolfssl/wolfcrypt/src/asn.o \
    ./lib/wolfssl/wolfcrypt/src/hash.o \
    ./lib/wolfssl/wolfcrypt/src/wc_port.o
  PUBLIC_KEY_OBJS=./src/rsa4096_pub_key.o
  CFLAGS+=-DWOLFBOOT_SIGN_RSA4096 -DXMALLOC_USER $(RSA_EXTRA_CFLAGS) \
		  -DIMAGE_HEADER_SIZE=1024
  ifeq ($(WOLFTPM),0)
    CFLAGS+=-Wstack-usage=18064
  else
    CFLAGS+=-Wstack-usage=10680
  endif
endif



ifeq ($(RAM_CODE),1)
  CFLAGS+= -DRAM_CODE
endif

ifeq ($(FLAGS_HOME),1)
  CFLAGS+=-DFLAGS_HOME=1
endif

ifeq ($(FLAGS_INVERT),1)
  CFLAGS+=-DWOLFBOOT_FLAGS_INVERT=1
endif

ifeq ($(DUALBANK_SWAP),1)
  CFLAGS+=-DDUALBANK_SWAP=1
endif

ifeq ($(SPI_FLASH),1)
  EXT_FLASH=1
  CFLAGS+=-DSPI_FLASH=1
  OBJS+= src/spi_flash.o
  WOLFCRYPT_OBJS+=hal/spi/spi_drv_$(SPI_TARGET).o
endif

ifeq ($(UART_FLASH),1)
  CFLAGS += -DUART_FLASH=1
  CFLAGS += -DUART_TARGET=$(UART_TARGET)
endif

ifeq ($(ENCRYPT),1)
  CFLAGS+=-DEXT_ENCRYPTED=1
  WOLFCRYPT_OBJS+=./lib/wolfssl/wolfcrypt/src/chacha.o
endif

ifeq ($(EXT_FLASH),1)
  CFLAGS+= -DEXT_FLASH=1 -DPART_UPDATE_EXT=1 -DPART_SWAP_EXT=1
  ifeq ($(NO_XIP),1)
    CFLAGS+=-DPART_BOOT_EXT=1
  endif
  ifeq ($(UART_FLASH),1)
    CFLAGS+=-DUART_FLASH=1
    OBJS+=src/uart_flash.o
    WOLFCRYPT_OBJS+=hal/uart/uart_drv_$(UART_TARGET).o
  endif
endif



ifeq ($(ALLOW_DOWNGRADE),1)
  CFLAGS+= -DALLOW_DOWNGRADE
endif

ifeq ($(NVM_FLASH_WRITEONCE),1)
  CFLAGS+= -DNVM_FLASH_WRITEONCE
endif

ifeq ($(DISABLE_BACKUP),1)
  CFLAGS+= -DDISABLE_BACKUP
endif


ifeq ($(DEBUG),1)
  CFLAGS+=-O0 -g -ggdb3 -DDEBUG=1
else
  CFLAGS+=-Os
endif

ifeq ($(V),0)
  Q=@
endif

ifeq ($(NO_MPU),1)
  CFLAGS+=-DWOLFBOOT_NO_MPU
endif

ifeq ($(VTOR),0)
  CFLAGS+=-DNO_VTOR
endif

ifeq ($(PKA),1)
  OBJS += $(PKA_EXTRA_OBJS)
  CFLAGS+=$(PKA_EXTRA_CFLAGS)
endif

OBJS+=$(PUBLIC_KEY_OBJS)
OBJS+=$(UPDATE_OBJS)

ifeq ($(WOLFTPM),1)
  OBJS += lib/wolfTPM/src/tpm2.o \
    lib/wolfTPM/src/tpm2_packet.o \
    lib/wolfTPM/src/tpm2_tis.o \
    lib/wolfTPM/src/tpm2_wrap.o \
    lib/wolfTPM/src/tpm2_param_enc.o
  CFLAGS+=-DWOLFBOOT_TPM -DSIZEOF_LONG=4 -Ilib/wolfTPM \
    -DMAX_COMMAND_SIZE=1024 -DMAX_RESPONSE_SIZE=1024 -DWOLFTPM2_MAX_BUFFER=1500 \
    -DMAX_SESSION_NUM=1 -DMAX_DIGEST_BUFFER=973 \
    -DWOLFTPM_SMALL_STACK
  # Chip Type: WOLFTPM_SLB9670, WOLFTPM_ST33, WOLFTPM_MCHP
  CFLAGS+=-DWOLFTPM_SLB9670
  # Use TPM for hashing (slow)
  #CFLAGS+=-DWOLFBOOT_HASH_TPM
  ifneq ($(SPI_FLASH),1)
    WOLFCRYPT_OBJS+=hal/spi/spi_drv_$(SPI_TARGET).o
  endif
  WOLFCRYPT_OBJS+=./lib/wolfssl/wolfcrypt/src/aes.o
  WOLFCRYPT_OBJS+=./lib/wolfssl/wolfcrypt/src/hmac.o
endif

## Hash settings
ifeq ($(HASH),SHA256)
  CFLAGS+=-DWOLFBOOT_HASH_SHA256
endif

ifeq ($(HASH),SHA3)
  WOLFCRYPT_OBJS+=./lib/wolfssl/wolfcrypt/src/sha3.o
  CFLAGS+=-DWOLFBOOT_HASH_SHA3_384
  SIGN_OPTIONS+=--sha3
endif

OBJS+=$(WOLFCRYPT_OBJS)

CFLAGS+=-DIMAGE_HEADER_SIZE=$(IMAGE_HEADER_SIZE)
OBJS+=$(SECURE_OBJS)

# check if both encryption and self update are on
#
ifeq ($(RAM_CODE),1)
  ifeq ($(ENCRYPT),1)
    ifeq ($(ENCRYPT_WITH_CHACHA),1)
       LSCRIPT_IN=hal/$(TARGET)_chacha_ram.ld
    endif
  endif
  ifeq ($(ARCH),ARM)
    CFLAGS+=-mlong-calls
  endif
endif

# Support external encryption cache
#
ifeq ($(ENCRYPT),1)
  ifeq ($(ENCRYPT_CACHE),1)
	CFLAGS+=-D"WOLFBOOT_ENCRYPT_CACHE=$(ENCRYPT_CACHE)"
  endif
endif

# support for elf32 or elf64 loader
ifeq ($(ELF),1)
  CFLAGS+=-DWOLFBOOT_ELF
  OBJS += src/elf.o

  ifneq ($(DEBUG_ELF),)
    CFLAGS+=-DDEBUG_ELF=$(DEBUG_ELF)
  endif
  ifeq ($(ELF_FLASH_SCATTER),1)
    CFLAGS+=-D"WOLFBOOT_ELF_FLASH_SCATTER=1"
  endif

endif

ifeq ($(MULTIBOOT2),1)
  CFLAGS+=-DWOLFBOOT_MULTIBOOT2
  OBJS += src/multiboot.o
endif

ifeq ($(LINUX_PAYLOAD),1)
  CFLAGS+=-DWOLFBOOT_LINUX_PAYLOAD
  ifeq ($(ARCH),x86_64)
    OBJS+=src/x86/linux_loader.o
  endif
endif

ifeq ($(64BIT),1)
  CFLAGS+=-DWOLFBOOT_64BIT
endif

ifeq ($(WOLFBOOT_UNIVERSAL_KEYSTORE),1)
  CFLAGS+=-DWOLFBOOT_UNIVERSAL_KEYSTORE
endif

ifeq ($(DISK_LOCK),1)
  CFLAGS+=-DWOLFBOOT_ATA_DISK_LOCK
  ifneq ($(DISK_LOCK_PASSWORD),)
    CFLAGS+=-DWOLFBOOT_ATA_DISK_LOCK_PASSWORD=\"$(DISK_LOCK_PASSWORD)\"
  endif
  OBJS+=./lib/wolfssl/wolfcrypt/src/coding.o
endif

ifeq ($(FSP), 1)
  X86_FSP_OPTIONS := \
    X86_UART_BASE \
    X86_UART_REG_WIDTH \
    X86_UART_MMIO \
    PCH_HAS_PCR \
    PCI_USE_ECAM \
    PCH_PCR_BASE \
    PCI_ECAM_BASE \
    WOLFBOOT_LOAD_BASE \
    FSP_S_LOAD_BASE

    # set CFLAGS defines for each x86_fsp option
    $(foreach option,$(X86_FSP_OPTIONS),$(if $($(option)), $(eval CFLAGS += -D$(option)=$($(option)))))
endif

ifeq ($(FLASH_MULTI_SECTOR_ERASE),1)
    CFLAGS+=-DWOLFBOOT_FLASH_MULTI_SECTOR_ERASE
endif

CFLAGS+=$(CFLAGS_EXTRA)
OBJS+=$(OBJS_EXTRA)

ifeq ($(USE_GCC_HEADLESS),1)
  ifneq ($(ARCH),RENESAS_RX)
    CFLAGS+="-Wstack-usage=$(STACK_USAGE)"
  endif
endif

ifeq ($(SIGN_ALG),)
  SIGN_ALG=$(SIGN)
endif

ifneq ($(KEYVAULT_OBJ_SIZE),)
  CFLAGS+=-DKEYVAULT_OBJ_SIZE=$(KEYVAULT_OBJ_SIZE)
endif

ifneq ($(KEYVAULT_MAX_ITEMS),)
  CFLAGS+=-DKEYVAULT_MAX_ITEMS=$(KEYVAULT_MAX_ITEMS)
endif

# Support for using a custom partition ID
ifneq ($(WOLFBOOT_PART_ID),)
  CFLAGS+=-DHDR_IMG_TYPE_APP=$(WOLFBOOT_PART_ID)
  SIGN_OPTIONS+=--id $(WOLFBOOT_PART_ID)
endif

# wolfHSM client options
ifeq ($(WOLFHSM_CLIENT),1)
  LIBDIR := $(dir $(lastword $(MAKEFILE_LIST)))lib
  WOLFCRYPT_OBJS += \
    $(LIBDIR)/wolfssl/wolfcrypt/src/cryptocb.o \
    $(LIBDIR)/wolfssl/wolfcrypt/src/coding.o

  ifeq ($(SIGN),ML_DSA)
    WOLFCRYPT_OBJS += $(MATH_OBJS)
    # Dilithium asn.c decode/encode requires mp_xxx functions
    WOLFCRYPT_OBJS += \
        $(LIBDIR)/wolfssl/wolfcrypt/src/random.o

    # Large enough to handle the largest Dilithium key/signature
    CFLAGS += -DWOLFHSM_CFG_COMM_DATA_LEN=5000
  endif

  WOLFHSM_OBJS += \
    $(LIBDIR)/wolfHSM/src/wh_client.o \
    $(LIBDIR)/wolfHSM/src/wh_client_nvm.o \
    $(LIBDIR)/wolfHSM/src/wh_client_cryptocb.o \
    $(LIBDIR)/wolfHSM/src/wh_client_crypto.o \
    $(LIBDIR)/wolfHSM/src/wh_crypto.o \
    $(LIBDIR)/wolfHSM/src/wh_utils.o \
    $(LIBDIR)/wolfHSM/src/wh_comm.o \
    $(LIBDIR)/wolfHSM/src/wh_message_comm.o \
    $(LIBDIR)/wolfHSM/src/wh_message_nvm.o \
    $(LIBDIR)/wolfHSM/src/wh_message_customcb.o
  #includes
  CFLAGS += -I"$(LIBDIR)/wolfHSM"
  # defines
  CFLAGS += -DWOLFBOOT_ENABLE_WOLFHSM_CLIENT -DWOLFHSM_CFG_ENABLE_CLIENT
  # Make sure we export generated public keys so they can be used to load into
  # HSM out-of-band
  KEYGEN_OPTIONS += --exportpubkey --der

  # Default to using public keys on the HSM
  ifneq ($(WOLFHSM_CLIENT_LOCAL_KEYS),1)
    KEYGEN_OPTIONS += --nolocalkeys
    CFLAGS += -DWOLFBOOT_USE_WOLFHSM_PUBKEY_ID
    # big enough for cert chain
    CFLAGS += -DWOLFHSM_CFG_COMM_DATA_LEN=5000
  endif

  # Ensure wolfHSM is configured to use certificate manager if we are
  # doing cert chain verification
  ifneq ($(CERT_CHAIN_VERIFY),)
    WOLFHSM_OBJS += \
      $(LIBDIR)/wolfHSM/src/wh_client_cert.o \
      $(LIBDIR)/wolfHSM/src/wh_message_cert.o
    CFLAGS += -DWOLFHSM_CFG_CERTIFICATE_MANAGER
  endif
endif

# wolfHSM server options
ifeq ($(WOLFHSM_SERVER),1)
  LIBDIR := $(dir $(lastword $(MAKEFILE_LIST)))lib
  WOLFCRYPT_OBJS += \
    $(LIBDIR)/wolfssl/wolfcrypt/src/cryptocb.o \
    $(LIBDIR)/wolfssl/wolfcrypt/src/coding.o \
    $(LIBDIR)/wolfssl/wolfcrypt/src/random.o

  ifeq ($(SIGN),ML_DSA)
    WOLFCRYPT_OBJS += $(MATH_OBJS)
    # Large enough to handle the largest Dilithium key/signature
    CFLAGS += -DWOLFHSM_CFG_COMM_DATA_LEN=5000
  endif

  WOLFHSM_OBJS += \
    $(LIBDIR)/wolfHSM/src/wh_utils.o \
    $(LIBDIR)/wolfHSM/src/wh_comm.o \
    $(LIBDIR)/wolfHSM/src/wh_nvm.o \
    $(LIBDIR)/wolfHSM/src/wh_nvm_flash.o \
    $(LIBDIR)/wolfHSM/src/wh_flash_unit.o \
    $(LIBDIR)/wolfHSM/src/wh_crypto.o \
    $(LIBDIR)/wolfHSM/src/wh_server.o \
    $(LIBDIR)/wolfHSM/src/wh_server_nvm.o \
    $(LIBDIR)/wolfHSM/src/wh_server_crypto.o \
    $(LIBDIR)/wolfHSM/src/wh_server_counter.o \
    $(LIBDIR)/wolfHSM/src/wh_server_keystore.o \
    $(LIBDIR)/wolfHSM/src/wh_server_customcb.o \
    $(LIBDIR)/wolfHSM/src/wh_message_customcb.o \
    $(LIBDIR)/wolfHSM/src/wh_message_keystore.o \
    $(LIBDIR)/wolfHSM/src/wh_message_crypto.o \
    $(LIBDIR)/wolfHSM/src/wh_message_counter.o \
    $(LIBDIR)/wolfHSM/src/wh_message_nvm.o \
    $(LIBDIR)/wolfHSM/src/wh_message_comm.o \
    $(LIBDIR)/wolfHSM/src/wh_transport_mem.o \
    $(LIBDIR)/wolfHSM/port/posix/posix_flash_file.o

  #includes
  CFLAGS += -I"$(LIBDIR)/wolfHSM"
  # defines'
  CFLAGS += -DWOLFBOOT_ENABLE_WOLFHSM_SERVER -DWOLFHSM_CFG_ENABLE_SERVER

  # Ensure wolfHSM is configured to use certificate manager if we are
  # doing cert chain verification
  ifneq ($(CERT_CHAIN_VERIFY),)
    CFLAGS += -I"$(LIBDIR)/wolfssl"
    WOLFCRYPT_OBJS += \
      $(LIBDIR)/wolfssl/src/internal.o \
      $(LIBDIR)/wolfssl/src/ssl.o \
      $(LIBDIR)/wolfssl/src/ssl_certman.o

    WOLFHSM_OBJS += \
      $(LIBDIR)/wolfHSM/src/wh_message_cert.o \
      $(LIBDIR)/wolfHSM/src/wh_server_cert.o
    CFLAGS += -DWOLFHSM_CFG_CERTIFICATE_MANAGER
  endif
endif

# Cert chain verification options
ifneq ($(CERT_CHAIN_VERIFY),)
  CFLAGS += -DWOLFBOOT_CERT_CHAIN_VERIFY
  # export the private key in DER format so it can be used with certificates
  KEYGEN_OPTIONS += --der
  ifneq ($(CERT_CHAIN_GEN),)
    # Use dummy cert chain file if not provided (needs to be generated when keys are generated)
    CERT_CHAIN_FILE = test-dummy-ca/raw-chain.der

    # Set appropriate cert gen options based on sigalg
    ifeq ($(SIGN),ECC256)
      CERT_CHAIN_GEN_ALGO+=ecc256
    endif
    ifeq ($(SIGN),RSA2048)
      CERT_CHAIN_GEN_ALGO+=rsa2048
    endif
    ifeq ($(SIGN),RSA4096)
      CERT_CHAIN_GEN_ALGO+=rsa4096
    endif
  else
    ifeq ($(CERT_CHAIN_FILE),)
      $(error CERT_CHAIN_FILE must be specified when CERT_CHAIN_VERIFY is enabled and not using CERT_CHAIN_GEN)
    endif
  endif
  SIGN_OPTIONS += --cert-chain $(CERT_CHAIN_FILE)
endif

# Clock Speed (Hz)
ifneq ($(CLOCK_SPEED),)
	CFLAGS += -DCLOCK_SPEED=$(CLOCK_SPEED)
endif

# STM32F4 clock options
ifneq ($(STM32_PLLM),)
	CFLAGS += -DSTM32_PLLM=$(STM32_PLLM)
endif
ifneq ($(STM32_PLLN),)
	CFLAGS += -DSTM32_PLLN=$(STM32_PLLN)
endif
ifneq ($(STM32_PLLP),)
	CFLAGS += -DSTM32_PLLP=$(STM32_PLLP)
endif
ifneq ($(STM32_PLLQ),)
	CFLAGS += -DSTM32_PLLQ=$(STM32_PLLQ)
endif

# STM32 UART options
ifeq ($(USE_UART1),1)
	CFLAGS += -DUSE_UART1=1
endif

ifeq ($(USE_UART3),1)
	CFLAGS += -DUSE_UART3=1
endif

ifeq ($(WOLFBOOT_QUIET_IMAGE_LOG),1)
  CFLAGS += -DWOLFBOOT_QUIET_IMAGE_LOG
endif

ifeq ($(PART_ADDR_ABSOLUTE),1)
  CFLAGS += -DPART_ADDR_ABSOLUTE=1
else
  CFLAGS += -DPART_ADDR_ABSOLUTE=0
  CFLAGS += -DARCH_FLASH_OFFSET=$(ARCH_FLASH_OFFSET)
endif

