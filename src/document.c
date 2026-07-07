#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <document.h>

int get_newlines(const char *file, const int size) {
    int newlines = 0;
    for (size_t i = 0; i < size; i++)
    {
        if (file[i] == '\n') newlines++;        
    }
    return newlines;
}

void get_sizes(int *sizes, const char *file, const int size) {
    int sizes_idx = 0;

    const char *cur_start = file;

    for (size_t i = 0; i < size; i++)
    {
        if (file[i] == '\n') {
            sizes[sizes_idx++] = &file[i] - cur_start;
            cur_start = &file[i] + 1;
        }        
    }

    sizes[sizes_idx] = (file + size) - cur_start;
}

line_t line_new() {
    line_t line = { 0 };
    
    line.capacity = 64;
    line.size = 0;
    line.text = calloc(line.capacity, 1);

    return line;
}

line_t line_new2(const char *txt, int size) {
    line_t line = { 0 };
    
    line.capacity = size;
    line.size = size;
    line.text = calloc(line.capacity, line.capacity);

    memcpy(line.text, txt, size);

    return line;
}

document_t document_new2(const char *file, const int size) {
    
    const int newlines = get_newlines(file, size) + 1;

    int *sizes = malloc(newlines*sizeof(*sizes));

    get_sizes(sizes, file, size);

    document_t document = { 0 };
    document.capacity = newlines;
    document.size = newlines;
    document.lines = malloc(document.capacity * sizeof(*document.lines));

    int cur_start = 0;

    for (size_t i = 0; i < document.size; i++)
    {
        document.lines[i] = line_new2(&file[cur_start], sizes[i]);
        cur_start += sizes[i] + 1;
    }
    
    free(sizes);

    return document;
}

document_t document_new() {
    document_t document = { 0 };
    document.capacity = 8;
    document.size = 1;
    document.lines = malloc(document.capacity * sizeof(*document.lines));

    for (size_t i = 0; i < document.size; i++)
    {
        document.lines[i] = line_new();
    }
    
    return document;
}

void line_grow(line_t *this) {
    this->capacity *= 2;
    this->text = realloc(this->text, this->capacity);
}

void line_unshift(line_t *this, int column) {
    for (int i = column - 1; i < this->size - 1; i++)
    {
        this->text[i] = this->text[i+1];
    }
    this->size--;
}

void line_shift(line_t *this, int amount, int column) {
    for (int i = this->size - 1; i >= column; i--)
    {
        this->text[i + amount] = this->text[i];
    }
    this->size += amount;
}

void line_add_char(line_t *this, unsigned char chr, int column) {
    if (this->size == this->capacity) line_grow(this);
    line_shift(this, 1, column);
    this->text[column] = chr;
}

void line_add_text(line_t *this, char *txt, int size, int column) {
    while (this->size + size >= this->capacity) line_grow(this);
    line_shift(this, size, column);
    memcpy(this->text + column, txt, size);
}


void line_delete_char(line_t *this, int column) {
    line_unshift(this, column);
}

void line_destroy(const line_t *line) {
    free(line->text);
}

void document_add_char(document_t *this, unsigned char chr, int line, int column) {
    line_add_char(&this->lines[line], chr, column);
}

void document_insert_document(document_t *this, document_t *to_add, int line, int column) {

    size_t i = 0;

    struct
    {
        int line;
        int column;
    } cursor = {
        .line = line,
        .column = column
    };
    
    line_add_text(&this->lines[cursor.line], to_add->lines[i].text, to_add->lines[i].size, cursor.column);
    cursor.column += to_add->lines[i].size;
    document_break_line(this, cursor.line, cursor.column);
    cursor.line++;
    cursor.column = 0;

    i++;

    for (; i < to_add->size - 1; i++)
    {
        line_add_text(&this->lines[cursor.line], to_add->lines[i].text, to_add->lines[i].size, cursor.column);
        cursor.column += to_add->lines[i].size;
        document_break_line(this, cursor.line, cursor.column);
        cursor.line++;
        cursor.column = 0;
    }

    line_add_text(&this->lines[cursor.line], to_add->lines[i].text, to_add->lines[i].size, cursor.column);
    cursor.column += to_add->lines[i].size;
    
}

void document_add_text(document_t *this, const char *text, int size, int line, int column) {
    
    document_t to_add = document_new2(text, size);

    document_insert_document(this, &to_add, line, column);

    document_destroy(&to_add);

}

void document_grow(document_t *this) {
    
    this->capacity *= 2;
    this->lines = realloc(this->lines, this->capacity * sizeof(*this->lines));
}

void document_shift(document_t *this, int line) {
    for (int i = this->size - 1; i > line; i--)
    {
        this->lines[i + 1] = this->lines[i];
    }
    this->size++;    
}

void document_unshift(document_t *this, int line) {
    for (int i = line; i < this->size - 1; i++)
    {
        this->lines[i] = this->lines[i+1];
    }
    this->size--;
}

void document_merge_line(document_t *this, int line) {
    line_add_text(&this->lines[line-1], this->lines[line].text, this->lines[line].size, this->lines[line-1].size);
    const line_t *to_delete = &this->lines[line];
    line_destroy(to_delete);
    document_unshift(this, line);
}

void document_break_line(document_t *this, int line, int column) {
    if (this->size == this->capacity) document_grow(this);
    document_shift(this, line);
    this->lines[line+1] = line_new();
    line_add_text(&this->lines[line+1], this->lines[line].text + column, this->lines[line].size - column, 0),
    this->lines[line].size = column;
}

void document_delete_char(document_t *this, int line, int column) {
    line_delete_char(&this->lines[line], column);
}

save_str_t document_to_string(document_t *this) {
    save_str_t ret = {0};

    int final_size = 0;

    for (size_t i = 0; i < this->size; i++)
    {
        final_size += this->lines[i].size;
    }

    final_size += this->size - 1;
    
    ret.text = malloc(final_size);
    ret.size = final_size;

    int str_ptr = 0;

    for (size_t i = 0; i < this->size; i++)
    {
        memcpy(ret.text + str_ptr, this->lines[i].text, this->lines[i].size);
        str_ptr += this->lines[i].size + 1;
        if (i != this->size-1)
        {
            ret.text[str_ptr-1] = '\n';
        }
        
    }

    return ret;
}

void document_destroy(document_t *document) {

    for (size_t i = 0; i < document->size; i++)
    {
        const line_t *line = &document->lines[i];
        line_destroy(line);
    }
    
    free(document->lines);
    document->size = 0;
    document->lines = NULL;
}