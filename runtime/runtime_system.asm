%pragma nasm warning disable label-redef-late

section .data
    _init   dd 0
    _argc   dd 0
    _argv   dd 0
    half    dd 0.5
    ten      dd 10.0
    one      dd 1.0
    temp_digit dd 0
    str_true  db "true",0
    str_false db "false",0
    int_min_str db "-2147483648",0
    _sys_argc      dd -1     
    _sys_argv_base dd 0    
    _sys_cmdline_w dd 0       
    _sys_empty_str db 0 

section .bss
    _heap   resd 1
    _start  resd 1
    _end    resd 1
    _index  resd 1

    out_buffer      resb 4096
    int_temp_buf    resb 32
    double_temp_int resd 1
    cw_old          resw 1
    cw_new          resw 1
    _input_buffer   resb 1024
    bytesRead       resd 1
    write_float_int_part resd 1


section .text
    global _GetCommandLineArgc
    global _GetCommandLineArgv

    extern _GetCommandLineA@0
    extern _GetProcessHeap@0
    extern _HeapAlloc@12
    extern _GetStdHandle@4
    extern _WriteConsoleA@20
    extern _ReadConsoleA@20
    extern _AllocMemory
    extern _FreeMemory
    extern _ExitProcess@4
    extern _Sleep@4
    extern _CloseHandle@4
    extern _GetTickCount@0
    extern _GetEnvironmentVariableA@12
    extern _GetExitCodeProcess@8
    extern _CreateProcessA@40
    extern _GetCommandLineW@0

STD_INPUT_HANDLE       equ -10
INVALID_HANDLE_VALUE   equ -1
CP_UTF8    equ 65001

extern _GetConsoleMode@8
extern _SetConsoleMode@8
extern _WaitForSingleObject@8
extern _FlushConsoleInputBuffer@4
extern _GetNumberOfConsoleInputEvents@8
extern _ReadConsoleInputA@16
extern _WideCharToMultiByte@32

%macro  GET_STDIN 1
    push    STD_INPUT_HANDLE
    call    _GetStdHandle@4
    cmp     eax, INVALID_HANDLE_VALUE
    je      %1
%endmacro

_init_cmdline:
    push ebx
    push esi
    push edi

    cmp dword [_init], 1
    je  .exit

    call _GetCommandLineA@0
    mov  esi, eax

    call _GetProcessHeap@0
    mov  [_heap], eax
    
    test eax, eax
    jz   .exit

    mov  edx, esi
    xor  ebx, ebx

.count_loop:
.cskip:
    cmp  byte [edx], ' '
    je   .cskip_inc
    cmp  byte [edx], 9
    jne  .cskip_end
.cskip_inc:
    inc  edx
    jmp  .cskip
.cskip_end:
    cmp  byte [edx], 0
    je   .count_done

    inc  ebx
    xor  eax, eax

.scan_arg:
    cmp  byte [edx], 0
    je   .count_loop
    cmp  byte [edx], '"'
    jne  .s_not_quote

    cmp  byte [edx+1], '"'
    jne  .s_toggle_quote
    add  edx, 2
    jmp  .scan_arg
.s_toggle_quote:
    xor  eax, 1
    jmp  .s_next

.s_not_quote:
    cmp  eax, 0
    jne  .s_next
    cmp  byte [edx], ' '
    je   .count_loop
    cmp  byte [edx], 9
    je   .count_loop
.s_next:
    inc  edx
    jmp  .scan_arg

.count_done:
    mov  [_argc], ebx

    mov  eax, ebx
    inc  eax
    shl  eax, 2
    push eax
    push 8
    push dword [_heap]
    call _HeapAlloc@12
    test eax, eax
    jz   .exit
    mov  [_argv], eax

    mov  edx, esi
    xor  ecx, ecx

.fill_loop:
    cmp  ecx, [_argc]
    jae  .fill_done

.fskip:
    cmp  byte [edx], ' '
    je   .fskip_inc
    cmp  byte [edx], 9
    jne  .fskip_end
.fskip_inc:
    inc  edx
    jmp  .fskip
.fskip_end:
    mov  [_start], edx
    mov  [_index], ecx

    xor  eax, eax
    mov  edx, [_start]
.find_end_loop:
    cmp  byte [edx], 0
    je   .found_end
    cmp  byte [edx], '"'
    jne  .fe_not_quote

    cmp  byte [edx+1], '"'
    jne  .fe_toggle_quote
    add  edx, 2
    jmp  .find_end_loop
.fe_toggle_quote:
    xor  eax, 1
    jmp  .fe_next

.fe_not_quote:
    cmp  eax, 0
    jne  .fe_next
    cmp  byte [edx], ' '
    je   .found_end
    cmp  byte [edx], 9
    je   .found_end
.fe_next:
    inc  edx
    jmp  .find_end_loop
.found_end:
    mov  [_end], edx

    mov  eax, edx
    sub  eax, [_start]
    inc  eax
    push eax
    push 8
    push dword [_heap]
    call _HeapAlloc@12
    test eax, eax
    jz   .exit

    mov  ecx, [_index]
    mov  edx, [_argv]
    mov  [edx + ecx*4], eax

    push ecx
    mov  esi, [_start]
    mov  edi, eax
    xor  ebx, ebx

.copy_loop:
    cmp  esi, [_end]
    jae  .copy_done
    mov  al, [esi]
    cmp  al, '"'
    jne  .c_not_quote
    cmp  byte [esi+1], '"'
    jne  .c_single_quote
    mov  byte [edi], '"'
    inc  edi
    add  esi, 2
    jmp  .copy_loop
.c_single_quote:
    xor  ebx, 1
    inc  esi
    jmp  .copy_loop
.c_not_quote:
    mov  [edi], al
    inc  edi
    inc  esi
    jmp  .copy_loop
.copy_done:
    mov  byte [edi], 0
    pop  ecx
    inc  ecx
    mov  edx, [_end]
    jmp  .fill_loop

.fill_done:
    mov  edx, [_argv]
    mov  eax, [_argc]
    mov  dword [edx + eax*4], 0
    mov  dword [_init], 1

.exit:
    pop  edi
    pop  esi
    pop  ebx
    ret

_GetCommandLineArgc:
    call _init_cmdline
    mov  eax, [_argc]
    ret

_GetCommandLineArgv:
    call _init_cmdline
    mov  eax, [_argv]
    ret


