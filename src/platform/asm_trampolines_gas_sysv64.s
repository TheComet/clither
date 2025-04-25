.section .note.GNU-stack

.macro trampoline func_name
  .globl \func_name
  .type \func_name, @function
  \func_name:
    nop
    nop
    nop
    nop
    nop
.endm

.section .text
trampoline morton_encode_qwpos
trampoline morton_decode_qwpos
