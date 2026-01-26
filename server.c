//to complie: g++ server.c -o server.exe -lsqlite3 -lm

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <stdbool.h>
#include <sqlite3.h>
#include <math.h>

#define MULTICAST_GROUP "239.0.0.1"
#define MULTICAST_PORT 5000
#define TCP_PORT 5001
#define MAX_CLIENTS 10
#define BOARD_SIZE 10
#define MAX_SHIPS 10
#define BUFFER_SIZE 1024
#define K_FACTOR 32

typedef enum { EMPTY, SHIP, HIT, MISS, SUNK } CellState;
typedef enum { LOGIN, SPECTATOR, WAITING_FOR_OPPONENT, PLACING_SHIPS, READY, PLAYING, GAME_OVER, QUIT } ClientState;

typedef struct {
    int socket;
    ClientState state;
    char myView[BOARD_SIZE][BOARD_SIZE];
    char opponentView[BOARD_SIZE][BOARD_SIZE];
    int shipsRemaining;
    int playerID;
    int ammountOfCarriers = 0;      // max 1 (4 cells)
    int ammountOfBattleships = 0;   // max 2 (3 cells)
    int ammountOfDestroyers = 0;    // max 3 (2 cells)
    int ammountOfSubmarines = 0;    // max 4 (1 cell)
    int elo;
    int wins;
    int losses;
    int total_matches;
    char name[50];
} Client;

typedef struct {
    int gameID;
    Client *player1;
    Client *player2;
    int currentTurn; // 1 for player1's turn, 2 for player2's turn
} Game;

Client clients[MAX_CLIENTS];
Game games[MAX_CLIENTS / 2];
int numClients = 0;
int numGames = 0;
sqlite3 *db;

#define SA struct sockaddr
#define LISTENQ 10

int handle_client_message(Client *client, char *buffer);
bool is_ship_sunk(char board[][BOARD_SIZE], int x, int y);
int find_free_client_slot();
void init_db();
void calculate_elo(Client *winner, Client *loser);
void save_game_result(Client *winner, Client *loser);
void load_or_create_user(Client *client, const char *username);

void initialize_board(char board[BOARD_SIZE][BOARD_SIZE]) {
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            board[i][j] = EMPTY;
        }
    }
}

int find_free_client_slot() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket == 0) return i;
    }
    return -1;
}

bool is_ship_sunk(char board[][BOARD_SIZE], int x, int y) {

    int j = y;
    
    while (j > 0 && (board[x][j - 1] == SHIP || board[x][j - 1] == HIT)) {
        j--;
    }

    while (j < BOARD_SIZE && (board[x][j] == SHIP || board[x][j] == HIT)) {
        if (board[x][j] == SHIP) {
            return false;
        }
        j++;
    }
    
    int i = x;
    
    while (i > 0 && (board[i - 1][y] == SHIP || board[i - 1][y] == HIT)) {
        i--;
    }
    
    while (i < BOARD_SIZE && (board[i][y] == SHIP || board[i][y] == HIT)) {
        if (board[i][y] == SHIP) {
            return false;
        }
        i++;
    }
    return true;
}