global _input_parse
global _ConsoleReadLine
_ConsoleReadLine:
    push ebp
    mov ebp, esp
    push ebx
    
    push -10
    call _GetStdHandle@4
    cmp eax, -1
    je .error
    
    push 0
    push bytesRead
    push 1024
    push _input_buffer
    push eax
    call _ReadConsoleA@20
    test eax, eax
    jz .error
    
    mov ecx, [bytesRead]
    cmp ecx, 0
    je .error
    cmp ecx, 2
    jb .done
    lea edx, [_input_buffer + ecx - 2]
    cmp word [edx], 0x0A0D
    jne .done
    mov word [edx], 0
.done:
    mov eax, _input_buffer
    pop ebx
    pop ebp
    ret
.error:
    xor eax, eax
    pop ebx
    pop ebp
    ret

_parse_int:
    push ebx
    push edx
    push ecx
    push esi              

    xor ebx, ebx          
    mov al, [esi]
    cmp al, '-'
    jne .not_minus
    inc esi
    mov ebx, 1
    jmp .start
.not_minus:
    cmp al, '+'
    jne .start
    inc esi

.start:
    xor eax, eax     
    xor edx, edx        

.read_loop:
    mov cl, [esi]
    cmp cl, '0'
    jb  .check_done
    cmp cl, '9'
    ja  .check_done

    sub cl, '0'
    movzx ecx, cl

    cmp eax, 214748364
    ja  .error
    jne .safe_mul
    cmp ebx, 1
    jne .pos_limit
    cmp ecx, 8
    ja  .error
    jmp .safe_mul
.pos_limit:
    cmp ecx, 7
    ja  .error

.safe_mul:
    imul eax, 10
    add eax, ecx
    inc esi
    mov edx, 1
    jmp .read_loop

.check_done:
    test edx, edx
    jz  .error           

    test ebx, ebx
    jz  .done
    neg eax

.done:
    clc
    mov [esp], esi
    pop esi               
    pop ecx
    pop edx
    pop ebx
    ret

.error:
    stc
    pop esi           
    pop ecx
    pop edx
    pop ebx
    ret

_str_to_float:
    push ebx
    push edx
    push ecx
    push esi
    
    xor ebx, ebx
    mov al, [esi]
    cmp al, '-'
    jne .not_neg
    mov ebx, 1
    inc esi
    jmp .after_sign
.not_neg:
    cmp al, '+'
    jne .after_sign
    inc esi
.after_sign:
    fninit
    fldz
    xor ecx, ecx
    mov al, [esi]
    cmp al, '.'
    je .parse_fraction
.parse_integer:
    mov al, [esi]
    cmp al, '0'
    jb .chk_dot
    cmp al, '9'
    ja .chk_dot
    sub al, '0'
    fld dword [ten]
    fmulp st1, st0
    mov [temp_digit], al
    fild dword [temp_digit]
    faddp st1, st0
    inc esi
    jmp .parse_integer
.chk_dot:
    cmp al, '.'
    jne .chk_exp
    inc esi
.parse_fraction:
    mov al, [esi]
    cmp al, '0'
    jb .chk_exp
    cmp al, '9'
    ja .chk_exp
    sub al, '0'
    inc ecx
    fld dword [ten]
    fmulp st1, st0
    mov [temp_digit], al
    fild dword [temp_digit]
    faddp st1, st0
    inc esi
    jmp .parse_fraction
.chk_exp:
    cmp ecx, 0
    je .no_frac
    push ecx
    fld1
.prep_div_loop:
    fld dword [ten]
    fmulp st1, st0
    loop .prep_div_loop
    fdivp st1, st0
    pop ecx
.no_frac:
    mov al, [esi]
    cmp al, 'e'
    je .read_exp
    cmp al, 'E'
    je .read_exp
    jmp .apply_sign
.read_exp:
    inc esi
    xor edi, edi
    mov al, [esi]
    cmp al, '-'
    jne .exp_pos
    mov edi, 1
    inc esi
    jmp .exp_digits
.exp_pos:
    cmp al, '+'
    jne .exp_digits
    inc esi
.exp_digits:
    xor eax, eax
    xor ecx, ecx
.exp_loop:
    cmp ecx, 4
    jae .exp_done
    mov dl, [esi]
    cmp dl, '0'
    jb .exp_done
    cmp dl, '9'
    ja .exp_done
    sub dl, '0'
    movzx edx, dl
    imul eax, 10
    add eax, edx
    inc esi
    inc ecx
    jmp .exp_loop
.exp_done:
    cmp edi, 1
    jne .exp_positive
    neg eax
.exp_positive:
    cmp eax, 0
    je .apply_sign
    mov ecx, eax
    cmp ecx, 0
    jg .mul_loop
    neg ecx
.div_loop:
    fld dword [ten]
    fdivp st1, st0
    loop .div_loop
    jmp .apply_sign
.mul_loop:
    fld dword [ten]
    fmulp st1, st0
    loop .mul_loop
.apply_sign:
    cmp ebx, 1
    jne .done
    fchs
.done:
    mov [esp], esi
    pop esi
    pop ecx
    pop edx
    pop ebx
    ret

_strnicmp:
    push esi
    push edi
    push ecx
    push ebx
    mov edi, edx
    cld
    cmp ecx, 0
    je .equal
.loop:
    mov al, [esi]
    mov ah, [edi]
    mov bl, al
    cmp bl, 'A'
    jb .skip1
    cmp bl, 'Z'
    ja .skip1
    or bl, 0x20
.skip1:
    mov bh, ah
    cmp bh, 'A'
    jb .skip2
    cmp bh, 'Z'
    ja .skip2
    or bh, 0x20
.skip2:
    cmp bl, bh
    jne .neq
    inc esi
    inc edi
    dec ecx
    jnz .loop
.equal:
    xor eax, eax
    pop ebx
    pop ecx
    pop edi
    pop esi
    ret
.neq:
    mov eax, 1
    pop ebx
    pop ecx
    pop edi
    pop esi
    ret

_input_parse:
    push ebp
    mov ebp, esp
    sub esp, 8
    push ebx
    push esi
    push edi

    call _ConsoleReadLine
    test eax, eax
    jz .error_exit
    mov esi, eax
    mov edi, [ebp+8]
    xor ebx, ebx

.parse_loop:
    mov al, [edi]
    test al, al
    jnz .template_not_end
    cmp byte [esi], 0
    jne .format_error
    jmp .success
