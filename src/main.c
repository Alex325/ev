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

#define debug(str) write(STDOUT_FILENO, str, strlen(str))
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
    char pressed_key[9];
    int length;
} input_t;

typedef struct {
    int line;
    int column;
} cursor_t;

struct {
    int row;
    int col;
    int start_line;
    int start_column;
} screen_cursor = {0};

static struct {
    char *screen;
    int screen_length;
    MODE mode;
    input_t input;
    document_t document;
    cursor_t cursor;
    int should_close;
    int should_save;
    int should_resize;
    struct winsize window_size;
} state = { 0 };


void init_state() {
    state.document = document_new();
    state.mode = MODE_EDIT;
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

int try_write_line(const line_t *line, int start, int line_column) {
    int to_write = min(state.screen_length - start, line->size);
    memcpy(state.screen + start, line->text + line_column, to_write);
    return to_write;
}

void add_char(char c, int line, int column) {
    document_add_char(&state.document, c, line, column);
    state.cursor.column++;
}

void break_line(int line, int column) {
    document_break_line(&state.document, line, column);
    state.cursor.line++;
    state.cursor.column = 0;
}

void move_cursor_up() {
    if (state.cursor.column/state.window_size.ws_col == 0) {
        state.cursor.line = max(state.cursor.line - 1, 0);
        state.cursor.column = min(state.cursor.column % state.window_size.ws_col, state.document.lines[state.cursor.line].size);
        return;
    }
    
    state.cursor.column = max(min(state.cursor.column - state.window_size.ws_col, state.document.lines[state.cursor.line].size), 0);
}

void move_cursor_down() {
    if (state.cursor.column/state.window_size.ws_col >= state.document.lines[state.cursor.line].size/state.window_size.ws_col) {
        state.cursor.line = min(state.cursor.line + 1, state.document.size - 1);
        state.cursor.column = min(state.cursor.column % state.window_size.ws_col, state.document.lines[state.cursor.line].size % state.window_size.ws_col);
        return;
    }
    
    state.cursor.column = min(state.cursor.column + state.window_size.ws_col, state.document.lines[state.cursor.line].size);
}

void move_cursor_right() {
    state.cursor.column++;
    if (state.cursor.column > state.document.lines[state.cursor.line].size)
    {
        state.cursor.column = state.cursor.line < state.document.size - 1 ? 0 : state.document.lines[state.cursor.line].size;
        state.cursor.line = min(state.cursor.line + 1, state.document.size - 1);
    }
    
}

void move_cursor_left() {
    state.cursor.column--;
    if (state.cursor.column < 0)
    {
        state.cursor.column = state.cursor.line > 0 ? state.document.lines[state.cursor.line].size : 0;
        state.cursor.line = max(state.cursor.line - 1, 0);
    }
}

void handle_edit_input(const input_t *input) {
    if (input->length == 1)
    {
        const char key = input->pressed_key[0];
        
        if (key >= 0x20 && key <= 0x7e)
        {
            add_char(key, state.cursor.line, state.cursor.column);
            return;
        }
        
        switch (key)
        {
            case 0xa:
                break_line(state.cursor.line, state.cursor.column);
                break;
        }
    }
    else
    {
        if (input->pressed_key[0] == 0x1b)
        {
            if (input->pressed_key[1] == '[')
            {
                switch (input->pressed_key[2])
                {
                case 'A':
                    move_cursor_up();
                    break;
                case 'B':
                    move_cursor_down();
                    break;
                case 'C':
                    move_cursor_right();
                    break;
                case 'D':
                    move_cursor_left();
                    break;
                
                default:
                    break;
                }

            }            
        }
        
    }
}

void handle_command_input(const input_t *input) {

}

void poll_input() {

    state.input.length = 0;

    if (poll(&pfd, 1, -1) <= 0) return;

    int bytes_read = read(STDIN_FILENO, state.input.pressed_key, 9);
    state.input.length = bytes_read;
}

void update() {

    if (state.should_resize)
    {
        state.should_resize = 0;
        handle_resize();
    }

    if (state.input.length == 0) return;

    if (state.input.pressed_key[0] == 0x3)
    {
        state.should_close = 1;
        return;
    }

    switch (state.mode)
    {
        case MODE_EDIT:
            handle_edit_input(&state.input);
            break;
        case MODE_COMMAND:
            handle_command_input(&state.input);
            break;
    }


}

void render() {

    clear_screen();
    write(STDOUT_FILENO, "\e[H", 3);

    const int row_size = state.window_size.ws_col;

    int start = 0;

    int cached_start = 0;

    for (size_t i = screen_cursor.start_line; i < state.document.size; i++)
    {
        const line_t *line = &state.document.lines[i];

        int written = try_write_line(line, start, i == screen_cursor.start_line ? screen_cursor.start_column : 0);

        if (i == state.cursor.line) cached_start = start;
        if (written < line->size) break;
        start = ((int)ceilf((float)(start + written)/row_size))*row_size;
        if (written == 0) start += row_size;
        
    }

    const int screen_offset = cached_start + state.cursor.column - (state.cursor.line == screen_cursor.start_line ? screen_cursor.start_column : 0);

    screen_cursor.row = screen_offset / row_size + 1;
    screen_cursor.col = screen_offset % row_size + 1;

    char cur_seq[12] = "";

    sprintf(cur_seq, "\e[%d;%dH", screen_cursor.row, screen_cursor.col);
    write(STDOUT_FILENO, state.screen, state.screen_length);
    write(STDOUT_FILENO, cur_seq, strlen(cur_seq));
    
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