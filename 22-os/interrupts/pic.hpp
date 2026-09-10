#pragma once

#include <stdint.h>

void pic_init();

extern "C" void pic_unmask_irq(uint8_t irq);
extern "C" void pic_send_eoi(uint8_t irq);