.template_not_end:
    cmp al, '{'
    jne .normal_char
    cmp byte [edi+1], '}'
    jne .normal_char
    cmp ebx, [ebp+12]
    jae .format_error
    add edi, 2

    mov eax, [ebp+16]
    mov eax, [eax + ebx*4]
    mov edx, [ebp+20]
    mov edx, [edx + ebx*4]

    test edx, edx
    jz .format_error

    cmp eax, 0
    je .parse_int
    cmp eax, 1
    je .parse_float
    cmp eax, 2
    je .parse_char
    cmp eax, 3
    je .parse_string
    cmp eax, 4
    je .parse_bool
    jmp .format_error

.parse_int:
    call _parse_int
    jc .format_error
    mov [edx], eax
    jmp .next_var

.parse_float:
    call _str_to_float
    fstp dword [edx]
    jmp .next_var

.parse_char:
    mov al, [esi]
    test al, al
    jz .format_error
    mov [edx], al
    inc esi
    jmp .next_var

.parse_string:
    mov [ebp-4], edx
    movzx eax, byte [edi]
    mov [ebp-8], eax
    test eax, eax
    jz .str_to_end

    push esi
    xor ecx, ecx
    mov edx, [ebp-8]
.str_len_delim:
    cmp byte [esi], 0
    je .str_len_done_delim
    cmp byte [esi], dl
    je .str_len_done_delim
    inc esi
    inc ecx
    cmp ecx, 255
    jb .str_len_delim
.str_len_done_delim:
    pop esi

    push ecx
    mov eax, ecx
    inc eax
    push eax
    push 0
    call _AllocMemory
    add esp, 8
    pop ecx
    test eax, eax
    jz .format_error

    mov edx, [ebp-4]
    mov [edx], eax

    push edi
    mov edi, eax
    xor ecx, ecx
    mov edx, [ebp-8]
.copy_loop_delim:
    cmp ecx, 255
    jae .copy_done_delim
    mov al, [esi]
    test al, al
    jz .copy_done_delim
    cmp al, dl
    je .copy_done_delim
    mov [edi], al
    inc esi
    inc edi
    inc ecx
    jmp .copy_loop_delim
.copy_done_delim:
    mov byte [edi], 0
    pop edi
    jmp .next_var

.str_to_end:
    push esi
    xor ecx, ecx
.str_len_end:
    cmp byte [esi], 0
    je .str_len_done_end
    inc esi
    inc ecx
    cmp ecx, 255
    jb .str_len_end
.str_len_done_end:
    pop esi

    push ecx
    mov eax, ecx
    inc eax
    push eax
    push 0
    call _AllocMemory
    add esp, 8
    pop ecx
    test eax, eax
    jz .format_error

    mov edx, [ebp-4]
    mov [edx], eax

    push edi
    mov edi, eax
    xor ecx, ecx
.copy_loop_end:
    cmp ecx, 255
    jae .copy_done_end
    mov al, [esi]
    test al, al
    jz .copy_done_end
    mov [edi], al
    inc esi
    inc edi
    inc ecx
    jmp .copy_loop_end
.copy_done_end:
    mov byte [edi], 0
    pop edi
    jmp .next_var

.parse_bool:
    push esi
    call _parse_int
    pop ecx
    jc .try_text
    mov byte [edx], 1
    cmp eax, 0
    jne .next_var
    mov byte [edx], 0
    jmp .next_var
.try_text:
    mov esi, ecx
    push edx
    mov ecx, 4
    mov edx, str_true
    call _strnicmp
    pop edx
    test eax, eax
    jz .is_true
    push edx
    mov ecx, 5
    mov edx, str_false
    call _strnicmp
    pop edx
    test eax, eax
    jz .is_false
    jmp .format_error
.is_true:
    mov byte [edx], 1
    add esi, 4
    jmp .next_var
.is_false:
    mov byte [edx], 0
    add esi, 5
    jmp .next_var

.next_var:
    inc ebx
    jmp .parse_loop

.normal_char:
    mov al, [edi]
    cmp al, [esi]
    jne .format_error
    inc esi
    inc edi
    jmp .parse_loop

.format_error:
.error_exit:
    xor eax, eax
    jmp .exit
.success:
    mov eax, 1
.exit:
    add esp, 8
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

write_char:
    mov  byte [edi], al
    inc  edi
    mov  eax, 1
    ret

write_str:
    push ebx
    push ecx
    xor  ebx, ebx
.loop_str:
    cmp  ecx, 1
    jbe  .overflow_str
    lodsb
    test al, al
    jz   .done_str
    stosb
    inc  ebx
    dec  ecx
    jmp  .loop_str
.done_str:
    mov  eax, ebx
    clc
    pop  ecx
    pop  ebx
    ret
.overflow_str:
    xor  eax, eax
    stc
    pop  ecx
    pop  ebx
    ret

write_bool:
    push ecx
    test eax, eax
    jz   .false_bool
    push esi
    mov  esi, str_true
    mov  ecx, 4
    rep  movsb
    pop  esi
    mov  eax, 4
    pop  ecx
    ret
.false_bool:
    push esi
    mov  esi, str_false
    mov  ecx, 5
    rep  movsb
    pop  esi
    mov  eax, 5
    pop  ecx
    ret

write_int_default:
    push ebx
    push edx
    push esi
    push ecx

    mov  ebx, edi

    cmp  eax, 0x80000000
    jne  .not_int_min_wid

    mov  esi, int_min_str
    mov  ecx, 11
    rep  movsb
    jmp  .calc_len_wid

.not_int_min_wid:
    test eax, eax
    jns  .positive_wid
    mov  byte [edi], '-'
    inc  edi
    neg  eax

.positive_wid:
    mov  ecx, 10
    push 0
.convert_loop_wid:
    xor  edx, edx
    div  ecx
    add  dl, '0'
    push edx
    test eax, eax
    jnz  .convert_loop_wid

.pop_loop_wid:
    pop  eax
    test eax, eax
    jz   .pop_done_wid
    mov  [edi], al
    inc  edi
    jmp  .pop_loop_wid
.pop_done_wid:

.calc_len_wid:
    pop  ecx
    mov  eax, edi
    sub  eax, ebx
    pop  esi
    pop  edx
    pop  ebx
    ret

write_int_padded:
    push ebp
    mov  ebp, esp
    push ebx
    push edx
    push esi
    push ecx

    mov  ebx, edi

    cmp  eax, 0x80000000
    jne  .not_min_wip

    mov  esi, int_min_str
    mov  ecx, 11
    mov  edx, [ebp+8]
    cmp  edx, ecx
    jae  .min_wide_wip
    mov  ecx, edx
