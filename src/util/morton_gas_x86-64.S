.section .note.GNU-stack
.section .text

.globl morton_encode_qwpos_asm
.type morton_encode_qwpos_asm, @function
morton_encode_qwpos_asm:
  # Arguments:
  #   rdi = y,x  (struct qwpos)

  # Not required because pdep masks the top byte too
  #and     $0x00FFFFFF00FFFFFF, %rdi

  # Y coordinate
  xor     $0x00800000, %rdi
  mov     $0x0000555555555555, %rdx
  pdep    %rdx, %rdi, %rsi

  shr     $32, %rdi

  # X coordinate
  xor     $0x00800000, %rdi
  shl     $1, %rdx
  pdep    %rdx, %rdi, %rax

  # Combine and return
  or      %rsi, %rax
  ret

.globl morton_decode_qwpos_asm
.type morton_decode_qwpos_asm, @function
morton_decode_qwpos_asm:
  # Arguments:
  #   rdi = morton number

  # X coordinate
  mov     $0x0000555555555555, %rdx
  pext    %rdx, %rdi, %rsi
  xor     $0x00800000, %rsi

  # Y coordinate
  shl     $1, %rdx
  pext    %rdx, %rdi, %rdi
  xor     $0x00800000, %rdi

  # Sign extend 24-bit qw to 32-bit
  call    .sext24      # Sign extend Y coordinate
  mov     %esi, %edi   # X coordinate -> %edi
  mov     %eax, %esi   # Save Y coordinate into %esi
  call    .sext24      # Sign extend X coordinate

  # Pack and return
  shl     $32, %rsi
  or      %rsi, %rax
  ret

.sext24:
  mov     %edi, %eax
  mov     %edi, %edx
  or      $0xFF000000, %edx
  testl   $0x800000, %eax
  cmovne  %edx, %eax
  ret
