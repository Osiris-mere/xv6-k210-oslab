#include "kernel/include/types.h"
#include "xv6-user/user.h"

#define PGSIZE 4096
#define PROT_READ  0x1
#define PROT_WRITE 0x2
#define MAP_PRIVATE   0x02
#define MAP_ANONYMOUS 0x20

int
main(void)
{
    int failed = 0;
    uint64 initial, queried, grown, after;
    char *mem;

    printf("\n=== Part 2 memory syscall tests ===\n");

    // Test brk(0): query the current program break.
    initial = brk(0);
    if (initial != 0) {
        printf("[PASS] brk(0): current break = %p\n", (void *)initial);
    } else {
        printf("[FAIL] brk(0)\n");
        failed++;
    }

    // Grow the break by one page.
    grown = brk(initial + PGSIZE);
    queried = brk(0);
    if (grown >= initial + PGSIZE &&
        queried >= initial + PGSIZE) {
        printf("[PASS] brk: grow heap\n");
    } else {
        printf("[FAIL] brk: grow heap (returned %p, queried %p)\n",
               (void *)grown, (void *)queried);
        failed++;
    }

    // Anonymous private mapping, matching the kernel's current implementation.
    mem = (char *)mmap(0, PGSIZE, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem != (char *)-1) {
        mem[0] = 'O';
        mem[1] = 'K';
        if (mem[0] == 'O' && mem[1] == 'K') {
            printf("[PASS] mmap: allocate and access memory\n");
        } else {
            printf("[FAIL] mmap: memory read/write\n");
            failed++;
        }

        if (munmap(mem, PGSIZE) == 0) {
            printf("[PASS] munmap: release mapping\n");
        } else {
            printf("[FAIL] munmap: release mapping\n");
            failed++;
        }
    } else {
        printf("[FAIL] mmap: allocation failed\n");
        failed++;
    }

    after = brk(0);
    if (after != 0) {
        printf("[PASS] brk(0): query after mmap tests\n");
    } else {
        printf("[FAIL] brk(0): query after mmap tests\n");
        failed++;
    }

    printf("=== Result: %d failure(s) ===\n", failed);
    exit(0);
}