.min_wide_wip:
    mov  eax, edx
    sub  eax, ecx
    jle  .min_no_pad_wip
    push ecx
    mov  ecx, eax
    mov  al, '0'
    rep  stosb
    pop  ecx
.min_no_pad_wip:
    rep  movsb
    jmp  .done_wip

.not_min_wip:
    xor  edx, edx
    test eax, eax
    jns  .no_sign_wip
    inc  edx
    neg  eax

.no_sign_wip:
    push edx
    mov  esi, int_temp_buf + 30
    mov  byte [esi], 0
    dec  esi
    mov  ecx, 10
.conv_loop_wip:
    xor  edx, edx
    div  ecx
    add  dl, '0'
    mov  [esi], dl
    dec  esi
    test eax, eax
    jnz  .conv_loop_wip
    inc  esi

    mov  eax, int_temp_buf + 30
    sub  eax, esi
    pop  edx
    mov  ecx, eax
    add  ecx, edx

    mov  eax, [ebp+8]
    cmp  eax, ecx
    jae  .need_pad_wip
    mov  eax, ecx

.need_pad_wip:
    test edx, edx
    jz   .pad_zeros_wip
    mov  byte [edi], '-'
    inc  edi
    dec  eax

.pad_zeros_wip:
    mov  edx, int_temp_buf + 30
    sub  edx, esi
    sub  eax, edx
    jle  .no_pad_wip
    mov  ecx, eax
    mov  al, '0'
    rep  stosb

.no_pad_wip:
    mov  ecx, edx
    rep  movsb

.done_wip:
    pop  ecx
    mov  eax, edi
    sub  eax, ebx
    pop  esi
    pop  edx
    pop  ebx
    pop  ebp
    ret

write_fraction_padded:
    push ebx
    push edx
    push esi
    push ecx

    mov  ebx, edi
    mov  edx, ecx
    test edx, edx
    jz   .done_wfp

    mov  dword [double_temp_int], ecx
    mov  ecx, edx
    mov  esi, 10
.push_loop_wfp:
    xor  edx, edx
    div  esi
    add  dl, '0'
    push edx
    loop .push_loop_wfp
    mov  ecx, dword [double_temp_int]
.pop_loop_wfp:
    pop  eax
    stosb
    loop .pop_loop_wfp

.done_wfp:
    pop  ecx
    mov  eax, edi
    sub  eax, ebx
    pop  esi
    pop  edx
    pop  ebx
    ret

write_float_single:
    push ebx
    push edx
    push esi
    push ecx

    mov  ebx, edx
    mov  esi, eax
    push edi

    fld  dword [esi]
    fstcw [cw_old]
    mov  ax, [cw_old]
    and  ax, 0xF3FF
    or   ax, 0x0C00
    mov  [cw_new], ax
    fldcw [cw_new]

    ftst
    fstsw ax
    sahf
    jae  .pos_wfs
    mov  byte [edi], '-'
    inc  edi
    fchs
.pos_wfs:
    cmp  ebx, 9
    jbe  .prec_ok
    mov  ebx, 9
.prec_ok:
    fld  st0
    frndint
    fist dword [double_temp_int]
    fsub st1, st0
    fstp st0

    push dword [double_temp_int]

    test ebx, ebx
    jz   .no_frac

    push ebx
    mov  eax, ebx
    mov  ecx, 1
.pow_loop:
    imul ecx, ecx, 10
    dec  eax
    jnz  .pow_loop

    push ecx
    fild dword [esp]
    fmulp st1, st0
    fld  dword [half]
    faddp st1, st0
    fistp dword [double_temp_int]
    pop  ecx
    pop  ebx

    mov  eax, [double_temp_int]
    cmp  eax, ecx
    jb   .no_carry
    pop  eax
    inc  eax
    push eax
    mov  dword [double_temp_int], 0
    jmp  .carry_done
.no_carry:
    pop  eax
    push eax
.carry_done:

    push ecx
    mov  eax, [esp+4]
    call write_int_default
    pop  ecx
    cmp  eax, ecx
    ja   .error_wfs
    sub  ecx, eax

    mov  byte [edi], '.'
    inc  edi
    dec  ecx
    jle  .error_wfs

    mov  eax, [double_temp_int]
    mov  ecx, ebx
    call write_fraction_padded
    add  esp, 4
    jmp  .done_wfs

.no_frac:
    fstp st0
    pop  eax
    push ecx
    call write_int_default
    pop  ecx
    cmp  eax, ecx
    ja   .error_wfs
    sub  ecx, eax
    jmp  .done_wfs

.done_wfs:
    fldcw [cw_old]
    pop  eax
    mov  edx, edi
    sub  edx, eax
    mov  eax, edx
    pop  ecx
    pop  esi
    pop  edx
    pop  ebx
    ret

.error_wfs:
    fstp st0
    fldcw [cw_old]
    pop  eax
    xor  eax, eax
    pop  ecx
    pop  esi
    pop  edx
    pop  ebx
    ret
    
_itoa_base:
    push edi
    push ecx
    lea  edi, [int_temp_buf + 31]
    mov  byte [edi], 0
    dec  edi
.next_digit:
    xor  edx, edx
    div  ecx
    cmp  dl, 10
    jb   .digit
    add  dl, 'a'-10
.digit:
    add  dl, '0'
    mov  [edi], dl
    dec  edi
    test eax, eax
    jnz  .next_digit
    inc  edi
    mov  esi, edi
    lea  edx, [int_temp_buf + 31]
    sub  edx, edi
    pop  ecx
    pop  edi
    ret

write_hex:
    push ebx
    push ecx
    push edx
    push esi
    push ecx
    mov  ebx, edi
    mov  ecx, 16
    call _itoa_base
    mov  ecx, edx
    rep  movsb
    mov  eax, edi
    sub  eax, ebx
    pop  ecx
    pop  esi
    pop  edx
    pop  ecx
    pop  ebx
    ret

write_hex_padded:
    push ebp
    mov  ebp, esp
    push ebx
    push ecx
    push edx
    push esi

    mov  ebx, edi
    mov  ecx, 16
    call _itoa_base
    mov  eax, [ebp+8]
    cmp  eax, edx
    jbe  .no_pad_hex
    mov  ecx, eax
    sub  ecx, edx
    mov  al, '0'
    rep  stosb
