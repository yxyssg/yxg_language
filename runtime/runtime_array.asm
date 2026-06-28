section .data
    const_1e6   dd  1000000.0
    const_0p5   dd  0.5

section .text
    global _array_to_string
    extern _AllocMemory

;------------------------------------------------------------------------------
; 内部辅助函数：无符号整数转字符串（未变）
uint_to_str:
    push ebp
    mov ebp, esp
    push ebx
    push edi

    mov eax, [ebp+8]      ; val
    mov edi, [ebp+12]     ; buf

    test eax, eax
    jnz .nonzero
    mov byte [edi], '0'
    inc edi
    mov eax, edi
    pop edi
    pop ebx
    pop ebp
    ret

.nonzero:
    mov ebx, 10
    xor ecx, ecx
.div_loop:
    xor edx, edx
    div ebx
    push dx
    inc ecx
    test eax, eax
    jnz .div_loop

.pop_loop:
    pop ax
    add al, '0'
    stosb
    loop .pop_loop

    mov eax, edi
    pop edi
    pop ebx
    pop ebp
    ret

;------------------------------------------------------------------------------
int_to_str:
    push ebp
    mov ebp, esp
    push edi

    mov eax, [ebp+8]
    mov edi, [ebp+12]

    cmp eax, 0
    jge .positive
    mov byte [edi], '-'
    inc edi
    neg eax

.positive:
    push edi
    push eax
    call uint_to_str
    add esp, 8
    pop edi
    pop ebp
    ret

;------------------------------------------------------------------------------
float_to_str:
    push ebp
    mov ebp, esp
    sub esp, 16
    push ebx
    push esi
    push edi

    mov edi, [ebp+12]
    fld dword [ebp+8]

    ftst
    fstsw ax
    sahf
    jae .non_negative
    mov byte [ebp-12], 1
    fabs
    jmp .abs_ok
.non_negative:
    mov byte [ebp-12], 0
    fabs
.abs_ok:
    fld st0
    fisttp dword [ebp-4]
    fisub dword [ebp-4]

    fld dword [const_1e6]
    fmulp st1, st0
    fld dword [const_0p5]
    faddp st1, st0
    fisttp dword [ebp-8]

    cmp dword [ebp-8], 1000000
    jl .no_carry
    sub dword [ebp-8], 1000000
    inc dword [ebp-4]
.no_carry:

    cmp byte [ebp-12], 1
    jne .skip_sign
    mov byte [edi], '-'
    inc edi
.skip_sign:

    push edi
    push dword [ebp-4]
    call uint_to_str
    add esp, 8
    mov edi, eax

    mov byte [edi], '.'
    inc edi

    mov eax, [ebp-8]
    mov ecx, 6
    push edi
    add edi, 6
    push ebx
    mov ebx, 10
.frac_loop:
    xor edx, edx
    div ebx
    add dl, '0'
    dec edi
    mov [edi], dl
    loop .frac_loop
    pop ebx
    pop edi
    add edi, 6

    mov eax, edi
    pop edi
    pop esi
    pop ebx
    mov esp, ebp
    pop ebp
    ret

;------------------------------------------------------------------------------
str_quote:
    push ebp
    mov ebp, esp
    push esi
    push edi

    mov esi, [ebp+8]
    mov edi, [ebp+12]

    mov byte [edi], '"'
    inc edi

    test esi, esi
    jnz .copy

    mov dword [edi], 0x6C6C756E   ; "null"
    add edi, 4
    jmp .end_quote

.copy:
    cld
.copy_loop:
    lodsb
    test al, al
    jz .end_quote
    stosb
    jmp .copy_loop

.end_quote:
    mov byte [edi], '"'
    inc edi
    mov eax, edi

    pop edi
    pop esi
    pop ebp
    ret

;------------------------------------------------------------------------------
bool_to_str:
    push ebp
    mov ebp, esp
    push edi

    mov eax, [ebp+8]
    mov edi, [ebp+12]

    test eax, eax
    jnz .true

    mov dword [edi], 0x736C6166   ; "fals"
    mov byte [edi+4], 'e'
    add edi, 5
    jmp .done

.true:
    mov dword [edi], 0x65757274
    add edi, 4

.done:
    mov eax, edi
    pop edi
    pop ebp
    ret

;------------------------------------------------------------------------------
_array_to_string:
    push ebp
    mov ebp, esp
    sub esp, 24
    push ebx
    push esi
    push edi

    mov esi, [ebp+8]      ; array ptr
    mov ecx, [ebp+12]     ; length
    mov ebx, [ebp+16]     ; type

    cmp ecx, 0
    jg .calc_length

    ; 空数组
    push 0
    push 3
    call _AllocMemory
    add esp, 8
    test eax, eax
    jz .return_null

    mov byte [eax], '['
    mov byte [eax+1], ']'
    mov byte [eax+2], 0
    jmp .done

