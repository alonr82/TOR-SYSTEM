#include "db_manager.h"
#include <sqlcipher/sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static sqlite3 *g_db = NULL;

bool db_init(const char *db_path, const char *password)
{
    if (sqlite3_open(db_path, &g_db) != SQLITE_OK)
    {
        printf("[DB Error] Can't open database: %s\n", sqlite3_errmsg(g_db));
        return false;
    }

    if (sqlite3_key(g_db, password, (int)strlen(password)) != SQLITE_OK)
    {
        printf("[DB Error] Failed to set encryption key.\n");
        return false;
    }

    const char *sql_identity = 
        "CREATE TABLE IF NOT EXISTS identity ("
        "id INTEGER PRIMARY KEY, "
        "pub_key BLOB, "
        "priv_key BLOB);";

    const char *sql_peers = 
        "CREATE TABLE IF NOT EXISTS peers ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "identity BLOB UNIQUE);";

    const char *sql_messages = 
        "CREATE TABLE IF NOT EXISTS messages ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "peer_db_id INTEGER, "
        "is_sender INTEGER, "
        "content TEXT, "
        "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);";

    char *err_msg = NULL;
    if (sqlite3_exec(g_db, sql_identity, NULL, 0, &err_msg) != SQLITE_OK) {
        sqlite3_free(err_msg); return false;
    }
    if (sqlite3_exec(g_db, sql_peers, NULL, 0, &err_msg) != SQLITE_OK) {
        sqlite3_free(err_msg); return false;
    }
    if (sqlite3_exec(g_db, sql_messages, NULL, 0, &err_msg) != SQLITE_OK) {
        sqlite3_free(err_msg); return false;
    }

    return true;
}

void db_close(void)
{
    if (g_db != NULL) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
}

bool db_load_identity(uint8_t pub_key[32], uint8_t priv_key[32], bool *found)
{
    *found = false;
    if (g_db == NULL) return false;

    const char *sql = "SELECT pub_key, priv_key FROM identity WHERE id = 1;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const void *pub = sqlite3_column_blob(stmt, 0);
            int pub_len = sqlite3_column_bytes(stmt, 0);
            const void *priv = sqlite3_column_blob(stmt, 1);
            int priv_len = sqlite3_column_bytes(stmt, 1);

            if (pub_len == 32 && priv_len == 32) {
                memcpy(pub_key, pub, 32);
                memcpy(priv_key, priv, 32);
                *found = true;
            }
        }
        sqlite3_finalize(stmt);
        return true;
    }
    return false;
}

bool db_save_identity(const uint8_t pub_key[32], const uint8_t priv_key[32])
{
    if (g_db == NULL) return false;
    const char *sql = "INSERT OR REPLACE INTO identity (id, pub_key, priv_key) VALUES (1, ?, ?);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_blob(stmt, 1, pub_key, 32, SQLITE_STATIC);
        sqlite3_bind_blob(stmt, 2, priv_key, 32, SQLITE_STATIC);
        bool res = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return res;
    }
    return false;
}

int db_get_or_create_peer(const uint8_t identity_pub[32])
{
    if (g_db == NULL) return -1;
    
    const char *select_sql = "SELECT id FROM peers WHERE identity = ?;";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(g_db, select_sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_blob(stmt, 1, identity_pub, 32, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
            return id;
        }
        sqlite3_finalize(stmt);
    }

    const char *insert_sql = "INSERT INTO peers (identity) VALUES (?);";
    if (sqlite3_prepare_v2(g_db, insert_sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_blob(stmt, 1, identity_pub, 32, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    if (sqlite3_prepare_v2(g_db, select_sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_blob(stmt, 1, identity_pub, 32, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
            return id;
        }
        sqlite3_finalize(stmt);
    }
    return -1;
}

bool db_save_message(int peer_db_id, const char *message, bool is_sender)
{
    if (g_db == NULL || peer_db_id < 0) return false;

    const char *insert_sql = "INSERT INTO messages (peer_db_id, is_sender, content) VALUES (?, ?, ?);";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(g_db, insert_sql, -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, peer_db_id);
    sqlite3_bind_int(stmt, 2, is_sender ? 1 : 0);
    sqlite3_bind_text(stmt, 3, message, -1, SQLITE_STATIC);

    bool retval = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    
    return retval;
}

bool db_print_chat_history(int peer_db_id)
{
    if (g_db == NULL || peer_db_id < 0) return false;

    const char *select_sql = "SELECT is_sender, content, timestamp FROM messages WHERE peer_db_id = ? ORDER BY timestamp ASC;";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(g_db, select_sql, -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, peer_db_id);

    printf("\n--- Chat History with DB Chat ID %d ---\n", peer_db_id);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int is_sender = sqlite3_column_int(stmt, 0);
        const unsigned char *content = sqlite3_column_text(stmt, 1);
        const unsigned char *timestamp = sqlite3_column_text(stmt, 2);

        if (is_sender) {
            printf("[%s] You: %s\n", timestamp, content);
        } else {
            printf("[%s] Peer: %s\n", timestamp, content);
        }
    }
    printf("----------------------------------------\n> ");
    fflush(stdout);

    sqlite3_finalize(stmt);
    return true;
}

void db_print_all_chats(void)
{
    if (g_db == NULL) return;
    const char *sql = "SELECT id, identity FROM peers;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        printf("\n--- Saved Chat Contacts ---\n");
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            const unsigned char *ident = sqlite3_column_blob(stmt, 1);
            int ident_len = sqlite3_column_bytes(stmt, 1);
            if (ident_len >= 4) {
                printf("  Chat DB ID: %d | Identity: %02X%02X%02X%02X...\n", id, ident[0], ident[1], ident[2], ident[3]);
            }
        }
        printf("---------------------------\n> ");
        fflush(stdout);
        sqlite3_finalize(stmt);
    }
}