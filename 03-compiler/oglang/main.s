.text
.global main
main:
    movl $20, %eax
    movl $22, %ecx
    addl %ecx, %eax
    ret
