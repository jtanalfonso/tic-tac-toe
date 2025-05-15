#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Function prototypes
void resetBoard();
void printBoard();
void printGameCount();
void printOutcome(const char *outcome);
void playerXMove();
void playerOMove();
void checkWinner();
void announceWinner(char player);
void playRound();
void playGame();

// Sets up game
char board[3][3];
bool gameOver = false;
int xWins = 0, oWins = 0, drawCount = 0, gameCount = 0;

// Resets game board
void resetBoard() {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            board[i][j] = ' ';
        }
    }
}

// Prints game board
void printBoard() {
    printf("\n");
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            printf(" %c ", board[i][j]);
            if (j < 2) printf("|");
        }
        printf("\n");
        if (i < 2) printf("-----------\n");
    }
    printf("\n");
}

void printGameCount() {
    printf("\nGame #%d\n", gameCount);
}

// Prints outcome of the game
void printOutcome(const char *outcome) {
    printf("%s wins!\n", outcome);
    printf("X Wins: %d | O Wins: %d | Draws: %d\n", xWins, oWins, drawCount);
}

// Player X makes a move
void playerXMove() {
    int row, col;
    do {
        row = rand() % 3;
        col = rand() % 3;
    } while (board[row][col] != ' ');
    board[row][col] = 'X';
}

// Player O makes a move
void playerOMove() {
    int row, col;
    do {
        row = rand() % 3;
        col = rand() % 3;
    } while (board[row][col] != ' ');
    board[row][col] = 'O';
}

// Checks for a winner
void checkWinner() {
    for (int i = 0; i < 3; i++) {
        if (board[i][0] != ' ' && board[i][0] == board[i][1] && board[i][1] == board[i][2]) {
            announceWinner(board[i][0]);
            gameOver = true;
            return;
        }
        if (board[0][i] != ' ' && board[0][i] == board[1][i] && board[1][i] == board[2][i]) {
            announceWinner(board[0][i]);
            gameOver = true;
            return;
        }
    }
    if (board[0][0] != ' ' && board[0][0] == board[1][1] && board[1][1] == board[2][2]) {
        announceWinner(board[0][0]);
        gameOver = true;
        return;
    }
    if (board[0][2] != ' ' && board[0][2] == board[1][1] && board[1][1] == board[2][0]) {
        announceWinner(board[0][2]);
        gameOver = true;
        return;
    }
}

// Announces winner
void announceWinner(char player) {
    if (player == 'X') {
        xWins++;
        printOutcome("X");
        vTaskDelay(pdMS_TO_TICKS(1000));
    } else if (player == 'O') {
        oWins++;
        printOutcome("O");
        vTaskDelay(pdMS_TO_TICKS(1000));
    } else {
        drawCount++;
        printOutcome("None");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Plays one round
void playRound() {
    printGameCount();
    vTaskDelay(pdMS_TO_TICKS(500));
    resetBoard();
    gameOver = false;
    int turn = 0;
    while (!gameOver && turn < 9) {
        if (turn % 2 == 0) {
            playerXMove();
        } else {
            playerOMove();
        }
        printBoard();
        vTaskDelay(pdMS_TO_TICKS(500));
        checkWinner();
        turn++;
    }
    if (!gameOver) {
        announceWinner(' '); // Draw
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Plays 100 rounds 
void playGame() {
    while (gameCount < 100) {
        gameCount++;
        playRound();
    }
    printf("\nAll 100 games completed.\n");
    printf("Final Score: X Wins: %d | O Wins: %d | Draws: %d\n", xWins, oWins, drawCount);
}

void app_main() {
    // Show menu
    // int gameMode;
    printf("Starting Tic-Tac-Toe Simulation...\n\n");
    vTaskDelay(pdMS_TO_TICKS(1000));
    srand((unsigned int) time(NULL));
    playGame();
}