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
    cmp r0, r1
    bhs 6f
    mov r1, r0
    movs r0, #0
    pop {r4, pc}
6:
    movs r2, #0
    movs r3, #1
7:
    cmp r1, r0
    bhi 8f
    lsrs r4, r1, #31
    cmp r4, #0
    bne 8f
    lsls r1, r1, #1
    lsls r3, r3, #1
    b 7b
8:
    cmp r0, r1
    bcc 9f
    subs r0, r0, r1
    orrs r2, r3
9:
    lsrs r1, r1, #1
    lsrs r3, r3, #1
    bne 8b
    mov r1, r0
    mov r0, r2
    pop {r4, pc}

    .section .text.__aeabi_uidiv,"ax",%progbits
    .global __aeabi_uidiv
    .thumb_func
__aeabi_uidiv:
    push {r4, lr}
    cmp r1, #0
    bne 1f
    movs r0, #0
    subs r0, r0, #1
    pop {r4, pc}
1:
    cmp r0, r1
    bhs 6f
    movs r0, #0
    pop {r4, pc}
6:
    movs r2, #0
    movs r3, #1
7:
    cmp r1, r0
    bhi 8f
    lsrs r4, r1, #31
    cmp r4, #0
    bne 8f
    lsls r1, r1, #1
    lsls r3, r3, #1
    b 7b
8:
    cmp r0, r1
    bcc 9f
    subs r0, r0, r1
    orrs r2, r3
9:
    lsrs r1, r1, #1
    lsrs r3, r3, #1
    bne 8b
    mov r0, r2
    pop {r4, pc}

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
    push {r4, lr}
    movs r4, #0
    cmp r0, #0
    bge 1f
    rsbs r0, r0, #0
    movs r2, #1
    eors r4, r2
1:
    cmp r1, #0
    bge 2f
    rsbs r1, r1, #0
    movs r2, #1
    eors r4, r2
2:
    bl __aeabi_uidiv
    cmp r4, #0
    beq 3f
    rsbs r0, r0, #0
3:
    pop {r4, pc}

    .section .text.__aeabi_idiv0,"ax",%progbits
    .weak __aeabi_idiv0
    .thumb_func
__aeabi_idiv0:
    bx lr

    .weak __aeabi_ldiv0
    .thumb_set __aeabi_ldiv0, __aeabi_idiv0