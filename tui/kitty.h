#pragma once
#include "chart.h"

/* Transmit img to the terminal at the current cursor position
 * using the Kitty graphics protocol (action=T, format=RGBA).
 * id: unique image id (1-based, reused to overwrite on refresh). */
void kitty_display(const image_t *img, int id);

/* Move terminal cursor to row, col (1-based). */
void term_move(int row, int col);

/* Clear screen and hide cursor. */
void term_init(void);

/* Show cursor and reset terminal. */
void term_reset(void);

/* Get terminal size in cells. */
void term_size(int *cols, int *rows);
