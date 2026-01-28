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

	n = read(sockfd, buffer, BUFFER_SIZE - 1);
	buffer[n] = '\0';
	if (strncmp(buffer, "LOGIN_OK", 8) == 0) printf("Logged in !\n");
	printf("Waiting for opponent...\n");
	
	ClientState state = STATE_CONNECTING;
	char pair_buff[64];
	while (state != STATE_GAME_OVER) {
		switch (state){
			case STATE_CONNECTING:
				n = read(sockfd, buffer, BUFFER_SIZE - 1);
				buffer[n] = '\0';
				if (strncmp(buffer, "GAME_START|", 11) == 0) {
						printf("Opponent found! Start placing ships.\n");
				}
				
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

				char my_board[ROWS][COLS];
				for(int i=0; i<ROWS; i++) {
					for(int j=0; j<COLS; j++) {
						my_board[i][j] = '.';
					}
				}

				char fight_board[ROWS][COLS];
				for(int i=0; i<ROWS; i++) {
					for(int j=0; j<COLS; j++) {
					 fight_board[i][j] = '.';
					}
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
				buffer[n] = '\0'; // Here need to recieve signal from server when user finished placing ships
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
				while(1){
					struct coordinates_pair pair;
					pair = choose_shot(fight_board);
					int len = snprintf(pair_buff, sizeof(pair_buff), "FIRE|%d,%d\n",
						pair.row,
						pair.col
					);
					write(sockfd, pair_buff, len);

					n = read(sockfd, buffer, BUFFER_SIZE - 1);
					buffer[n] = '\0';
					if (strncmp(buffer, "HIT", 3) == 0) {
						update_fight_board(fight_board, pair.row, pair.col, true);
					} else if (strncmp(buffer, "MISS", 4) == 0) {
						update_fight_board(fight_board, pair.row, pair.col, false);
					} else if (strncmp(buffer, "SUNK", 4) == 0) {
						update_fight_board(fight_board, pair.row, pair.col, true);
					} else if (strncmp(buffer, "YOU_WIN", 7) == 0) {
						state = STATE_GAME_OVER;
						break;
					}
					n = read(sockfd, buffer, BUFFER_SIZE - 1);
					state = STATE_OPPONENT_TURN;
					break;
				}
			}
			case STATE_OPPONENT_TURN:
				draw_waiting_screen("Opponent's turn...");
				n = read(sockfd, buffer, BUFFER_SIZE - 1);
				buffer[n] = '\0';
				
				if (strncmp(buffer, "YOUR_SHIP_HIT|", 14) == 0) {
					int row, col;
					sscanf(buffer + 14, "%d,%d", &row, &col);
					my_board[row][col] = 'X';
				} else if (strncmp(buffer, "OPP_MISSED|", 11) == 0) {
					int row, col;
					sscanf(buffer + 11, "%d,%d", &row, &col);
					my_board[row][col] = 'O';
				} else if (strncmp(buffer, "YOU_LOSE", 8) == 0) {
					state = STATE_GAME_OVER;
					break;
				}
				
				// Read TURN message
				n = read(sockfd, buffer, BUFFER_SIZE - 1);
				state = STATE_MY_TURN;
				break;
		}
	}
	endwin();
	close(sockfd);
    return 0;
}