int main() {
    int maxfd, connfd;
    int tcp_sock, multicast_sock;
    fd_set readsdset;
    struct timeval timeout;
    time_t now, last_announce = 0;

    memset(clients, 0, sizeof(clients));
    memset(games, 0, sizeof(games));

    init_db();
    
    //Set up multicast socket for server announcements
    multicast_sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in multicast_addr;
    memset(&multicast_addr, 0, sizeof(multicast_addr));
    multicast_addr.sin_family = AF_INET;
    multicast_addr.sin_port = htons(MULTICAST_PORT);
    multicast_addr.sin_addr.s_addr = inet_addr(MULTICAST_GROUP);

    // @Brief Set up TCP socket for client connections
    tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(tcp_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in tcp_addr;
    memset(&tcp_addr, 0, sizeof(tcp_addr));
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    tcp_addr.sin_port = htons(TCP_PORT);

    if (bind(tcp_sock, (SA*)&tcp_addr, sizeof(tcp_addr)) < 0) {
        perror("TCP Bind failed");
        exit(EXIT_FAILURE);
    }
    listen(tcp_sock, LISTENQ);

    printf("Server started on TCP %d. Multicasting to %s:%d\n", TCP_PORT, MULTICAST_GROUP, MULTICAST_PORT);

    while (1) {
        FD_ZERO(&readsdset);
        FD_SET(tcp_sock, &readsdset);
        maxfd = tcp_sock;

        int active_count = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket > 0) {
                FD_SET(clients[i].socket, &readsdset);
                if (clients[i].socket > maxfd) maxfd = clients[i].socket;
                active_count++;
            }
        }
        numClients = active_count;

        now = time(NULL);
        if (now - last_announce >= 5) {
            char announcement[256];
            snprintf(announcement, sizeof(announcement), "BATTLESHIP_SERVER|%d|PLAYERS:%d", TCP_PORT, numClients);
            sendto(multicast_sock, announcement, strlen(announcement), 0, (SA*)&multicast_addr, sizeof(multicast_addr));
            printf("Multicast sent: %s\n", announcement);
            last_announce = now;
        }

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int activity = select(maxfd + 1, &readsdset, NULL, NULL, &timeout);

        if (activity < 0 && errno != EINTR) {
            perror("Select error");
        }

        //Handle New Connections
        if (FD_ISSET(tcp_sock, &readsdset)) {
            struct sockaddr_in cliaddr;
            socklen_t clilen = sizeof(cliaddr);
            connfd = accept(tcp_sock, (SA*)&cliaddr, &clilen);

            int slot = find_free_client_slot();
            if (slot != -1) {
                clients[slot].socket = connfd;
                clients[slot].state = WAITING_FOR_OPPONENT;
                clients[slot].playerID = slot + 1;
                clients[slot].shipsRemaining = MAX_SHIPS;
                initialize_board(clients[slot].myView);
                initialize_board(clients[slot].opponentView);

                printf("New connection: %s (Player ID: %d)\n", inet_ntoa(cliaddr.sin_addr), clients[slot].playerID);
                send(connfd, "WELCOME|You are connected.\n", 27, 0);

                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (i != slot && clients[i].socket > 0 && clients[i].state == WAITING_FOR_OPPONENT) {
                        games[numGames].gameID = numGames + 1;
                        games[numGames].player1 = &clients[i];
                        games[numGames].player2 = &clients[slot];
                        games[numGames].currentTurn = 1;
                        
                        clients[i].state = PLACING_SHIPS;
                        clients[slot].state = PLACING_SHIPS;

                        numGames++;
                        printf("Game Started: Player %d vs Player %d\n", clients[i].playerID, clients[slot].playerID);
                        
                        send(clients[i].socket, "GAME_START|OPPONENT_FOUND\n", 26, 0);
                        send(clients[slot].socket, "GAME_START|OPPONENT_FOUND\n", 26, 0);
                        break;
                    }
                }
            }
            else {
                printf("Server full. Rejecting %s\n", inet_ntoa(cliaddr.sin_addr));
                close(connfd);
            }
        }

        //Handle Client Messages
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket > 0 && FD_ISSET(clients[i].socket, &readsdset)) {
                char buffer[BUFFER_SIZE];
                ssize_t n = read(clients[i].socket, buffer, sizeof(buffer) - 1);

                if (n <= 0) {
                    if (clients[i].state == GAME_OVER) {
                        printf("Player %d disconnected after match.\n", clients[i].playerID);
                    } 
                    else {
                        printf("Player %d disconnected unexpectedly.\n", clients[i].playerID);
                    }
                    close(clients[i].socket);
                    clients[i].socket = 0;
                }
                else {
                    buffer[n] = '\0';
                    char *ptr = strchr(buffer, '\n');
                    if (ptr) *ptr = '\0';
                    char *rptr = strchr(buffer, '\r');
                    if (rptr) *rptr = '\0';

                    if (strlen(buffer) > 0) {
                        printf("Msg from Player %d: %s\n", clients[i].playerID, buffer);
                        handle_client_message(&clients[i], buffer);
                    }
                }
            }
        }

        for (int i = 0; i < numGames; i++) {
            if (games[i].player1->state == GAME_OVER || games[i].player2->state == GAME_OVER) {
                printf("Cleaning up Game %d\n", games[i].gameID);

                //games[i].player1->state = WAITING_FOR_OPPONENT;
                //games[i].player2->state = WAITING_FOR_OPPONENT;
                for (int j = i; j < numGames - 1; j++) {
                    games[j] = games[j + 1];
                }
                numGames--;
                i--;

            }
        }
    }
    return 0;
}

