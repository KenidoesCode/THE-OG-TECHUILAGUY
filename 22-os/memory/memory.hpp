#pragma once

#include <stdint.h>
#include <stddef.h>

extern "C" void memory_init();
extern "C" uintptr_t memory_alloc_page();
extern "C" void memory_free_page(uintptr_t address);
extern "C" uint32_t memory_used_pages();
extern "C" uint32_t memory_free_pages();