.no_pad_hex:
    mov  ecx, edx
    rep  movsb
    mov  eax, edi
    sub  eax, ebx
    pop  esi
    pop  edx
    pop  ecx
    pop  ebx
    pop  ebp
    ret

write_HEX:
    call write_hex
    push ecx
    push edi
    sub  edi, eax
    mov  ecx, eax
.upcase_loop:
    cmp  byte [edi], 'a'
    jb   .next
    sub  byte [edi], 0x20
.next:
    inc  edi
    loop .upcase_loop
    pop  edi
    pop  ecx
    ret

write_HEX_padded:
    push ebp
    mov  ebp, esp
    push ebx
    push ecx
    push edx
    push esi

    mov  ebx, edi
    mov  ecx, 16
    call _itoa_base
    mov  eax, [ebp+8]
    cmp  eax, edx
    jbe  .no_pad_hex
    mov  ecx, eax
    sub  ecx, edx
    mov  al, '0'
    rep  stosb
.no_pad_hex:
    mov  ecx, edx
    rep  movsb

    mov  eax, edi
    sub  eax, ebx
    sub  edi, eax
    mov  ecx, eax
.upcase_padded:
    cmp  byte [edi], 'a'
    jb   .next2
    sub  byte [edi], 0x20
.next2:
    inc  edi
    loop .upcase_padded

    add  edi, eax
    mov  eax, edi
    sub  eax, ebx
    pop  esi
    pop  edx
    pop  ecx
    pop  ebx
    pop  ebp
    ret

write_oct:
    push ebx
    push ecx
    push edx
    push esi
    push ecx
    mov  ebx, edi
    mov  ecx, 8
    call _itoa_base
    mov  ecx, edx
    rep  movsb
    mov  eax, edi
    sub  eax, ebx
    pop  ecx
    pop  esi
    pop  edx
    pop  ecx
    pop  ebx
    ret

write_oct_padded:
    push ebp
    mov  ebp, esp
    push ebx
    push ecx
    push edx
    push esi

    mov  ebx, edi
    mov  ecx, 8
    call _itoa_base
    mov  eax, [ebp+8]
    cmp  eax, edx
    jbe  .no_pad_oct
    mov  ecx, eax
    sub  ecx, edx
    mov  al, '0'
    rep  stosb
.no_pad_oct:
    mov  ecx, edx
    rep  movsb
    mov  eax, edi
    sub  eax, ebx
    pop  esi
    pop  edx
    pop  ecx
    pop  ebx
    pop  ebp
    ret

write_bin:
    push ebx
    push ecx
    push edx
    push esi
    push ecx
    mov  ebx, edi
    mov  ecx, 2
    call _itoa_base
    mov  ecx, edx
    rep  movsb
    mov  eax, edi
    sub  eax, ebx
    pop  ecx
    pop  esi
    pop  edx
    pop  ecx
    pop  ebx
    ret

write_bin_padded:
    push ebp
    mov  ebp, esp
    push ebx
    push ecx
    push edx
    push esi

    mov  ebx, edi
    mov  ecx, 2
    call _itoa_base
    mov  eax, [ebp+8]
    cmp  eax, edx
    jbe  .no_pad_bin
    mov  ecx, eax
    sub  ecx, edx
    mov  al, '0'
    rep  stosb
.no_pad_bin:
    mov  ecx, edx
    rep  movsb
    mov  eax, edi
    sub  eax, ebx
    pop  esi
    pop  edx
    pop  ecx
    pop  ebx
    pop  ebp
    ret

global _format_string
_format_string:
    push ebp
    mov  ebp, esp
    sub  esp, 16
    
    push esi
    push edi
    push ebx
    cld

    mov  eax, [ebp+12]
    mov  [ebp-4], eax
    mov  eax, [ebp+16]
    mov  [ebp-8], eax
    mov  dword [ebp-12], 0
    mov  dword [ebp-16], 20

    mov  esi, [ebp+8]
    test esi, esi
    jz   .error_fs
    mov  edi, out_buffer
    mov  ecx, 4096

.parse_loop_fs:
    cmp  ecx, 1
    jbe  .error_fs
    lodsb
    test al, al
    jz   .finish_fs

    cmp  al, '}'
    jne  .not_close_fs
    cmp  byte [esi], '}'
    jne  .copy_char_fs
    mov  byte [edi], '}'
    inc  edi
    dec  ecx
    inc  esi
    jmp  .parse_loop_fs

.not_close_fs:
    cmp  al, '{'
    je   .handle_brace_fs

.copy_char_fs:
    stosb
    dec  ecx
    jmp  .parse_loop_fs

.handle_brace_fs:
    cmp  byte [esi], '{'
    jne  .check_empty_fs
    mov  byte [edi], '{'
    inc  edi
    dec  ecx
    inc  esi
    jmp  .parse_loop_fs

.check_empty_fs:
    cmp  byte [esi], '}'
    jne  .check_format_fs
    inc  esi
    
    mov  eax, [ebp-12]
    cmp  eax, [ebp-4]
    jae  .error_fs
    
    mov  ebx, [ebp-8]
    mov  ebx, [ebx + eax*4]
    
    mov  edx, [ebp-16]
    add  edx, ebp
    mov  eax, [edx]
    
    inc  dword [ebp-12]
    add  dword [ebp-16], 4

    cmp  ebx, 0
    je   .do_int_fs
    cmp  ebx, 1
    je   .do_float_fs
    cmp  ebx, 2
    je   .do_char_fs
    cmp  ebx, 3
    je   .do_string_fs
    cmp  ebx, 4
    je   .do_bool_fs
    jmp  .error_fs

.do_int_fs:
    mov  eax, [eax]
    push ecx
    call write_int_default
    pop  ecx
    cmp  eax, ecx
    ja   .error_fs
    sub  ecx, eax
    jmp  .parse_loop_fs

.do_float_fs:
    push ecx
    push esi
    mov  esi, [eax]
    mov  edx, 6
    call write_float_single
    pop  esi
    pop  ecx
    cmp  eax, ecx
    ja   .error_fs
    sub  ecx, eax
    jmp  .parse_loop_fs

.do_char_fs:
    mov  al, [eax]
    push ecx
    call write_char
    pop  ecx
    cmp  eax, ecx
    ja   .error_fs
    sub  ecx, eax
    jmp  .parse_loop_fs

