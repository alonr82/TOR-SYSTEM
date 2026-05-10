const WebSocket = require('ws');
const { spawn } = require('child_process');

const wss = new WebSocket.Server({ port: 8081 });
console.log("🚀 Mail Bridge Server is running on ws://localhost:8081");

let peerProcess = null;
let isPeerReady = false;
let connectionCallback = null;
let connectionConfig = null;

function startPeer(onReady) {
    if (peerProcess) {
        if (isPeerReady) {
            onReady();
        } else {
            connectionCallback = onReady;
        }
        return;
    }

    const CONTAINER_NAME = 'tor_client_1'; 
    const CLIENT_EXECUTABLE = './client_test'; 

    console.log(`[NODE] Booting up C Client INSIDE Docker (${CONTAINER_NAME})...`);
    peerProcess = spawn('docker', ['exec', '-i', CONTAINER_NAME, CLIENT_EXECUTABLE]);

    isPeerReady = false;
    connectionCallback = onReady;

    peerProcess.stdin.on('error', (err) => console.error(`[PIPE ERROR]: ${err.message}`));

    peerProcess.stdout.on('data', (data) => {
        const text = data.toString();
        console.log(`[C CLIENT]: ${text.trim()}`);

        // אתחול אוטומטי של תוכנת ה-C
        if (text.includes('Enter local listen port:')) {
            const port = connectionConfig?.listenPort || "34523";
            console.log(`[NODE] Auto-typing port: ${port}`);
            peerProcess.stdin.write(`${port}\n`);
        }
        else if (text.includes('Enter Master Password')) {
            const pwd = connectionConfig?.dbPassword || "12345";
            console.log(`[NODE] Auto-typing vault password...`);
            peerProcess.stdin.write(`${pwd}\n`);
        }
        else if (text.includes("Press ENTER to build a default protected circuit")) {
            if (connectionConfig?.routeMode === 'custom') {
                console.log("[NODE] Selecting Custom Route ('c')");
                peerProcess.stdin.write("c\n");
            } else {
                console.log("[NODE] Selecting Default Route (ENTER)");
                peerProcess.stdin.write("\n");
            }
        }
        else if (text.includes('Chat interface ready') || text.includes('> ')) {
            if (!isPeerReady) {
                console.log("✅ [NODE] C Client inside Docker is READY!");
                isPeerReady = true;
                if (connectionCallback) {
                    connectionCallback();
                    connectionCallback = null;
                }
            }
        }

        // --- התיקון הגדול מס' 1: חיבור מוצלח ---
        if (text.includes('Connected! Circuit built')) {
            // שולפים את ה-ID של החיבור שה-C קבע (לרוב זה 0)
            const match = text.match(/Connection ID (\d+)/);
            const connId = match ? parseInt(match[1]) : 0;

            wss.clients.forEach(client => {
                if (client.readyState === WebSocket.OPEN) {
                    // 1. אומרים ל-useMailbox.js שהפקודה הצליחה ומעבירים לו את ה-ID
                    client.send(JSON.stringify({
                        type: 'ack',
                        command: 'CONNECT',
                        ok: true,
                        connectionId: connId
                    }));

                    // 2. מקפיצים ל-Inbox הודעת מערכת ירוקה שהחיבור הושלם
                    client.send(JSON.stringify({
                        type: 'connected',
                        connectionId: connId,
                        chatId: 1
                    }));
                }
            });
        }

        // --- התיקון הגדול מס' 2: קבלת הודעות חזרה! ---
        if (text.includes('[Peer ')) {
            const match = text.match(/\[Peer (\d+)\] (.*)/);
            if (match) {
                wss.clients.forEach(client => {
                    if (client.readyState === WebSocket.OPEN) {
                        // מעבירים את ההודעה לריאקט כדי שתקפוץ ב-Inbox
                        client.send(JSON.stringify({
                            type: 'incoming_message',
                            chatId: parseInt(match[1]),
                            connectionId: 0,
                            body: match[2].trim()
                        }));
                    }
                });
            }
        }
    });

    peerProcess.stderr.on('data', (err) => console.error(`[C CLIENT ERR]: ${err.toString().trim()}`));

    peerProcess.on('close', (code) => {
        console.log(`🔴 Docker process exited with code ${code}`);
        peerProcess = null;
        isPeerReady = false;
    });
}

wss.on('connection', (ws) => {
    console.log("🟢 React Mail Client connected!");

    // מסמנים לריאקט שהגשר פתוח כדי להוריד חיווי שגיאות מהממשק
    ws.send(JSON.stringify({ type: 'bridge_ready', port: 8081 }));

    ws.on('message', (message) => {
        try {
            const data = JSON.parse(message);
            
            if (data.type === 'connect') {
                console.log(`\n[REACT] Connection request to ${data.ip}:${data.port}`);
                connectionConfig = data; 
                startPeer(() => {
                    console.log(`[NODE] Injecting command: /connect ${data.ip} ${data.port}`);
                    peerProcess.stdin.write(`/connect ${data.ip} ${data.port}\n`);
                });
            }

            // --- התיקון הגדול מס' 3: תפיסת השליחה מהריאקט ---
            if (data.type === 'send_message') {
                console.log(`\n[REACT] Sending message: ${data.message}`);
                
                if (!peerProcess || !isPeerReady) {
                    return;
                }
                
                const connId = data.connectionId !== undefined ? data.connectionId : 0;
                console.log(`[NODE] Injecting command: /send ${connId} ${data.message}`);
                
                // הזרקת ההודעה לתוכנת ה-C
                peerProcess.stdin.write(`/send ${connId} ${data.message}\n`);
                
                // שליחת ACK לריאקט כדי שיסיים את האנימציה של המודל ויעביר לתיקיית Sent
                ws.send(JSON.stringify({
                    type: 'ack',
                    command: 'SEND',
                    ok: true
                }));
            }

        } catch (e) {
            console.error("Failed to parse message:", e);
        }
    });

    ws.on('close', () => console.log("🔴 React Client disconnected."));
});