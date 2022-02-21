#pragma once

#include <termios.h>
#include <unistd.h>

class ConsoleSetter {
    struct termios old_tio_ {};

public:
    ConsoleSetter() {
        /* get the terminal settings for stdin */
        tcgetattr(STDIN_FILENO, &old_tio_);

        /* we want to keep the old setting to restore them a the end */
        struct termios new_tio = old_tio_;

        /* disable canonical mode (buffered i/o) and local echo */
        // new_tio.c_lflag &= (~ICANON & ~ECHO);

        /* disable canonical mode (buffered i/o) */
        new_tio.c_lflag &= (~ICANON);

        /* set the new settings immediately */
        tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
    }

    ~ConsoleSetter() {
        /* restore the former settings */
        tcsetattr(STDIN_FILENO, TCSANOW, &old_tio_);
    }
};
