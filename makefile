CC      = gcc
CFLAGS  = -Wall -Wextra -pedantic -std=c11 -g
LDFLAGS = -pthread

INCLUDES = \
	-Icommon \
	-Iserver/configurations/header \
	-Iserver/run_time/header \
	-Iserver/connections_manager/header \
	-Iserver/relay_manager/header \
	-Isock_utilities/header\
	-Irelay/relay_reg/header

DIR_BIN   = dir_server
RELAY_BIN = relay_test
DIR_CFG ?= dir.cfg

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
	relay/relay_reg/src/realy_reg.c \
	server/configurations/src/dir_server_config.c \
	sock_utilities/src/connect_server.c \
	server/relay_manager/src/relay_manager.c \
	sock_utillities/src/relay_run_time.c


OBJ_DIR   = $(SRC_DIR:.c=.o)
OBJ_RELAY = $(SRC_RELAY:.c=.o)

all: $(DIR_BIN) $(RELAY_BIN)

$(DIR_BIN): $(OBJ_DIR)
	$(CC) $(CFLAGS) $(OBJ_DIR) -o $@ $(LDFLAGS)

$(RELAY_BIN): $(OBJ_RELAY)
	$(CC) $(CFLAGS) $(OBJ_RELAY) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

cfg:
	@printf "ip=127.0.0.1\nport=9000\n" > $(DIR_CFG)
	@echo "Wrote $(DIR_CFG)"

run_dir: cfg $(DIR_BIN)
	./$(DIR_BIN) $(DIR_CFG)

run_relays10: cfg $(RELAY_BIN)
	./$(RELAY_BIN) $(DIR_CFG) 10

valgrind_dir: cfg $(DIR_BIN)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(DIR_BIN) $(DIR_CFG)

valgrind_relays10: cfg $(RELAY_BIN)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(RELAY_BIN) $(DIR_CFG) 10

clean:
	rm -f $(OBJ_DIR) $(OBJ_RELAY) $(DIR_BIN) $(RELAY_BIN) $(DIR_CFG)

.PHONY: all clean cfg run_dir run_relays10 valgrind_dir valgrind_relays10
