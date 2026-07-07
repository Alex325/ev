#include <sys/termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <fcntl.h>
#include <document.h>
#include <cursor.h>
#include <filename.h>
#include <minmax.h>

#define writel(str) write(STDOUT_FILENO, str, strlen(str))


static struct pollfd pfd = {
    .fd = STDIN_FILENO,
    .events = POLLIN
};

static struct termios orig;

typedef enum MODE {
    MODE_COMMAND,
    MODE_EDIT,
    MODE_SAVE,
    MODE_INSERT_FILE
} MODE;

typedef enum {
    ACTION_BACKSPACE,
    ACTION_DELETE
} DELETE_ACTION;

typedef struct
{
    char pressed_key[9];
    int length;
} input_t;

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
    cursor_t filename_open_cursor;
    cursor_t filename_insert_cursor;
    int should_close;
    int should_resize;
    struct winsize window_size;
    filename_t filename_open;
    filename_t filename_insert;
} state = { 0 };


void file_insert(const char* filename) {
    int file_fd = open(filename, O_RDONLY);
    if (file_fd < 0) return;

    struct stat file_info;

    if (fstat(file_fd, &file_info) == 0)
    {
        const int size = file_info.st_size;
        char *file_cont = malloc(size*sizeof(*file_cont));
        int red = read(file_fd, file_cont, size);
        document_add_text(&state.document, file_cont, red, state.cursor.line, state.cursor.column);
        free(file_cont);
    }

    close(file_fd);
}

void hide_cursor() {
    writel("\e[?25l");
}

void show_cursor() {
    writel("\e[?25h");
}

void init_state(const char *filename) {
    state.document = document_new();
    state.filename_open = filename_new(filename);
    state.filename_insert = filename_new(NULL);

    if (state.filename_open.size > 0) {
        file_insert(state.filename_open.name);
    }

    state.mode = MODE_COMMAND;
    hide_cursor();
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
    writel("\x1b[?1049l");
}

void enable_raw_terminal() {
    struct termios terminal = orig;
    terminal.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    terminal.c_oflag &= ~(OPOST) | OPOST;
    terminal.c_cc[VTIME] = 0;
    terminal.c_cc[VMIN] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal);
    writel("\x1b[?1049h");
    writel("\x1b[H");
}


void cleanup() {
    disable_raw_terminal();
    show_cursor();
    document_destroy(&state.document);
}

void setup(const char *filename) {
    tcgetattr(STDIN_FILENO, &orig);
    atexit(cleanup);
    enable_raw_terminal();
    init_state(filename);
    signal(SIGWINCH, on_window_resize);
    handle_resize();
}

void set_mode(MODE mode) {
    state.mode = mode;
}

void ensure_cursor_visible()
{
    while (state.cursor.line < screen_cursor.start_line)
        screen_cursor.start_line--;

    while (1)
    {
        int rows = 0;

        for (int i = screen_cursor.start_line;
             i <= state.cursor.line;
             i++)
        {
            rows += max(
                1,
                (state.document.lines[i].size +
                 state.window_size.ws_col - 1)
                / state.window_size.ws_col
            );
        }

        if (rows <= state.window_size.ws_row)
            break;

        screen_cursor.start_line++;
    }
}

int try_write_line(const line_t *line, int start, int line_column) {
    int to_write = min(state.screen_length - start, line->size);
    memcpy(state.screen + start, line->text + line_column, to_write);
    return to_write;
}
int try_write(const filename_t *line, int start, int start_column) {
    int to_write = min(state.screen_length - start - 1, line->size);
    memcpy(state.screen + start, line->name + start_column, to_write);
    return to_write;
}

void add_char(char c, int line, int column) {
    document_add_char(&state.document, c, line, column);
    state.cursor.column++;
}

void delete_char(DELETE_ACTION action, int line, int column) {

    switch (action)
    {
        case ACTION_BACKSPACE:
            if (column == 0)
            {
                if (line == 0) return;
        
                const int new_column = state.document.lines[line-1].size;
        
                document_merge_line(&state.document, line);
                
                state.cursor.line--;
                state.cursor.column = new_column;
        
                return;
            }
            
            document_delete_char(&state.document, line, column);
            state.cursor.column--;
        break;
    
        case ACTION_DELETE:
            if (column == state.document.lines[line].size)
            {
                if (line == state.document.size-1) return;
                document_merge_line(&state.document, line+1);
                return;
            }
            
            document_delete_char(&state.document, line, column+1);

        default:
            break;
    }
}

