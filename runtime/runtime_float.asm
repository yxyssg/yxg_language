; ============================================================
; runtime_float.asm — 浮点比较函数 (修正版)
; 调用约定: int _fXX(float a, float b)
;   参数位置: [esp+4] = a, [esp+8] = b
;   返回: eax = 0 或 1
; ============================================================

section .text

; ----------------------------------------
; 辅助宏: 清理 FPU 栈中的两个操作数
; ----------------------------------------
%macro fpu_pop2 0
    fstp st0
    fstp st0
%endmacro

; ==================== _fgt: a > b ====================
global _fgt
_fgt:
    fld  dword [esp+8]      ; st0 = b
    fld  dword [esp+4]      ; st0 = a, st1 = b
    fcomip st0, st1         ; 比较 a 和 b，弹出 a
    fstp st0                ; 弹出 b
    seta al                 ; CF=0 且 ZF=0 → a > b
    movzx eax, al
    ret

; ==================== _fge: a >= b ====================
global _fge
_fge:
    fld  dword [esp+8]      ; st0 = b
    fld  dword [esp+4]      ; st0 = a, st1 = b
    fcomip st0, st1
    fstp st0
    setae al                ; CF=0 → a >= b
    movzx eax, al
    ret

; ==================== _feq: a == b ====================
global _feq
_feq:
    fld  dword [esp+8]
    fld  dword [esp+4]
    fcomip st0, st1
    fstp st0
    sete al                 ; ZF=1 → a == b 且有序
    movzx eax, al
    ret

; ==================== _fne: a != b ====================
global _fne
_fne:
    fld  dword [esp+8]
    fld  dword [esp+4]
    fcomip st0, st1
    fstp st0
    setne al                ; ZF=0 或 PF=1 → a != b 或 NaN
    movzx eax, al
    ret

; ==================== _fle: a <= b ====================
global _fle
_fle:
    fld  dword [esp+8]      ; st0 = b
    fld  dword [esp+4]      ; st0 = a, st1 = b
    fcomip st0, st1
    fstp st0
    setbe al                ; CF=1 或 ZF=1 → a <= b
    movzx eax, al
    ret

; ==================== _flt: a < b ====================
global _flt
_flt:
    fld  dword [esp+8]      ; st0 = b
    fld  dword [esp+4]      ; st0 = a, st1 = b
    fcomip st0, st1
    fstp st0
    setb al                 ; CF=1 → a < b
    movzx eax, al
    ret

; ==================== _fmod_fpu: x % y ====================
global _fmod_fpu
_fmod_fpu:
    fld  dword [esp+8]      ; st0 = y
    fld  dword [esp+4]      ; st0 = x, st1 = y
.loop:
    fprem                   ; st0 = x % y (部分余数)
    fnstsw ax
    test ah, 04h            ; C2=1 表示还需继续
    jnz .loop
    fstp st1                ; 弹出 y，保留余数在 st0
    ret