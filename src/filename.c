#include <filename.h>
#include <cursor.h>
#include <string.h>
#include <minmax.h>

filename_t filename_new(const char *file) {

    filename_t filename = { 0 };
    
    if (!file)
    {
        filename.size = 0;
    }
    else
    {
        const int filename_size = strlen(file);
        filename.size = filename_size;
        memcpy(filename.name, file, filename_size);
    }
    
    return filename;
}

void filename_shift(filename_t *this, int column) {
    for (int i = this->size - 1; i >= column; i--)
    {
        this->name[i + 1] = this->name[i];
    }
    this->size++;
    this->name[this->size] = 0;
}

void filename_clear(filename_t *this) {
    this->size = 0;
}

void filename_add_char(filename_t *this, cursor_t *filename_cursor, unsigned char chr, int column) {
    if (filename_cursor->column == FILENAME_MAX) return;
    if (this->size == FILENAME_MAX-1) return;
    filename_shift(this, column);
    this->name[column] = chr;
    move_filename_cursor_right(this, filename_cursor);
}

void filename_unshift(filename_t *this, int column) {
    for (int i = column - 1; i < this->size - 1; i++)
    {
        this->name[i] = this->name[i+1];
    }
    this->size--;
    this->name[this->size] = 0;
}

void filename_delete_char(filename_t *this, cursor_t *filename_cursor, int column) {
    if (filename_cursor->column == 0) return;
    if (this->size == 0) return;
    filename_unshift(this, column);
    move_filename_cursor_left(filename_cursor);
}

void move_filename_cursor_right(filename_t *filename, cursor_t *filename_cursor) {
    filename_cursor->column = min(filename_cursor->column + 1, filename->size);    
}

void move_filename_cursor_left(cursor_t *filename_cursor) {
    filename_cursor->column = max(filename_cursor->column - 1, 0);
}