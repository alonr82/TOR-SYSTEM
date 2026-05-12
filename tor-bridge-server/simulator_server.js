const WebSocket = require('ws');
const { exec, spawn } = require('child_process');

const wss = new WebSocket.Server({ port: 8082 });
console.log("🛡️  Simulator Bridge Server running on ws://localhost:8082");

let clientProcess = null;
let dirLogsProcess = null; 
let isClientStarting = false;
let isClientReady = false; 
let attackPending = false; 
let currentScenario = 1; 

let availableRelays = [];
let maliciousRelayIndex = -1;
let clientLocalPort = 34500; 
let synQueueInterval = null;

function broadcastVisualLog(level, message) {
    if (!message) return;
    wss.clients.forEach(c => { if (c.readyState === WebSocket.OPEN) c.send(JSON.stringify({ type: 'log', tab: 'visual', level, message: message.trim() })); });
}
function broadcastCEngineLog(message) {
    if (!message) return;
    wss.clients.forEach(c => { if (c.readyState === WebSocket.OPEN) c.send(JSON.stringify({ type: 'log', tab: 'c_engine', message: message.trim() })); });
}
function broadcastNetworkLog(level, message) {
    if (!message) return;
    wss.clients.forEach(c => { if (c.readyState === WebSocket.OPEN) c.send(JSON.stringify({ type: 'log', tab: 'network', level, message: message.trim() })); });
}
function broadcastVisualEvent(eventName, payload = {}) {
    wss.clients.forEach(c => { if (c.readyState === WebSocket.OPEN) c.send(JSON.stringify({ type: 'visual', event: eventName, ...payload })); });
}

function runDockerCmd(cmd, successEvent) {
    broadcastCEngineLog(`[SYS EXEC] ${cmd}`);
    exec(cmd, (error, stdout, stderr) => {
        if (stdout) broadcastCEngineLog(`[SYS OUT] ${stdout}`);
        if (successEvent) broadcastVisualEvent(successEvent);
    });
}

function startDirectoryLogs() {
    if (dirLogsProcess) dirLogsProcess.kill();
    dirLogsProcess = spawn('docker', ['logs', '-f', '--tail', '50', 'tor_directory_1']);
    let dirStdoutBuffer = "";
    
    dirLogsProcess.stdout.on('data', (data) => {
        dirStdoutBuffer += data.toString();
        let lines = dirStdoutBuffer.split('\n');
        dirStdoutBuffer = lines.pop(); 
        
        for (const line of lines) {
            // קריאת ציונים משלימה מה-Directory (אם הדוקר יואיל בטובו לפלוט אותם)
            const dirMatch = line.match(/IP adress:\s*([\d\.]+).*Trust:\s*(\d+).*SameSource:\s*(\d+).*Uptime:\s*(\d+)/);
            if (dirMatch) {
                broadcastVisualEvent('SYBIL_DIR_UPDATE', {
                    ip: dirMatch[1],
                    trustScore: parseInt(dirMatch[2], 10),
                    sameSource: parseInt(dirMatch[3], 10),
                    uptime: parseInt(dirMatch[4], 10)
                });
            }
            if (line.includes('[Pinned Guards Array]')) {
                 broadcastVisualEvent('PINNED_GUARDS_ARRAY', { message: line.trim() });
            }
        }
    });
    dirLogsProcess.on('close', () => { 
        dirLogsProcess = null; 
        setTimeout(startDirectoryLogs, 1000); 
    });
}

function gracefulClientRestart(callback) {
    if (clientProcess && clientProcess.stdin && isClientReady) {
        broadcastCEngineLog(`\n> [Auto] Sending /quit to safely shut down old process...`);
        clientProcess.stdin.write("/quit\n");
    }
    setTimeout(() => {
        if (clientProcess) { clientProcess.kill(); clientProcess = null; }
        isClientReady = false;
        exec('docker exec tor_client_1 pkill -9 client_test', () => { if (callback) callback(); });
    }, 500);
}