int handle_client_message(Client *client, char *buffer) {
    /* 
     * @Brief Handle messages from a connected client
     * @Param client Pointer to the client structure
     * @Param buffer The received message
     * @Return 0 on success, -1 on error 
     */
    int x, y, length;
    char orientation;

    if(strncmp(buffer, "LOGIN|", 6) == 0){
        char username [50];
        sscanf(buffer + 6, "%s", username);
        strcpy(client->name, username);
        load_or_create_user(client, username);

        char response[50];
        snprintf(response, sizeof(response), "LOGIN_OK");
        send(client->socket, response, strlen(response), 0);
    }

    else if (strncmp(buffer, "PLACE_SHIP|", 11) == 0) {
        sscanf(buffer + 11, "%d,%d,%d,%c", &x, &y, &length, &orientation);
        char normOri = toupper(orientation);

        if (length == 4) {
            if (client->ammountOfCarriers >= 1) {
                send(client->socket, "ERROR|Max carriers placed\n", 26, 0);
                return -1;
            }
            client->ammountOfCarriers++;
        } else if (length == 3) {
            if (client->ammountOfBattleships >= 2) {
                send(client->socket, "ERROR|Max battleships placed\n", 29, 0);
                return -1;
            }
            client->ammountOfBattleships++;
        } else if (length == 2) {
            if (client->ammountOfDestroyers >= 3) {
                send(client->socket, "ERROR|Max destroyers placed\n", 28, 0);
                return -1;
            }
            client->ammountOfDestroyers++;
        } else if (length == 1) {
            if (client->ammountOfSubmarines >= 4) {
                send(client->socket, "ERROR|Max submarines placed\n", 28, 0);
                return -1;
            }
            client->ammountOfSubmarines++;
        } else {
            send(client->socket, "ERROR|Invalid ship length\n", 26, 0);
            return -1;
        }

        if (x < 0 || x >= BOARD_SIZE || y < 0 || y >= BOARD_SIZE) {
            send(client->socket, "ERROR|Invalid coordinates\n", 26, 0);
            return -1;
        }

        bool space_is_clear = true;

        for (int k = 0; k < length; k++) {
            int cx, cy; 

            if (normOri == 'H') {
                cx = x;
                cy = y + k;
            }
            else {
                cx = x + k;
                cy = y;
            }

            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    int nx = cx + dx;
                    int ny = cy + dy;

                    if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE) {
                        if (client->myView[nx][ny] == SHIP) {
                            space_is_clear = false;
                            break;
                        }
                    }
                }
                if (!space_is_clear) break;
            }
            if (!space_is_clear) break;
        }

        if (!space_is_clear) {
            if (length == 4) client->ammountOfCarriers--;
            else if (length == 3) client->ammountOfBattleships--;
            else if (length == 2) client->ammountOfDestroyers--;
            else if (length == 1) client->ammountOfSubmarines--;
            
            send(client->socket, "ERROR|Ships touching or overlapping\n", 36, 0);
            return -1;
        }

        if (normOri == 'H') {
            if (y + length <= BOARD_SIZE) {
                for (int i = 0; i < length; i++) client->myView[x][y + i] = SHIP;
                space_is_clear = true;
            }
        } else if (normOri == 'V') {
            if (x + length <= BOARD_SIZE) {
                for (int i = 0; i < length; i++) client->myView[x + i][y] = SHIP;
                space_is_clear = true;
            }
        }

        if (space_is_clear) {
            printf("Player %d placed ship at %d,%d\n", client->playerID, x, y);
            send(client->socket, "SHIP_PLACED\n", 12, 0);
        } else {
            send(client->socket, "ERROR|Invalid placement\n", 24, 0);
        }
    } 

    else if(strncmp(buffer, "READY", 5) == 0) {
        client->state = READY;
        send(client->socket, "STATUS|READY\n", 14, 0);
        printf("Player %d is ready.\n", client->playerID);
        
        for (int i = 0; i < numGames; i++) {
            if (games[i].player1 == client || games[i].player2 == client) {
                if (games[i].player1->state == READY && games[i].player2->state == READY) {
                    games[i].player1->state = PLAYING;
                    games[i].player2->state = PLAYING;
                    send(games[i].player1->socket, "GAME_ON|Your turn\n", 18, 0);
                    send(games[i].player2->socket, "GAME_ON|Opponent's turn\n", 24, 0);
                    printf("Game %d started between Player %d and Player %d\n", 
                           games[i].gameID, games[i].player1->playerID, games[i].player2->playerID);
                }
                break;
            }
        }
    }

    else if(strncmp(buffer, "QUIT", 4) == 0) {
        send(client->socket, "GOODBYE|Disconnecting\n", 23, 0);
        close(client->socket);
        client->socket = 0;
        printf("Player %d disconnected via QUIT.\n", client->playerID);
    }

    else if (strncmp(buffer, "FIRE|", 5) == 0) {
        sscanf(buffer + 5, "%d,%d", &x, &y);

        Game *myGame = NULL;
        for (int k = 0; k < numGames; k++) {
            if (games[k].player1 == client || games[k].player2 == client) {
                myGame = &games[k];
                break;
            }
        }

        if (!myGame) {
            send(client->socket, "ERROR|No active game\n", 21, 0);
            return -1;
        }

        Client *opponent = (myGame->player1 == client) ? myGame->player2 : myGame->player1;

        if ((myGame->currentTurn == 1 && myGame->player1 != client) || 
            (myGame->currentTurn == 2 && myGame->player2 != client)) {
            send(client->socket, "ERROR|Not your turn\n", 20, 0);
            return -1;
        }

        if (x < 0 || x >= BOARD_SIZE || y < 0 || y >= BOARD_SIZE) {
            send(client->socket, "ERROR|Out of bounds\n", 20, 0);
            return -1;
        }

        char response[50] = "MISS\n";
        char oppMsg[50] = "OPP_MISSED\n";

        if (opponent->myView[x][y] == SHIP) {
            opponent->myView[x][y] = HIT;
            client->opponentView[x][y] = HIT;
            strcpy(response, "HIT\n");
            strcpy(oppMsg, "YOUR_SHIP_HIT\n");
            
            if (is_ship_sunk(opponent->myView, x, y)) {
                opponent->shipsRemaining--;
                printf("Player %d's ship sunk! Ships remaining: %d\n", opponent->playerID, opponent->shipsRemaining);

                // print the ship board for debug purpose
                printf("Debug View for Player %d:\n", opponent->playerID);
                for (int i = 0; i < BOARD_SIZE; i++) {
                    for (int j = 0; j < BOARD_SIZE; j++) {
                        printf("%d ", opponent->myView[i][j]);
                    }
                    printf("\n");
                }

                strcpy(response, "SUNK\n");
                strcpy(oppMsg, "YOUR_SHIP_SUNK\n");
                if (opponent->shipsRemaining == 0) {
                     strcpy(response, "YOU_WIN\n");
                     strcpy(oppMsg, "YOU_LOSE\n");
                    client->state = GAME_OVER;
                    opponent->state = GAME_OVER;
                    printf("Game %d over. Player %d wins!\n", myGame->gameID, client->playerID);
                    calculate_elo(client, opponent);
                    printf("New ELO - Player %d: %d, Player %d: %d\n", 
                           client->playerID, client->elo, opponent->playerID, opponent->elo);
                    save_game_result(client, opponent);
                    return 0;
                }
            }
        } 
        else {
            opponent->myView[x][y] = MISS;
            client->opponentView[x][y] = MISS;
        }

        send(client->socket, response, strlen(response), 0);
        send(opponent->socket, oppMsg, strlen(oppMsg), 0);

        myGame->currentTurn = (myGame->currentTurn == 1) ? 2 : 1;
        
        if(myGame->currentTurn == 1) {
            send(myGame->player1->socket, "TURN|YOURS\n", 11, 0);
            send(myGame->player2->socket, "TURN|OPPONENT\n", 14, 0);
        } else {
            send(myGame->player2->socket, "TURN|YOURS\n", 11, 0);
            send(myGame->player1->socket, "TURN|OPPONENT\n", 14, 0);
        }
    }
    else {
        send(client->socket, "ERROR|Unknown command\n", 22, 0);
        return -1;
    }
    return 0;
}

