#
# Copyright(c) 2023
# SPDX-License-Identifier: BSD-3-Clause
#

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c90 -pedantic

HEADERS = \
	pmem_nvme_table.h

all: test_lookup

test_lookup: test_lookup.c $(HEADERS)
	$(CC) $(CFLAGS) -o test_lookup test_lookup.c

clean:
	rm -f test_lookup

.PHONY: all clean 