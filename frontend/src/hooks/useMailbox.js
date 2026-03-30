import { useEffect, useMemo, useState } from "react";
import {
  DATE_FILTERS,
  initialMessages,
  liveMessage,
  SEND_STAGES,
} from "@/data/mockMessages";
import { withinDateFilter } from "@/utils/mail";

export function useMailbox() {
  const [isAuthenticated, setIsAuthenticated] = useState(false);
  const [messages, setMessages] = useState(initialMessages);
  const [folder, setFolder] = useState("Inbox");
  const [search, setSearch] = useState("");
  const [senderFilter, setSenderFilter] = useState("All senders");
  const [dateFilter, setDateFilter] = useState(DATE_FILTERS[0]);
  const [selectedId, setSelectedId] = useState("m-1002");
  const [composeOpen, setComposeOpen] = useState(false);
  const [sendStep, setSendStep] = useState(-1);
  const [liveToastVisible, setLiveToastVisible] = useState(false);
  const [composeForm, setComposeForm] = useState({
    recipient: "",
    subject: "",
    message: "",
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
    if (!isAuthenticated) {
      return;
    }

    const timer = window.setTimeout(() => {
      setMessages((prev) => [liveMessage, ...prev]);
      setFolder("Inbox");
      setSelectedId(liveMessage.id);
      setLiveToastVisible(true);
    }, 5500);

    return () => window.clearTimeout(timer);
  }, [isAuthenticated]);

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
    if (sendStep < 0 || sendStep >= SEND_STAGES.length - 1) {
      return;
    }

    const timer = window.setTimeout(() => {
      setSendStep((prev) => prev + 1);
    }, 1100);

    return () => window.clearTimeout(timer);
  }, [sendStep]);

  useEffect(() => {
    if (sendStep !== SEND_STAGES.length - 1) {
      return;
    }

    const timer = window.setTimeout(() => {
      const sentMessage = {
        id: `m-sent-${Date.now()}`,
        folder: "Sent",
        fromName: "Alex Rivera",
        fromId: "alex@sams.io",
        to: composeForm.recipient,
        subject: composeForm.subject || "Untitled secure message",
        preview: composeForm.message.slice(0, 120) || "Secure message sent.",
        body: composeForm.message.split("\n").filter(Boolean),
        timestamp: "2026-03-17T11:25:00",
        dateLabel: "11:25 AM",
        unread: false,
        verified: true,
        encrypted: true,
        attachments: [],
      };

      setMessages((prev) => [sentMessage, ...prev]);
      setFolder("Sent");
      setSelectedId(sentMessage.id);
      setComposeOpen(false);
      setSendStep(-1);
      setComposeForm({
        recipient: "",
        subject: "",
        message: "",
      });
    }, 900);

    return () => window.clearTimeout(timer);
  }, [sendStep, composeForm]);

  function login() {
    setIsAuthenticated(true);
  }

  function updateComposeField(field, value) {
    setComposeForm((prev) => ({
      ...prev,
      [field]: value,
    }));
  }

  function startSecureSend() {
    if (!composeForm.recipient.trim() || !composeForm.message.trim()) {
      return;
    }

    setSendStep(0);
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
      sendStep,
      liveToastVisible,
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
      startSecureSend,
    },
  };
}