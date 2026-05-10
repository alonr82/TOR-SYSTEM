const WebSocket = require('ws');
const { exec, spawn } = require('child_process');

const wss = new WebSocket.Server({ port: 8082 });
console.log("🛡️  Simulator Bridge Server running on ws://localhost:8082");

let clientProcess = null;
let isClientStarting = false;
let isClientReady = false; 
let attackPending = false; 
let currentScenario = 1; 

let availableRelays = [];
let maliciousRelayIndex = -1;

let synQueueInterval = null;
let sybilScoreInterval = null;

function clearCustomIntervals() {
    if (synQueueInterval) clearInterval(synQueueInterval);
    if (sybilScoreInterval) clearInterval(sybilScoreInterval);
}

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

function gracefulClientRestart(callback) {
    if (clientProcess && clientProcess.stdin && isClientReady) {
        broadcastCEngineLog(`\n> [Auto] Sending /quit to safely release port...`);
        clientProcess.stdin.write("/quit\n");
        setTimeout(() => {
            if (clientProcess) {
                clientProcess.kill();
                clientProcess = null;
            }
            isClientReady = false;
            if (callback) callback();
        }, 800);
    } else {
        if (clientProcess) {
            clientProcess.kill();
            clientProcess = null;
        }
        isClientReady = false;
        exec('docker exec tor_client_1 pkill client_test', () => {
            if (callback) callback();
        });
    }
}

function ensureClientRunning() {
    if (clientProcess || isClientStarting) return;
    isClientStarting = true;
    isClientReady = false;
    
    exec('docker exec tor_client_1 mkdir -p /client_data', () => {
        clientProcess = spawn('docker', ['exec', '-i', 'tor_client_1', './client_test']);
        isClientStarting = false;

        let stdoutBuffer = ""; 

        clientProcess.stdout.on('data', (data) => {
            const chunk = data.toString();
            broadcastCEngineLog(chunk);

            if (chunk.includes('Enter local listen port:')) clientProcess.stdin.write("34523\n");
            if (chunk.includes('Enter Master Password')) clientProcess.stdin.write("12345\n");
            
            if (chunk.includes('[System] Chat interface ready.')) {
                isClientReady = true;
                if (attackPending) {
                    attackPending = false;
                    setTimeout(() => {
                        broadcastCEngineLog(`\n> [Auto-executing] /connect 10.0.0.5 80`);
                        availableRelays = [];
                        maliciousRelayIndex = -1;
                        clientProcess.stdin.write("/connect 10.0.0.5 80\n");
                    }, 500);
                }
            }

            if (chunk.includes("Press ENTER to build a default protected circuit, or 'c' to build a custom route:")) {
                if (currentScenario === 2) {
                    setTimeout(() => {
                        if (clientProcess && clientProcess.stdin) {
                            broadcastCEngineLog(`\n> [Auto-Selecting Custom Route for Hijack]`);
                            clientProcess.stdin.write("c\n");
                        }
                    }, 300);
                } else {
                    setTimeout(() => {
                        if (clientProcess && clientProcess.stdin) {
                            broadcastCEngineLog(`\n> [Auto-Pressing ENTER]`);
                            clientProcess.stdin.write("\n");
                        }
                    }, 300);
                }
            }
            
            if (chunk.includes("Enter 3 relay indices separated by space")) {
                setTimeout(() => {
                    if (clientProcess && clientProcess.stdin) {
                        let route = "0 1 2";
                        if (maliciousRelayIndex !== -1 && availableRelays.length >= 2) {
                            route = `${availableRelays[0]} ${maliciousRelayIndex} ${availableRelays[1]}`;
                            broadcastCEngineLog(`\n> [Forcing Hijack Route: Guard=${availableRelays[0]}, MaliciousMiddle=${maliciousRelayIndex}, Exit=${availableRelays[1]}]`);
                        } else {
                            broadcastCEngineLog(`\n> [WARNING: Malicious relay NOT found! Using fallback: 0 1 2]`);
                        }
                        clientProcess.stdin.write(`${route}\n`);
                    }
                }, 300);
            }

            stdoutBuffer += chunk;
            let lines = stdoutBuffer.split('\n');
            stdoutBuffer = lines.pop(); 

            // התיקון הקריטי: יצירת מרווח זמן (Delay) בין אירועים כדי ש-React לא ידרוס אותם
            let eventDelay = 0;

            for (const line of lines) {
                const relayMatch = line.match(/\[(\d+)\] IP:.*Malicious\(sim\):\s*(yes|no)/);
                if (relayMatch) {
                    const idx = relayMatch[1];
                    const isMalicious = relayMatch[2] === 'yes';
                    if (isMalicious) maliciousRelayIndex = idx;
                    else availableRelays.push(idx);
                }

                if (line.includes('Selecting route') || line.includes('Route built')) {
                    setTimeout(() => broadcastVisualEvent('REAL_CIRCUIT_START'), eventDelay);
                    eventDelay += 150;
                }

                if (line.includes('[defense is active] invalid signature in EXTENDED!') || line.includes('Signature verification FAILED')) {
                    setTimeout(() => {
                        broadcastVisualLog('error', `[C Engine Native] Security Breach Detected: Invalid MAC`);
                        broadcastVisualEvent('REAL_MAC_FAILED_DEFENSE');
                    }, eventDelay);
                    eventDelay += 150;
                } else if (line.includes('SECURITY ALERT')) {
                    setTimeout(() => broadcastVisualEvent('REAL_MAC_FAILED_ATTACK'), eventDelay);
                    eventDelay += 150;
                }

                if (line.includes('build_default_circuit: circuit_extend failed') || line.includes('circuit_create failed')) {
                    setTimeout(() => {
                        broadcastVisualLog('alert', `[C Engine Native] Circuit Teardown Triggered`);
                        broadcastVisualEvent('REAL_CIRCUIT_TEARDOWN');
                    }, eventDelay);
                    eventDelay += 150;
                }

                const circuitMatch = line.match(/Connected! Circuit built \(Connection ID (\d+)\)/);
                if (circuitMatch) {
                    setTimeout(() => broadcastVisualEvent('CIRCUIT_BUILT', { connectionId: parseInt(circuitMatch[1]) }), eventDelay);
                    eventDelay += 150;
                }
            }
        });

        clientProcess.on('close', () => {
            clientProcess = null;
            isClientStarting = false;
            isClientReady = false;
            broadcastVisualEvent('CLIENT_DISCONNECTED');
        });
    });
}

