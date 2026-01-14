CC      = gcc
CFLAGS  = -Wall -Wextra -pedantic -std=c11 -g
LDFLAGS = -pthread

INCLUDES = \
	-Icommon \
	-Iserver/configurations/header \
	-Iserver/run_time/header \
	-Iserver/connections_manager/header \
	-Iserver/relay_manager/header \
	-Isock_utilities/header \
	-Irelay/relay_reg/header \
	-Irelay/run_time/header

DIR_BIN    = dir_server
RELAY_BIN  = relay_node
CLIENT_BIN = client_test
DIR_CFG   ?= dir.cfg

VALGRIND_FLAGS = --leak-check=full --show-leak-kinds=all --track-origins=yes

# ---------- Sources ----------
SRC_DIR = \
	server/src/main_test.c \
	server/run_time/src/run_time.c \
	server/configurations/src/dir_server_config.c \
	server/connections_manager/src/relay_handler.c \
	server/relay_manager/src/relay_manager.c \
	sock_utilities/src/create_bind.c \
	sock_utilities/src/accept.c \
	sock_utilities/src/tor_in_out.c

SRC_RELAY = \
	relay/relay_main_test.c \
	relay/run_time/src/relay_run_time.c \
	relay/relay_reg/src/realy_reg.c \
	server/configurations/src/dir_server_config.c \
	sock_utilities/src/connect_server.c \
	sock_utilities/src/accept.c \
	sock_utilities/src/relay_listen_tor.c \
	sock_utilities/src/tor_in_out.c

SRC_CLIENT = \
	client/src/client_main.c

OBJ_DIR    = $(SRC_DIR:.c=.o)
OBJ_RELAY  = $(SRC_RELAY:.c=.o)
OBJ_CLIENT = $(SRC_CLIENT:.c=.o)

all: $(DIR_BIN) $(RELAY_BIN) $(CLIENT_BIN)

$(DIR_BIN): $(OBJ_DIR)
	$(CC) $(CFLAGS) $(OBJ_DIR) -o $@ $(LDFLAGS)

$(RELAY_BIN): $(OBJ_RELAY)
	$(CC) $(CFLAGS) $(OBJ_RELAY) -o $@ $(LDFLAGS)

$(CLIENT_BIN): $(OBJ_CLIENT)
	$(CC) $(CFLAGS) $(OBJ_CLIENT) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

cfg:
	@printf "ip=127.0.0.1\nport=9000\n" > $(DIR_CFG)
	@echo "Wrote $(DIR_CFG)"

# ---------- Run ----------
run_dir: cfg $(DIR_BIN)
	./$(DIR_BIN) $(DIR_CFG)

run_relay: cfg $(RELAY_BIN)
	./$(RELAY_BIN) $(DIR_CFG)

run_relays10: cfg $(RELAY_BIN)
	for i in 1 2 3 4 5 6 7 8 9 10; do \
		./$(RELAY_BIN) $(DIR_CFG) & \
	done; \
	wait

# שימוש: make run_client PORT=12345 MSG="HELLO"
run_client: $(CLIENT_BIN)
	@if [ -z "$(PORT)" ]; then \
		echo "Usage: make run_client PORT=<relay_port> MSG='HELLO'"; \
		exit 1; \
	fi; \
	MSG_VAL="$${MSG:-HELLO}"; \
	./$(CLIENT_BIN) 127.0.0.1 $(PORT) "$$MSG_VAL"

# ---------- Valgrind (single) ----------
valgrind_dir: cfg $(DIR_BIN)
	valgrind $(VALGRIND_FLAGS) ./$(DIR_BIN) $(DIR_CFG)

valgrind_relay: cfg $(RELAY_BIN)
	valgrind $(VALGRIND_FLAGS) ./$(RELAY_BIN) $(DIR_CFG)

valgrind_client: $(CLIENT_BIN)
	@if [ -z "$(PORT)" ]; then \
		echo "Usage: make valgrind_client PORT=<relay_port> MSG='HELLO'"; \
		exit 1; \
	fi; \
	MSG_VAL="$${MSG:-HELLO}"; \
	valgrind $(VALGRIND_FLAGS) ./$(CLIENT_BIN) 127.0.0.1 $(PORT) "$$MSG_VAL"

# ---------- Valgrind (10 relays) ----------
valgrind_relays10: cfg $(RELAY_BIN)
	@rm -f valgrind.relay.*.log
	for i in 1 2 3 4 5 6 7 8 9 10; do \
		valgrind $(VALGRIND_FLAGS) --log-file=valgrind.relay.%p.log ./$(RELAY_BIN) $(DIR_CFG) & \
	done; \
	echo "Started 10 relays under valgrind. Logs: valgrind.relay.<pid>.log"; \
	wait

# ---------- Stop relays reliably ----------
# הורג גם relay_node וגם valgrind שעוטף אותו, עם fallback אם לא נסגר
stop_relays:
	@echo "Stopping valgrind relays cleanly (SIGINT -> wait -> TERM -> KILL)..."
	@echo "1) SIGINT to valgrind wrappers"
	@pkill -INT -f "valgrind.*./$(RELAY_BIN) $(DIR_CFG)" 2>/dev/null || true
	@sleep 2
	@echo "2) If valgrind still running -> SIGTERM"
	@pkill -TERM -f "valgrind.*./$(RELAY_BIN) $(DIR_CFG)" 2>/dev/null || true
	@sleep 1
	@echo "3) If valgrind still running -> SIGKILL"
	@pkill -KILL -f "valgrind.*./$(RELAY_BIN) $(DIR_CFG)" 2>/dev/null || true
	@sleep 1
	@echo "4) Cleanup any orphan relay_node (should be none if valgrind ended cleanly)"
	@pkill -TERM -f "./$(RELAY_BIN) $(DIR_CFG)" 2>/dev/null || true
	@sleep 1
	@pkill -KILL -f "./$(RELAY_BIN) $(DIR_CFG)" 2>/dev/null || true
	@echo "Done. Now check valgrind logs."


# מציג תקציר סיכומים מכל לוג
show_valgrind_summaries:
	@for f in valgrind.relay.*.log; do \
		echo "==================== $$f ===================="; \
		grep -E "HEAP SUMMARY:|LEAK SUMMARY:|ERROR SUMMARY:" $$f || true; \
		echo; \
	done

clean:
	rm -f $(OBJ_DIR) $(OBJ_RELAY) $(OBJ_CLIENT) \
	      $(DIR_BIN) $(RELAY_BIN) $(CLIENT_BIN) $(DIR_CFG) \
	      valgrind.relay.*.log

.PHONY: all clean cfg \
	run_dir run_relay run_relays10 run_client \
	valgrind_dir valgrind_relay valgrind_relays10 valgrind_client \
	stop_relays show_valgrind_summaries
