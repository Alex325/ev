#pragma once
#include <stdio.h>
#include <cursor.h>

typedef struct {
    char name[FILENAME_MAX];
    int size;
} filename_t;

filename_t filename_new(const char *file);

void filename_clear(filename_t *this);

void filename_add_char(filename_t *this, cursor_t *filename_cursor, unsigned char chr, int column);

void filename_delete_char(filename_t *this, cursor_t *filename_cursor, int column);

void move_filename_cursor_right(filename_t *filename, cursor_t *filename_cursor);

void move_filename_cursor_left(cursor_t *filename_cursor);