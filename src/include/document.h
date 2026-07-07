#pragma once

typedef struct {
    char *text;
    int size;
} save_str_t;

typedef struct {
    char *text;
    int size;
    int capacity;
} line_t;

typedef struct {
    line_t *lines;
    int size;
    int capacity;
} document_t;

document_t document_new();
void document_add_char(document_t *this, unsigned char chr, int line, int column);
void document_merge_line(document_t *this, int line);
void document_add_text(document_t *this, const char *text, int size, int line, int column);
save_str_t document_to_string(document_t *this);
void document_delete_char(document_t *this, int line, int column);
void document_break_line(document_t *this, int line, int column);
void document_destroy(document_t *document);