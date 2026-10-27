/* ARM EABI unsigned division for size-optimized freestanding code.
 * Both ABI entries return quotient in r0, remainder in r1. All callers use
 * positive constant divisors. Zero is handled defensively without a trap.
 */
.syntax unified
.arm
.text
.global __aeabi_uidiv
.global __aeabi_uidivmod
__aeabi_uidiv:
__aeabi_uidivmod:
    cmp r1, #0
    moveq r0, #0
    bxeq lr
    clz r2, r1
    clz r3, r0
    subs r2, r2, r3
    mov r3, #0
    bmi 2f
    lsl r1, r1, r2
    mov ip, #1
    lsl ip, ip, r2
1:
    cmp r0, r1
    subcs r0, r0, r1
    orrcs r3, r3, ip
    lsrs ip, ip, #1
    lsr r1, r1, #1
    bne 1b
2:
    mov r1, r0
    mov r0, r3
    bx lr
