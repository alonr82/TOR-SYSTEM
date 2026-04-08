#ifndef DB_MANAGER_H
#define DB_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#define DB_MAX_CHAT_RESULTS 128
#define DB_MAX_HISTORY_RESULTS 256
#define DB_MAX_MESSAGE_TEXT 2048
#define DB_MAX_TIMESTAMP_TEXT 64

typedef struct
{
    int peer_db_id;
    uint8_t identity_pub[32];
} db_chat_info_t;

typedef struct
{
    bool is_sender;
    char content[DB_MAX_MESSAGE_TEXT];
    char timestamp[DB_MAX_TIMESTAMP_TEXT];
} db_history_item_t;

bool db_init(const char *db_path, const char *password);
void db_close(void);

bool db_load_identity(uint8_t pub_key[32], uint8_t priv_key[32], bool *found);
bool db_save_identity(const uint8_t pub_key[32], const uint8_t priv_key[32]);

int db_get_or_create_peer(const uint8_t identity_pub[32]);

bool db_save_message(int peer_db_id, const char *message, bool is_sender);

bool db_get_all_chats(db_chat_info_t *out_chats, int max_chats, int *out_found);
bool db_get_chat_history(int peer_db_id, db_history_item_t *out_items, int max_items, int *out_found);

bool db_print_chat_history(int peer_db_id);
void db_print_all_chats(void);

#endif