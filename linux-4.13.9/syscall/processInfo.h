#ifndef PROCESS_INFO_H
#define PROCESS_INFO_H

#include <linux/linkage.h>
#include <linux/compiler.h>

asmlinkage long sys_listProcessInfo(long pid, char __user *buf, int size);

asmlinkage long sys_listSleepingProcesses(char __user *buf, int size);

asmlinkage long sys_logUserMessage(const char __user *message, int size);
#endif
