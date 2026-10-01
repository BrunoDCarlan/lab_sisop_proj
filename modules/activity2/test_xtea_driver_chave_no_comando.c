#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_LENGTH 256
#define DEVICE_PATH "/dev/xtea_driver"

int main(int argc, char *argv[])
{
        int fd;
        int ret;
        int command_length;
        char command[BUFFER_LENGTH];
        char receive[BUFFER_LENGTH];

        if (argc != 8) {
                fprintf(stderr,
                        "Uso: %s <enc|dec> <key0> <key1> <key2> <key3> "
                        "<tamanho_em_bytes> <dados_hex>\n",
                        argv[0]);
                return EXIT_FAILURE;
        }

        if (strcmp(argv[1], "enc") != 0 && strcmp(argv[1], "dec") != 0) {
                fprintf(stderr, "A operação deve ser enc ou dec.\n");
                return EXIT_FAILURE;
        }

        command_length = snprintf(command, sizeof(command),
                                  "%s %s %s %s %s %s %s",
                                  argv[1], argv[2], argv[3], argv[4],
                                  argv[5], argv[6], argv[7]);

        if (command_length < 0 || command_length >= sizeof(command)) {
                fprintf(stderr, "Comando muito longo.\n");
                return EXIT_FAILURE;
        }

        fd = open(DEVICE_PATH, O_RDWR);
        if (fd < 0) {
                perror("Não foi possível abrir " DEVICE_PATH);
                return errno;
        }

        ret = write(fd, command, command_length);
        if (ret < 0) {
                perror("Falha ao enviar comando ao driver");
                close(fd);
                return errno;
        }

        ret = read(fd, receive, sizeof(receive) - 1);
        if (ret < 0) {
                perror("Falha ao ler a resposta do driver");
                close(fd);
                return errno;
        }

        receive[ret] = '\0';
        printf("%s\n", receive);

        close(fd);
        return EXIT_SUCCESS;
}
