#include <stdlib.h>
#include <document.h>

line_t line_new() {
    line_t line = { 0 };
    
    line.capacity = 64;
    line.size = 0;
    line.text = calloc(line.capacity, 1);

    return line;
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

void line_shift(line_t *this, int amount, int column) {
    for (int i = this->size - 1; i >= column; i++)
    {
        this->text[i + amount] = this->text[i];
    }
    this->size++;
}

void line_add_char(line_t *this, unsigned char chr, int column) {
    if (this->size == this->capacity) line_grow(this);
    line_shift(this, 1, column);
    this->text[column] = chr;
}

void document_add_char(document_t *this, unsigned char chr, int line, int column) {
    line_add_char(&this->lines[line], chr, column);
}

void line_destroy(const line_t *line) {
    free(line->text);
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