[BITS 64]




global add_two
add_two:
    adr     x0, .
    add     w0, w0, w1
    ret
    .size   add_two, . - add_two
