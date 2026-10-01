#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>

#define SYSCALL_PROCESSINFO 385

int main(int argc, char **argv)
{
        char buf[256] = {0};
        long pid;
        long ret;

        if (argc != 2) {
                fprintf(stderr, "Uso: %s <PID>\n", argv[0]);
                return EXIT_FAILURE;
        }

        pid = strtol(argv[1], NULL, 10);

        printf("Invocando a syscall listProcessInfo...\n");

        ret = syscall(SYSCALL_PROCESSINFO, pid, buf, sizeof(buf));
        if (ret == -1) {
                perror("A syscall falhou");
                return EXIT_FAILURE;
        }

        printf("%s", buf);
        return EXIT_SUCCESS;
}