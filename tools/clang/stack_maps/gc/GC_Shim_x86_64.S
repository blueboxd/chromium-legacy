.text
.globl GetFramePtr

// Places the frame pointer value in the return register
GetFramePtr:
    mov (%rbp), %rax
    ret

.extern StackWalk
.globl BeginGC

// We place the frame pointer in the first arg slot register. We jump to
// StackWalk instead of calling it so that RET returns to BeginGC's caller
// caller.
BeginGC:
    mov %rbp, %rdi
    jmp StackWalk
