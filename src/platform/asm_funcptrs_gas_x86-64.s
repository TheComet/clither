.section .note.GNU-stack

.macro funcptr func_name
  .globl func_name
  func_name: .quad 0
.endm

.section .data
funcptr morton_encode_qwpos
funcptr morton_decode_qwpos
