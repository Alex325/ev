#include <sys/termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <document.h>

#define max(a, b) ((a > b) ? (a) : (b))
#define min(a, b) ((a < b) ? (a) : (b))

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
} input_t;

static struct {
    char *screen;
    int screen_length;
    MODE mode;
    input_t input;
    document_t document;
    int start_line;
    int start_column;
    int should_close;
    int should_save;
    int should_resize;
    struct winsize window_size;
} state = { 0 };

int cur_column = 0;

void init_state() {
    state.document = document_new();
}

void clear_screen() {
    memset(state.screen, (int)' ', state.screen_length);
}

void handle_resize() {

    ioctl(STDOUT_FILENO, TIOCGWINSZ, &state.window_size);

    const int screen_length = state.window_size.ws_col * state.window_size.ws_row;
    state.screen = realloc(state.screen, screen_length);
    state.screen_length = screen_length;
    clear_screen();
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
    document_destroy(&state.document);
}

void setup() {
    tcgetattr(STDIN_FILENO, &orig);
    atexit(cleanup);
    enable_raw_terminal();
    init_state();
    signal(SIGWINCH, on_window_resize);
    handle_resize();
}

int try_write_line(const line_t *line, int start) {
    int to_write = min(state.screen_length - start, line->size);
    memcpy(state.screen + start, line->text, to_write);
    return to_write;
}

void poll_input() {

    state.input.length = 0;

    if (poll(&pfd, 1, -1) <= 0) return;

    int bytes_read = read(STDIN_FILENO, state.input.pressed_key, 3);
    state.input.length = bytes_read;
}

void update() {

    if (state.should_resize)
    {
        state.should_resize = 0;
        handle_resize();
    }

    if (state.input.length == 0) return;

    if (state.input.pressed_key[0] == 'q')
    {
        state.should_close = 1;
    }

    
    document_add_char(&state.document, state.input.pressed_key[0], 0, cur_column);
    cur_column++;
}

void render() {

    clear_screen();
    write(STDOUT_FILENO, "\e[H", 3);

    const int row_size = state.window_size.ws_col;

    int start = 0;

    for (size_t i = 0; i < state.document.size; i++)
    {
        const line_t *line = &state.document.lines[i];

        int written = try_write_line(line, start);
        start = ((int)ceilf((float)(start + written)/row_size))*row_size;

        if (written < line->size) break;
    }

    write(STDOUT_FILENO, state.screen, state.screen_length);

    write(STDOUT_FILENO, "\e[H", 3);
    
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