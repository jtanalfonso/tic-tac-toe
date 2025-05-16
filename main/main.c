#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "mqtt_client.h"

// Wifi set up 
#define WIFI_SSID      "Julians_iPhone"
#define WIFI_PASSWORD  "16781678"
#define MAXIMUM_RETRY  5

// MQTT Configuration
#define MQTT_BROKER_URI "mqtt://35.235.98.7"
#define MQTT_USERNAME   ""
#define MQTT_PASSWORD   ""
#define MQTT_OUTPUT_TOPIC "ttt/game_output"

// FreeRTOS event group handle to signal when we are connected to WiFi or connection failed
static EventGroupHandle_t s_wifi_event_group;

// Define bits to signal connection status via event group
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// MQTT client handle
static esp_mqtt_client_handle_t mqtt_client = NULL;

// WiFi connection retry counter
static int s_retry_num = 0;

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
void wifi_init_sta(void);
void mqtt_init(void);
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

// Sets up game
char board[3][3];
bool gameOver = false;
static int gameMode = 0;
static bool gameModeReceived = false;
int xWins = 0, oWins = 0, drawCount = 0, gameCount = 0;

// ===== WiFi and MQTT Setup =====

// Publish message to MQTT broker
void mqtt_publish_message(const char *topic, const char *message) {
    if (mqtt_client != NULL) {
        int msg_id = esp_mqtt_client_publish(mqtt_client, topic, message, 0, 1, 0);
    }
}

// WiFi event handler- reacts to WiFi events
static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{   
    // Attempt to connect to WiFi
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    // If disconnected, retry up to MAXIMUM_RETRY times
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            mqtt_publish_message("ttt/game_output", "Retrying connection...\n");
        } else {
            // Max retires reached, set fail bit
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        mqtt_publish_message("ttt/game_output", "Connection failed.\n");
    // When IP address is obtained, reset retry counter and set connected bit
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// MQTT event handler
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    
    // Check if connected to MQTT broker
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            mqtt_publish_message("ttt/game_output", "MQTT Connected to broker\n");
            esp_mqtt_client_subscribe(mqtt_client, "ttt/menu/receive", 0);
            break;
        case MQTT_EVENT_DISCONNECTED:
            mqtt_publish_message("ttt/game_output", "MQTT disconnected from broker\n");
            break;
        case MQTT_EVENT_DATA:
        {
            esp_mqtt_event_handle_t event = event_data;

            if (strncmp(event->topic, "ttt/menu/receive", event->topic_len) == 0) {
                char payload[10] = {0};
                int len = event->data_len < sizeof(payload) - 1 ? event->data_len : sizeof(payload) - 1;
                strncpy(payload, event->data, len);
                payload[len] = '\0';

                int choice = atoi(payload);
                if (choice == 1 || choice == 2) {
                    gameMode = choice;
                    gameModeReceived = true;
                }
            }
            break;
        }

        default:
            break;
    }
}

// Initialize MQTT client
void mqtt_init(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .credentials.username = MQTT_USERNAME,
        .credentials.authentication.password = MQTT_PASSWORD
    };

    // Initialize MQTT client with configuration
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    
    // Register event handlers
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    
    // Start MQTT client
    esp_mqtt_client_start(mqtt_client);
}

// Initializes WiFi in station mode and connects
void wifi_init_sta(void)
{
    // Event group to signal connection status
    s_wifi_event_group = xEventGroupCreate();

    // Initialize TCP/IP network interface
    ESP_ERROR_CHECK(esp_netif_init());

    // Initialize event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create default WiFi station interface
    esp_netif_create_default_wifi_sta();

    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers for WiFi events
    esp_event_handler_instance_t instance_any_id;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    // Register event handler for IP events
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // Set WiFi configuration
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
	        .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    // Set WiFi to station mode
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    // Set WiFi configuration
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    // Start WiFi
    ESP_ERROR_CHECK(esp_wifi_start() );

    // Wait for successful connection
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);
}

// ===== Tic-Tac-Toe Game Logic =====

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
    char line[50];

    mqtt_publish_message("ttt/game_output", "\n");

    for (int i = 0; i < 3; i++) {
        int len = 0;
        for (int j = 0; j < 3; j++) {
            len += snprintf(line + len, sizeof(line) - len, " %c ", board[i][j]);
            if (j < 2) {
                len += snprintf(line + len, sizeof(line) - len, "|");
            }
        }
        mqtt_publish_message("ttt/game_output", line);

        if (i < 2) {
            mqtt_publish_message("ttt/game_output", "-----------");
        }
    }

    mqtt_publish_message("ttt/game_output", "\n");
}

void printGameCount() {
    char msg[50];
    snprintf(msg, sizeof(msg), "Game #%d", gameCount);
    mqtt_publish_message("ttt/game_output", msg);
}

// Prints outcome of the game
void printOutcome(const char *outcome) {
    char msg[100];

    snprintf(msg, sizeof(msg), "%s wins!", outcome);
    mqtt_publish_message("ttt/game_output", msg);

    snprintf(msg, sizeof(msg), "X Wins: %d | O Wins: %d | Draws: %d", xWins, oWins, drawCount);
    mqtt_publish_message("ttt/game_output", msg);
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

    char msg[100];
    snprintf(msg, sizeof(msg), "\nAll 100 games completed.\n");
    mqtt_publish_message("ttt/game_output", msg);

    snprintf(msg, sizeof(msg), "Final Score: X Wins: %d | O Wins: %d | Draws: %d\n", xWins, oWins, drawCount);
    mqtt_publish_message("ttt/game_output", msg);
}

int displayMenu() {
    gameModeReceived = false;

     mqtt_publish_message("ttt/menu/request", "SELECT MENU");

     while (!gameModeReceived) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    return gameMode;
}

void app_main() {
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Initialize NVS
    nvs_flash_init();

    // Initialize and connect to WiFi
    wifi_init_sta();
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Initialize MQTT
    mqtt_init();
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    gameMode = displayMenu();
    mqtt_publish_message("ttt/game_output", "Starting Tic-Tac-Toe Simulation...\n\n");
    vTaskDelay(pdMS_TO_TICKS(1000));
    srand((unsigned int) time(NULL));
    playGame();
    
    // Clean up MQTT client when done
    esp_mqtt_client_stop(mqtt_client);
    esp_mqtt_client_destroy(mqtt_client);
}