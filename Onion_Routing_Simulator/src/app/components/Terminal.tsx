import { useState, useRef, useEffect } from "react";
import { LogEntry } from "../types";

export interface TerminalLogEntry {
  id: string;
  tab: string;
  type: string;
  message: string;
  timestamp: string;
}

export interface TerminalProps {
  logs: TerminalLogEntry[];
  onCommandSubmit?: (command: string) => void;
}

export function Terminal({ logs, onCommandSubmit }: TerminalProps) {
  const [activeTab, setActiveTab] = useState<"visual" | "c_engine" | "network">("visual");
  const [input, setInput] = useState("");
  const endRef = useRef<HTMLDivElement>(null);

  const filteredLogs = logs.filter((log) => log.tab === activeTab);

  useEffect(() => {
    endRef.current?.scrollIntoView({ behavior: "smooth" });
  }, [filteredLogs]);

  const handleKeyDown = (e: React.KeyboardEvent<HTMLInputElement>) => {
    if (e.key === "Enter" && input.trim() !== "") {
      if (onCommandSubmit) {
        onCommandSubmit(input);
      }
      setInput("");
      // התיקון שלנו: קופצים אוטומטית לטאב של ה-C כדי לראות את התגובה!
      setActiveTab("c_engine");
    }
  };

  return (
    <div className="flex flex-col h-full bg-black border-t-2 border-slate-700">
      {/* טאבים */}
      <div className="flex gap-2 p-2 bg-slate-900 border-b border-slate-800">
        <button
          onClick={() => setActiveTab("visual")}
          className={`px-3 py-1 rounded text-xs font-bold font-mono transition-colors ${
            activeTab === "visual" ? "bg-slate-700 text-blue-400" : "text-slate-500 hover:text-slate-300"
          }`}
        >
          SIMULATOR_EVENTS
        </button>
        <button
          onClick={() => setActiveTab("c_engine")}
          className={`px-3 py-1 rounded text-xs font-bold font-mono transition-colors ${
            activeTab === "c_engine" ? "bg-slate-700 text-green-400" : "text-slate-500 hover:text-slate-300"
          }`}
        >
          C_ENGINE_STDOUT
        </button>
        <button
          onClick={() => setActiveTab("network")}
          className={`px-3 py-1 rounded text-xs font-bold font-mono transition-colors ${
            activeTab === "network" ? "bg-slate-700 text-yellow-400" : "text-slate-500 hover:text-slate-300"
          }`}
        >
          KERNEL_TCP_LOGS
        </button>
      </div>

      {/* אזור הלוגים (Scrollable) */}
      <div className="flex-1 overflow-y-auto p-4 font-mono text-xs sm:text-sm">
        {filteredLogs.map((log) => {
          let colorClass = "text-slate-300";
          if (log.type === "error" || log.message.includes("SECURITY ALERT") || log.message.includes("FAILED")) colorClass = "text-red-500";
          else if (log.type === "success" || log.message.includes("established") || log.message.includes("verified")) colorClass = "text-green-500";
          else if (log.type === "alert" || log.message.includes("WARNING")) colorClass = "text-yellow-400";
          else if (log.tab === "c_engine") colorClass = "text-green-400";

          return (
            <div key={log.id} className={`${colorClass} mb-1 whitespace-pre-wrap`}>
              <span className="text-slate-600 mr-2">[{log.timestamp}]</span>
              {log.message}
            </div>
          );
        })}
        <div ref={endRef} />
      </div>

      {/* שורת הפקודה האינטראקטיבית (Interactive Prompt) */}
      <div className="bg-slate-950 border-t border-slate-800 p-2 px-4 flex items-center font-mono text-xs sm:text-sm">
        <span className="text-green-500 mr-2 font-bold">tor-client@ubuntu:~$</span>
        <input
          type="text"
          value={input}
          onChange={(e) => setInput(e.target.value)}
          onKeyDown={handleKeyDown}
          spellCheck={false}
          autoComplete="off"
          className="flex-1 bg-transparent text-slate-100 outline-none placeholder-slate-700"
          placeholder="Type command (/connect IP PORT, /history, /clients)..."
        />
      </div>
    </div>
  );
}