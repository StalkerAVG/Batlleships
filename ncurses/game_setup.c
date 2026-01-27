//to compile g++ game_setup.c -o game_setup.exe -lform -lmenu -lpanel -lncurses

#include <ncurses.h>
#include <string.h>

#define ROWS 10
#define COLS 10

#define COLOR_WATER 1
#define COLOR_SHIP  2
#define COLOR_HOVER 3

#define MAX_OF_BATTLESHIP 2
#define MAX_OF_DESTROYERS 3
#define MAX_OF_SUBMARINE 4

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
            int color = (cell == 'S') ? COLOR_SHIP : COLOR_WATER;
            
            attron(COLOR_PAIR(color));
            mvprintw(4 + r, 4 + (c * 2), "%c", cell); 
            attroff(COLOR_PAIR(color));
        }

        for (int c = 0; c < COLS; c++) {
            char cell = opponent_board[r][c];
            int color = (cell == 'S') ? COLOR_SHIP : COLOR_WATER;
            
            attron(COLOR_PAIR(color));
            mvprintw(4 + r, 44 + (c * 2), "%c", cell);
            attroff(COLOR_PAIR(color));
        }
    }
    
    mvprintw(LINES - 2, 2, "Battle started! Press any key to exit...");
    refresh();
}

bool place_single_ship(char board[ROWS][COLS], int length) {
    int cursor_y = 0;
    int cursor_x = 0;
    bool vertical = false;
    bool placed = false;
    int ch;
    bool collision;

    while (!placed) {
        draw_placement_board(board, cursor_y, cursor_x, length, vertical);
        
        ch = getch();
        switch (ch) {
            case KEY_UP:    if (cursor_y > 0) cursor_y--; break;
            case KEY_DOWN:  
                if (vertical) {
                    if (cursor_y + length < ROWS) cursor_y++;
                } else {
                    if (cursor_y < ROWS - 1) cursor_y++;
                }
                break;
            case KEY_LEFT:  if (cursor_x > 0) cursor_x--; break;
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
            case '\n':
                collision = false;
                for (int i = 0; i < length; i++) {
                    int r = vertical ? cursor_y + i : cursor_y;
                    int c = vertical ? cursor_x : cursor_x + i;
                    if (board[r][c] == 'S') collision = true;
                }

                if (!collision) {
                    for (int i = 0; i < length; i++) {
                        int r = vertical ? cursor_y + i : cursor_y;
                        int c = vertical ? cursor_x : cursor_x + i;
                        board[r][c] = 'S';
                    }
                    placed = true;
                } else {
                    flash();
                }
                break;
            case 'q':
                return false;
        }
    }
    return true;
}

int main() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (has_colors()) {
        start_color();
        init_pair(COLOR_WATER, COLOR_BLUE, COLOR_BLACK);
        init_pair(COLOR_SHIP, COLOR_WHITE, COLOR_BLACK);
        init_pair(COLOR_HOVER, COLOR_GREEN, COLOR_BLACK);
    }

    char my_board[ROWS][COLS];
    for(int i=0; i<ROWS; i++) {
        for(int j=0; j<COLS; j++) {
            my_board[i][j] = '.';
        }
    }

    char opponent_board[ROWS][COLS];
    for(int i=0; i<ROWS; i++) {
        for(int j=0; j<COLS; j++) {
            opponent_board[i][j] = '.';
        }
    }

    place_single_ship(my_board, 4);
    
    for(int i=0; i < MAX_OF_BATTLESHIP; i++){
        place_single_ship(my_board, 3);
    }
    
    for(int i=0; i < MAX_OF_DESTROYERS; i++){
        place_single_ship(my_board, 2);
    }

    for(int i=0; i < MAX_OF_SUBMARINE; i ++){
        place_single_ship(my_board,1);
    }

    refresh();

    mvprintw(5, 70, "Ships placements completed");
    mvprintw(5,70, "Press any key to continue");
    getch();

    draw_battle_screen(my_board, opponent_board);

    refresh();
    
    mvprintw(5, 70, "It's all for now");
    mvprintw(5, 70, "Thank you for choosing our facinating game");
    getch();

    clear();
    refresh();

    endwin();
    return 0;
}