ensureClientRunning();

wss.on('connection', (ws) => {
    ws.on('message', (message) => {
        const data = JSON.parse(message);

        if (data.scenario) currentScenario = data.scenario;

        if (data.action === "STDIN") {
            if (clientProcess && clientProcess.stdin) {
                broadcastCEngineLog(`\n> ${data.payload}`);
                clientProcess.stdin.write(data.payload + "\n");
            }
            return;
        }

        if (data.scenario === 1) { 
            if (data.action === "ATTACK") {
                clearCustomIntervals();
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
                clearCustomIntervals();
                runDockerCmd('docker exec -u root tor_relay_legit_1 sysctl -w net.ipv4.tcp_syncookies=1', 'SYN_DEFENSE_ON');
                runDockerCmd('docker exec -u root tor_relay_legit_2 sysctl -w net.ipv4.tcp_syncookies=1', null);
                runDockerCmd('docker exec -u root tor_relay_legit_3 sysctl -w net.ipv4.tcp_syncookies=1', null);
                broadcastVisualLog('success', "🛡️ SYN Cookies activated on all relays! TCP Backlogs bypassed.");
            } else if (data.action === "RESET") {
                clearCustomIntervals();
                runDockerCmd('docker exec -u root tor_client_1 pkill syn_flooder', 'SYN_FLOOD_STOPPED');
            }
        }

        if (data.scenario === 2) { 
            if (data.action === "ATTACK") {
                runDockerCmd(`docker exec tor_directory_1 sh -c "echo 'sim circ_ext attack on' > /proc/1/fd/0"`, null);

                if (!clientProcess) {
                    attackPending = true;
                    ensureClientRunning();
                } else if (isClientReady) {
                    availableRelays = [];
                    maliciousRelayIndex = -1;
                    broadcastCEngineLog(`\n> [Auto-executing] /connect 10.0.0.5 80`);
                    clientProcess.stdin.write("/connect 10.0.0.5 80\n");
                } else {
                    attackPending = true; 
                }
            } else if (data.action === "DEFENSE_ON" || data.action === "DEFENSE_OFF") {
                const defenseState = data.action === "DEFENSE_ON" ? "on" : "off";
                runDockerCmd(`docker exec tor_directory_1 sh -c "echo 'sim circ_ext defense ${defenseState}' > /proc/1/fd/0"`, null);
                
                if (!clientProcess) {
                    ensureClientRunning();
                } else if (isClientReady) {
                    clientProcess.stdin.write(data.action === "DEFENSE_ON" ? "/defense on\n" : "/defense off\n");
                }
            } else if (data.action === "RESET") {
                runDockerCmd(`docker exec tor_directory_1 sh -c "echo 'sim reset' > /proc/1/fd/0"`, null);
                attackPending = false;
                gracefulClientRestart(() => {
                    ensureClientRunning();
                });
            }
        }
    });
});