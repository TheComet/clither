.code

; Windows x64 ABI:
;   Integer args : RCX, RDX, R8, R9
;   Volatile     : RAX, RCX, RDX, R8-R11, XMM0-XMM5

PUBLIC morton_encode_qwpos_asm
morton_encode_qwpos_asm PROC
  ; Arguments:
  ;   rcx = y,x  (struct qwpos)

  ; Y coordinate
  xor     rcx, 800000h
  mov     rdx, 555555555555h
  pdep    r8, rcx, rdx

  shr     rcx, 32

  ; X coordinate
  xor     rcx, 800000h
  shl     rdx, 1
  pdep    rax, rcx, rdx

  ; Combine and return
  or      rax, r8
  ret
morton_encode_qwpos_asm ENDP

PUBLIC morton_decode_qwpos_asm
morton_decode_qwpos_asm PROC
  ; Arguments:
  ;   rcx = morton number

  ; X coordinate
  mov     rdx, 555555555555h
  pext    r8, rcx, rdx
  xor     r8, 800000h

  ; Y coordinate
  shl     rdx, 1
  pext    rcx, rcx, rdx
  xor     rcx, 800000h

  ; Sign extend 24-bit qw to 32-bit
  call    sext24       ; Sign extend Y coordinate
  mov     ecx, r8d     ; X coordinate -> ecx
  mov     r8d, eax     ; Save Y coordinate into r8
  call    sext24       ; Sign extend X coordinate

  ; Pack and return
  shl     r8, 32
  or      rax, r8
  ret
morton_decode_qwpos_asm ENDP

sext24 PROC
  mov    eax, ecx
  mov    edx, ecx
  or     edx, 0FF000000h
  test   eax, 800000h
  cmovne eax, edx
  ret
sext24 ENDP

END