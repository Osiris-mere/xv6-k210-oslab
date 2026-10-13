
#include "include/sbi.h"
#include "include/types.h"
#include "include/riscv.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/spinlock.h"
#include "include/proc.h"
#include "include/syscall.h"
#include "include/timer.h"
#include "include/kalloc.h"
#include "include/string.h"
#include "include/printf.h"
#include "include/vm.h"

extern int exec(char *path, char **argv);

uint64
sys_exec(void)
{
  char path[FAT32_MAX_PATH], *argv[MAXARG];
  int i;
  uint64 uargv, uarg;

  if(argstr(0, path, FAT32_MAX_PATH) < 0 || argaddr(1, &uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv)){
      goto bad;
    }
    if(fetchaddr(uargv+sizeof(uint64)*i, (uint64*)&uarg) < 0){
      goto bad;
    }
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    argv[i] = kalloc();
    if(argv[i] == 0)
      goto bad;
    if(fetchstr(uarg, argv[i], PGSIZE) < 0)
      goto bad;
  }

  int ret = exec(path, argv);

  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);

  return ret;

 bad:
  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);
  return -1;
}

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_trace(void)
{
  int mask;
  if(argint(0, &mask) < 0) {
    return -1;
  }
  myproc()->tmask = mask;
  return 0;
}
/* Linux RISC-V compatible times() */
struct user_tms {
  uint64 tms_utime;
  uint64 tms_stime;
  uint64 tms_cutime;
  uint64 tms_cstime;
};

uint64
sys_times(void)
{
  uint64 addr;
  uint64 now;

  if (argaddr(0, &addr) < 0)
    return -1;

  acquire(&tickslock);
  now = ticks;
  release(&tickslock);

  if (addr != 0) {
    struct user_tms t = {0, 0, 0, 0};
    if (copyout2(addr, (char *)&t, sizeof(t)) < 0)
      return -1;
  }

  return now;
}

/* Linux RISC-V compatible uname() */
struct user_utsname {
  char sysname[65];
  char nodename[65];
  char release[65];
  char version[65];
  char machine[65];
  char domainname[65];
};

uint64
sys_uname(void)
{
  uint64 addr;
  struct user_utsname u;

  if (argaddr(0, &addr) < 0 || addr == 0)
    return -1;

  memset(&u, 0, sizeof(u));
  safestrcpy(u.sysname, "Linux", sizeof(u.sysname));
  safestrcpy(u.nodename, "xv6-k210", sizeof(u.nodename));
  safestrcpy(u.release, "6.1.0", sizeof(u.release));
  safestrcpy(u.version, "#1", sizeof(u.version));
  safestrcpy(u.machine, "riscv64", sizeof(u.machine));
  safestrcpy(u.domainname, "(none)", sizeof(u.domainname));

  if (copyout2(addr, (char *)&u, sizeof(u)) < 0)
    return -1;

  return 0;
}

/* SBI shutdown */
uint64
sys_shutdown(void)
{
  sbi_shutdown();
  return 0;
}


/*
 * Minimal Linux RISC-V brk/mmap/munmap compatibility.
 * mmap currently supports anonymous mappings appended to the process heap.
 * munmap currently supports releasing the most recently allocated mapping.
 */
uint64
sys_linux_brk(void)
{
  uint64 requested;
  struct proc *p = myproc();

  if (argaddr(0, &requested) < 0)
    return p->sz;

  if (requested == 0)
    return p->sz;

  if (requested > p->sz) {
    uint64 delta = requested - p->sz;
    if (delta > 0x7fffffffULL || growproc((int)delta) < 0)
      return p->sz;
  } else if (requested < p->sz) {
    uint64 delta = p->sz - requested;
    if (delta > 0x7fffffffULL || growproc(-(int)delta) < 0)
      return p->sz;
  }

  return p->sz;
}

uint64
sys_linux_mmap(void)
{
  uint64 addr, len, offset;
  int prot, flags, fd;
  struct proc *p = myproc();

  if (argaddr(0, &addr) < 0 ||
      argaddr(1, &len) < 0 ||
      argint(2, &prot) < 0 ||
      argint(3, &flags) < 0 ||
      argint(4, &fd) < 0 ||
      argaddr(5, &offset) < 0)
    return -1;

  (void)prot;
  (void)fd;
  (void)offset;

  /* MAP_ANONYMOUS is 0x20 on Linux. */
  if (len == 0 || !(flags & 0x20))
    return -1;

  /* Avoid overflow and unreasonable allocations. */
  if (len > 0x7fffffffULL - PGSIZE)
    return -1;

  uint64 rounded = PGROUNDUP(len);
  uint64 oldsz = p->sz;

  /* This minimal implementation appends mappings to the process heap. */
  if (addr != 0 && addr != oldsz)
    return -1;

  if (rounded > 0x7fffffffULL ||
      oldsz > 0x7fffffffULL - rounded)
    return -1;

  if (growproc((int)rounded) < 0)
    return -1;

  return oldsz;
}

uint64
sys_linux_munmap(void)
{
  uint64 addr, len;
  struct proc *p = myproc();

  if (argaddr(0, &addr) < 0 || argaddr(1, &len) < 0 || len == 0)
    return -1;

  if (len > 0x7fffffffULL - PGSIZE)
    return -1;

  uint64 rounded = PGROUNDUP(len);

  /* This minimal implementation releases only the topmost mapping. */
  if ((addr % PGSIZE) != 0 ||
      rounded > p->sz ||
      addr + rounded != p->sz)
    return -1;

  if (growproc(-(int)rounded) < 0)
    return -1;

  return 0;
}