void break_line(int line, int column) {
    document_break_line(&state.document, line, column);
    state.cursor.line++;
    state.cursor.column = 0;
}

void move_screen_cursor_up() {
    if (state.cursor.column/state.window_size.ws_col == 0) {
        state.cursor.line = max(state.cursor.line - 1, 0);
        state.cursor.column = min(state.cursor.column % state.window_size.ws_col, state.document.lines[state.cursor.line].size);
        return;
    }
    
    state.cursor.column = max(min(state.cursor.column - state.window_size.ws_col, state.document.lines[state.cursor.line].size), 0);
}

void move_screen_cursor_down() {
    if (state.cursor.column/state.window_size.ws_col >= state.document.lines[state.cursor.line].size/state.window_size.ws_col) {
        state.cursor.line = min(state.cursor.line + 1, state.document.size - 1);
        state.cursor.column = min(state.cursor.column % state.window_size.ws_col, state.document.lines[state.cursor.line].size % state.window_size.ws_col);
        return;
    }
    
    state.cursor.column = min(state.cursor.column + state.window_size.ws_col, state.document.lines[state.cursor.line].size);
}

void move_screen_cursor_right() {
    state.cursor.column++;
    if (state.cursor.column > state.document.lines[state.cursor.line].size)
    {
        state.cursor.column = state.cursor.line < state.document.size - 1 ? 0 : state.document.lines[state.cursor.line].size;
        state.cursor.line = min(state.cursor.line + 1, state.document.size - 1);
    }
    
}

void move_screen_cursor_left() {
    state.cursor.column--;
    if (state.cursor.column < 0)
    {
        state.cursor.column = state.cursor.line > 0 ? state.document.lines[state.cursor.line - 1].size : 0;
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
            case 0x7f:
                delete_char(ACTION_BACKSPACE, state.cursor.line, state.cursor.column);
                break;
            case 0x1b:
                set_mode(MODE_COMMAND);
                hide_cursor();
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
                    move_screen_cursor_up();
                    break;
                case 'B':
                    move_screen_cursor_down();
                    break;
                case 'C':
                    move_screen_cursor_right();
                    break;
                case 'D':
                    move_screen_cursor_left();
                    break;
                case '3':
                    if (input->pressed_key[3] == '~')
                        delete_char(ACTION_DELETE, state.cursor.line, state.cursor.column);
                    break;
                default:
                    break;
                }
            }
        }
    }
}

void handle_command_input(const input_t *input) {
    if (input->length != 1) return;

    const char key = input->pressed_key[0];

    switch (key)
    {
    case 'e':
        set_mode(MODE_EDIT);
        show_cursor();
        break;
    case 's':
        set_mode(MODE_SAVE);
        show_cursor();
        break;
    case 'a':
        set_mode(MODE_INSERT_FILE);
        show_cursor();
        break;
    case 'q':
        state.should_close = 1;
        break;
    default:
        break;
    }
    
}

int is_allowed_character(unsigned int chr) {
    return (chr >= 'A' && chr <= 'Z') || (chr >= 'a' && chr <= 'z') || (chr >= '0' && chr <= '9') || chr == '.' || chr == ',' || chr == '_' || chr == ' ';
}