.calc_length:
    mov dword [ebp-4], 1       ; 预留 '[' 和 null
    mov dword [ebp-16], 0      ; 循环计数器

    ; 临时缓冲区（仅用于 int/float/bool 的长度测量，64字节已足够）
    sub esp, 64
    mov edi, esp

.length_loop:
    mov edx, [ebp+12]
    cmp [ebp-16], edx
    jge .length_done

    cmp dword [ebp-16], 0
    je .no_sep
    add dword [ebp-4], 2       ; ", "
.no_sep:

    cmp dword [ebp+16], 0
    je .len_int
    cmp dword [ebp+16], 1
    je .len_float
    cmp dword [ebp+16], 2
    je .len_char
    cmp dword [ebp+16], 3
    je .len_string
    cmp dword [ebp+16], 4
    je .len_bool
    jmp .len_next

.len_int:
    push edi
    push dword [esi]
    call int_to_str
    add esp, 8
    sub eax, edi
    add [ebp-4], eax
    jmp .len_next

.len_float:
    push edi
    push dword [esi]
    call float_to_str
    add esp, 8
    sub eax, edi
    add [ebp-4], eax
    jmp .len_next

.len_char:
    add dword [ebp-4], 3
    jmp .len_next

.len_string:
    ; ===== 修正：直接计算长度，不写入临时缓冲区 =====
    mov eax, [esi]              ; 获取字符串指针
    test eax, eax
    jnz .len_str_notnull
    add dword [ebp-4], 6        ; "null" 加引号共6字符
    jmp .len_next
.len_str_notnull:
    mov edx, eax                ; edx = 字符串首地址
    xor ecx, ecx                ; ecx = 字符计数
.str_len_loop:
    cmp byte [edx+ecx], 0
    je .str_len_done
    inc ecx
    jmp .str_len_loop
.str_len_done:
    add ecx, 2                  ; 前后引号
    add [ebp-4], ecx
    ; ===== 修正结束 =====
    jmp .len_next

.len_bool:
    push edi
    push dword [esi]
    call bool_to_str
    add esp, 8
    sub eax, edi
    add [ebp-4], eax

.len_next:
    cmp dword [ebp+16], 2
    je .len_step1
    add esi, 4
    jmp .len_step_done
.len_step1:
    inc esi
.len_step_done:
    inc dword [ebp-16]
    jmp .length_loop

.length_done:
    add dword [ebp-4], 1        ; ']'
    add dword [ebp-4], 1        ; null
    add esp, 64                 ; 释放临时缓冲区

    ; 分配实际缓冲区
    push 0
    push dword [ebp-4]
    call _AllocMemory
    add esp, 8
    test eax, eax
    jz .return_null

    mov [ebp-8], eax
    mov edi, eax
    mov byte [edi], '['
    inc edi
    mov dword [ebp-16], 0
    mov esi, [ebp+8]

.fill_loop:
    mov ecx, [ebp+12]
    cmp [ebp-16], ecx
    jge .fill_done

    cmp dword [ebp-16], 0
    je .fill_no_sep
    mov byte [edi], ','
    mov byte [edi+1], ' '
    add edi, 2
.fill_no_sep:

    cmp dword [ebp+16], 0
    je .fill_int
    cmp dword [ebp+16], 1
    je .fill_float
    cmp dword [ebp+16], 2
    je .fill_char
    cmp dword [ebp+16], 3
    je .fill_string
    cmp dword [ebp+16], 4
    je .fill_bool
    jmp .fill_next

.fill_int:
    push edi
    push dword [esi]
    call int_to_str
    add esp, 8
    mov edi, eax
    jmp .fill_next

.fill_float:
    push edi
    push dword [esi]
    call float_to_str
    add esp, 8
    mov edi, eax
    jmp .fill_next

.fill_char:
    mov al, [esi]
    mov byte [edi], "'"
    mov [edi+1], al
    mov byte [edi+2], "'"
    add edi, 3
    jmp .fill_next

.fill_string:
    push edi
    push dword [esi]
    call str_quote
    add esp, 8
    mov edi, eax
    jmp .fill_next

.fill_bool:
    push edi
    push dword [esi]
    call bool_to_str
    add esp, 8
    mov edi, eax

.fill_next:
    cmp dword [ebp+16], 2
    je .fill_step1
    add esi, 4
    jmp .fill_step_done
.fill_step1:
    inc esi
.fill_step_done:
    inc dword [ebp-16]
    jmp .fill_loop

.fill_done:
    mov byte [edi], ']'
    inc edi
    mov byte [edi], 0

    mov eax, [ebp-8]
    jmp .done

.return_null:
    xor eax, eax

.done:
    pop edi
    pop esi
    pop ebx
    mov esp, ebp
    pop ebp
    ret