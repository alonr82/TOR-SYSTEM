CC      = gcc
CFLAGS  = -Wall -Wextra -pedantic -std=c11 -g
LDFLAGS = -pthread

# ---------- Includes ----------
INCLUDES = \
	-Icommon \
	-Icommon/header \
	-Iserver/configurations/header \
	-Iserver/run_time/header \
	-Iserver/connections_manager/header \
	-Iserver/relay_manager/header \
	-Isock_utilities/header \
	-Irelay/relay_reg/header \
	-Irelay/run_time/header \
	-Irelay/forward_msg/header \
	-Iclient/create_circuit/header \
	-Idest_server/header

# ---------- Binaries ----------
DIR_BIN    = dir_server
RELAY_BIN  = relay_node
CLIENT_BIN = client_test
DEST_BIN   = dest_server_bin

# ---------- Config ----------
DIR_CFG ?= dir.cfg
CLIENT_CFG_DIR = config
CLIENT_CFG     = $(CLIENT_CFG_DIR)/dir_server_config.cfg

VALGRIND_FLAGS = --leak-check=full --show-leak-kinds=all --track-origins=yes

# ---------- Sources ----------
SRC_DIR = \
	server/src/main_test.c \
	server/run_time/src/run_time.c \
	common/src/dir_server_config.c \
	common/src/relay_server_config.c \
	server/connections_manager/src/relay_handler.c \
	server/relay_manager/src/relay_manager.c \
	sock_utilities/src/create_bind.c \
	sock_utilities/src/accept.c \
	sock_utilities/src/tor_in_out.c

SRC_RELAY = \
	relay/relay_main_test.c \
	relay/run_time/src/relay_run_time.c \
	relay/forward_msg/src/forward_msg.c \
	relay/relay_reg/src/realy_reg.c \
	common/src/dir_server_config.c \
	common/src/relay_server_config.c \
	sock_utilities/src/connect_server.c \
	sock_utilities/src/accept.c \
	sock_utilities/src/relay_listen_tor.c \
	sock_utilities/src/tor_in_out.c

SRC_CLIENT = \
	client/src/client_main.c \
	client/create_circuit/src/create_circuit.c \
	common/src/dir_server_config.c \
	sock_utilities/src/connect_server.c \
	sock_utilities/src/tor_in_out.c

# NOTE: אם הקובץ אצלך לא נקרא dest_main.c, תתקן כאן לשם האמיתי
SRC_DEST = \
	dest_server/src/dest_server.c \
	dest_server/src/dest_main.c \
	sock_utilities/src/tor_in_out.c

OBJ_DIR    = $(SRC_DIR:.c=.o)
OBJ_RELAY  = $(SRC_RELAY:.c=.o)
OBJ_CLIENT = $(SRC_CLIENT:.c=.o)
OBJ_DEST   = $(SRC_DEST:.c=.o)

# ---------- Default ----------
all: $(DIR_BIN) $(RELAY_BIN) $(CLIENT_BIN) $(DEST_BIN)

# ---------- Build rules ----------
$(DIR_BIN): $(OBJ_DIR)
	$(CC) $(CFLAGS) $(OBJ_DIR) -o $@ $(LDFLAGS)

$(RELAY_BIN): $(OBJ_RELAY)
	$(CC) $(CFLAGS) $(OBJ_RELAY) -o $@ $(LDFLAGS)

$(CLIENT_BIN): $(OBJ_CLIENT)
	$(CC) $(CFLAGS) $(OBJ_CLIENT) -o $@ $(LDFLAGS)

$(DEST_BIN): $(OBJ_DEST)
	$(CC) $(CFLAGS) $(OBJ_DEST) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# ---------- Config ----------
cfg:
	@printf "ip=127.0.0.1\nport=9000\n" > $(DIR_CFG)
	@echo "Wrote $(DIR_CFG)"
	@mkdir -p $(CLIENT_CFG_DIR)
	@cp -f $(DIR_CFG) $(CLIENT_CFG)
	@echo "Wrote $(CLIENT_CFG)"

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

run_client: cfg $(CLIENT_BIN)
	./$(CLIENT_BIN)

run_dest: $(DEST_BIN)
	./$(DEST_BIN)

# ---------- Valgrind ----------
valgrind_dir: cfg $(DIR_BIN)
	valgrind $(VALGRIND_FLAGS) ./$(DIR_BIN) $(DIR_CFG)

valgrind_relay: cfg $(RELAY_BIN)
	valgrind $(VALGRIND_FLAGS) ./$(RELAY_BIN) $(DIR_CFG)

valgrind_client: cfg $(CLIENT_BIN)
	valgrind $(VALGRIND_FLAGS) ./$(CLIENT_BIN)

valgrind_dest: $(DEST_BIN)
	valgrind $(VALGRIND_FLAGS) ./$(DEST_BIN)

# ---------- Stop relays ----------
stop_relays:
	@echo "Stopping relay nodes..."
	@pkill -f "$(RELAY_BIN)" || true

# ---------- Cleanup ----------
clean:
	rm -f \
		$(OBJ_DIR) $(OBJ_RELAY) $(OBJ_CLIENT) $(OBJ_DEST) \
		$(DIR_BIN) $(RELAY_BIN) $(CLIENT_BIN) $(DEST_BIN) \
		$(DIR_CFG) $(CLIENT_CFG) \
		valgrind.relay.*.log valgrind.dir.*.log valgrind.client.*.log valgrind.dest.*.log

.PHONY: all clean cfg \
	run_dir run_relay run_relays10 run_client run_dest \
	valgrind_dir valgrind_relay valgrind_client valgrind_dest \
	stop_relays
