#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include "processInfo.h"
#include <linux/slab.h>

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

#define SLEEP_LIST_MAX_SIZE 4096

asmlinkage long sys_listSleepingProcesses(char __user *buf, int size)
{
        struct task_struct *process;
        char *kbuf;
        size_t capacity;
        size_t used = 0;
        int written;
        long state;
        long ret;

        if (!buf || size <= 0)
                return -EINVAL;

        capacity = min_t(size_t, size, SLEEP_LIST_MAX_SIZE);

        kbuf = kmalloc(capacity, GFP_KERNEL);
        if (!kbuf)
                return -ENOMEM;

        rcu_read_lock();

        for_each_process(process) {
                state = (long)process->state;

                if (state != TASK_INTERRUPTIBLE &&
                    state != TASK_UNINTERRUPTIBLE)
                        continue;

                if (used >= capacity - 1) {
                        rcu_read_unlock();
                        kfree(kbuf);
                        return -ENOSPC;
                }

                written = scnprintf(
                        kbuf + used, capacity - used,
                        "PID: %ld  Process: %s  State: %ld\n",
                        (long)task_pid_nr(process),
                        process->comm, state);

                if (written >= capacity - used - 1) {
                        rcu_read_unlock();
                        kfree(kbuf);
                        return -ENOSPC;
                }

                used += written;
        }

        rcu_read_unlock();

        kbuf[used] = '\0';

        if (copy_to_user(buf, kbuf, used + 1))
                ret = -EFAULT;
        else
                ret = used + 1;

        kfree(kbuf);
        return ret;
}

#define LOG_MESSAGE_MAX_SIZE 256

asmlinkage long sys_logUserMessage(const char __user *message, int size)
{
        char kbuf[LOG_MESSAGE_MAX_SIZE];
        size_t copy_size;
        long copied;

        if (!message || size <= 0)
                return -EINVAL;

        copy_size = min_t(size_t, (size_t)size, sizeof(kbuf) - 1);

        copied = strncpy_from_user(kbuf, message, copy_size);
        if (copied < 0)
                return copied;

        if (copied >= copy_size)
                kbuf[copy_size] = '\0';
        else
                kbuf[copied] = '\0';

        printk(KERN_INFO "logUserMessage: %s\n", kbuf);

        return copied;
}