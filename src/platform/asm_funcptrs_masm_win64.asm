.data

funcptr MACRO name
  PUBLIC name
  name QWORD 0
ENDM

funcptr morton_encode_qwpos
funcptr morton_decode_qwpos

END