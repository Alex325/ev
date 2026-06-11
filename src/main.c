#include <sys/termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <signal.h>

static struct pollfd pfd = {
    .fd = STDIN_FILENO,
    .events = POLLIN
};

static struct termios orig;

typedef enum MODE {
    MODE_COMMAND,
    MODE_EDIT
} MODE;

typedef struct
{
    char pressed_key[3];
    int length;
} Input;

static struct {
    char *screen;
    MODE mode;
    Input input;
    int should_close;
    int should_save;
    int should_resize;
} state = { 0 };

void handle_resize() {

}

void on_window_resize(int _) {
    state.should_resize = 1;
}

void disable_raw_terminal() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
    write(STDOUT_FILENO, "\x1b[?1049l", 8);
}

void enable_raw_terminal() {
    struct termios terminal = orig;
    terminal.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    terminal.c_oflag &= ~(OPOST) | OPOST;
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
    signal(SIGWINCH, on_window_resize);
}

void poll_input() {

    poll(&pfd, 1, -1);

    int bytes_read = read(STDIN_FILENO, state.input.pressed_key, 3);
    state.input.length = bytes_read;
}

void update() {
    if (state.input.pressed_key[0] == 'q')
    {
        state.should_close = 1;
    }

    if (state.should_resize)
    {
        state.should_resize = 0;
        handle_resize();
    }
    
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