//to compile g++ game_setup.c -o game_setup.exe -lform -lmenu -lpanel -lncurses

#include <ncurses.h>
#include <string.h>
#include "game_setup.h"

void draw_ship(char board[ROWS][COLS], int length, int cursor_x, int cursor_y, bool vertical){
	for (int i = 0; i < length; i++) {
		int r = vertical ? cursor_y + i : cursor_y;
		int c = vertical ? cursor_x : cursor_x + i;
		board[r][c] = 'S';
	}
}

void draw_placement_board(char board[ROWS][COLS], int cursor_y, int cursor_x, int length, bool vertical) {
    clear();
    mvprintw(0, 0, "--- PLACE YOUR SHIPS ---");
    mvprintw(1, 0, "Arrows: Move | 'r': Rotate | Space: Place | 'q': Quit");
    mvprintw(3, 4, "0 1 2 3 4 5 6 7 8 9");

    for (int r = 0; r < ROWS; r++) {
        mvprintw(4 + r, 0, "%c ", 'A' + r);
        for (int c = 0; c < COLS; c++) {
            bool is_cursor_part = false;
            
            if (vertical) {
                if (c == cursor_x && r >= cursor_y && r < cursor_y + length)
                    is_cursor_part = true;
            } else {
                if (r == cursor_y && c >= cursor_x && c < cursor_x + length)
                    is_cursor_part = true;
            }

            if (is_cursor_part) {
                attron(COLOR_PAIR(COLOR_HOVER) | A_BOLD);
                mvprintw(4 + r, 4 + (c * 2), "S");
                attroff(COLOR_PAIR(COLOR_HOVER) | A_BOLD);
            } else if (board[r][c] == 'S') {
                attron(COLOR_PAIR(COLOR_SHIP));
                mvprintw(4 + r, 4 + (c * 2), "S");
                attroff(COLOR_PAIR(COLOR_SHIP));
            } else {
                attron(COLOR_PAIR(COLOR_WATER));
                mvprintw(4 + r, 4 + (c * 2), ".");
                attroff(COLOR_PAIR(COLOR_WATER));
            }
        }
    }
    refresh();
}

void draw_battle_screen(char my_board[ROWS][COLS], char opponent_board[ROWS][COLS]) {
    clear();

    attron(A_BOLD);
    mvprintw(1, 5, "MY FLEET");
    mvprintw(1, 45, "ENEMY WATERS");
    attroff(A_BOLD);

    mvprintw(3, 4, "0 1 2 3 4 5 6 7 8 9");
    mvprintw(3, 44, "0 1 2 3 4 5 6 7 8 9");

    for (int r = 0; r < ROWS; r++) {
        mvprintw(4 + r, 0, "%c", 'A' + r);
        mvprintw(4 + r, 40, "%c", 'A' + r);

        for (int c = 0; c < COLS; c++) {
            char cell = my_board[r][c];
            int color;
            if (cell == 'X') color = COLOR_HIT;
            else if (cell == 'O') color = COLOR_MISS;
            else if (cell == 'S') color = COLOR_SHIP;
            else color = COLOR_WATER;
            
            attron(COLOR_PAIR(color));
            mvprintw(4 + r, 4 + (c * 2), "%c", cell); 
            attroff(COLOR_PAIR(color));
        }

        for (int c = 0; c < COLS; c++) {
            char cell = opponent_board[r][c];
            int color;
            if (cell == 'X') color = COLOR_HIT;
            else if (cell == 'O') color = COLOR_MISS;
            else color = COLOR_WATER;
            
            attron(COLOR_PAIR(color));
            mvprintw(4 + r, 44 + (c * 2), "%c", cell);
            attroff(COLOR_PAIR(color));
        }
    }
    refresh();
}

struct ship_placement place_single_ship(char board[ROWS][COLS], int length) {
    int cursor_y = 0;
    int cursor_x = 0;
    bool vertical = false;
    int ch;
    bool collision;

    while (true) {
        draw_placement_board(board, cursor_y, cursor_x, length, vertical);
        
        ch = getch();
        switch (ch) {
            case KEY_UP:
                if (cursor_y > 0) cursor_y--;
                break;
            case KEY_DOWN:  
                if (vertical) {
                    if (cursor_y + length < ROWS) cursor_y++;
                } else {
                    if (cursor_y < ROWS - 1) cursor_y++;
                }
                break;
            case KEY_LEFT:
                if (cursor_x > 0) cursor_x--;
                break;
            case KEY_RIGHT: 
                if (vertical) {
                    if (cursor_x < COLS - 1) cursor_x++;
                } else {
                    if (cursor_x + length < COLS) cursor_x++;
                }
                break;
            case 'r':
            case 'R':
                vertical = !vertical;
                if (vertical && cursor_y + length > ROWS) cursor_y = ROWS - length;
                if (!vertical && cursor_x + length > COLS) cursor_x = COLS - length;
                break;
            case ' ':
            case '\n':{
                collision = false;
                for (int i = 0; i < length; i++) {
                    int r = vertical ? cursor_y + i : cursor_y;
                    int c = vertical ? cursor_x : cursor_x + i;
                    if (board[r][c] == 'S') collision = true;
                }

                if (!collision) {
                    struct ship_placement placement = {cursor_y, cursor_x, length, vertical};
                    return placement;
                } else {
                    flash();
                }
                break;
			}
            case 'q': {
                struct ship_placement quit = {-1, -1, -1, false};
                return quit;
            }
        }
    }
}