void file_save() {
    int fd = open(state.filename_open.name, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    save_str_t to_save = document_to_string(&state.document);
    write(fd, to_save.text, to_save.size);
    close(fd);
    free(to_save.text);
}

void handle_save_input(const input_t *input) {
    if (input->length == 1) {
        const char key = input->pressed_key[0];

        if (is_allowed_character(key))
        {
            filename_add_char(&state.filename_open, &state.filename_open_cursor, key, state.filename_open_cursor.column);
            return;
        }
        

        switch (key)
        {
        case 0x1b:
            set_mode(MODE_COMMAND);
            hide_cursor();
            break;
        case 0x7f:
            filename_delete_char(&state.filename_open, &state.filename_open_cursor, state.filename_open_cursor.column);
            break;
        case 0xa:
            file_save();
            set_mode(MODE_COMMAND);
            hide_cursor();
            break;

        default:
            break;
        }
    }
    else {
        if (input->pressed_key[0] == 0x1b)
        {
            if (input->pressed_key[1] == '[')
            {
                switch (input->pressed_key[2])
                {
                case 'C':
                    move_filename_cursor_right(&state.filename_open, &state.filename_open_cursor);
                    break;
                case 'D':
                    move_filename_cursor_left(&state.filename_open_cursor);
                    break;
                }
            }
        }
    }
}

void handle_insert_input(const input_t *input) {
    if (input->length == 1) {
        const char key = input->pressed_key[0];

        if (is_allowed_character(key))
        {
            filename_add_char(&state.filename_insert, &state.filename_insert_cursor, key, state.filename_insert_cursor.column);
            return;
        }
        

        switch (key)
        {
        case 0x1b:
            set_mode(MODE_COMMAND);
            hide_cursor();
            break;
        case 0x7f:
            filename_delete_char(&state.filename_insert, &state.filename_insert_cursor, state.filename_insert_cursor.column);
            break;
        case 0xa:
            file_insert(state.filename_insert.name);
            filename_clear(&state.filename_insert);
            state.filename_insert_cursor.column = 0;
            set_mode(MODE_COMMAND);
            hide_cursor();
            break;

        default:
            break;
        }
    }
    else {
        if (input->pressed_key[0] == 0x1b)
        {
            if (input->pressed_key[1] == '[')
            {
                switch (input->pressed_key[2])
                {
                case 'C':
                    move_filename_cursor_right(&state.filename_insert, &state.filename_insert_cursor);
                    break;
                case 'D':
                    move_filename_cursor_left(&state.filename_insert_cursor);
                    break;
                }
            }
        }
    }
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
        case MODE_SAVE:
            handle_save_input(&state.input);
            break;
        case MODE_INSERT_FILE:
            handle_insert_input(&state.input);
            break;
    }

    ensure_cursor_visible();

}

void render_edit(int row_size) {
    int start = 0;
    int cached_start = 0;

    for (size_t i = screen_cursor.start_line; i < state.document.size; i++)
    {
        if (start >= state.screen_length) break;

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
}

#define strl(str) str, strlen(str)
void render_command(int row_size) {
    memcpy(state.screen, strl("EDITOR VISUAL"));
    memcpy(state.screen + row_size, strl("s = salvar"));
    memcpy(state.screen + 2*row_size, strl("e = editar"));
    memcpy(state.screen + 3*row_size, strl("a = inserir arquivo"));
    memcpy(state.screen + 4*row_size, strl("q = sair"));
}

void render_save(int row_size) {
    memcpy(state.screen, strl("EDITOR VISUAL"));
    memcpy(state.screen + row_size, strl("Digite um nome"));
    memcpy(state.screen + 2*row_size, strl("esc = modo de comando"));
    memcpy(state.screen + 3*row_size, strl("enter = salvar"));
    try_write(&state.filename_open, state.screen_length - row_size, max(state.filename_open_cursor.column - row_size, 0));

    screen_cursor.col = state.filename_open_cursor.column + 1;
    screen_cursor.row = state.window_size.ws_row;

}

void render_insert(int row_size) {
    memcpy(state.screen, strl("EDITOR VISUAL"));
    memcpy(state.screen + row_size, strl("Digite um nome"));
    memcpy(state.screen + 2*row_size, strl("esc = modo de comando"));
    memcpy(state.screen + 3*row_size, strl("enter = abrir"));
    try_write(&state.filename_insert, state.screen_length - row_size, max(state.filename_insert_cursor.column - row_size, 0));

    screen_cursor.col = state.filename_insert_cursor.column + 1;
    screen_cursor.row = state.window_size.ws_row;

}

void render() {
    const int row_size = state.window_size.ws_col;
    clear_screen();
    write(STDOUT_FILENO, "\e[H", 3);

    switch (state.mode) {
        case MODE_EDIT:
            render_edit(row_size);
            break;
        case MODE_COMMAND:
            render_command(row_size);
            break;
        case MODE_SAVE:
            render_save(row_size);
            break;
        case MODE_INSERT_FILE:
            render_insert(row_size);
            break;
    }

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

int main(int argc, char **argv) {
    setup(argv[1]);
    render();
    loop();
}