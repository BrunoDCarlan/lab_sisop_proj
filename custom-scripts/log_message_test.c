#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#define SYSCALL_LOG_USER_MESSAGE 387

int main(int argc, char **argv)
{
        long ret;

        if (argc != 2) {
                fprintf(stderr, "Uso: %s \"mensagem\"\n", argv[0]);
                return EXIT_FAILURE;
        }

        ret = syscall(SYSCALL_LOG_USER_MESSAGE,
                      argv[1], strlen(argv[1]) + 1);

        if (ret == -1) {
                perror("A syscall falhou");
                return EXIT_FAILURE;
        }

        printf("Mensagem enviada ao log do kernel.\n");
        return EXIT_SUCCESS;
}