void init_db() {
    if (sqlite3_open("battleship.db", &db)) {
        fprintf(stderr, "DB Error: %s\n", sqlite3_errmsg(db));
        exit(1);
    }

    char *errMsg = 0;
    const char *sql = "CREATE TABLE IF NOT EXISTS players ("
                      "username TEXT PRIMARY KEY,"
                      "elo INTIGER DEFAULT 1000,"
                      "wins INTIGER DEFAULT 0,"
                      "losses INTIGER DEFAULT 0,"
                      "total games INTIGER DEFAULT 0)";

    if (sqlite3_exec(db, sql, 0, 0, &errMsg) != SQLITE_OK) {
        fprintf(stderr, "SQL Error: %s\n", errMsg);
        sqlite3_free(errMsg);
    }
}

void load_or_create_user(Client *client, const char *username){
    /*
     * @ Brief checking if the user has already existed
     * @ Param client Pointer to the client structure
     * @ Param username the username entered by user
     * @ Return none
     */
    sqlite3_stmt *stmt;
    const char *sql = "SELECT elo, wins, losses, total_games FROM players where username = ?";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

        if(sqlite3_step(stmt) == SQLITE_ROW) {
            client -> elo = sqlite3_column_int(stmt, 0);
            client -> wins = sqlite3_column_int(stmt, 1);
            client -> losses = sqlite3_column_int(stmt, 2);
            client -> total_matches = sqlite3_column_int (stmt, 3);

            printf("Player %s loaded: ELO=%d, Games=%d, W/L=%d/%d\n", 
                   username, client->elo, client->total_matches, 
                   client->wins, client->losses);
        }
        else{
            client -> elo = 1000;
            client -> wins = 0;
            client -> losses = 0;
            client -> total_matches = 0;

            const char *insert_sql = "INSERT INTO players (username, elo, total_games, wins, losses) "
                                     "VALUES (?, 1000, 0, 0, 0)";
            sqlite3_stmt *insert_stmt;
            sqlite3_prepare_v2(db, insert_sql, -1, &insert_stmt, 0);
            sqlite3_bind_text(insert_stmt, 1, username, -1, SQLITE_STATIC);
            sqlite3_step(insert_stmt);
            sqlite3_finalize(insert_stmt);
            
            printf("New player %s created with default stats\n", username);
        }
        sqlite3_finalize(stmt);
    }
}

