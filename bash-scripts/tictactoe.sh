#!/bin/bash

BROKER_HOST="localhost"
BROKER_PORT="1883"
MENU_REQUEST_TOPIC="ttt/menu/request"
MENU_RECEIVE_TOPIC="ttt/menu/receive"
OUTPUT_TOPIC="ttt/game_output"
USERNAME=""
PASSWORD=""

# Create a temporary named pipe
FIFO_PATH="/tmp/menu_trigger_fifo"
[[ -p "$FIFO_PATH" ]] || mkfifo "$FIFO_PATH"

# Background listener for game output
mosquitto_sub -h "$BROKER_HOST" -p "$BROKER_PORT" -t "$OUTPUT_TOPIC" \
  ${USERNAME:+-u "$USERNAME"} ${PASSWORD:+-P "$PASSWORD"} &
output_listener_pid=$!

# Background listener for menu request (writes to FIFO to trigger menu)
mosquitto_sub -h "$BROKER_HOST" -p "$BROKER_PORT" -t "$MENU_REQUEST_TOPIC" > "$FIFO_PATH" &
menu_trigger_pid=$!

# Display menu and publish user choice
display_menu() {
    while true; do
        echo "Select game mode:"
        echo "1) 1 player"
        echo "2) 2 players"
        read -p "Enter your choice: " choice

        if [[ "$choice" =~ ^[12]$ ]]; then
	  # Valid input, publish choice to the broker
            mosquitto_pub -h "$BROKER_HOST" -p "$BROKER_PORT" -t "$MENU_RECEIVE_TOPIC" -m "$choice"
            break
        else
	# Ask again
            echo "Invalid input. Please enter 1 or 2."
            mosquitto_pub -h "$BROKER_HOST" -p "$BROKER_PORT" -t "$MENU_REQUEST_TOPIC" -m "SELECTE MENU"
        fi
    done
}

# Main loop: wait for trigger message via FIFO, then show menu
while read -r line < "$FIFO_PATH"; do
    display_menu
done

# Cleanup on exit
trap "kill $output_listener_pid $menu_trigger_pid; rm -f $FIFO_PATH" EXIT
