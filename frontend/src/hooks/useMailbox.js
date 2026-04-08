import { useEffect, useMemo, useRef, useState } from "react";
import { DATE_FILTERS, FOLDERS } from "@/data/mockMessages";
import { withinDateFilter } from "@/utils/mail";

const BRIDGE_URL = "ws://localhost:8081";

function createSystemMessage(subject, preview, bodyLines = []) {
  const now = new Date();

  return {
    id: `sys-${Date.now()}-${Math.random().toString(16).slice(2)}`,
    folder: "Inbox",
    fromName: "System",
    fromId: "bridge@local",
    to: "alex@sams.io",
    subject,
    preview,
    body: bodyLines.length > 0 ? bodyLines : [preview],
    timestamp: now.toISOString(),
    dateLabel: now.toLocaleTimeString("en-US", {
      hour: "2-digit",
      minute: "2-digit",
    }),
    unread: true,
    verified: true,
    encrypted: true,
    attachments: [],
  };
}

function createPeerMessage(chatId, connectionId, bodyText) {
  const now = new Date();

  return {
    id: `msg-${Date.now()}-${Math.random().toString(16).slice(2)}`,
    folder: "Inbox",
    fromName: `Peer ${chatId >= 0 ? chatId : connectionId}`,
    fromId: `peer-${chatId >= 0 ? chatId : connectionId}@tor.local`,
    to: "alex@sams.io",
    subject: `Secure message on connection ${connectionId}`,
    preview: bodyText.slice(0, 120),
    body: [bodyText],
    timestamp: now.toISOString(),
    dateLabel: now.toLocaleTimeString("en-US", {
      hour: "2-digit",
      minute: "2-digit",
    }),
    unread: true,
    verified: true,
    encrypted: true,
    attachments: [],
    connectionId,
    chatId,
  };
}

function createSentMessage(connectionId, bodyText) {
  const now = new Date();

  return {
    id: `sent-${Date.now()}-${Math.random().toString(16).slice(2)}`,
    folder: "Sent",
    fromName: "Alex Rivera",
    fromId: "alex@sams.io",
    to: `connection-${connectionId}@tor.local`,
    subject: `Secure send on connection ${connectionId}`,
    preview: bodyText.slice(0, 120),
    body: [bodyText],
    timestamp: now.toISOString(),
    dateLabel: now.toLocaleTimeString("en-US", {
      hour: "2-digit",
      minute: "2-digit",
    }),
    unread: false,
    verified: true,
    encrypted: true,
    attachments: [],
    connectionId,
  };
}