void calculate_elo(Client *winner, Client *loser) {
    double expected_win = 1.0 / (1.0 + pow(10, (loser->elo - winner->elo) / 400.0));
    
    winner->elo = (int)(winner->elo + K_FACTOR * (1.0 - expected_win));
    loser->elo  = (int)(loser->elo + K_FACTOR * (0.0 - (1.0 - expected_win)));
    
    printf("New Ratings -> Winner: %d, Loser: %d\n", winner->elo, loser->elo);
}

void save_game_result(Client *winner, Client *loser) {
    char *errMsg = 0;
    const char *update_winner = "UPDATE players SET "
                                "elo = ?, "
                                "total_games = total_games + 1, "
                                "wins = wins + 1 "
                                "WHERE username = ?";
    
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, update_winner, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, winner->elo);
    sqlite3_bind_text(stmt, 2, winner->name, -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    const char *update_loser = "UPDATE players SET "
                               "elo = ?, "
                               "total_games = total_games + 1, "
                               "losses = losses + 1 "
                               "WHERE username = ?";
    
    sqlite3_prepare_v2(db, update_loser, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, loser->elo);
    sqlite3_bind_text(stmt, 2, loser->name, -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    printf("Game result saved: %s (ELO %d) beat %s (ELO %d)\n", 
           winner->name, winner->elo, loser->name, loser->elo);
}