void draw_shooting_board(char board[ROWS][COLS], int cursor_y, int cursor_x) {
    clear();
    mvprintw(0, 0, "--- SHOOT AT ENEMY ---");
    mvprintw(1, 0, "Arrows: Move | Space: Shoot | 'q': Quit");
    mvprintw(3, 4, "0 1 2 3 4 5 6 7 8 9");

    for (int r = 0; r < ROWS; r++) {
        mvprintw(4 + r, 0, "%c ", 'A' + r);
        for (int c = 0; c < COLS; c++) {
            
            if (r == cursor_y && c == cursor_x) {
                attron(COLOR_PAIR(COLOR_HOVER) | A_BOLD);
                mvprintw(4 + r, 4 + (c * 2), "+");
                attroff(COLOR_PAIR(COLOR_HOVER) | A_BOLD);
            } else if (board[r][c] == 'X') {
                attron(COLOR_PAIR(COLOR_HIT) | A_BOLD);
                mvprintw(4 + r, 4 + (c * 2), "X");
                attroff(COLOR_PAIR(COLOR_HIT) | A_BOLD);
            } else if (board[r][c] == 'O') {
                attron(COLOR_PAIR(COLOR_MISS));
                mvprintw(4 + r, 4 + (c * 2), "O");
                attroff(COLOR_PAIR(COLOR_MISS));
            } else {
                attron(COLOR_PAIR(COLOR_WATER));
                mvprintw(4 + r, 4 + (c * 2), ".");
                attroff(COLOR_PAIR(COLOR_WATER));
            }
        }
    }
    refresh();
}

struct coordinates_pair choose_shot(char my_board[ROWS][COLS], char fight_board[ROWS][COLS]) {
    int cursor_y = 0;
    int cursor_x = 0;
    int ch;
    
    while (true) {
        draw_battle_screen(my_board, fight_board);

        attron(A_BOLD);
        mvprintw(0, 0, "--- YOUR TURN: FIRE! ---");
        mvprintw(1, 0, "Arrow Keys: Aim | Space: Shooooot");
        attroff(A_BOLD);

        int screen_y = 4 + cursor_y;
        int screen_x = 44 + (cursor_x * 2);

        attron(COLOR_PAIR(COLOR_HOVER) | A_BOLD);
        mvprintw(screen_y, screen_x, "+");
        attroff(COLOR_PAIR(COLOR_HOVER) | A_BOLD);

        refresh();
        
        ch = getch();
        switch (ch) {
            case KEY_UP:    if (cursor_y > 0) cursor_y--; break;
            case KEY_DOWN:  if (cursor_y < ROWS - 1) cursor_y++; break;
            case KEY_LEFT:  if (cursor_x > 0) cursor_x--; break;
            case KEY_RIGHT: if (cursor_x < COLS - 1) cursor_x++; break;
            case ' ':
            case '\n': {
                if (fight_board[cursor_y][cursor_x] == 'X' || 
                    fight_board[cursor_y][cursor_x] == 'O') {
                    flash();
                    break;
                }
                struct coordinates_pair shot = {cursor_y, cursor_x};
                return shot;
            }
            case 'q': {
                struct coordinates_pair quit = {-1, -1};
                return quit;
            }
        }
    }
}

int receive_shot(char my_board[ROWS][COLS], int row, int col) {
    if (my_board[row][col] == 'S') {
        my_board[row][col] = 'X';
        return 1;
    } else {
        my_board[row][col] = 'O';
        return 0;
    }
}

void update_fight_board(char fight_board[ROWS][COLS], int row, int col, bool hit) {
    fight_board[row][col] = hit ? 'X' : 'O';
}

void draw_waiting_screen(const char* message) {
    clear();
    mvprintw(ROWS / 2, (COLS * 2) / 2 - 10, "%s", message);
    refresh();
}

void draw_spectator_board(char p1_board[ROWS][COLS], char p2_board[ROWS][COLS], char* p1_name, char* p2_name) {
    clear();
    mvprintw(1, 5, "PLAYER 1: %s", p1_name);
    mvprintw(1, 45, "PLAYER 2: %s", p2_name);

    mvprintw(3, 4, "0 1 2 3 4 5 6 7 8 9");
    mvprintw(3, 44, "0 1 2 3 4 5 6 7 8 9");

    for (int r = 0; r < ROWS; r++) {
        mvprintw(4 + r, 0, "%c", 'A' + r);
        mvprintw(4 + r, 40, "%c", 'A' + r);

        for (int c = 0; c < COLS; c++) {
            char cell = p1_board[r][c];
            int color = (cell == 'X') ? COLOR_HIT : (cell == 'S' ? COLOR_SHIP : (cell == 'O' ? COLOR_MISS : COLOR_WATER));
            attron(COLOR_PAIR(color));
            mvprintw(4 + r, 4 + (c * 2), "%c", cell); 
            attroff(COLOR_PAIR(color));
        }

        for (int c = 0; c < COLS; c++) {
            char cell = p2_board[r][c];
            int color = (cell == 'X') ? COLOR_HIT : (cell == 'S' ? COLOR_SHIP : (cell == 'O' ? COLOR_MISS : COLOR_WATER));
            attron(COLOR_PAIR(color));
            mvprintw(4 + r, 44 + (c * 2), "%c", cell); 
            attroff(COLOR_PAIR(color));
        }
    }
    refresh();
}
