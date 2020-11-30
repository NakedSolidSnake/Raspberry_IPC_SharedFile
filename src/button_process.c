#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <button.h>

#define _1ms 1000

#define FILENAME "/tmp/data.dat"

static Button_t button7 = {
    .gpio.pin = 7,
    .gpio.eMode = eModeInput,
    .ePullMode = ePullModePullUp,
    .eIntEdge = eIntEdgeFalling,
    .cb = NULL};

int main(int argc, char const *argv[])
{
    char buffer[2];
    struct flock lock;
 
    int fd;
    int state = 0;

    if (Button_init(&button7))
        return EXIT_FAILURE;

    while (1)
    {

        while (1)
        {
            if (!Button_read(&button7))
            {
                usleep(_1ms * 40);
                while (!Button_read(&button7))
                    ;
                usleep(_1ms * 40);
                state ^= 0x01;
                break;
            }
            else
            {
                usleep(_1ms);
            }
        }

        if ((fd = open(FILENAME, O_RDWR | O_CREAT, 0666)) < 0)
            continue;

        lock.l_type = F_WRLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = 0;
        lock.l_len = 0;
        lock.l_pid = getpid();
        while (fcntl(fd, F_SETLK, &lock) < 0)
            usleep(_1ms);

        snprintf(buffer, sizeof(buffer), "%d", state);

        write(fd, buffer, strlen(buffer));

        lock.l_type = F_UNLCK;
        if (fcntl(fd, F_SETLK, &lock) < 0)
            return EXIT_FAILURE;

        close(fd);
        usleep(100 * _1ms);
    }

    return 0;
}