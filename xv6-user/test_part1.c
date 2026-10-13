#include "kernel/include/types.h"
#include "xv6-user/user.h"

struct user_tms {
    uint64 utime;
    uint64 stime;
    uint64 cutime;
    uint64 cstime;
};

struct user_utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

int
main(void)
{
    int failed = 0;
    int pid;
    char cwd[128];
    struct user_tms t;
    struct user_utsname u;

    printf("\n=== Part 1 syscall tests ===\n");

    pid = getpid();
    if (pid > 0)
        printf("[PASS] getpid: %d\n", pid);
    else {
        printf("[FAIL] getpid\n");
        failed++;
    }

    if (write(1, "[PASS] write: output works\n", 27) < 0) {
        printf("[FAIL] write\n");
        failed++;
    }

    cwd[0] = '\0';
    if (getcwd(cwd, sizeof(cwd)) >= 0)
        printf("[PASS] getcwd: %s\n", cwd);
    else {
        printf("[FAIL] getcwd\n");
        failed++;
    }

    if (times(&t) >= 0)
        printf("[PASS] times\n");
    else {
        printf("[FAIL] times\n");
        failed++;
    }

    if (uname(&u) == 0)
        printf("[PASS] uname: %s %s %s\n",
               u.sysname, u.machine, u.release);
    else {
        printf("[FAIL] uname\n");
        failed++;
    }

    printf("=== Result: %d failure(s) ===\n", failed);
    exit(0);
}
