#include <sys/termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>

static struct pollfd pfd = {
    .fd = STDIN_FILENO,
    .events = POLLIN
};

static struct termios orig;

typedef enum MODE {
    MODE_COMMAND,
    MODE_EDIT
} MODE;

static struct {
    char *screen;
    MODE mode;
    int should_close;
    int should_save;
} state;

void on_window_resize(int _) {

}

void disable_raw_terminal() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
    write(STDOUT_FILENO, "\x1b[?1049l", 8);
}

void enable_raw_terminal() {
    struct termios terminal = orig;
    terminal.c_lflag &= ~(ECHO | ICANON | ISIG);
    terminal.c_cc[VTIME] = 0;
    terminal.c_cc[VMIN] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal);
    write(STDOUT_FILENO, "\x1b[?1049h", 8);
    write(STDOUT_FILENO, "\x1b[H", 3);
}

void cleanup() {
    disable_raw_terminal();
}

void setup() {
    tcgetattr(STDIN_FILENO, &orig);
    atexit(cleanup);
    enable_raw_terminal();
}

void poll_input() {

    poll(&pfd, 1, -1);

    char red = 0;

    char rite[] = "Tecla lida: 0\n";

    int readd = read(STDIN_FILENO, &red, 1);

    if (readd > 0)
        rite[12] = red;
    write(STDOUT_FILENO, rite, 14);

    if (red == 'q')
    {
        state.should_close = 1;
    }
    
}

void update() {

}

void render() {

}

void loop() {
    while (!state.should_close)
    {
        poll_input();
        update();
        render();
    }
    
}

int main() {
    setup();
    loop();
}