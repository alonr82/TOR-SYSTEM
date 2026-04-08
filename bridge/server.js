const http = require("http");
const path = require("path");
const { spawn } = require("child_process");
const WebSocket = require("ws");

const PORT = process.env.BRIDGE_PORT ? Number(process.env.BRIDGE_PORT) : 8081;
const SERVICE_BIN = process.env.CLIENT_SERVICE_BIN
  ? process.env.CLIENT_SERVICE_BIN
  : path.resolve(__dirname, "../client_service");

let serviceProcess = null;
let serviceStdoutBuffer = "";
let serviceStderrBuffer = "";

const server = http.createServer((req, res) => {
  if (req.url === "/health") {
    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(JSON.stringify({ ok: true }));
    return;
  }

  res.writeHead(404, { "Content-Type": "application/json" });
  res.end(JSON.stringify({ ok: false }));
});

const wss = new WebSocket.Server({ server });

function broadcast(payload) {
  const serialized = JSON.stringify(payload);

  wss.clients.forEach((client) => {
    if (client.readyState === WebSocket.OPEN) {
      client.send(serialized);
    }
  });
}

function flushBufferedLines(buffer, handler) {
  let workingBuffer = buffer;
  let newlineIndex = workingBuffer.indexOf("\n");

  while (newlineIndex !== -1) {
    const line = workingBuffer.slice(0, newlineIndex).trim();
    workingBuffer = workingBuffer.slice(newlineIndex + 1);

    if (line.length > 0) {
      handler(line);
    }

    newlineIndex = workingBuffer.indexOf("\n");
  }

  return workingBuffer;
}

function handleServiceEventLine(parts, line) {
  const eventName = parts[1] || "";

  if (eventName === "CONNECTED") {
    broadcast({
      type: "connected",
      connectionId: Number(parts[2]),
      chatId: Number(parts[3]),
    });
  } else if (eventName === "PING_OK") {
    broadcast({
      type: "ping_ok",
      connectionId: Number(parts[2]),
      chatId: Number(parts[3]),
    });
  } else if (eventName === "INCOMING_MESSAGE") {
    const first = line.indexOf("|");
    const second = line.indexOf("|", first + 1);
    const third = line.indexOf("|", second + 1);
    const fourth = line.indexOf("|", third + 1);

    let body = "";

    if (fourth !== -1) {
      body = line.slice(fourth + 1);
    }

    broadcast({
      type: "incoming_message",
      connectionId: Number(parts[2]),
      chatId: Number(parts[3]),
      body,
    });
  } else if (eventName === "CONNECTION_DROPPED") {
    const first = line.indexOf("|");
    const second = line.indexOf("|", first + 1);
    const third = line.indexOf("|", second + 1);
    const fourth = line.indexOf("|", third + 1);

    let reason = "";

    if (fourth !== -1) {
      reason = line.slice(fourth + 1);
    }

    broadcast({
      type: "connection_dropped",
      connectionId: Number(parts[2]),
      chatId: Number(parts[3]),
      reason,
    });
  } else {
    broadcast({
      type: "service_event_raw",
      message: line,
    });
  }
}

function handleServiceStdoutLine(line) {
  const parts = line.split("|");

  if (parts[0] === "OK") {
    const payload = {
      type: "ack",
      ok: true,
      command: parts[1] || "",
    };

    if (parts.length > 2) {
      payload.connectionId = Number(parts[2]);
    }

    broadcast(payload);
  } else if (parts[0] === "ERR") {
    broadcast({
      type: "ack",
      ok: false,
      command: parts[1] || "",
    });
  } else if (parts[0] === "EVENT") {
    handleServiceEventLine(parts, line);
  } else {
    broadcast({
      type: "service_stdout",
      message: line,
    });
  }
}

function handleServiceStderrLine(line) {
  broadcast({
    type: "service_log",
    message: line,
  });
}

function startServiceProcess() {
  serviceProcess = spawn(SERVICE_BIN, [], {
    cwd: path.resolve(__dirname, ".."),
    stdio: ["pipe", "pipe", "pipe"],
  });

  serviceProcess.stdout.on("data", (chunk) => {
    serviceStdoutBuffer += chunk.toString("utf8");
    serviceStdoutBuffer = flushBufferedLines(serviceStdoutBuffer, handleServiceStdoutLine);
  });

  serviceProcess.stderr.on("data", (chunk) => {
    serviceStderrBuffer += chunk.toString("utf8");
    serviceStderrBuffer = flushBufferedLines(serviceStderrBuffer, handleServiceStderrLine);
  });

  serviceProcess.on("exit", (code, signal) => {
    broadcast({
      type: "service_exit",
      code,
      signal,
    });
    serviceProcess = null;
  });
}

function writeCommand(line) {
  if (!serviceProcess || !serviceProcess.stdin.writable) {
    broadcast({
      type: "bridge_error",
      message: "client_service is not running",
    });
    return;
  }

  serviceProcess.stdin.write(`${line}\n`);
}

function sanitizeMessageText(text) {
  let retval = "";

  if (typeof text === "string") {
    retval = text.replace(/\r?\n/g, " ");
  }

  return retval;
}

function buildCommand(payload) {
  let command = null;

  if (payload.type === "init") {
    command = `INIT|${payload.listenPort}|${payload.dbPassword}`;
  } else if (payload.type === "connect") {
    if (payload.routeMode === "custom" && Array.isArray(payload.route) && payload.route.length === 3) {
      command = `CONNECT_CUSTOM|${payload.ip}|${payload.port}|${payload.route[0]}|${payload.route[1]}|${payload.route[2]}`;
    } else {
      command = `CONNECT|${payload.ip}|${payload.port}`;
    }
  } else if (payload.type === "send_message") {
    command = `SEND|${payload.connectionId}|${sanitizeMessageText(payload.message)}`;
  } else if (payload.type === "ping") {
    command = `PING|${payload.connectionId}`;
  } else if (payload.type === "disconnect") {
    command = `DISCONNECT|${payload.connectionId}`;
  } else if (payload.type === "list_connections") {
    command = "LIST";
  } else if (payload.type === "shutdown_service") {
    command = "QUIT";
  }

  return command;
}

wss.on("connection", (socket) => {
  socket.send(
    JSON.stringify({
      type: "bridge_ready",
      port: PORT,
    })
  );

  socket.on("message", (raw) => {
    let payload = null;
    let command = null;

    try {
      payload = JSON.parse(raw.toString("utf8"));
    } catch (error) {
      socket.send(
        JSON.stringify({
          type: "bridge_error",
          message: "invalid json",
        })
      );
      return;
    }

    command = buildCommand(payload);

    if (command === null) {
      socket.send(
        JSON.stringify({
          type: "bridge_error",
          message: "unsupported command",
        })
      );
      return;
    }

    writeCommand(command);
  });
});

startServiceProcess();

server.listen(PORT, () => {
  console.log(`Node bridge is listening on http://localhost:${PORT}`);
  console.log(`Using client_service binary: ${SERVICE_BIN}`);
});