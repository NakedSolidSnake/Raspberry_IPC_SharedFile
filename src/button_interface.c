#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <button_interface.h>

#define _1ms 1000

#define FILENAME "/tmp/data.dat"

static void create_file(void);

bool Button_Run(void *object, Button_Interface *button)
{
    char buffer[2];
    struct flock lock;
 
    int fd;
    int state = 0;

    create_file();

    if (button->Init(object) == false)
        return EXIT_FAILURE;

    while (true)
    {

        while(true)
        {
            if(!button->Read(object)){
                usleep(_1ms * 100);
                state ^= 0x01;
                break;
            }else{
                usleep( _1ms );
            }
        }

        if ((fd = open(FILENAME, O_RDWR, 0666)) < 0)
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

    return false;
}

static void create_file(void)
{
    int fd;
    fd = open(FILENAME, O_RDWR | O_CREAT, 0666);
    if(fd < 0)
        abort();
    write(fd, "0", 1);
    close(fd);
}