#include "db_manager.h"
#include <sqlcipher/sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

static sqlite3 *g_db = NULL;

static bool ensure_directory_exists(const char *dir_path)
{
    bool retval = true;
    struct stat st;

    if (dir_path == NULL || dir_path[0] == '\0')
    {
        retval = false;
    }
    else
    {
        if (stat(dir_path, &st) == 0)
        {
            if (S_ISDIR(st.st_mode) == 0)
            {
                retval = false;
            }
        }
        else
        {
            if (mkdir(dir_path, 0700) != 0)
            {
                if (errno != EEXIST)
                {
                    retval = false;
                }
            }
        }
    }

    return retval;
}

static bool ensure_parent_directory_for_db(const char *db_path)
{
    bool retval = true;
    char path_copy[512];
    char *last_slash = NULL;

    if (db_path == NULL)
    {
        retval = false;
    }
    else
    {
        memset(path_copy, 0, sizeof(path_copy));
        strncpy(path_copy, db_path, sizeof(path_copy) - 1);

        last_slash = strrchr(path_copy, '/');
        if (last_slash != NULL)
        {
            if (last_slash == path_copy)
            {
                last_slash[1] = '\0';
            }
            else
            {
                *last_slash = '\0';
            }

            if (strcmp(path_copy, "/") != 0)
            {
                retval = ensure_directory_exists(path_copy);
            }
        }
    }

    return retval;
}

static bool try_open_database(const char *db_path, const char *password)
{
    bool retval = true;

    if (ensure_parent_directory_for_db(db_path) == false)
    {
        retval = false;
    }
    else
    {
        if (sqlite3_open(db_path, &g_db) != SQLITE_OK)
        {
            retval = false;
        }
        else
        {
            if (sqlite3_key(g_db, password, (int)strlen(password)) != SQLITE_OK)
            {
                printf("[DB Error] Failed to set encryption key.\n");
                sqlite3_close(g_db);
                g_db = NULL;
                retval = false;
            }
        }
    }

    return retval;
}

bool db_init(const char *db_path, const char *password)
{
    bool retval = true;
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

    if (db_path == NULL || password == NULL)
    {
        retval = false;
    }
    else
    {
        if (g_db != NULL)
        {
            sqlite3_close(g_db);
            g_db = NULL;
        }

        if (try_open_database(db_path, password) == false)
        {
            if (strcmp(db_path, "/client_data/chat_history.db") == 0)
            {
                if (ensure_directory_exists("client_data") == true)
                {
                    if (try_open_database("client_data/chat_history.db", password) == true)
                    {
                        retval = true;
                    }
                    else
                    {
                        retval = false;
                    }
                }
                else
                {
                    retval = false;
                }
            }
            else
            {
                retval = false;
            }
        }
    }

    if (retval == false)
    {
        if (g_db != NULL)
        {
            printf("[DB Error] Can't open database: %s\n", sqlite3_errmsg(g_db));
            sqlite3_close(g_db);
            g_db = NULL;
        }
        else
        {
            printf("[DB Error] Can't open database: unable to open database file\n");
        }
    }
    else
    {
        if (sqlite3_exec(g_db, sql_identity, NULL, 0, &err_msg) != SQLITE_OK)
        {
            sqlite3_free(err_msg);
            retval = false;
        }
        else
        {
            if (sqlite3_exec(g_db, sql_peers, NULL, 0, &err_msg) != SQLITE_OK)
            {
                sqlite3_free(err_msg);
                retval = false;
            }
            else
            {
                if (sqlite3_exec(g_db, sql_messages, NULL, 0, &err_msg) != SQLITE_OK)
                {
                    sqlite3_free(err_msg);
                    retval = false;
                }
            }
        }
    }

    if (retval == false)
    {
        if (g_db != NULL)
        {
            sqlite3_close(g_db);
            g_db = NULL;
        }
    }

    return retval;
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

bool db_get_all_chats(db_chat_info_t *out_chats, int max_chats, int *out_found)
{
    bool retval = false;
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT id, identity FROM peers;";
    int found = 0;

    if (out_found != NULL)
    {
        *out_found = 0;
    }

    if (g_db != NULL && out_chats != NULL && out_found != NULL && max_chats > 0)
    {
        if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) == SQLITE_OK)
        {
            while (sqlite3_step(stmt) == SQLITE_ROW && found < max_chats)
            {
                out_chats[found].peer_db_id = sqlite3_column_int(stmt, 0);

                {
                    const void *identity_blob = sqlite3_column_blob(stmt, 1);
                    int identity_len = sqlite3_column_bytes(stmt, 1);

                    memset(out_chats[found].identity_pub, 0, sizeof(out_chats[found].identity_pub));

                    if (identity_blob != NULL && identity_len == 32)
                    {
                        memcpy(out_chats[found].identity_pub, identity_blob, 32);
                    }
                }

                found++;
            }

            *out_found = found;
            retval = true;
            sqlite3_finalize(stmt);
        }
    }

    return retval;
}

bool db_get_chat_history(int peer_db_id, db_history_item_t *out_items, int max_items, int *out_found)
{
    bool retval = false;
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT is_sender, content, timestamp FROM messages WHERE peer_db_id = ? ORDER BY timestamp ASC;";
    int found = 0;

    if (out_found != NULL)
    {
        *out_found = 0;
    }

    if (g_db != NULL && out_items != NULL && out_found != NULL && max_items > 0 && peer_db_id >= 0)
    {
        if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) == SQLITE_OK)
        {
            sqlite3_bind_int(stmt, 1, peer_db_id);

            while (sqlite3_step(stmt) == SQLITE_ROW && found < max_items)
            {
                const unsigned char *content = NULL;
                const unsigned char *timestamp = NULL;

                out_items[found].is_sender = (sqlite3_column_int(stmt, 0) != 0);

                memset(out_items[found].content, 0, sizeof(out_items[found].content));
                memset(out_items[found].timestamp, 0, sizeof(out_items[found].timestamp));

                content = sqlite3_column_text(stmt, 1);
                timestamp = sqlite3_column_text(stmt, 2);

                if (content != NULL)
                {
                    strncpy(out_items[found].content, (const char*)content, sizeof(out_items[found].content) - 1);
                }

                if (timestamp != NULL)
                {
                    strncpy(out_items[found].timestamp, (const char*)timestamp, sizeof(out_items[found].timestamp) - 1);
                }

                found++;
            }

            *out_found = found;
            retval = true;
            sqlite3_finalize(stmt);
        }
    }

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