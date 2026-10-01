#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

int main(void)
{
        const char message[] = "Hello World!";
        int fd = open("/dev/sdb", O_WRONLY);
        ssize_t written;

        if (fd < 0) {
                perror("Não foi possível abrir /dev/sdb");
                return 1;
        }

        if (lseek(fd, 0, SEEK_SET) < 0) {
                perror("lseek");
                close(fd);
                return 1;
        }

        written = write(fd, message, strlen(message));
        if (written != (ssize_t)strlen(message)) {
                perror("write");
                close(fd);
                return 1;
        }

        if (fsync(fd) < 0) {
                perror("fsync");
                close(fd);
                return 1;
        }

        close(fd);
        puts("Gravado no início de /dev/sdb.");
        return 0;
}