.do_string_fs:
    push ecx
    push esi
    mov  esi, [eax]
    call write_str
    pop  esi
    pop  ecx
    jc   .error_fs
    cmp  eax, ecx
    ja   .error_fs
    sub  ecx, eax
    jmp  .parse_loop_fs

.do_bool_fs:
    movzx eax, byte [eax]
    push ecx
    call write_bool
    pop  ecx
    cmp  eax, ecx
    ja   .error_fs
    sub  ecx, eax
    jmp  .parse_loop_fs

.check_format_fs:
    cmp  byte [esi], ':'
    jne  .copy_current_brace_fs
    inc  esi
    cmp  byte [esi], 'd'
    je   .do_int_fs_from_fmt
    cmp  byte [esi], 'x'
    je   .do_hex_fs
    cmp  byte [esi], 'X'
    je   .do_HEX_fs
    cmp  byte [esi], 'o'
    je   .do_oct_fs
    cmp  byte [esi], 'b'
    je   .do_bin_fs
    cmp  byte [esi], '.'
    je   .float_fmt_fs
    cmp  byte [esi], '0'
    je   .int_fmt_fs
    jmp  .skip_braces_fs

.do_int_fs_from_fmt:
    inc  esi
    cmp  byte [esi], '}'
    jne  .skip_braces_fs
    inc  esi
    mov  eax, [ebp-12]
    cmp  eax, [ebp-4]
    jae  .error_fs
    mov  ebx, [ebp-8]
    mov  ebx, [ebx + eax*4]
    mov  edx, [ebp-16]
    add  edx, ebp
    mov  eax, [edx]
    inc  dword [ebp-12]
    add  dword [ebp-16], 4
    cmp  ebx, 0
    je   .int_ok
    cmp  ebx, 4
    jne  .error_fs
.int_ok:
    mov  eax, [eax]
    push ecx
    call write_int_default
    pop  ecx
    cmp  eax, ecx
    ja   .error_fs
    sub  ecx, eax
    jmp  .parse_loop_fs

.do_hex_fs:
    inc  esi
    cmp  byte [esi], '}'
    jne  .skip_braces_fs
    inc  esi
    mov  eax, [ebp-12]
    cmp  eax, [ebp-4]
    jae  .error_fs
    mov  ebx, [ebp-8]
    mov  ebx, [ebx + eax*4]
    mov  edx, [ebp-16]
    add  edx, ebp
    mov  eax, [edx]
    inc  dword [ebp-12]
    add  dword [ebp-16], 4
    cmp  ebx, 0
    jne  .error_fs
    mov  eax, [eax]
    push ecx
    call write_hex
    pop  ecx
    cmp  eax, ecx
    ja   .error_fs
    sub  ecx, eax
    jmp  .parse_loop_fs

.do_HEX_fs:
    inc  esi
    cmp  byte [esi], '}'
    jne  .skip_braces_fs
    inc  esi
    mov  eax, [ebp-12]
    cmp  eax, [ebp-4]
    jae  .error_fs
    mov  ebx, [ebp-8]
    mov  ebx, [ebx + eax*4]
    mov  edx, [ebp-16]
    add  edx, ebp
    mov  eax, [edx]
    inc  dword [ebp-12]
    add  dword [ebp-16], 4
    cmp  ebx, 0
    jne  .error_fs
    mov  eax, [eax]
    push ecx
    call write_HEX
    pop  ecx
    cmp  eax, ecx
    ja   .error_fs
    sub  ecx, eax
    jmp  .parse_loop_fs

.do_oct_fs:
    inc  esi
    cmp  byte [esi], '}'
    jne  .skip_braces_fs
    inc  esi
    mov  eax, [ebp-12]
    cmp  eax, [ebp-4]
    jae  .error_fs
    mov  ebx, [ebp-8]
    mov  ebx, [ebx + eax*4]
    mov  edx, [ebp-16]
    add  edx, ebp
    mov  eax, [edx]
    inc  dword [ebp-12]
    add  dword [ebp-16], 4
    cmp  ebx, 0
    jne  .error_fs
    mov  eax, [eax]
    push ecx
    call write_oct
    pop  ecx
    cmp  eax, ecx
    ja   .error_fs
    sub  ecx, eax
    jmp  .parse_loop_fs

.do_bin_fs:
    inc  esi
    cmp  byte [esi], '}'
    jne  .skip_braces_fs
    inc  esi
    mov  eax, [ebp-12]
    cmp  eax, [ebp-4]
    jae  .error_fs
    mov  ebx, [ebp-8]
    mov  ebx, [ebx + eax*4]
    mov  edx, [ebp-16]
    add  edx, ebp
    mov  eax, [edx]
    inc  dword [ebp-12]
    add  dword [ebp-16], 4
    cmp  ebx, 0
    jne  .error_fs
    mov  eax, [eax]
    push ecx
    call write_bin
    pop  ecx
    cmp  eax, ecx
    ja   .error_fs
    sub  ecx, eax
    jmp  .parse_loop_fs

.float_fmt_fs:
    inc  esi
    xor  eax, eax
.parse_prec_fs:
    cmp  byte [esi], '0'
    jb   .prec_done_fs
    cmp  byte [esi], '9'
    ja   .prec_done_fs
    movzx edx, byte [esi]
    sub  edx, '0'
    imul eax, 10
    add  eax, edx
    inc  esi
    jmp  .parse_prec_fs
.prec_done_fs:
    cmp  byte [esi], 'f'
    jne  .skip_braces_fs
    inc  esi
    cmp  byte [esi], '}'
    jne  .skip_braces_fs
    inc  esi

    push eax
    
    mov  eax, [ebp-12]
    cmp  eax, [ebp-4]
    jae  .error_fs_cleanup
    
    mov  ebx, [ebp-8]
    mov  ebx, [ebx + eax*4]
    
    mov  edx, [ebp-16]
    add  edx, ebp
    mov  eax, [edx]
    
    inc  dword [ebp-12]
    add  dword [ebp-16], 4

    cmp  ebx, 1
    jne  .error_fs_cleanup

    pop  edx
    push ecx
    push esi
    mov  esi, [eax]
    call write_float_single
    pop  esi
    pop  ecx
    cmp  eax, ecx
    ja   .error_fs
    sub  ecx, eax
    jmp  .parse_loop_fs

.error_fs_cleanup:
    pop  eax
    jmp  .error_fs

.int_fmt_fs:
    inc  esi
    xor  edx, edx
