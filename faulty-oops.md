# Analysis of kernel oops for `faulty` module
Running `echo "hello_world" > /dev/faulty` on target produces the following oops message:
```shell
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
Mem abort info:
  ESR = 0x96000045
  EC = 0x25: DABT (current EL), IL = 32 bits
  SET = 0, FnV = 0
  EA = 0, S1PTW = 0
  FSC = 0x05: level 1 translation fault
Data abort info:
  ISV = 0, ISS = 0x00000045
  CM = 0, WnR = 1
user pgtable: 4k pages, 39-bit VAs, pgdp=000000004205f000
[0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
Internal error: Oops: 96000045 [#1] SMP
Modules linked in: hello(O) faulty(O) scull(O)
CPU: 0 PID: 129 Comm: sh Tainted: G           O      5.15.18 #1
Hardware name: linux,dummy-virt (DT)
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
pc : faulty_write+0x14/0x20 [faulty]
lr : vfs_write+0xa8/0x2b0
sp : ffffffc008d13d80
x29: ffffffc008d13d80 x28: ffffff80020d0000 x27: 0000000000000000
x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
x23: 0000000040001000 x22: 000000000000000c x21: 00000055652429c0
x20: 00000055652429c0 x19: ffffff800209d400 x18: 0000000000000000
x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000
x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
x5 : 0000000000000001 x4 : ffffffc0006f7000 x3 : ffffffc008d13df0
x2 : 000000000000000c x1 : 0000000000000000 x0 : 0000000000000000
Call trace:
 faulty_write+0x14/0x20 [faulty]
 ksys_write+0x68/0x100
 __arm64_sys_write+0x20/0x30
 invoke_syscall+0x54/0x130
 el0_svc_common.constprop.0+0x44/0xf0
 do_el0_svc+0x40/0xa0
 el0_svc+0x20/0x60
 el0t_64_sync_handler+0xe8/0xf0
 el0t_64_sync+0x1a0/0x1a4
Code: d2800001 d2800000 d503233f d50323bf (b900003f) 
---[ end trace 689d824fbbd0db0c ]---
```

## Decoding the oops message
The oops message informs that a NULL pointer deference error, also called page fault, has occured due to incorrect use of pointer. From the oops message, we can also deduce that the error occured in `faulty_write` function in `[faulty]` module. It also informs that the error occured at offset `0x14` into the `faulty_write` function.

## Looking at the `faulty` module dissasembly
Running `./output/host/bin/aarch64-linux-objdump -S output/target/lib/modules/5.15.18/extra/faulty.ko` from the buildroot directory gives the following disassembly for the `faulty_write` function mentioned in oops message.
```asm
0000000000000000 <faulty_write>:
   0:   d503245f        bti     c
   4:   d2800001        mov     x1, #0x0                        // #0
   8:   d2800000        mov     x0, #0x0                        // #0
   c:   d503233f        paciasp
  10:   d50323bf        autiasp
  14:   b900003f        str     wzr, [x1]
  18:   d65f03c0        ret
  1c:   d503201f        nop
```
From the disassembly, we can see that at offset `0x14` of `<faulty_write>` function, there is indeed a try to store zero value to address `0x0` which is an invalid operation, causes the fault and produces the oops message above.

## Looking at the `faulty` module source code
```c
/* make a simple fault by dereferencing a NULL pointer */
*(int *)0 = 0;
return 0;
``` 
Looking at the source code for faulty module, `faulty.c`, we can see that there is NULL pointer deferencing which causes the fault and produces the oops message above.