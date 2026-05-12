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

    let stdoutBuffer = "";

    peerProcess.stdout.on('data', (data) => {
        const text = data.toString();
        console.log(`[C CLIENT]: ${text.trim()}`);
        stdoutBuffer += text;

        if (stdoutBuffer.includes('Enter local listen port:')) {
            const port = Math.floor(30000 + Math.random() * 10000);
            console.log(`[NODE] Auto-typing random port: ${port}`);
            peerProcess.stdin.write(`${port}\n`);
            stdoutBuffer = stdoutBuffer.replace('Enter local listen port:', '');
        }
        else if (stdoutBuffer.includes('Enter Master Password')) {
            const pwd = connectionConfig?.dbPassword || "12345";
            console.log(`[NODE] Auto-typing vault password...`);
            peerProcess.stdin.write(`${pwd}\n`);
            stdoutBuffer = stdoutBuffer.replace('Enter Master Password', '');
        }
        else if (stdoutBuffer.includes("Press ENTER to build a default protected circuit")) {
            if (connectionConfig?.routeMode === 'custom') {
                console.log("[NODE] Selecting Custom Route ('c')");
                peerProcess.stdin.write("c\n");
            } else {
                console.log("[NODE] Selecting Default Route (ENTER)");
                peerProcess.stdin.write("\n");
            }
            stdoutBuffer = stdoutBuffer.replace('Press ENTER to build a default protected circuit', '');
        }
        // --- התיקון הקריטי למסלול קאסטום ---
        else if (stdoutBuffer.includes("Enter 3 relay indices separated by space")) {
            const routeStr = connectionConfig?.customRoute; 
            
            if (routeStr) {
                console.log(`[NODE] Typing user custom route from React UI: ${routeStr}`);
                peerProcess.stdin.write(`${routeStr}\n`);
            } else {
                // מקרה חירום למקרה שהריאקט לא שלח נתונים
                console.error("[NODE] ERROR: React did not send customRoute! Falling back to 0 1 2 to prevent crash.");
                peerProcess.stdin.write("0 1 2\n");
            }
            stdoutBuffer = stdoutBuffer.replace("Enter 3 relay indices", "");
        }
        else if (stdoutBuffer.includes('Chat interface ready') || stdoutBuffer.includes('> ')) {
            if (!isPeerReady) {
                console.log("✅ [NODE] C Client inside Docker is READY!");
                isPeerReady = true;
                if (connectionCallback) {
                    connectionCallback();
                    connectionCallback = null;
                }
            }
            stdoutBuffer = stdoutBuffer.replace('Chat interface ready', '').replace('> ', '');
        }

        if (text.includes('Connected! Circuit built')) {
            const match = text.match(/Connection ID (\d+)/);
            const connId = match ? parseInt(match[1]) : 0;

            wss.clients.forEach(client => {
                if (client.readyState === WebSocket.OPEN) {
                    client.send(JSON.stringify({
                        type: 'ack',
                        command: 'CONNECT',
                        ok: true,
                        connectionId: connId
                    }));

                    client.send(JSON.stringify({
                        type: 'connected',
                        connectionId: connId,
                        chatId: 1
                    }));
                }
            });
        }

        if (text.includes('[Peer ')) {
            const match = text.match(/\[Peer (\d+)\] (.*)/);
            if (match) {
                wss.clients.forEach(client => {
                    if (client.readyState === WebSocket.OPEN) {
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

        if (text.includes('[System] Failed to build connection.')) {
            if (connectionConfig) {
                console.log("⚠️ [NODE] Connection intercepted by malicious relay! Retrying automatically...");
                setTimeout(() => {
                    console.log(`[NODE] Re-Injecting command: /connect ${connectionConfig.ip} ${connectionConfig.port}`);
                    peerProcess.stdin.write(`/connect ${connectionConfig.ip} ${connectionConfig.port}\n`);
                }, 1500); 
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

    ws.send(JSON.stringify({ type: 'bridge_ready', port: 8081 }));

    ws.on('message', (message) => {
        try {
            const data = JSON.parse(message);
            
            if (data.type === 'connect') {
                console.log(`\n[REACT] Connection request to ${data.ip}:${data.port}`);
                connectionConfig = data; 
                startPeer(() => {
                    console.log("[NODE] Activating Security Defenses for Mail Client...");
                    peerProcess.stdin.write("/defense on\n");

                    setTimeout(() => {
                        console.log(`[NODE] Injecting command: /connect ${data.ip} ${data.port}`);
                        peerProcess.stdin.write(`/connect ${data.ip} ${data.port}\n`);
                    }, 500);
                });
            }

            if (data.type === 'send_message') {
                console.log(`\n[REACT] Sending message: ${data.message}`);
                
                if (!peerProcess || !isPeerReady) {
                    return;
                }
                
                const connId = data.connectionId !== undefined ? data.connectionId : 0;
                console.log(`[NODE] Injecting command: /send ${connId} ${data.message}`);
                
                peerProcess.stdin.write(`/send ${connId} ${data.message}\n`);
                
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