# hello.s - Simple x86-64 assembly function
.section .rodata
hello_msg:
    .string "Hello from assembly!\n"

.section .text
.global hello_asm
.type hello_asm, @function

hello_asm:
    # Save registers
    push %rbp
    mov %rsp, %rbp
    
    # Call printf with our message
    lea hello_msg(%rip), %rdi
    xor %eax, %eax
    call printf@PLT
    
    # Restore and return
    pop %rbp
    ret