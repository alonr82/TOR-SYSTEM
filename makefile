CC      = gcc
CFLAGS  = -Wall -Wextra -pedantic -std=c11 -g
LDFLAGS = -pthread

CRYPTO_LIBS = -lcrypto

INCLUDES = \
	-Icommon \
	-Icommon/header \
	-Icommon/simulation/header \
	-Iserver/configurations/header \
	-Iserver/run_time/header \
	-Iserver/connections_manager/header \
	-Iserver/relay_manager/header \
	-Isock_utilities/header \
	-Irelay/relay_reg/header \
	-Irelay/run_time/header \
	-Irelay/forward_msg/header \
	-Iclient/create_circuit/header \
	-Iclient/header \
	-Iclient/client_run \
	-Iclient/client_run/header \
	-Iclient/service/header \
	-Idest_server/header \
	-Icrypto/header \
	-Idata_base/header

DIR_BIN            = dir_server
RELAY_BIN          = relay_node
CLIENT_BIN         = client_test
CLIENT_SERVICE_BIN = client_service
DEST_BIN           = dest_server_bin
TEST_CRYPTO_BIN    = test_crypto_bin

DIR_CFG ?= dir.cfg
CLIENT_CFG_DIR = config
CLIENT_CFG     = $(CLIENT_CFG_DIR)/dir_server_config.cfg
SIMULATION_DIR = simulation
MALICIOUS_REGISTRY_FILE = $(SIMULATION_DIR)/malicious_relays_registry.txt

VALGRIND_FLAGS = --leak-check=full --show-leak-kinds=all --track-origins=yes

SRC_SIM = \
	common/simulation/src/simulation_state.c \
	common/simulation/src/simulation_events.c

SRC_CRYPTO = \
	crypto/src/chacha20.c \
	crypto/src/diffie_hellman.c \
	crypto/src/signatures.c

SRC_TEST_CRYPTO = \
	crypto/src/crypto_test.c \
	$(SRC_CRYPTO)

SRC_DIR = \
	server/src/main_test.c \
	server/run_time/src/run_time.c \
	common/src/dir_server_config.c \
	common/src/relay_server_config.c \
	server/connections_manager/src/relay_handler.c \
	server/relay_manager/src/relay_manager.c \
	sock_utilities/src/create_bind.c \
	sock_utilities/src/accept.c \
	sock_utilities/src/tor_in_out.c \
	$(SRC_SIM) \
	$(SRC_CRYPTO)

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
	sock_utilities/src/tor_in_out.c \
	$(SRC_SIM) \
	$(SRC_CRYPTO)

SRC_CLIENT = \
	client/src/client_main.c \
	data_base/src/db_manager.c \
	client/client_run/src/client_run.c \
	client/create_circuit/src/create_circuit.c \
	common/src/dir_server_config.c \
	sock_utilities/src/connect_server.c \
	sock_utilities/src/tor_in_out.c \
	$(SRC_SIM) \
	$(SRC_CRYPTO)

SRC_CLIENT_SERVICE = \
	client/service/src/client_service_main.c \
	client/service/src/client_service.c \
	data_base/src/db_manager.c \
	client/client_run/src/client_run.c \
	client/create_circuit/src/create_circuit.c \
	common/src/dir_server_config.c \
	sock_utilities/src/connect_server.c \
	sock_utilities/src/tor_in_out.c \
	$(SRC_SIM) \
	$(SRC_CRYPTO)

SRC_DEST = \
	dest_server/src/dest_server.c \
	dest_server/src/dest_main.c \
	sock_utilities/src/tor_in_out.c \
	$(SRC_CRYPTO)

OBJ_DIR            = $(SRC_DIR:.c=.o)
OBJ_RELAY          = $(SRC_RELAY:.c=.o)
OBJ_CLIENT         = $(SRC_CLIENT:.c=.o)
OBJ_CLIENT_SERVICE = $(SRC_CLIENT_SERVICE:.c=.o)
OBJ_DEST           = $(SRC_DEST:.c=.o)

all: $(DIR_BIN) $(RELAY_BIN) $(CLIENT_BIN) $(CLIENT_SERVICE_BIN) $(DEST_BIN) $(TEST_CRYPTO_BIN)

$(DIR_BIN): $(OBJ_DIR)
	$(CC) $(CFLAGS) $(OBJ_DIR) -o $@ $(LDFLAGS) $(CRYPTO_LIBS)

$(RELAY_BIN): $(OBJ_RELAY)
	$(CC) $(CFLAGS) $(OBJ_RELAY) -o $@ $(LDFLAGS) $(CRYPTO_LIBS)

