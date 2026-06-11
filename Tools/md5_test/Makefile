# Makefile for standalone mbedTLS MD5 verification test
#
# Uses the EXACT same md5.c from the embedded project.
# Compiles with MinGW/GCC on Windows.

CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -std=c99 -O2
TARGET  := md5_test.exe

# Include paths:
#   .                    -> finds our minimal "mbedtls_config.h"
#   ../../Middlewares/... -> finds "mbedtls/md5.h" and "mbedtls/platform_util.h"
INCLUDES := -I . -I ../../Middlewares/Third_Party/mbedTLS/include

# Tells all mbedTLS modules to use our minimal config instead of
# the default "mbedtls/config.h"
DEFINES  := -DMBEDTLS_CONFIG_FILE=\"mbedtls_config.h\"

# Only two source files: our test driver + the real mbedTLS md5.c
SRCS     := main.c ../../Middlewares/Third_Party/mbedTLS/library/md5.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS) mbedtls_config.h
	$(CC) $(CFLAGS) $(INCLUDES) $(DEFINES) -o $@ $(SRCS)

clean:
	rm -f $(TARGET)
