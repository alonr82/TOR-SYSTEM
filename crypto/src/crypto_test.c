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
    
    // מפתח דמי (32 בייט) - בדרך כלל מגיע מ-Diffie-Hellman
    uint8_t key[E2E_KEY_LEN]; 
    memset(key, 0x42, E2E_KEY_LEN); // ממלאים ב-'B' סתם לצורך הבדיקה

    // 2. הקצאת זיכרון לתוצאה
    // הנוסחה: הטקסט + 12 (Nonce) + 2 (Len) + 16 (Tag)
    uint16_t frame_buffer_size = msg_len + E2E_NONCE_LEN + E2E_CIPHERTEXT_LEN_LEN + E2E_TAG_LEN;
    uint8_t *frame_buffer = malloc(frame_buffer_size);
    uint16_t final_len = 0;

    if (!frame_buffer) {
        fprintf(stderr, "Malloc failed!\n");
        return 1;
    }

    // 3. קריאה לפונקציה שלך
    printf("Encrypting '%s' (%d bytes)...\n", msg, msg_len);
    
    bool success = gen_encrypted_framed_message(
        (const uint8_t*)msg, 
        msg_len, 
        key, 
        frame_buffer, 
        &final_len
    );

    // 4. בדיקת תוצאות
    if (success) {
        printf("Encryption SUCCESS!\n");
        printf("Total frame length: %d bytes\n", final_len);
        
        // הדפסת המבנה שנוצר
        print_hex("Nonce (first 12)", frame_buffer, 12);
        print_hex("Encrypted Frame", frame_buffer, final_len);
    } else {
        printf("Encryption FAILED!\n");
    }

    // 5. שחרור זיכרון (קריטי לוואלגריינד!)
    free(frame_buffer);
    
    printf("=== Test Finished ===\n");
    return 0;
}