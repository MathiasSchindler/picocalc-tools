    .syntax unified
    .cpu cortex-m0plus
    .thumb

    .section .text.__aeabi_uidivmod,"ax",%progbits
    .global __aeabi_uidivmod
    .thumb_func
__aeabi_uidivmod:
    push {r4, lr}
    cmp r1, #0
    bne 1f
    mov r1, r0
    movs r0, #0
    subs r0, r0, #1
    pop {r4, pc}
1:
    movs r2, #0
    movs r3, #0
    movs r4, #32
2:
    lsls r0, r0, #1
    adcs r3, r3
    cmp r3, r1
    bcc 3f
    subs r3, r3, r1
    lsls r2, r2, #1
    adds r2, r2, #1
    b 4f
3:
    lsls r2, r2, #1
4:
    subs r4, r4, #1
    bne 2b
    mov r0, r2
    mov r1, r3
    pop {r4, pc}

    .section .text.__aeabi_uidiv,"ax",%progbits
    .global __aeabi_uidiv
    .thumb_func
__aeabi_uidiv:
    push {lr}
    bl __aeabi_uidivmod
    pop {pc}

    .section .text.__aeabi_idivmod,"ax",%progbits
    .global __aeabi_idivmod
    .thumb_func
__aeabi_idivmod:
    push {r4, r5, lr}
    movs r4, #0
    movs r5, #0
    cmp r0, #0
    bge 1f
    rsbs r0, r0, #0
    movs r5, #1
    movs r2, #1
    eors r4, r2
1:
    cmp r1, #0
    bge 2f
    rsbs r1, r1, #0
    movs r2, #1
    eors r4, r2
2:
    bl __aeabi_uidivmod
    cmp r4, #0
    beq 3f
    rsbs r0, r0, #0
3:
    cmp r5, #0
    beq 4f
    rsbs r1, r1, #0
4:
    pop {r4, r5, pc}

    .section .text.__aeabi_idiv,"ax",%progbits
    .global __aeabi_idiv
    .thumb_func
__aeabi_idiv:
    push {lr}
    bl __aeabi_idivmod
    pop {pc}

    .section .text.__aeabi_idiv0,"ax",%progbits
    .weak __aeabi_idiv0
    .thumb_func
__aeabi_idiv0:
    bx lr

    .weak __aeabi_ldiv0
    .thumb_set __aeabi_ldiv0, __aeabi_idiv0