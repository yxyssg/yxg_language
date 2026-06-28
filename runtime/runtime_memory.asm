; ================================================
; 文件名: memalloc.asm
; 功能:   动态内存分配库 (Windows 32位)
; 编译:   nasm -f win32 memalloc.asm -o memalloc.obj
; 链接:   link /lib memalloc.obj 或直接链接到程序
; ================================================

global _AllocMemory
global _FreeMemory

extern _GetProcessHeap@0
extern _HeapAlloc@12
extern _HeapFree@12
extern _print

section .data
str_heap_fail   db 'GetProcessHeap failed', 0
str_alloc_fail  db 'HeapAlloc failed', 0

section .text

; ================================================
; void* __cdecl _AllocMemory(DWORD dwSize, DWORD dwFlags)
; 参数:
;   [esp+4] = dwSize
;   [esp+8] = dwFlags
; 返回值:
;   eax = 内存指针, 失败返回0
; ================================================
_AllocMemory:
    push ebp
    mov  ebp, esp
    
    ; 获取进程堆句柄
    call _GetProcessHeap@0
    test eax, eax
    jnz  .get_heap_ok
    
    ; GetProcessHeap 失败
    push 3                  ; 类型 3 表示字符串
    push str_heap_fail      ; 要输出的字符串
    call _print
    add  esp, 8             ; 清理 print 参数
    jmp  .fail
    
.get_heap_ok:
    ; 保存堆句柄
    push eax                ; 暂存堆句柄
    
    ; 分配内存
    push dword [ebp+8]      ; dwSize
    push dword [ebp+12]     ; dwFlags
    push eax                ; hHeap
    call _HeapAlloc@12
    
    ; 检查分配结果
    test eax, eax
    jnz  .success
    
    ; HeapAlloc 失败
    push 3                  ; 类型 3 表示字符串
    push str_alloc_fail     ; 要输出的字符串
    call _print
    add  esp, 8             ; 清理 print 参数
    
    ; 清理堆句柄
    pop  eax                ; 平衡堆栈（之前 push 的堆句柄）
    jmp  .fail
    
.success:
    ; 成功，清理堆句柄（之前 push 的）
    add  esp, 4             ; 平衡堆栈
    pop  ebp
    ret
    
.fail:
    xor eax, eax
    pop ebp
    ret

; ================================================
; void __stdcall _FreeMemory(void* lpMem)
; 参数:
;   [esp+4] = lpMem
; ================================================
_FreeMemory:
    push ebp
    mov  ebp, esp
    
    call _GetProcessHeap@0
    test eax, eax
    jz   .done
    
    push dword [ebp+8]      ; lpMem
    push 0                  ; dwFlags
    push eax                ; hHeap
    call _HeapFree@12
    
.done:
    pop ebp
    ret 4