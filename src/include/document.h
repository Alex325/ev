#pragma once

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
void document_destroy(document_t *document);