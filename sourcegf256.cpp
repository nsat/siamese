asm
// File: gf256_arm64.s
// Compile with: gcc -c gf256_arm64.s -o gf256_arm64.o
// Integrated via gf256.cpp using asm directives

.section .text
.align 4
.global gf256_encode_arm64

// void gf256_encode_arm64(uint8_t* data, uint8_t* parity, size_t size, uint8_t coeff)
// x0 = data, x1 = parity, x2 = size, w3 = coeff
gf256_encode_arm64:
    stp x29, x30, [sp, #-64]!
    mov x29, sp

    // Load entire 256-byte reduction table into 16 registers (v16-v31)
    adrp x4, reduction_table
    add x4, x4, :lo12:reduction_table
    ld1 {v16.16b-v19.16b}, [x4], #64
    ld1 {v20.16b-v23.16b}, [x4], #64
    ld1 {v24.16b-v27.16b}, [x4], #64
    ld1 {v28.16b-v31.16b}, [x4]

    // Broadcast coefficient to all lanes
    dup v0.16b, w3

    // Main loop processing 64 bytes/iteration
1:  subs x2, x2, #64
    b.lt 2f

    // Load 64 bytes of data
    ld1 {v1.16b-v4.16b}, [x0], #64

    // Process 4 vectors in parallel
    .rept 4
    {
        pmull v5.8h, v1.8b, v0.8b       // [0-7]
        pmull2 v6.8h, v1.16b, v0.16b    // [8-15]
        ushr v7.8h, v5.8h, #8
        ushr v8.8h, v6.8h, #8
        xtn v7.8b, v7.8h                // h_low
        xtn v8.8b, v8.8h                // h_high
        xtn v5.8b, v5.8h                // l_low
        xtn v6.8b, v6.8h                // l_high
        ins v7.d[1], v8.d[0]            // v7 = combined h
        ins v5.d[1], v6.d[0]            // v5 = combined l

        // Reduction table lookup
        ushr v9.16b, v7.16b, #6         // part_idx = h >> 6
        and v10.16b, v7.16b, #0x3f      // offset = h & 0x3f

        // 4-way table lookup
        tbl v11.16b, {v16.16b-v19.16b}, v10.16b
        tbl v12.16b, {v20.16b-v23.16b}, v10.16b
        tbl v13.16b, {v24.16b-v27.16b}, v10.16b
        tbl v14.16b, {v28.16b-v31.16b}, v10.16b

        // Blend results
        cmeq v15.16b, v9.16b, #0
        cmeq v16.16b, v9.16b, #1
        cmeq v17.16b, v9.16b, #2
        cmeq v18.16b, v9.16b, #3

        bsl v15.16b, v11.16b, v12.16b
        bsl v17.16b, v13.16b, v14.16b
        bsl v16.16b, v15.16b, v17.16b

        // Final XOR with low bytes
        eor v16.16b, v16.16b, v5.16b

        // Accumulate to parity
        ld1 {v17.16b}, [x1]
        eor v17.16b, v17.16b, v16.16b
        st1 {v17.16b}, [x1], #16
    }
    .endr

    b 1b

2:  // Handle tail (0-63 bytes)
    adds x2, x2, #64
    b.eq 3f

    // ... tail handling omitted for brevity ...

3:  ldp x29, x30, [sp], #64
    ret

.section .rodata
.align 4
reduction_table:
    .incbin "gf256_reduction.bin"
