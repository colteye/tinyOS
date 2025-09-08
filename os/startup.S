.global _Reset
_Reset:
    LDR r0, =stack_top
    MOV sp, r0
    BL main
1:  B 1b
