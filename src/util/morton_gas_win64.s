.text

# Windows x64 ABI:
#   Integer args : RCX, RDX, R8, R9
#   Volatile     : RAX, RCX, RDX, R8-R11, XMM0-XMM5

.globl morton_encode_qwpos_asm
.def morton_encode_qwpos_asm
    .scl 2
    .type 32
.endef
morton_encode_qwpos_asm:
  # Arguments:
  #   rcx = y,x  (struct qwpos)

  # Not required because pdep masks the top byte too
  #and     $0x00FFFFFF00FFFFFF, %rcx

  # Y coorcxnate
  xor     $0x00800000, %rcx
  mov     $0x0000555555555555, %rdx
  pdep    %rdx, %rcx, %r8

  shr     $32, %rcx

  # X coorcxnate
  xor     $0x00800000, %rcx
  shl     $1, %rdx
  pdep    %rdx, %rcx, %rax

  # Combine and return
  or      %r8, %rax
  ret

.globl morton_decode_qwpos_asm
.def morton_decode_qwpos_asm
    .scl 2
    .type 32
.endef
morton_decode_qwpos_asm:
  # Arguments:
  #   rcx = morton number

  # X coorcxnate
  mov     $0x0000555555555555, %rdx
  pext    %rdx, %rcx, %r8
  xor     $0x00800000, %r8

  # Y coorcxnate
  shl     $1, %rdx
  pext    %rdx, %rcx, %rcx
  xor     $0x00800000, %rcx

  # Sign extend 24-bit qw to 32-bit
  call    .sext24      # Sign extend Y coorcxnate
  mov     %r8d, %ecx   # X coorcxnate -> %edi
  mov     %eax, %r8d   # Save Y coorcxnate into %esi
  call    .sext24      # Sign extend X coorcxnate

  # Pack and return
  shl     $32, %r8
  or      %r8, %rax
  ret

.sext24:
  mov     %ecx, %eax
  mov     %ecx, %edx
  or      $0xFF000000, %edx
  testl   $0x800000, %eax
  cmovne  %edx, %eax
  ret

