.code

trampoline MACRO name
  PUBLIC name
  name PROC
    nop
    nop
    nop
    nop
    nop
  name ENDP
ENDM

trampoline morton_encode_qwpos
trampoline morton_decode_qwpos

END
