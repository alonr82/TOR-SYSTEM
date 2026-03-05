#ifndef DB_MANAGER_H
#define DB_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

bool db_init(const char *db_path, const char *password);
void db_close(void);

/* ניהול הזהות האישית שלנו */
bool db_load_identity(uint8_t pub_key[32], uint8_t priv_key[32], bool *found);
bool db_save_identity(const uint8_t pub_key[32], const uint8_t priv_key[32]);

/* איתור או יצירת איש קשר חדש לפי המפתח שלו */
int db_get_or_create_peer(const uint8_t identity_pub[32]);

/* שמירה והצגה */
bool db_save_message(int peer_db_id, const char *message, bool is_sender);
bool db_print_chat_history(int peer_db_id);
void db_print_all_chats(void);

#endif