function ensureClientRunning() {
    if (clientProcess || isClientStarting) return;
    isClientStarting = true;
    isClientReady = false;
    startDirectoryLogs(); 
    
    exec('docker exec tor_client_1 mkdir -p /client_data', () => {
        clientProcess = spawn('docker', ['exec', '-i', 'tor_client_1', './client_test']);
        isClientStarting = false;
        let stdoutBuffer = ""; 

        clientProcess.stdout.on('data', (data) => {
            const chunk = data.toString();
            broadcastCEngineLog(chunk);

            if (chunk.includes('Enter local listen port:')) {
                clientLocalPort++;
                if (clientProcess && clientProcess.stdin) clientProcess.stdin.write(`${clientLocalPort}\n`);
            }
            if (chunk.includes('Enter Master Password')) {
                if (clientProcess && clientProcess.stdin) clientProcess.stdin.write("12345\n");
            }
            if (chunk.includes('[System] Chat interface ready.')) {
                isClientReady = true;
                if (attackPending) {
                    attackPending = false;
                    setTimeout(() => {
                        availableRelays = [];
                        maliciousRelayIndex = -1;
                        if (clientProcess && clientProcess.stdin) clientProcess.stdin.write("/connect 10.0.0.5 80\n");
                    }, 500);
                }
            }

            if (chunk.includes("Press ENTER to build a default protected circuit") || chunk.includes("or 'c' to build a custom route")) {
                if (currentScenario === 2) {
                    setTimeout(() => { if (clientProcess && clientProcess.stdin) clientProcess.stdin.write("c\n"); }, 300);
                } else {
                    setTimeout(() => { if (clientProcess && clientProcess.stdin) clientProcess.stdin.write("\n"); }, 300);
                }
            }
            
            if (chunk.includes("Enter 3 relay indices")) {
                setTimeout(() => {
                    if (clientProcess && clientProcess.stdin) {
                        let route = "0 1 2";
                        if (maliciousRelayIndex !== -1 && availableRelays.length >= 2) {
                            route = `${availableRelays[0]} ${maliciousRelayIndex} ${availableRelays[1]}`;
                        }
                        clientProcess.stdin.write(`${route}\n`);
                    }
                }, 300);
            }

            stdoutBuffer += chunk;
            let lines = stdoutBuffer.split('\n');
            stdoutBuffer = lines.pop(); 
            let eventDelay = 0;

            for (const line of lines) {
                if (line.includes('using pinned Guard') || line.includes('pinned Guard already exists') || line.includes('[Pinned Guard Active]')) {
                    broadcastVisualEvent('PINNED_GUARD_ACTIVE', { active: true });
                }
                if (line.includes('No pinned Guard found') || line.includes('failed')) {
                    broadcastVisualEvent('PINNED_GUARD_ACTIVE', { active: false });
                }

                // הפיתרון! יצירת הראוטרים על המסך מתוך ההדפסה של הלקוח (תמיד עובד)
                const clientRelayMatch = line.match(/\[(\d+)\]\s*IP:\s*([\d\.]+).*Malicious\(sim\):\s*(yes|no)/);
                if (clientRelayMatch) {
                    const idx = clientRelayMatch[1];
                    const ip = clientRelayMatch[2];
                    const isMalicious = clientRelayMatch[3] === 'yes';
                    
                    if (isMalicious) maliciousRelayIndex = idx;
                    else availableRelays.push(idx);

                    broadcastVisualEvent('SYBIL_NODE_UPDATE', {
                        index: idx,
                        ip: ip,
                        isMalicious: isMalicious
                    });
                }

                // תפיסת ה-IP שאליו ה-C מתחבר כדי שהאנימציה תעקוב אחריו!
                const connectMatch = line.match(/circuit_connect_guard: attempting to connect to IP:\s*([\d\.]+)/);
                if (connectMatch) {
                    setTimeout(() => broadcastVisualEvent('REAL_CIRCUIT_START', { targetIp: connectMatch[1] }), eventDelay);
                    eventDelay += 150;
                }

                if (line.includes('invalid signature in EXTENDED!') || line.includes('Signature verification FAILED')) {
                    setTimeout(() => {
                        broadcastVisualLog('error', `[C Engine Native] Security Breach Detected`);
                        broadcastVisualEvent('REAL_MAC_FAILED_DEFENSE');
                    }, eventDelay);
                    eventDelay += 150;
                } else if (line.includes('SECURITY ALERT')) {
                    setTimeout(() => broadcastVisualEvent('REAL_MAC_FAILED_ATTACK'), eventDelay);
                    eventDelay += 150;
                }
                if (line.includes('build_default_circuit: circuit_extend failed') || line.includes('circuit_create failed')) {
                    setTimeout(() => { broadcastVisualEvent('REAL_CIRCUIT_TEARDOWN'); }, eventDelay);
                    eventDelay += 150;
                }
                if (line.match(/Connected! Circuit built/)) {
                    setTimeout(() => broadcastVisualEvent('CIRCUIT_BUILT'), eventDelay);
                    eventDelay += 150;
                }
            }
        });

        clientProcess.on('close', () => {
            clientProcess = null;
            isClientStarting = false;
            isClientReady = false;
        });
    });
}

ensureClientRunning();

