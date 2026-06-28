; gc.asm
; 基于标记-清除算法的保守 GC 运行时 (x86, Windows, NASM)
; 约定: _AllocMemory/_FreeMemory 使用 stdcall 调用约定

%define HEADER_SIZE     16          ; 对象头大小 (next, size, marked, pad)
%define MARK_STACK_SIZE 4096        ; 标记栈容量 (指针数)

section .text

global _gc_init
global _gc_alloc
global _gc_collect
global _gc_force
global _gc_status

extern gc_data_start_label
extern gc_data_end_label
extern gc_bss_start_label
extern gc_bss_end_label

extern _AllocMemory
extern _FreeMemory
extern _ExitProcess@4

; ------------------------------------------------------------
section .data
alloc_list:       dd 0             ; 已分配对象链表头（存用户指针）
total_allocated:  dd 0             ; 累计分配字节数
stack_bottom:     dd 0             ; 记录初始栈底（高地址）

; ------------------------------------------------------------
section .bss
mark_stack_ptr:   resd 1           ; 标记栈当前大小（索引）
mark_stack:       resd MARK_STACK_SIZE  ; 标记栈数组

; ------------------------------------------------------------
section .text

; void _gc_init(void)
_gc_init:
    mov [stack_bottom], esp        ; 记录当前栈顶作为栈底（高地址）
    mov dword [alloc_list], 0
    mov dword [total_allocated], 0
    ret

; void* _gc_alloc(size)
_gc_alloc:
    push ebp
    mov ebp, esp
    push dword [ebp+8]             ; 用户请求的字节数
    call alloc_impl
    add esp, 4
    test eax, eax
    jnz .done
    call _gc_collect               ; 分配失败，尝试 GC 后重试
    push dword [ebp+8]
    call alloc_impl
    add esp, 4
.done:
    pop ebp
    ret

; 内部分配：分配并链接到 alloc_list，返回用户指针
alloc_impl:
    push ebp
    mov ebp, esp
    mov eax, [ebp+8]               ; size
    add eax, HEADER_SIZE
    push 0                         ; flags = 0
    push eax                       ; 总大小
    call _AllocMemory              
    add esp, 8
    test eax, eax
    jz .fail
    ; eax = header_ptr
    mov edx, eax
    lea ebx, [edx + HEADER_SIZE]   ; user_ptr = header + 16
    mov ecx, [ebp+8]               ; 用户 size
    ; 插入链表头
    mov eax, [alloc_list]
    mov [edx], eax                 ; header->next = old_head (user_ptr)
    mov [alloc_list], ebx          ; head = user_ptr
    mov [edx+4], ecx               ; header->size
    mov dword [edx+8], 0           ; header->marked = 0
    ; 更新统计
    add [total_allocated], ecx
    mov eax, ebx                   ; 返回 user_ptr
.fail:
    pop ebp
    ret

; void _gc_collect(void)
_gc_collect:
    pushad                         ; 将所有寄存器压栈（使它们成为根扫描的一部分）
    cld
    mov dword [mark_stack_ptr], 0

    ; 1. 扫描栈
    mov edi, esp                   ; 当前栈顶（低地址）
    mov esi, [stack_bottom]        ; 栈底（高地址）
    call scan_memory_range

    ; 2. 扫描全局数据段
    mov edi, gc_data_start_label
    mov esi, gc_data_end_label
    call scan_memory_range
    mov edi, gc_bss_start_label
    mov esi, gc_bss_end_label
    call scan_memory_range

    ; 3. 处理标记栈（扫描堆内指针）
    call process_mark_stack

    ; 4. 清除未标记对象
    call sweep

    ; 5. 清空标记栈，防止残留指针污染下次 GC
    mov edi, mark_stack
    mov ecx, MARK_STACK_SIZE
    xor eax, eax
    rep stosd

    popad
    ret

; 按 dword 扫描内存区域 [edi, esi)
scan_memory_range:
    cmp edi, esi
    jae .done
.loop:
    mov eax, [edi]
    call maybe_mark
    add edi, 4
    cmp edi, esi
    jb .loop
.done:
    ret

; 检查 eax 是否指向已管理堆块，若是则标记并入栈
maybe_mark:
    push esi
    push edi
    mov esi, [alloc_list]          ; esi = 当前 user_ptr
.check_loop:
    test esi, esi
    jz .not_found
    lea edi, [esi - HEADER_SIZE]   ; edi = header
    mov ecx, [edi+4]               ; size
    cmp eax, esi
    jb .next
    lea edx, [esi + ecx]
    cmp eax, edx
    jae .next
    ; 找到 —— 未标记则标记并入栈
    cmp byte [edi+8], 0
    jne .already_marked
    mov byte [edi+8], 1
    mov edx, [mark_stack_ptr]
    mov [mark_stack + edx*4], esi  ; 压入 user_ptr
    inc dword [mark_stack_ptr]
.already_marked:
    pop edi
    pop esi
    ret
.next:
    mov esi, [edi]                 ; next user_ptr
    jmp .check_loop
.not_found:
    pop edi
    pop esi
    ret

; 弹出标记栈中的对象，扫描其内容寻找新指针
process_mark_stack:
.while:
    cmp dword [mark_stack_ptr], 0
    je .done
    dec dword [mark_stack_ptr]
    mov edx, [mark_stack_ptr]
    mov esi, [mark_stack + edx*4]  ; user_ptr
    mov edi, [esi - HEADER_SIZE + 4] ; size
    lea edx, [esi + edi]           ; 结束地址
    cmp esi, edx
    jae .while
.scan_loop:
    mov eax, [esi]
    push edx
    call maybe_mark
    pop edx
    add esi, 4
    cmp esi, edx
    jb .scan_loop
    jmp .while
.done:
    ret

; 遍历 alloc_list，释放未标记对象，清除存活对象的标记
sweep:
    lea edi, [alloc_list]          ; edi = &alloc_list
.loop:
    mov esi, [edi]                 ; esi = current user_ptr
    test esi, esi
    jz .done
    lea edx, [esi - HEADER_SIZE]   ; edx = header
    cmp byte [edx+8], 0
    jne .keep
    ; 未标记：移除并释放
    mov ecx, [edx]                 ; next_user
    mov [edi], ecx
    mov eax, [edx+4]               ; size
    sub [total_allocated], eax
    push edx                       ; header 指针
    call _FreeMemory               ; stdcall
    jmp .loop
.keep:
    ; 已标记：清除标记，前进到 next 字段地址
    mov byte [edx+8], 0
    lea edi, [edx]                 ; edi = &(header->next)
    jmp .loop
.done:
    ret

; void _gc_force(void)
_gc_force:
    call _gc_collect
    ret

; int _gc_status(void) → eax = 已分配总字节数
_gc_status:
    mov eax, [total_allocated]
    ret