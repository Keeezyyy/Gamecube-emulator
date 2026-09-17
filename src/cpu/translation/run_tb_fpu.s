.equ FPU_FPR_OFFSET, 8
.equ FPU_PS1_OFFSET, 264

.text
.p2align 2
.globl _run_tb_with_fpu
_run_tb_with_fpu:
    stp x29, x30, [sp, #-112]!
    mov x29, sp
    stp d8, d9, [sp, #16]
    stp d10, d11, [sp, #32]
    stp d12, d13, [sp, #48]
    stp d14, d15, [sp, #64]
    stp x19, x20, [sp, #80]
    str x1, [sp, #96]

    add x9, x1, #FPU_FPR_OFFSET
    add x10, x1, #FPU_PS1_OFFSET
    ldr d0, [x9], #8
    ld1 {v0.d}[1], [x10], #8
    ldr d1, [x9], #8
    ld1 {v1.d}[1], [x10], #8
    ldr d2, [x9], #8
    ld1 {v2.d}[1], [x10], #8
    ldr d3, [x9], #8
    ld1 {v3.d}[1], [x10], #8
    ldr d4, [x9], #8
    ld1 {v4.d}[1], [x10], #8
    ldr d5, [x9], #8
    ld1 {v5.d}[1], [x10], #8
    ldr d6, [x9], #8
    ld1 {v6.d}[1], [x10], #8
    ldr d7, [x9], #8
    ld1 {v7.d}[1], [x10], #8
    ldr d8, [x9], #8
    ld1 {v8.d}[1], [x10], #8
    ldr d9, [x9], #8
    ld1 {v9.d}[1], [x10], #8
    ldr d10, [x9], #8
    ld1 {v10.d}[1], [x10], #8
    ldr d11, [x9], #8
    ld1 {v11.d}[1], [x10], #8
    ldr d12, [x9], #8
    ld1 {v12.d}[1], [x10], #8
    ldr d13, [x9], #8
    ld1 {v13.d}[1], [x10], #8
    ldr d14, [x9], #8
    ld1 {v14.d}[1], [x10], #8
    ldr d15, [x9], #8
    ld1 {v15.d}[1], [x10], #8
    ldr d16, [x9], #8
    ld1 {v16.d}[1], [x10], #8
    ldr d17, [x9], #8
    ld1 {v17.d}[1], [x10], #8
    ldr d18, [x9], #8
    ld1 {v18.d}[1], [x10], #8
    ldr d19, [x9], #8
    ld1 {v19.d}[1], [x10], #8
    ldr d20, [x9], #8
    ld1 {v20.d}[1], [x10], #8
    ldr d21, [x9], #8
    ld1 {v21.d}[1], [x10], #8
    ldr d22, [x9], #8
    ld1 {v22.d}[1], [x10], #8
    ldr d23, [x9], #8
    ld1 {v23.d}[1], [x10], #8
    ldr d24, [x9], #8
    ld1 {v24.d}[1], [x10], #8
    ldr d25, [x9], #8
    ld1 {v25.d}[1], [x10], #8
    ldr d26, [x9], #8
    ld1 {v26.d}[1], [x10], #8
    ldr d27, [x9], #8
    ld1 {v27.d}[1], [x10], #8
    ldr d28, [x9], #8
    ld1 {v28.d}[1], [x10], #8
    ldr d29, [x9], #8
    ld1 {v29.d}[1], [x10], #8
    ldr d30, [x9], #8
    ld1 {v30.d}[1], [x10], #8
    ldr d31, [x9], #8
    ld1 {v31.d}[1], [x10], #8

    blr x0

    ldr x1, [sp, #96]
    add x9, x1, #FPU_FPR_OFFSET
    add x10, x1, #FPU_PS1_OFFSET
    str d0, [x9], #8
    st1 {v0.d}[1], [x10], #8
    str d1, [x9], #8
    st1 {v1.d}[1], [x10], #8
    str d2, [x9], #8
    st1 {v2.d}[1], [x10], #8
    str d3, [x9], #8
    st1 {v3.d}[1], [x10], #8
    str d4, [x9], #8
    st1 {v4.d}[1], [x10], #8
    str d5, [x9], #8
    st1 {v5.d}[1], [x10], #8
    str d6, [x9], #8
    st1 {v6.d}[1], [x10], #8
    str d7, [x9], #8
    st1 {v7.d}[1], [x10], #8
    str d8, [x9], #8
    st1 {v8.d}[1], [x10], #8
    str d9, [x9], #8
    st1 {v9.d}[1], [x10], #8
    str d10, [x9], #8
    st1 {v10.d}[1], [x10], #8
    str d11, [x9], #8
    st1 {v11.d}[1], [x10], #8
    str d12, [x9], #8
    st1 {v12.d}[1], [x10], #8
    str d13, [x9], #8
    st1 {v13.d}[1], [x10], #8
    str d14, [x9], #8
    st1 {v14.d}[1], [x10], #8
    str d15, [x9], #8
    st1 {v15.d}[1], [x10], #8
    str d16, [x9], #8
    st1 {v16.d}[1], [x10], #8
    str d17, [x9], #8
    st1 {v17.d}[1], [x10], #8
    str d18, [x9], #8
    st1 {v18.d}[1], [x10], #8
    str d19, [x9], #8
    st1 {v19.d}[1], [x10], #8
    str d20, [x9], #8
    st1 {v20.d}[1], [x10], #8
    str d21, [x9], #8
    st1 {v21.d}[1], [x10], #8
    str d22, [x9], #8
    st1 {v22.d}[1], [x10], #8
    str d23, [x9], #8
    st1 {v23.d}[1], [x10], #8
    str d24, [x9], #8
    st1 {v24.d}[1], [x10], #8
    str d25, [x9], #8
    st1 {v25.d}[1], [x10], #8
    str d26, [x9], #8
    st1 {v26.d}[1], [x10], #8
    str d27, [x9], #8
    st1 {v27.d}[1], [x10], #8
    str d28, [x9], #8
    st1 {v28.d}[1], [x10], #8
    str d29, [x9], #8
    st1 {v29.d}[1], [x10], #8
    str d30, [x9], #8
    st1 {v30.d}[1], [x10], #8
    str d31, [x9], #8
    st1 {v31.d}[1], [x10], #8

    ldp x19, x20, [sp, #80]
    ldp d14, d15, [sp, #64]
    ldp d12, d13, [sp, #48]
    ldp d10, d11, [sp, #32]
    ldp d8, d9, [sp, #16]
    ldp x29, x30, [sp], #112
    ret

.text
.p2align 2
.globl _move_from_to_scratch_regs
_move_from_to_scratch_regs:
  

    //x1 -> src
    //x0 -> dst
    mov x2, x0
    add x9, x1, #FPU_FPR_OFFSET
    add x10, x1, #FPU_PS1_OFFSET
    ldr d0, [x9], #8
    ld1 {v0.d}[1], [x10], #8
    ldr d1, [x9], #8
    ld1 {v1.d}[1], [x10], #8
    ldr d2, [x9], #8
    ld1 {v2.d}[1], [x10], #8
    ldr d3, [x9], #8
    ld1 {v3.d}[1], [x10], #8
    ldr d4, [x9], #8
    ld1 {v4.d}[1], [x10], #8
    ldr d5, [x9], #8
    ld1 {v5.d}[1], [x10], #8
    ldr d6, [x9], #8
    ld1 {v6.d}[1], [x10], #8
    ldr d7, [x9], #8
    ld1 {v7.d}[1], [x10], #8
    ldr d8, [x9], #8
    ld1 {v8.d}[1], [x10], #8
    ldr d9, [x9], #8
    ld1 {v9.d}[1], [x10], #8
    ldr d10, [x9], #8
    ld1 {v10.d}[1], [x10], #8
    ldr d11, [x9], #8
    ld1 {v11.d}[1], [x10], #8
    ldr d12, [x9], #8
    ld1 {v12.d}[1], [x10], #8
    ldr d13, [x9], #8
    ld1 {v13.d}[1], [x10], #8
    ldr d14, [x9], #8
    ld1 {v14.d}[1], [x10], #8
    ldr d15, [x9], #8
    ld1 {v15.d}[1], [x10], #8
    ldr d16, [x9], #8
    ld1 {v16.d}[1], [x10], #8
    ldr d17, [x9], #8
    ld1 {v17.d}[1], [x10], #8
    ldr d18, [x9], #8
    ld1 {v18.d}[1], [x10], #8
    ldr d19, [x9], #8
    ld1 {v19.d}[1], [x10], #8
    ldr d20, [x9], #8
    ld1 {v20.d}[1], [x10], #8
    ldr d21, [x9], #8
    ld1 {v21.d}[1], [x10], #8
    ldr d22, [x9], #8
    ld1 {v22.d}[1], [x10], #8
    ldr d23, [x9], #8
    ld1 {v23.d}[1], [x10], #8
    ldr d24, [x9], #8
    ld1 {v24.d}[1], [x10], #8
    ldr d25, [x9], #8
    ld1 {v25.d}[1], [x10], #8
    ldr d26, [x9], #8
    ld1 {v26.d}[1], [x10], #8
    ldr d27, [x9], #8
    ld1 {v27.d}[1], [x10], #8
    ldr d28, [x9], #8
    ld1 {v28.d}[1], [x10], #8
    ldr d29, [x9], #8
    ld1 {v29.d}[1], [x10], #8
    ldr d30, [x9], #8
    ld1 {v30.d}[1], [x10], #8
    ldr d31, [x9], #8
    ld1 {v31.d}[1], [x10], #8

    mov x1, x2
    add x9, x1, #FPU_FPR_OFFSET
    add x10, x1, #FPU_PS1_OFFSET


    str d0, [x9], #8
    st1 {v0.d}[1], [x10], #8
    str d1, [x9], #8
    st1 {v1.d}[1], [x10], #8
    str d2, [x9], #8
    st1 {v2.d}[1], [x10], #8
    str d3, [x9], #8
    st1 {v3.d}[1], [x10], #8
    str d4, [x9], #8
    st1 {v4.d}[1], [x10], #8
    str d5, [x9], #8
    st1 {v5.d}[1], [x10], #8
    str d6, [x9], #8
    st1 {v6.d}[1], [x10], #8
    str d7, [x9], #8
    st1 {v7.d}[1], [x10], #8
    str d8, [x9], #8
    st1 {v8.d}[1], [x10], #8
    str d9, [x9], #8
    st1 {v9.d}[1], [x10], #8
    str d10, [x9], #8
    st1 {v10.d}[1], [x10], #8
    str d11, [x9], #8
    st1 {v11.d}[1], [x10], #8
    str d12, [x9], #8
    st1 {v12.d}[1], [x10], #8
    str d13, [x9], #8
    st1 {v13.d}[1], [x10], #8
    str d14, [x9], #8
    st1 {v14.d}[1], [x10], #8
    str d15, [x9], #8
    st1 {v15.d}[1], [x10], #8
    str d16, [x9], #8
    st1 {v16.d}[1], [x10], #8
    str d17, [x9], #8
    st1 {v17.d}[1], [x10], #8
    str d18, [x9], #8
    st1 {v18.d}[1], [x10], #8
    str d19, [x9], #8
    st1 {v19.d}[1], [x10], #8
    str d20, [x9], #8
    st1 {v20.d}[1], [x10], #8
    str d21, [x9], #8
    st1 {v21.d}[1], [x10], #8
    str d22, [x9], #8
    st1 {v22.d}[1], [x10], #8
    str d23, [x9], #8
    st1 {v23.d}[1], [x10], #8
    str d24, [x9], #8
    st1 {v24.d}[1], [x10], #8
    str d25, [x9], #8
    st1 {v25.d}[1], [x10], #8
    str d26, [x9], #8
    st1 {v26.d}[1], [x10], #8
    str d27, [x9], #8
    st1 {v27.d}[1], [x10], #8
    str d28, [x9], #8
    st1 {v28.d}[1], [x10], #8
    str d29, [x9], #8
    st1 {v29.d}[1], [x10], #8
    str d30, [x9], #8
    st1 {v30.d}[1], [x10], #8
    str d31, [x9], #8
    st1 {v31.d}[1], [x10], #8

    ret



