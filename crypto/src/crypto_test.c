#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "tor_crypto.h" // וודא שהקובץ הזה זמין

// פונקציית עזר להדפסת בייטים (כדי שנראה את ההצפנה בעיניים)
void print_hex(const char *label, const uint8_t *data, size_t len) {
    printf("%s: ", label);
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

int main() {
    printf("=== Starting Crypto Test ===\n");

    // 1. הכנת נתונים
    const char *msg = "Hello, this is a secret message!";
    uint16_t msg_len = (uint16_t)strlen(msg);
    uint8_t key[E2E_KEY_LEN]; 
    memset(key, 0x42, E2E_KEY_LEN);

    // 2. הקצאת זיכרון לתוצאה המוצפנת
    uint16_t frame_buffer_size = msg_len + E2E_NONCE_LEN + E2E_CIPHERTEXT_LEN_LEN + E2E_TAG_LEN;
    uint8_t *frame_buffer = malloc(frame_buffer_size);
    uint16_t final_len = 0;

    if (!frame_buffer) {
        fprintf(stderr, "Malloc failed!\n");
        return 1;
    }

    // 3. הצפנה
    printf("Encrypting '%s' (%d bytes)...\n", msg, msg_len);
    bool success = gen_encrypted_framed_message((const uint8_t*)msg, msg_len, key, frame_buffer, &final_len);

    if (success) {
        printf("Encryption SUCCESS! Total len: %d\n", final_len);
        print_hex("Encrypted Frame", frame_buffer, final_len);

        // --- חלק הפענוח (התיקון) ---
        
        // א. הקצאת זיכרון לפענוח (חובה!)
        uint8_t *decrypted_buffer = malloc(final_len); 
        uint16_t decrypted_len = 0;

        if (decrypted_buffer) {
            // ב. קריאה לפונקציה עם הפרמטרים הנכונים:
            // שים לב: &final_len (כי הפונקציה מקבלת מצביע לאורך הבלוב)
            if(decrypt_blob(frame_buffer, &final_len, key, decrypted_buffer, &decrypted_len))
            {
                // הוספת null terminator כדי להדפיס כמחרוזת
                decrypted_buffer[decrypted_len] = '\0'; 
                printf("Decryption SUCCESS: %s\n", decrypted_buffer);
            }
            else
            {
                printf("Decryption FAILED inside logic.\n");
            }
            free(decrypted_buffer); // לא לשכוח לשחרר
        }
    } else {
        printf("Encryption FAILED!\n");
    }

    free(frame_buffer);
    printf("=== Test Finished ===\n");
    return 0;
}