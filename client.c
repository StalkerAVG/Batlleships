#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "game_setup.h"
#include <errno.h>

#define MULTICAST_GROUP "239.0.0.1"
#define MULTICAST_PORT 5000
#define BUFFER_SIZE 256
#define BUFFER_SIZE_N 1024

typedef enum {
    STATE_CONNECTING,
    STATE_WAITING_OPPONENT,
    STATE_PLACING_SHIPS,
    STATE_WAITING_READY,
    STATE_MY_TURN,
    STATE_OPPONENT_TURN,
	STATE_SPECTATING,
    STATE_GAME_OVER
} ClientState;

ClientState state = STATE_CONNECTING;

bool send_placement_data(struct ship_placement plc, int sockfd, char *buffer){
	char placement_buff[64];
	int len = snprintf(placement_buff, sizeof(placement_buff), "PLACE_SHIP|%d,%d,%d,%c\n",
	    plc.row,
		plc.col,
		plc.length,
		plc.vertical ? 'V' : 'H');
	write(sockfd, placement_buff, len);

	ssize_t n = read(sockfd, buffer, BUFFER_SIZE - 1);
	buffer[n] = '\0';
	if (strncmp(buffer, "SHIP_PLACED", 11) == 0) {
			return true;
	}
	return false;
}
int main() {
    struct sockaddr_in addr;
    socklen_t addrlen;
    int sock;
    ssize_t cnt;
    struct ip_mreq mreq;
    char message[BUFFER_SIZE];
    char server_ip[INET_ADDRSTRLEN];
    int server_tcp_port;

    // Create UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(1);
    }

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(MULTICAST_PORT);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }

    mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_GROUP);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        perror("setsockopt multicast");
        exit(1);
    }

    printf("Waiting for server announcement on %s:%d...\n", MULTICAST_GROUP, MULTICAST_PORT);

    addrlen = sizeof(addr);
    cnt = recvfrom(sock, message, sizeof(message) - 1, 0,
                   (struct sockaddr *)&addr, &addrlen);
    
    if (cnt < 0) {
        perror("recvfrom");
        exit(1);
    }

    message[cnt] = '\0';
    strcpy(server_ip, inet_ntoa(addr.sin_addr));
    
    if (sscanf(message, "BATTLESHIP_SERVER|%d|", &server_tcp_port) == 1) {
        printf("Server found!\n");
        printf("  IP: %s\n", server_ip);
        printf("  TCP Port: %d\n", server_tcp_port);
        printf("  Message: %s\n", message);
    } else {
        printf("Unknown message: %s\n", message);
        exit(1);
    }

    close(sock);
	int sockfd;
	struct sockaddr_in servaddr;
	int err;
	char buffer[BUFFER_SIZE_N];
	ssize_t n;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);  

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(server_tcp_port);

	while(1){
		printf("\nPress Enter to connect...\n");
		int ch = getchar();
		if (ch == '\n' || ch == '\r');
	
		if (ch == '\n' || ch == '\r') {
			printf("Connecting...\n");
			break;
		} else {
			printf("You pressed something else: %c\n", ch);
			continue;
		}
	}
	if ( (err=inet_pton(AF_INET, server_ip, &servaddr.sin_addr)) == -1){
		fprintf(stderr,"ERROR: inet_pton error for %s : %s \n", server_ip, strerror(errno));
		return 1;
	}else if(err == 0){
		fprintf(stderr,"ERROR: Invalid address family \n");
		return 1;
	}

	if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
		fprintf(stderr,"connect error : %s \n", strerror(errno));
		return 1;
	}

	n = read(sockfd, buffer, BUFFER_SIZE - 1);
	if (n > 0) {
    		buffer[n] = '\0';
    
    		if (strncmp(buffer, "WELCOME|", 8) == 0) {
       		 printf("Connected to server!\n");
    		}
	}
	char username[50];
	printf("Enter username: ");
	scanf("%s", username);
	
	char cmd[100];
	snprintf(cmd, sizeof(cmd), "LOGIN|%s\n", username);
	write(sockfd, cmd, strlen(cmd));
	
	printf("\n--- MENU ---\n");
    printf("1. Play Game\n");
    printf("2. Spectate Game\n");
    printf("Choice: ");
    int choice;
    scanf("%d", &choice);
	
	if (choice == 2) {
		n = read(sockfd, buffer, BUFFER_SIZE - 1);
		buffer[n] = '\0';
		if (strncmp(buffer, "LOGIN_OK", 8) != 0) {
			printf("Login failed: %s\n", buffer);
			return 1;
		}
		
		write(sockfd, "LIST_GAMES\n", 11);
        n = read(sockfd, buffer, BUFFER_SIZE - 1);
        buffer[n] = '\0';
        printf("%s", buffer);

        int gameID;
        printf("Enter Game ID to spectate: ");
        scanf("%d", &gameID);

        char spec_cmd[64];
        snprintf(spec_cmd, sizeof(spec_cmd), "SPECTATE|%d\n", gameID);
        write(sockfd, spec_cmd, strlen(spec_cmd));

        n = read(sockfd, buffer, BUFFER_SIZE - 1);
        buffer[n] = '\0';
        if (strncmp(buffer, "SPECTATE_OK", 11) == 0) {
            initscr(); 
            start_color();
			cbreak();
			noecho();
			keypad(stdscr, TRUE);
			start_color();
            init_pair(COLOR_WATER, COLOR_BLUE, COLOR_BLACK);
            init_pair(COLOR_SHIP, COLOR_WHITE, COLOR_BLACK);
            init_pair(COLOR_HIT, COLOR_RED, COLOR_BLACK);
            init_pair(COLOR_MISS, COLOR_CYAN, COLOR_BLACK);
            curs_set(0);
			
			while (1) {
				n = read(sockfd, buffer, sizeof(buffer) - 1);
				if (n <= 0) break;
				buffer[n] = '\0';

				if (strncmp(buffer, "SPEC_TABLE", 10) == 0) {
					char p1_name[50], p2_name[50];
					char p1_board[ROWS][COLS], p2_board[ROWS][COLS];
					
					char *line = strtok(buffer, "\n");
					
					line = strtok(NULL, "\n");
					sscanf(line, "P1|%s", p1_name);
					
					line = strtok(NULL, "\n");
					
					for(int i=0; i<ROWS; i++) {
						line = strtok(NULL, "\n");
						char *ptr = line;
						for(int j=0; j<COLS; j++) {
							int val = strtol(ptr, &ptr, 10);
							if (val == 0) p1_board[i][j] = '.';    
							else if (val == 1) p1_board[i][j] = 'S';
							else if (val == 2) p1_board[i][j] = 'X';
							else if (val == 3) p1_board[i][j] = 'O';
							else if (val == 4) p1_board[i][j] = 'X';
						}
					}

					line = strtok(NULL, "\n"); 
					sscanf(line, "P2|%s", p2_name);
					line = strtok(NULL, "\n"); 

					for(int i=0; i<ROWS; i++) {
						line = strtok(NULL, "\n");
						char *ptr = line;
						for(int j=0; j<COLS; j++) {
							int val = strtol(ptr, &ptr, 10);
							if (val == 0) p2_board[i][j] = '.';
							else if (val == 1) p2_board[i][j] = 'S';
							else if (val == 2) p2_board[i][j] = 'X';
							else if (val == 3) p2_board[i][j] = 'O';
							else if (val == 4) p2_board[i][j] = 'X';
						}
					}

					draw_spectator_board(p1_board, p2_board, p1_name, p2_name);
				}
				
			}
			endwin();
        } else {
            printf("Failed to spectate: %s\n", buffer);
            return 1;
        }
		close(sockfd);
		return 0;
	} else {
		n = read(sockfd, buffer, BUFFER_SIZE - 1);
		buffer[n] = '\0';
		if (strncmp(buffer, "LOGIN_OK", 8) == 0) printf("Logged in !\n");
		bool immediate_start = false;
        if (strstr(buffer, "GAME_START") != NULL) {
            immediate_start = true;
            printf("Game started immediately!\n");
        } else {
		    printf("Waiting for opponent...\n");
        }
				
		ClientState state = STATE_CONNECTING;
		char pair_buff[64];
		
		char my_board[ROWS][COLS];
        char fight_board[ROWS][COLS];

        for(int i=0; i<ROWS; i++) {
            for(int j=0; j<COLS; j++) {
                my_board[i][j] = '.';
                fight_board[i][j] = '.';
            }
        }

		while (state != STATE_GAME_OVER) {
			switch (state){
				case STATE_CONNECTING:
					if (!immediate_start) {
					    n = read(sockfd, buffer, BUFFER_SIZE - 1);
					    buffer[n] = '\0';
					}
					
					if (strstr(buffer, "GAME_START|") != NULL) {
                        printf("Opponent found! Start placing ships.\n");
                    }
					
					immediate_start = false;
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
						init_pair(COLOR_HIT, COLOR_RED, COLOR_BLACK);
						init_pair(COLOR_MISS, COLOR_CYAN, COLOR_BLACK);
					}

					state = STATE_PLACING_SHIPS;
					break;
					// here we finish setup with connection
				case STATE_PLACING_SHIPS: {
					struct ship_placement placement;
					bool placed = false;
					
					while(1){
						placement = place_single_ship(my_board, 4);
						placed = send_placement_data(placement, sockfd, buffer);
						if(placed)draw_ship(my_board, placement.length, placement.col, placement.row, placement.vertical); break;
					}
					for(int i=0; i < MAX_OF_BATTLESHIP; i++){
						placement = place_single_ship(my_board, 3);
						placed = send_placement_data(placement, sockfd, buffer);
						if(!placed) i--; // back by one ship as it wasnt placed
						else draw_ship(my_board, placement.length, placement.col, placement.row, placement.vertical);
					}
					for(int i=0; i < MAX_OF_DESTROYERS; i++){
						placement = place_single_ship(my_board, 2);
						placed = send_placement_data(placement, sockfd, buffer);
						if(!placed) i--; // back by one ship as it wasnt placed
						else draw_ship(my_board, placement.length, placement.col, placement.row, placement.vertical);
					}

					for(int i=0; i < MAX_OF_SUBMARINE; i ++){
						placement = place_single_ship(my_board,1);
						placed = send_placement_data(placement, sockfd, buffer);
						if(!placed) i--; // back by one ship as it wasnt placed
						else draw_ship(my_board, placement.length, placement.col, placement.row, placement.vertical);
					}

					refresh();
					mvprintw(5, 70, "Ships placements completed");
					mvprintw(5, 70, "Waiting for the opponent to finish");
					write(sockfd, "READY\n", 6); 
					n = read(sockfd, buffer, BUFFER_SIZE - 1);
					buffer[n] = '\0';
					if (strncmp(buffer, "STATUS|READY", 11) == 0) {
						state = STATE_WAITING_READY;
						break;
					}
				}
				case STATE_WAITING_READY:
					n = read(sockfd, buffer, BUFFER_SIZE - 1);
					buffer[n] = '\0';
					if (strncmp(buffer, "GAME_ON|Your turn", 17) == 0) {
						state = STATE_MY_TURN;
					} else if (strncmp(buffer, "GAME_ON|Opponent", 16) == 0) {
						state = STATE_OPPONENT_TURN;
					}
					break;
				case STATE_MY_TURN: {
					struct coordinates_pair pair = choose_shot(my_board, fight_board);
					
					if (pair.row == -1) { // Handle Quit
						state = STATE_GAME_OVER; 
						break; 
					}

					int len = snprintf(pair_buff, sizeof(pair_buff), "FIRE|%d,%d\n", pair.row, pair.col);
					write(sockfd, pair_buff, len);

					n = read(sockfd, buffer, BUFFER_SIZE - 1);
					if (n <= 0) break;
					buffer[n] = '\0';

					if (strncmp(buffer, "HIT", 3) == 0) {
						update_fight_board(fight_board, pair.row, pair.col, true);
						mvprintw(2, 0, "result: HIT!            ");
					} else if (strncmp(buffer, "MISS", 4) == 0) {
						update_fight_board(fight_board, pair.row, pair.col, false);
						mvprintw(2, 0, "result: MISS!           ");
					} else if (strncmp(buffer, "SUNK", 4) == 0) {
						update_fight_board(fight_board, pair.row, pair.col, true);
						mvprintw(2, 0, "result: SHIP SUNK!      ");
					} else if (strncmp(buffer, "YOU_WIN", 7) == 0) {
						state = STATE_GAME_OVER;
						break;
					}
					refresh();
					napms(1000);

					if (strstr(buffer, "TURN|OPPONENT") != NULL) { // sometimes it was in previous message so need to check previous first just in case
						 state = STATE_OPPONENT_TURN;
					} else {
						 n = read(sockfd, buffer, BUFFER_SIZE - 1);
						 if (n > 0) state = STATE_OPPONENT_TURN;
					}
					break;
				}

				case STATE_OPPONENT_TURN: {
					draw_battle_screen(my_board, fight_board);
					mvprintw(0, 0, "--- OPPONENT'S TURN ---");
					mvprintw(1, 0, "Please wait...");
					refresh();

					n = read(sockfd, buffer, BUFFER_SIZE - 1);
					if (n <= 0) break;
					buffer[n] = '\0';
					
					if (strstr(buffer, "YOUR_SHIP_HIT") != NULL) {
						int row, col;
						char* ptr = strstr(buffer, "YOUR_SHIP_HIT|");
						sscanf(ptr + 14, "%d,%d", &row, &col);
						my_board[row][col] = 'X';
					} 
					if (strstr(buffer, "YOUR_SHIP_SUNK") != NULL) {
						int row, col;
						char* ptr = strstr(buffer, "YOUR_SHIP_SUNK|");
						sscanf(ptr + 15, "%d,%d", &row, &col);
						my_board[row][col] = 'X';
					}
					if (strstr(buffer, "OPP_MISSED") != NULL) {
						int row, col;
						char* ptr = strstr(buffer, "OPP_MISSED|");
						sscanf(ptr + 11, "%d,%d", &row, &col);
						my_board[row][col] = 'O';
					}
					if (strstr(buffer, "YOU_LOSE") != NULL) {
						state = STATE_GAME_OVER;
						break;
					}

					if (strstr(buffer, "TURN|YOURS") != NULL) { // happend once but just in case this shit brakes again
						state = STATE_MY_TURN;
					} else {
						n = read(sockfd, buffer, BUFFER_SIZE - 1);
						if (n > 0 && strstr(buffer, "TURN|YOURS") != NULL) {
							state = STATE_MY_TURN;
						}
					}
					break;
				}
			}
		}
	}
	clear();
	attron(A_BOLD);
	mvprintw(LINES/2, (COLS*2)/2 - 10, "GAME OVER");
	mvprintw(LINES/2 + 1, (COLS*2)/2 - 15, "Press ANY KEY to exit...");
	attroff(A_BOLD);
	refresh();
	
	nodelay(stdscr, FALSE);
	getch();

	endwin();
	close(sockfd);
    return 0;
}