#include "rp2040_regs.h"

extern uint32_t __data_source;
extern uint32_t __data_start;
extern uint32_t __data_end;
extern uint32_t __bss_start;
extern uint32_t __bss_end;
extern uint32_t __StackTop;

void bare_main(void);
void bare_reset(void);

void bare_fault(void) {
    while (1) {
    }
}

void bare_nmi(void) __attribute__((weak, alias("bare_fault")));
void bare_hardfault(void) __attribute__((weak, alias("bare_fault")));
void bare_svc(void) __attribute__((weak, alias("bare_fault")));
void bare_pendsv(void) __attribute__((weak, alias("bare_fault")));
void bare_systick(void) __attribute__((weak, alias("bare_fault")));
void bare_irq0(void) __attribute__((weak, alias("bare_fault")));

__attribute__((section(".vectors"), used))
void (* const bare_vectors[])(void) = {
    (void (*)(void))&__StackTop,
    bare_reset,
    bare_nmi,
    bare_hardfault,
    bare_fault,
    bare_fault,
    bare_fault,
    0,
    0,
    0,
    0,
    bare_svc,
    bare_fault,
    0,
    bare_pendsv,
    bare_systick,
    bare_irq0,
};

void bare_reset(void) {
    uint32_t *src = &__data_source;
    uint32_t *dst = &__data_start;
    while (dst < &__data_end) {
        *dst++ = *src++;
    }

    dst = &__bss_start;
    while (dst < &__bss_end) {
        *dst++ = 0;
    }

    bare_main();
    while (1) {
    }
}