.parse_width_fs:
    cmp  byte [esi], '0'
    jb   .width_done_fs
    cmp  byte [esi], '9'
    ja   .width_done_fs
    movzx eax, byte [esi]
    sub  eax, '0'
    imul edx, 10
    add  edx, eax
    inc  esi
    jmp  .parse_width_fs
.width_done_fs:
    mov  al, [esi]
    cmp  al, 'd'
    je   .pad_int
    cmp  al, 'x'
    je   .pad_hex
    cmp  al, 'X'
    je   .pad_HEX
    cmp  al, 'o'
    je   .pad_oct
    cmp  al, 'b'
    je   .pad_bin
    jmp  .skip_braces_fs

.pad_int:
    cmp  byte [esi+1], '}'
    jne  .skip_braces_fs
    add  esi, 2
    push ecx
    push edx
    pop  dword [double_temp_int]
    mov  eax, [ebp-12]
    cmp  eax, [ebp-4]
    jae  .error_fs_cleanup_pad
    mov  ebx, [ebp-8]
    mov  ebx, [ebx + eax*4]
    mov  edx, [ebp-16]
    add  edx, ebp
    mov  eax, [edx]
    inc  dword [ebp-12]
    add  dword [ebp-16], 4
    cmp  ebx, 0
    je   .int_ok_2
    cmp  ebx, 4
    jne  .error_fs_cleanup_pad
.int_ok_2:
    mov  eax, [eax]
    mov  ecx, [double_temp_int]
    test ecx, ecx
    jnz  .do_pad_int
    mov  ecx, 1
.do_pad_int:
    push ecx
    call write_int_padded
    add  esp, 4
    pop  ecx
    cmp  eax, ecx
    ja   .error_fs
    sub  ecx, eax
    jmp  .parse_loop_fs

.error_fs_cleanup_pad:
    add  esp, 4
    jmp  .error_fs

.pad_hex:
    cmp  byte [esi+1], '}'
    jne  .skip_braces_fs
    add  esi, 2
    push ecx
    push edx
    pop  dword [double_temp_int]
    mov  eax, [ebp-12]
    cmp  eax, [ebp-4]
    jae  .error_fs_cleanup_pad
    mov  ebx, [ebp-8]
    mov  ebx, [ebx + eax*4]
    mov  edx, [ebp-16]
    add  edx, ebp
    mov  eax, [edx]
    inc  dword [ebp-12]
    add  dword [ebp-16], 4
    cmp  ebx, 0
    je   .int_ok_3
    cmp  ebx, 4
    jne  .error_fs_cleanup_pad
.int_ok_3:
    mov  eax, [eax]
    mov  ecx, [double_temp_int]
    test ecx, ecx
    jnz  .do_pad_hex
    mov  ecx, 1
.do_pad_hex:
    push ecx
    call write_hex_padded
    add  esp, 4
    pop  ecx
    cmp  eax, ecx
    ja   .error_fs
    sub  ecx, eax
    jmp  .parse_loop_fs

.pad_HEX:
    cmp  byte [esi+1], '}'
    jne  .skip_braces_fs
    add  esi, 2
    push ecx
    push edx
    pop  dword [double_temp_int]
    mov  eax, [ebp-12]
    cmp  eax, [ebp-4]
    jae  .error_fs_cleanup_pad
    mov  ebx, [ebp-8]
    mov  ebx, [ebx + eax*4]
    mov  edx, [ebp-16]
    add  edx, ebp
    mov  eax, [edx]
    inc  dword [ebp-12]
    add  dword [ebp-16], 4
    cmp  ebx, 0
    je   .int_ok_4
    cmp  ebx, 4
    jne  .error_fs_cleanup_pad
.int_ok_4:
    mov  eax, [eax]
    mov  ecx, [double_temp_int]
    test ecx, ecx
    jnz  .do_pad_HEX
    mov  ecx, 1
.do_pad_HEX:
    push ecx
    call write_HEX_padded
    add  esp, 4
    pop  ecx
    cmp  eax, ecx
    ja   .error_fs
    sub  ecx, eax
    jmp  .parse_loop_fs

.pad_oct:
    cmp  byte [esi+1], '}'
    jne  .skip_braces_fs
    add  esi, 2
    push ecx
    push edx
    pop  dword [double_temp_int]
    mov  eax, [ebp-12]
    cmp  eax, [ebp-4]
    jae  .error_fs_cleanup_pad
    mov  ebx, [ebp-8]
    mov  ebx, [ebx + eax*4]
    mov  edx, [ebp-16]
    add  edx, ebp
    mov  eax, [edx]
    inc  dword [ebp-12]
    add  dword [ebp-16], 4
    cmp  ebx, 0
    je   .int_ok_5
    cmp  ebx, 4
    jne  .error_fs_cleanup_pad
.int_ok_5:
    mov  eax, [eax]
    mov  ecx, [double_temp_int]
    test ecx, ecx
    jnz  .do_pad_oct
    mov  ecx, 1
.do_pad_oct:
    push ecx
    call write_oct_padded
    add  esp, 4
    pop  ecx
    cmp  eax, ecx
    ja   .error_fs
    sub  ecx, eax
    jmp  .parse_loop_fs

.pad_bin:
    cmp  byte [esi+1], '}'
    jne  .skip_braces_fs
    add  esi, 2
    push ecx
    push edx
    pop  dword [double_temp_int]
    mov  eax, [ebp-12]
    cmp  eax, [ebp-4]
    jae  .error_fs_cleanup_pad
    mov  ebx, [ebp-8]
    mov  ebx, [ebx + eax*4]
    mov  edx, [ebp-16]
    add  edx, ebp
    mov  eax, [edx]
    inc  dword [ebp-12]
    add  dword [ebp-16], 4
    cmp  ebx, 0
    je   .int_ok_6
    cmp  ebx, 4
    jne  .error_fs_cleanup_pad
.int_ok_6:
    mov  eax, [eax]
    mov  ecx, [double_temp_int]
    test ecx, ecx
    jnz  .do_pad_bin
    mov  ecx, 1
.do_pad_bin:
    push ecx
    call write_bin_padded
    add  esp, 4
    pop  ecx
    cmp  eax, ecx
    ja   .error_fs
    sub  ecx, eax
    jmp  .parse_loop_fs

.skip_braces_fs:
    lodsb
    test al, al
    jz   .finish_fs
    cmp  al, '}'
    jne  .skip_braces_fs
    jmp  .parse_loop_fs

