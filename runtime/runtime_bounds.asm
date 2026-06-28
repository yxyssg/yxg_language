section .data
    bounds_msg db `Error: Array index out of bounds\n`, 0

section .text
extern _printf
extern _ExitProcess@4
global _array_bounds_error

_array_bounds_error:
    push bounds_msg
    call _printf
    add esp, 4
    push 1
    call _ExitProcess@4