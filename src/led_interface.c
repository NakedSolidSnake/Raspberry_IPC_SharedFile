#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <led_interface.h>

#define _1ms 1000
#define FILENAME "/tmp/data.dat"


bool LED_Run(void *object, LED_Interface *led)
{
    char buffer[2];
    struct flock lock;
  
    int fd = -1;
    int state_old = 0;
    int state_curr;

    if (led->Init(object) == false)
        return EXIT_FAILURE;

    while (1)
    {
        if ((fd = open(FILENAME, O_RDONLY)) < 0)
            return EXIT_FAILURE;

        lock.l_type = F_WRLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = 0;
        lock.l_len = 0;
        lock.l_pid = getpid();

        while (fcntl(fd, F_GETLK, &lock) < 0)
            usleep(_1ms);

        if (lock.l_type != F_UNLCK){
            close(fd);
            continue;
        }
            

        lock.l_type = F_RDLCK;
        while (fcntl(fd, F_SETLK, &lock) < 0)
            usleep(_1ms);

        while (read(fd, &buffer, sizeof(buffer)) > 0);            

        state_curr = atoi(buffer);
        if(state_curr != state_old){
            led->Set(object, (uint8_t)state_curr);
            state_old = state_curr;
        }

        lock.l_type = F_UNLCK;
        if (fcntl(fd, F_SETLK, &lock) < 0)
            return EXIT_FAILURE;

        close(fd);
        usleep(100 * _1ms);
    }

    return false;
}