.copy_current_brace_fs:
    cmp  ecx, 1
    jbe  .error_fs
    mov  byte [edi], '{'
    inc  edi
    dec  ecx
    jmp  .parse_loop_fs

.finish_fs:
    mov  byte [edi], 0
    mov  eax, out_buffer
    pop  ebx
    pop  edi
    pop  esi
    fninit
    mov  esp, ebp
    pop  ebp
    ret

.error_fs:
    xor  eax, eax
    pop  ebx
    pop  edi
    pop  esi
    fninit
    mov  esp, ebp
    pop  ebp
    ret

global _system_exit
_system_exit:
    push dword [esp+4]
    call _ExitProcess@4

global _system_sleep
_system_sleep:
    push dword [esp+4]
    call _Sleep@4
    ret 4

global _system_time
_system_time:
    call _GetTickCount@0
    ret

global _system_getenv
_system_getenv:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push 0
    push 0
    push dword [ebp+8]
    call _GetEnvironmentVariableA@12
    cmp eax, 0
    je .env_empty
    mov ebx, eax
    push 0
    push ebx
    call _AllocMemory
    add esp, 8
    mov esi, eax
    push ebx
    push esi
    push dword [ebp+8]
    call _GetEnvironmentVariableA@12
    mov eax, esi
    pop esi
    pop ebx
    pop ebp
    ret 4
.env_empty:
    mov eax, _sys_empty_str
    pop esi
    pop ebx
    pop ebp
    ret 4

global _system_call
_system_call:
    push ebp
    mov  ebp, esp
    sub  esp, 88

    mov  dword [ebp-4], -1

    mov  esi, [ebp+8]

    push esi
    call strlen
    add  esp, 4
    add  eax, 12

    push 0
    push eax
    call _AllocMemory
    add  esp, 8
    test eax, eax
    jz   .done

    mov  ebx, eax

    mov  dword [ebx+0], 0x2E646D63
    mov  dword [ebx+4], 0x20657865
    mov  word  [ebx+8], 0x632F
    mov  byte  [ebx+10], ' '
    lea  edi, [ebx+11]
    mov  esi, [ebp+8]
.cat:
    mov  al, [esi]
    mov  [edi], al
    inc  esi
    inc  edi
    test al, al
    jnz  .cat

    lea  edi, [ebp-72]
    mov  ecx, 68
    xor  eax, eax
    rep  stosb
    mov  dword [ebp-72], 68

    lea  edi, [ebp-88]
    mov  ecx, 16
    xor  eax, eax
    rep  stosb

    lea  eax, [ebp-88]
    push eax
    lea  eax, [ebp-72]
    push eax
    push 0
    push 0
    push 0
    push 0
    push 0
    push 0
    push ebx
    push 0
    call _CreateProcessA@40
    test eax, eax
    jz   .cleanup

    push -1
    push dword [ebp-88]
    call _WaitForSingleObject@8

    mov  dword [ebp-4], -1
    lea  eax, [ebp-4]
    push eax
    push dword [ebp-88]
    call _GetExitCodeProcess@8
    test eax, eax
    jz   .cleanup

.cleanup:
    cmp  dword [ebp-88], 0
    je   .skip_hproc
    push dword [ebp-88]
    call _CloseHandle@4
.skip_hproc:
    cmp  dword [ebp-84], 0
    je   .skip_hthread
    push dword [ebp-84]
    call _CloseHandle@4
.skip_hthread:

    push ebx
    call _FreeMemory
    add  esp, 4

    mov  eax, [ebp-4]
.done:
    mov  esp, ebp
    pop  ebp
    ret  4

global _sys_argv_at
_sys_argv_at:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    mov edi, [ebp+8]
    call _GetCommandLineW@0
    mov esi, eax
    call _sys_skip_ws
    xor edx, edx
.scan:
    cmp word [esi], 0
    je .not_found
    cmp edx, edi
    je .found
    cmp word [esi], '"'
    je .skip_q
.skip_u:
    cmp word [esi], 0
    je .not_found
    cmp word [esi], ' '
    je .skip_done
    cmp word [esi], 9
    je .skip_done
    add esi, 2
    jmp .skip_u
.skip_q:
    add esi, 2
.skip_ql:
    cmp word [esi], 0
    je .not_found
    cmp word [esi], '"'
    je .skip_qd
    add esi, 2
    jmp .skip_ql
.skip_qd:
    add esi, 2
.skip_done:
    call _sys_skip_ws
    inc edx
    jmp .scan
.found:
    cmp word [esi], '"'
    je .copy_q
    mov edx, esi
.cu_scan:
    cmp word [esi], 0
    je .cu_end
    cmp word [esi], ' '
    je .cu_end
    cmp word [esi], 9
    je .cu_end
    add esi, 2
    jmp .cu_scan
.cu_end:
    jmp .do_copy
.copy_q:
    add esi, 2
    mov edx, esi
.cq_scan:
    cmp word [esi], 0
    je .cq_end
    cmp word [esi], '"'
    je .cq_end
    add esi, 2
    jmp .cq_scan
.cq_end:
.do_copy:
    mov eax, esi
    sub eax, edx
    shr eax, 1
    mov ecx, eax
    jecxz .empty
    mov eax, ecx
    imul eax, 3
    inc eax
    push ecx
    push edx
    push 0
    push eax
    call _AllocMemory
    add esp, 8
    pop edx
    pop ecx
    mov ebx, eax
    push ebx
.cloop:
    test ecx, ecx
    jz .cdone
    mov ax, [edx]
    cmp ax, 128
    jb .ascii
    mov byte [ebx], '?'
    inc ebx
    jmp .cnext
.ascii:
    mov [ebx], al
    inc ebx
.cnext:
    add edx, 2
    dec ecx
    jmp .cloop
.cdone:
    mov byte [ebx], 0
    pop eax
    jmp .done
.empty:
    push 0
    push 1
    call _AllocMemory
    add esp, 8
    mov byte [eax], 0
    jmp .done
.not_found:
    mov eax, _sys_empty_str
.done:
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret 4

_sys_skip_ws:
    cmp word [esi], ' '
    je .sw
    cmp word [esi], 9
    je .sw
    ret
.sw:
    add esi, 2
    jmp _sys_skip_ws

strlen:
    push esi
    mov esi, [esp+8]
    xor eax, eax
.sll:
    cmp byte [esi], 0
    je .sld
    inc eax
    inc esi
    jmp .sll
.sld:
    pop esi
    ret