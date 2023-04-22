# faulty-oops.md analysis

Run echo "hello_world" > /dev/faulty from the command line of your running qemu
image and detail the resulting kernel oops to your assignment 3 repository in an
assignments/assignment7/faulty-oops.md.

Output of `"hello_world" > /dev/faulty`:
```
root@qemuarm64:~# echo "hello_world" > /dev/faulty
[  137.926606] Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
[  137.929332] Mem abort info:
[  137.929652]   ESR = 0x0000000096000045
[  137.929788]   EC = 0x25: DABT (current EL), IL = 32 bits
[  137.929966]   SET = 0, FnV = 0
[  137.930065]   EA = 0, S1PTW = 0
[  137.930212]   FSC = 0x05: level 1 translation fault
[  137.930389] Data abort info:
[  137.930522]   ISV = 0, ISS = 0x00000045
[  137.930628]   CM = 0, WnR = 1
[  137.930821] user pgtable: 4k pages, 39-bit VAs, pgdp=000000004368b000
[  137.931386] [0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
[  137.931909] Internal error: Oops: 96000045 [#1] PREEMPT SMP
[  137.932231] Modules linked in: hello(O) faulty(O) scull(O)
[  137.932865] CPU: 0 PID: 351 Comm: sh Tainted: G           O      5.15.96-yocto-standard #1
[  137.933225] Hardware name: linux,dummy-virt (DT)
[  137.933596] pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
[  137.933866] pc : faulty_write+0x18/0x20 [faulty]
[  137.934493] lr : vfs_write+0xf8/0x29c
[  137.934625] sp : ffffffc00b023d80
[  137.934765] x29: ffffffc00b023d80 x28: ffffff8003578000 x27: 0000000000000000
[  137.935103] x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
[  137.935555] x23: 0000000000000000 x22: ffffffc00b023df0 x21: 0000005578e95d90
[  137.935784] x20: ffffff80036b9500 x19: 000000000000000c x18: 0000000000000000
[  137.935999] x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
[  137.936195] x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
[  137.936390] x11: 0000000000000000 x10: 0000000000000000 x9 : ffffffc008265d4c
[  137.936621] x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
[  137.936910] x5 : 0000000000000001 x4 : ffffffc000b67000 x3 : ffffffc00b023df0
[  137.937133] x2 : 000000000000000c x1 : 0000000000000000 x0 : 0000000000000000
[  137.937548] Call trace:
[  137.937661]  faulty_write+0x18/0x20 [faulty]
[  137.937859]  ksys_write+0x70/0x100
[  137.937989]  __arm64_sys_write+0x24/0x30
[  137.938129]  invoke_syscall+0x5c/0x130
[  137.938265]  el0_svc_common.constprop.0+0x4c/0x100
[  137.938401]  do_el0_svc+0x4c/0xb4
[  137.938501]  el0_svc+0x28/0x80
[  137.938599]  el0t_64_sync_handler+0xa4/0x130
[  137.938749]  el0t_64_sync+0x1a0/0x1a4
[  137.939070] Code: d2800001 d2800000 d503233f d50323bf (b900003f)
[  137.939669] ---[ end trace ae7296fb4ba20b00 ]---
Segmentation fault
```

## Analysis
Looking at the oops message, we can see that the kernel panic happened because
of:

[  137.926606] Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000

This hints that a NULL pointer memory address was dereferenced, this creating a
segmentation fault and causing a kernel panic. To narrow down where the NULL
pointer is dereferenced, further down at the call trace section shows:

[  137.937661]  faulty_write+0x18/0x20 [faulty]

which hints that something in `faulty_write`, which is in the `faulty`
module, caused the oops message to occur. Searching for `faulty_write` in
`assignment-6` repo shows the `faulty_write` function:

```c
ssize_t faulty_write (struct file *filp, const char __user *buf, size_t count,
		loff_t *pos)
{
	/* make a simple fault by dereferencing a NULL pointer */
	*(int *)0 = 0;
	return 0;
}
```

Careful analysis of the function code, and as evidenced by the comment,
the the NULL pointer memory address was in fact dereferenced and is shown on
line `*(int *)0 = 0;`. Possible fix for this is to delete the line from the
function definition as it is not doing anything useful.
