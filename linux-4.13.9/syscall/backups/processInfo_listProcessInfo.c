#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include "processInfo.h"

asmlinkage long sys_listProcessInfo(long pid, char __user *buf, int size)
{
        struct task_struct *process;
        char kbuf[256];
        int bufsz;

        if (!buf || size <= 0 || pid < 0)
                return -EINVAL;

        rcu_read_lock();

        for_each_process(process) {
                if ((long)task_pid_nr(process) == pid) {
                        bufsz = scnprintf(
                                kbuf, sizeof(kbuf),
                                "Process: %s\n"
                                "PID_Number: %ld\n"
                                "Process State: %ld\n"
                                "Priority: %ld\n"
                                "RT_Priority: %ld\n"
                                "Static Priority: %ld\n"
                                "Normal Priority: %ld\n",
                                process->comm,
                                (long)task_pid_nr(process),
                                (long)process->state,
                                (long)process->prio,
                                (long)process->rt_priority,
                                (long)process->static_prio,
                                (long)process->normal_prio);

                        rcu_read_unlock();

                        bufsz++; /* inclui o terminador '\0' */

                        if (bufsz > size)
                                return -ENOSPC;

                        if (copy_to_user(buf, kbuf, bufsz))
                                return -EFAULT;

                        return bufsz;
                }
        }

        rcu_read_unlock();
        return -ESRCH;
}