export function useMailbox() {
  const socketRef = useRef(null);
  const initializedRef = useRef(false);

  const [isAuthenticated, setIsAuthenticated] = useState(false);
  const [messages, setMessages] = useState([]);
  const [folder, setFolder] = useState("Inbox");
  const [search, setSearch] = useState("");
  const [senderFilter, setSenderFilter] = useState("All senders");
  const [dateFilter, setDateFilter] = useState(DATE_FILTERS[0]);
  const [selectedId, setSelectedId] = useState(null);
  const [composeOpen, setComposeOpen] = useState(false);
  const [connectOpen, setConnectOpen] = useState(false);
  const [sendStep, setSendStep] = useState(-1);
  const [liveToastVisible, setLiveToastVisible] = useState(false);
  const [serviceStatus, setServiceStatus] = useState("Disconnected");
  const [lastAck, setLastAck] = useState("");
  const [connections, setConnections] = useState([]);
  const [activeConnectionId, setActiveConnectionId] = useState(null);

  const [composeForm, setComposeForm] = useState({
    recipient: "",
    subject: "",
    message: "",
  });

  const [connectForm, setConnectForm] = useState({
    ip: "127.0.0.1",
    port: "",
    dbPassword: "secret",
    listenPort: "34523",
    routeMode: "default",
    route0: "0",
    route1: "1",
    route2: "2",
  });

  const senderOptions = useMemo(() => {
    return [
      "All senders",
      ...Array.from(
        new Set(
          messages
            .filter((item) => item.folder === "Inbox")
            .map((item) => item.fromName)
        )
      ),
    ];
  }, [messages]);

  const folderCounts = useMemo(() => {
    return {
      Inbox: messages.filter((item) => item.folder === "Inbox").length,
      Sent: messages.filter((item) => item.folder === "Sent").length,
      Drafts: messages.filter((item) => item.folder === "Drafts").length,
      Trash: messages.filter((item) => item.folder === "Trash").length,
    };
  }, [messages]);

  const filteredMessages = useMemo(() => {
    return messages
      .filter((item) => item.folder === folder)
      .filter((item) => {
        const haystack =
          `${item.fromName} ${item.fromId} ${item.subject} ${item.preview}`.toLowerCase();
        return haystack.includes(search.toLowerCase());
      })
      .filter((item) => {
        if (folder !== "Inbox") {
          return true;
        }
        return senderFilter === "All senders"
          ? true
          : item.fromName === senderFilter;
      })
      .filter((item) => {
        if (folder !== "Inbox") {
          return true;
        }
        return withinDateFilter(item.timestamp, dateFilter);
      });
  }, [messages, folder, search, senderFilter, dateFilter]);

  const selectedMessage =
    filteredMessages.find((item) => item.id === selectedId) ||
    filteredMessages[0] ||
    null;

  useEffect(() => {
    if (!selectedMessage && filteredMessages.length > 0) {
      setSelectedId(filteredMessages[0].id);
    }
  }, [filteredMessages, selectedMessage]);

  useEffect(() => {
    if (!liveToastVisible) {
      return;
    }

    const timer = window.setTimeout(() => {
      setLiveToastVisible(false);
    }, 4000);

    return () => window.clearTimeout(timer);
  }, [liveToastVisible]);

  useEffect(() => {
    if (!isAuthenticated) {
      return;
    }

    if (socketRef.current !== null) {
      return;
    }

    const ws = new WebSocket(BRIDGE_URL);
    socketRef.current = ws;

    ws.onopen = () => {
      setServiceStatus("Connected to bridge");
    };

    ws.onclose = () => {
      setServiceStatus("Bridge disconnected");
      socketRef.current = null;
      initializedRef.current = false;
    };

    ws.onerror = () => {
      setServiceStatus("Bridge error");
    };

    ws.onmessage = (event) => {
      let payload = null;

      try {
        payload = JSON.parse(event.data);
      } catch (error) {
        return;
      }

      if (payload.type === "bridge_ready") {
        setServiceStatus(`Bridge ready on ${payload.port}`);

        if (initializedRef.current === false) {
          initializedRef.current = true;

          ws.send(
            JSON.stringify({
              type: "init",
              listenPort: Number(connectForm.listenPort),
              dbPassword: connectForm.dbPassword,
            })
          );
        }
      } else if (payload.type === "ack") {
        setLastAck(`${payload.command} ${payload.ok ? "ok" : "failed"}`);

        if (payload.command === "CONNECT" || payload.command === "CONNECT_CUSTOM") {
          if (payload.ok === true && typeof payload.connectionId === "number") {
            setActiveConnectionId(payload.connectionId);

            setConnections((prev) => {
              const exists = prev.some(
                (item) => item.connectionId === payload.connectionId
              );

              if (exists) {
                return prev;
              }

              return [
                ...prev,
                {
                  connectionId: payload.connectionId,
                  label: `Connection ${payload.connectionId}`,
                  status: "Active",
                },
              ];
            });

            setMessages((prev) => [
              createSystemMessage(
                "Secure route established",
                `Connection ${payload.connectionId} is ready.`,
                [
                  "A secure route was built successfully.",
                  `Connection ID: ${payload.connectionId}`,
                ]
              ),
              ...prev,
            ]);

            setConnectOpen(false);
            setFolder("Inbox");
          }
        }

        if (payload.command === "SEND" && payload.ok === true && activeConnectionId !== null) {
          setMessages((prev) => [
            createSentMessage(activeConnectionId, composeForm.message),
            ...prev,
          ]);

          setComposeForm({
            recipient: "",
            subject: "",
            message: "",
          });

          setComposeOpen(false);
          setSendStep(-1);
          setFolder("Sent");
        }
      } else if (payload.type === "connected") {
        setMessages((prev) => [
          createSystemMessage(
            "Handshake completed",
            `Connection ${payload.connectionId} is now fully established.`,
            [
              "Secure handshake completed.",
              `Connection ID: ${payload.connectionId}`,
              `Chat ID: ${payload.chatId}`,
            ]
          ),
          ...prev,
        ]);
      } else if (payload.type === "incoming_message") {
        const newMessage = createPeerMessage(
          payload.chatId,
          payload.connectionId,
          payload.body || ""
        );

        setMessages((prev) => [newMessage, ...prev]);
        setSelectedId(newMessage.id);
        setFolder("Inbox");
        setLiveToastVisible(true);
      } else if (payload.type === "ping_ok") {
        setMessages((prev) => [
          createSystemMessage(
            "Ping successful",
            `Connection ${payload.connectionId} responded successfully.`,
            [
              "Ping successful.",
              `Connection ID: ${payload.connectionId}`,
              `Chat ID: ${payload.chatId}`,
            ]
          ),
          ...prev,
        ]);
      } else if (payload.type === "connection_dropped") {
        setConnections((prev) =>
          prev.map((item) =>
            item.connectionId === payload.connectionId
              ? { ...item, status: "Dropped" }
              : item
          )
        );

        setMessages((prev) => [
          createSystemMessage(
            "Connection dropped",
            `Connection ${payload.connectionId} dropped.`,
            [
              `Connection ID: ${payload.connectionId}`,
              `Reason: ${payload.reason || "unknown"}`,
            ]
          ),
          ...prev,
        ]);
      } else if (payload.type === "service_log") {
        setServiceStatus(payload.message);
      }
    };

    return () => {
      if (socketRef.current === ws) {
        socketRef.current = null;
      }
      ws.close();
    };
  }, [isAuthenticated]);

  function sendToBridge(message) {
    if (socketRef.current && socketRef.current.readyState === WebSocket.OPEN) {
      socketRef.current.send(JSON.stringify(message));
    }
  }

  function login() {
    setIsAuthenticated(true);
  }

  function updateComposeField(field, value) {
    setComposeForm((prev) => ({
      ...prev,
      [field]: value,
    }));
  }

  function updateConnectField(field, value) {
    setConnectForm((prev) => ({
      ...prev,
      [field]: value,
    }));
  }

  function openConnectModal() {
    setConnectOpen(true);
  }

  function openComposeModal() {
    setComposeOpen(true);
  }

  function startConnect() {
    const payload = {
      type: "connect",
      ip: connectForm.ip,
      port: Number(connectForm.port),
      routeMode: connectForm.routeMode,
    };

    if (connectForm.routeMode === "custom") {
      payload.route = [
        Number(connectForm.route0),
        Number(connectForm.route1),
        Number(connectForm.route2),
      ];
    }

    sendToBridge(payload);
  }

  function startSecureSend() {
    if (activeConnectionId === null || !composeForm.message.trim()) {
      return;
    }

    setSendStep(0);

    sendToBridge({
      type: "send_message",
      connectionId: activeConnectionId,
      message: composeForm.message,
    });
  }

  function requestPing(connectionId) {
    sendToBridge({
      type: "ping",
      connectionId,
    });
  }

  function requestDisconnect(connectionId) {
    sendToBridge({
      type: "disconnect",
      connectionId,
    });

    setConnections((prev) =>
      prev.map((item) =>
        item.connectionId === connectionId
          ? { ...item, status: "Closing" }
          : item
      )
    );
  }

  function selectConnection(connectionId) {
    setActiveConnectionId(connectionId);
  }

  return {
    state: {
      isAuthenticated,
      folder,
      search,
      senderFilter,
      dateFilter,
      selectedId,
      selectedMessage,
      filteredMessages,
      folderCounts,
      senderOptions,
      composeOpen,
      composeForm,
      connectOpen,
      connectForm,
      sendStep,
      liveToastVisible,
      serviceStatus,
      lastAck,
      connections,
      activeConnectionId,
      folders: FOLDERS,
    },
    actions: {
      login,
      setFolder,
      setSearch,
      setSenderFilter,
      setDateFilter,
      setSelectedId,
      setComposeOpen,
      updateComposeField,
      openComposeModal,
      startSecureSend,
      openConnectModal,
      setConnectOpen,
      updateConnectField,
      startConnect,
      requestPing,
      requestDisconnect,
      selectConnection,
    },
  };
}