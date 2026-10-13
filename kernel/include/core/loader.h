#ifndef CORE_LOADER_H
#define CORE_LOADER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "minemu/boot.h"

#define USER_STACK_TOP UINT32_C(0x00800000)

/* Map the system ROM, then load the named module and its stack into the user
 * address space. Returns the module's entry VA, or 0 on failure. */
uint32_t user_load(const struct minemu_boot_info *boot_info, const char *name,
                   size_t name_length);

/* True if every page of [vaddr, vaddr + length) is mapped for user mode with
 * the given MINEMU_PTE_* permissions. A zero length is always valid. */
bool user_range_ok(uint32_t vaddr, uint32_t length, uint32_t pte_flags);

void minemu_enter_user(uint32_t entry_vaddr, uint32_t stack_top) __attribute__((noreturn));

#endif
