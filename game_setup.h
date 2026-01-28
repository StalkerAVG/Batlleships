#ifndef GAME_SETUP_H
#define GAME_SETUP_H

#include <ncurses.h>
#include <stdbool.h>

#define ROWS 10
#define COLS 10
#define COLOR_WATER 1
#define COLOR_SHIP  2
#define COLOR_HOVER 3
#define COLOR_HIT   4
#define COLOR_MISS  5
#define MAX_OF_BATTLESHIP 2
#define MAX_OF_DESTROYERS 3
#define MAX_OF_SUBMARINE 4

struct coordinates_pair {
    int row;
    int col;
};

struct ship_placement {
    int row;
    int col;
    int length;
    bool vertical;
};

void draw_ship(char board[ROWS][COLS], int length, int cursor_x, int cursor_y, bool vertical);
void draw_placement_board(char board[ROWS][COLS], int cursor_y, int cursor_x, int length, bool vertical);
void draw_battle_screen(char my_board[ROWS][COLS], char opponent_board[ROWS][COLS]);
struct ship_placement place_single_ship(char board[ROWS][COLS], int length);
void draw_shooting_board(char board[ROWS][COLS], int cursor_y, int cursor_x);
struct coordinates_pair choose_shot(char my_board[ROWS][COLS], char fight_board[ROWS][COLS]);
int receive_shot(char my_board[ROWS][COLS], int row, int col);
void update_fight_board(char fight_board[ROWS][COLS], int row, int col, bool hit);
void draw_waiting_screen(const char* message);
void draw_spectator_board(char p1_board[ROWS][COLS], char p2_board[ROWS][COLS], char* p1_name, char* p2_name);

#endif
