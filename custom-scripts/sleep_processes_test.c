#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>

#define SYSCALL_SLEEPING_PROCESSES 386

int main(void)
{
        char buf[4096] = {0};
        long ret;

        printf("Invocando a syscall listSleepingProcesses...\n");

        ret = syscall(SYSCALL_SLEEPING_PROCESSES, buf, sizeof(buf));
        if (ret == -1) {
                perror("A syscall falhou");
                return EXIT_FAILURE;
        }

        if (buf[0] == '\0')
                printf("Nenhum processo em sleep encontrado.\n");
        else
                printf("%s", buf);

        return EXIT_SUCCESS;
}