wss.on('connection', (ws) => {
    ws.on('message', (message) => {
        const data = JSON.parse(message);
        if (data.scenario) currentScenario = data.scenario;

        if (data.action === "STDIN") {
            if (clientProcess && clientProcess.stdin) clientProcess.stdin.write(data.payload + "\n");
            return;
        }

        if (data.scenario === 1) { 
            if (data.action === "ATTACK") {
                const syncookieState = data.isDefending ? 1 : 0;
                runDockerCmd(`docker exec -u root tor_relay_legit_1 sysctl -w net.ipv4.tcp_syncookies=${syncookieState}`, null);
                runDockerCmd(`docker exec -u root tor_relay_legit_2 sysctl -w net.ipv4.tcp_syncookies=${syncookieState}`, null);
                runDockerCmd(`docker exec -u root tor_relay_legit_3 sysctl -w net.ipv4.tcp_syncookies=${syncookieState}`, null);

                const attackProc = spawn('docker', ['exec', '-u', 'root', '-w', '/app', '-t', 'tor_client_1', './syn_flooder']);
                attackProc.stdout.on('data', d => broadcastCEngineLog(`[ATTACKER] ${d.toString()}`));
                attackProc.stderr.on('data', d => broadcastCEngineLog(`[ATTACKER ERROR] ${d.toString()}`));

                synQueueInterval = setInterval(async () => {
                    const relays = ['tor_relay_legit_1', 'tor_relay_legit_2', 'tor_relay_legit_3'];
                    for (const relay of relays) {
                        await new Promise((resolve) => {
                            exec(`docker exec -u root ${relay} bash -c "netstat -an | grep -c SYN_RECV"`, (err, stdout) => {
                                let count = parseInt(stdout) || 0;
                                if (data.isDefending) count = 0;
                                broadcastVisualEvent('REAL_BACKLOG_UPDATE', { relay: relay, count: count });
                                let level = 'info'; 
                                if (count >= 128) level = 'error'; 
                                else if (count >= 80) level = 'alert'; 
                                broadcastNetworkLog(level, `[Kernel TCP] ${relay} SYN_RECV: ${count}/128 ${data.isDefending ? '(SYN Cookies Active)' : ''}`);
                                resolve();
                            });
                        });
                    }
                }, 1000);
            } else if (data.action === "DEFENSE_ON") {
                if (synQueueInterval) clearInterval(synQueueInterval);
                runDockerCmd('docker exec -u root tor_relay_legit_1 sysctl -w net.ipv4.tcp_syncookies=1', 'SYN_DEFENSE_ON');
                runDockerCmd('docker exec -u root tor_relay_legit_2 sysctl -w net.ipv4.tcp_syncookies=1', null);
                runDockerCmd('docker exec -u root tor_relay_legit_3 sysctl -w net.ipv4.tcp_syncookies=1', null);
                broadcastVisualLog('success', "🛡️ SYN Cookies activated on all relays! TCP Backlogs bypassed.");
            } else if (data.action === "RESET") {
                if (synQueueInterval) clearInterval(synQueueInterval);
                runDockerCmd('docker exec -u root tor_client_1 pkill syn_flooder', 'SYN_FLOOD_STOPPED');
            }
        }

        if (data.scenario === 2) { 
            if (data.action === "ATTACK") {
                runDockerCmd(`docker exec tor_directory_1 sh -c "echo 'sim circ_ext attack on' > /proc/1/fd/0"`, null);
                if (!clientProcess || !isClientReady) {
                    attackPending = true;
                    ensureClientRunning();
                } else {
                    if (clientProcess && clientProcess.stdin) clientProcess.stdin.write("/connect 10.0.0.5 80\n");
                }
            } else if (data.action === "DEFENSE_ON" || data.action === "DEFENSE_OFF") {
                const defenseState = data.action === "DEFENSE_ON" ? "on" : "off";
                runDockerCmd(`docker exec tor_directory_1 sh -c "echo 'sim circ_ext defense ${defenseState}' > /proc/1/fd/0"`, null);
                if (!clientProcess || !isClientReady) {
                    ensureClientRunning();
                } else if (clientProcess && clientProcess.stdin) {
                    clientProcess.stdin.write(data.action === "DEFENSE_ON" ? "/defense on\n" : "/defense off\n");
                }
            } else if (data.action === "RESET") {
                runDockerCmd(`docker exec tor_directory_1 sh -c "echo 'sim reset' > /proc/1/fd/0"`, null);
                attackPending = false;
                gracefulClientRestart(() => { ensureClientRunning(); });
            }
        }

        if (data.scenario === 3) { 
            if (data.action === "ATTACK") {
                runDockerCmd(`docker exec tor_directory_1 sh -c "echo 'sim sybil attack on' > /proc/1/fd/0"`, null);
                if (!clientProcess || !isClientReady) {
                    attackPending = true;
                    ensureClientRunning();
                } else {
                    if (clientProcess && clientProcess.stdin) clientProcess.stdin.write("/connect 10.0.0.5 80\n");
                }
            } else if (data.action === "DEFENSE_ON" || data.action === "DEFENSE_OFF") {
                const defenseState = data.action === "DEFENSE_ON" ? "on" : "off";
                runDockerCmd(`docker exec tor_directory_1 sh -c "echo 'sim sybil defense ${defenseState}' > /proc/1/fd/0"`, null);
                if (!clientProcess || !isClientReady) {
                    ensureClientRunning();
                } else if (clientProcess && clientProcess.stdin) {
                    clientProcess.stdin.write(data.action === "DEFENSE_ON" ? "/defense on\n" : "/defense off\n");
                }
            } else if (data.action === "RESET") {
                runDockerCmd(`docker exec tor_directory_1 sh -c "echo 'sim reset' > /proc/1/fd/0"`, null);
                attackPending = false;
                broadcastVisualEvent('SYBIL_RESET');
                gracefulClientRestart(() => { ensureClientRunning(); });
            }
        }
    });
});