$(CLIENT_BIN): $(OBJ_CLIENT)
	$(CC) $(CFLAGS) $(OBJ_CLIENT) -o $@ $(LDFLAGS) $(CRYPTO_LIBS) -lsqlcipher

$(CLIENT_SERVICE_BIN): $(OBJ_CLIENT_SERVICE)
	$(CC) $(CFLAGS) $(OBJ_CLIENT_SERVICE) -o $@ $(LDFLAGS) $(CRYPTO_LIBS) -lsqlcipher

$(DEST_BIN): $(SRC_DEST)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRC_DEST) -o $@ $(LDFLAGS) $(CRYPTO_LIBS)

$(TEST_CRYPTO_BIN): $(SRC_TEST_CRYPTO)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRC_TEST_CRYPTO) -o $@ $(LDFLAGS) $(CRYPTO_LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

cfg:
	@printf "ip=127.0.0.1\nport=9000\n" > $(DIR_CFG)
	@echo "Wrote $(DIR_CFG)"
	@mkdir -p $(CLIENT_CFG_DIR)
	@mkdir -p $(SIMULATION_DIR)
	@cp -f $(DIR_CFG) $(CLIENT_CFG)
	@echo "Wrote $(CLIENT_CFG)"

run_dir: cfg $(DIR_BIN)
	./$(DIR_BIN) $(DIR_CFG)

run_relay: cfg $(RELAY_BIN)
	./$(RELAY_BIN) $(DIR_CFG)

run_relay_malicious: cfg $(RELAY_BIN)
	TOR_RELAY_MODE=malicious ./$(RELAY_BIN) $(DIR_CFG)

run_relays10: cfg $(RELAY_BIN)
	for i in 1 2 3 4 5 6 7 8 9 10; do \
		./$(RELAY_BIN) $(DIR_CFG) & \
	done; \
	wait

run_client: cfg $(CLIENT_BIN)
	./$(CLIENT_BIN)

run_client_service: cfg $(CLIENT_SERVICE_BIN)
	./$(CLIENT_SERVICE_BIN)

run_dest: $(DEST_BIN)
	./$(DEST_BIN)

run_crypto: $(TEST_CRYPTO_BIN)
	./$(TEST_CRYPTO_BIN)

valgrind_dir: cfg $(DIR_BIN)
	valgrind $(VALGRIND_FLAGS) ./$(DIR_BIN) $(DIR_CFG)

valgrind_relay: cfg $(RELAY_BIN)
	valgrind $(VALGRIND_FLAGS) ./$(RELAY_BIN) $(DIR_CFG)

valgrind_relay_malicious: cfg $(RELAY_BIN)
	TOR_RELAY_MODE=malicious valgrind $(VALGRIND_FLAGS) ./$(RELAY_BIN) $(DIR_CFG)

valgrind_client: cfg $(CLIENT_BIN)
	valgrind $(VALGRIND_FLAGS) ./$(CLIENT_BIN)

valgrind_client_service: cfg $(CLIENT_SERVICE_BIN)
	valgrind $(VALGRIND_FLAGS) ./$(CLIENT_SERVICE_BIN)

valgrind_dest: $(DEST_BIN)
	valgrind $(VALGRIND_FLAGS) ./$(DEST_BIN)

valgrind_crypto: $(TEST_CRYPTO_BIN)
	valgrind $(VALGRIND_FLAGS) ./$(TEST_CRYPTO_BIN)

stop_relays:
	@echo "Stopping relay nodes..."
	@pkill -f "$(RELAY_BIN)" || true

clean:
	rm -f \
		$(OBJ_DIR) $(OBJ_RELAY) $(OBJ_CLIENT) $(OBJ_CLIENT_SERVICE) $(OBJ_DEST) \
		$(DIR_BIN) $(RELAY_BIN) $(CLIENT_BIN) $(CLIENT_SERVICE_BIN) $(DEST_BIN) $(TEST_CRYPTO_BIN) \
		$(DIR_CFG) $(CLIENT_CFG) $(MALICIOUS_REGISTRY_FILE) \
		valgrind.relay.*.log valgrind.dir.*.log valgrind.client.*.log valgrind.client_service.*.log valgrind.dest.*.log

.PHONY: all clean cfg \
	run_dir run_relay run_relay_malicious run_relays10 run_client run_client_service run_dest run_crypto \
	valgrind_dir valgrind_relay valgrind_relay_malicious valgrind_client valgrind_client_service valgrind_dest valgrind_crypto \
	stop_relays