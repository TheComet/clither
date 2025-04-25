.macro trampoline func_name
  .globl \func_name
  .def \func_name; .scl 2; .type 32; .endef
  \func_name:
    nop
    nop
    nop
    nop
    nop
.endm

.text
trampoline morton_encode_qwpos
trampoline morton_decode_qwpos

