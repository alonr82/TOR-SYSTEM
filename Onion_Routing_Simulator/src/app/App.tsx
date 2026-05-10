import { useState, useEffect, useCallback, useRef } from "react";
import { NetworkCanvas } from "./components/NetworkCanvas";
import { ControlPanel } from "./components/ControlPanel";
import { Terminal, TerminalLogEntry } from "./components/Terminal";

export type Scenario = 1 | 2 | 3;

export default function App() {
  const [currentScenario, setCurrentScenario] = useState<Scenario>(1);
  const [isAttacking, setIsAttacking] = useState(false);
  const [isDefending, setIsDefending] = useState(false);
  const [logs, setLogs] = useState<TerminalLogEntry[]>([]);
  const [backendEvent, setBackendEvent] = useState<any>(null);

  const [terminalHeight, setTerminalHeight] = useState(250);
  const [isDragging, setIsDragging] = useState(false);

  const socketRef = useRef<WebSocket | null>(null);

  const addLog = useCallback((type: "info" | "success" | "alert" | "error", message: string) => {
    const timestamp = new Date().toLocaleTimeString();
    const isCLog = message.startsWith("[C] ");
    const tab = isCLog ? "c_engine" : "visual";
    const cleanMessage = isCLog ? message.substring(4) : message;
    
    setLogs((prev) => [...prev.slice(-99), { 
        id: `${Date.now()}-${Math.random()}`, 
        tab, 
        type, 
        message: cleanMessage, 
        timestamp 
    }]);
  }, []);

  useEffect(() => {
    const handleMouseMove = (e: MouseEvent) => {
      if (!isDragging) return;
      const newHeight = window.innerHeight - e.clientY;
      if (newHeight > 100 && newHeight < window.innerHeight - 150) {
        setTerminalHeight(newHeight);
      }
    };
    const handleMouseUp = () => setIsDragging(false);

    if (isDragging) {
      document.addEventListener('mousemove', handleMouseMove);
      document.addEventListener('mouseup', handleMouseUp);
      document.body.style.userSelect = 'none';
    } else {
      document.body.style.userSelect = '';
    }

    return () => {
      document.removeEventListener('mousemove', handleMouseMove);
      document.removeEventListener('mouseup', handleMouseUp);
    };
  }, [isDragging]);

  useEffect(() => {
    const ws = new WebSocket("ws://localhost:8082");

    ws.onopen = () => addLog("success", "Connected to Backend Simulator Bridge");
    ws.onclose = () => addLog("alert", "Disconnected from Backend");

    ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        if (data.type === 'log') {
          // --- התיקון הקריטי: הסרנו את החסימה! כל הלוגים מוצגים ---
          setLogs((prev) => [...prev.slice(-99), {
              id: `${Date.now()}-${Math.random()}`,
              tab: data.tab,
              type: data.level || "info",
              message: data.message,
              timestamp: new Date().toLocaleTimeString()
          }]);
        } else if (data.type === 'visual') {
          setBackendEvent(data);
        }
      } catch (e) {
        console.error("Failed to parse websocket message", e);
      }
    };

    socketRef.current = ws;
    return () => ws.close();
  }, [addLog]);

  const sendCommand = (action: string, overrideDefense?: boolean) => {
    const defState = overrideDefense !== undefined ? overrideDefense : isDefending;
    socketRef.current?.send(JSON.stringify({ scenario: currentScenario, action, isDefending: defState }));
  };

  const handleTerminalSubmit = (command: string) => {
    socketRef.current?.send(JSON.stringify({ action: "STDIN", payload: command }));
  };

  const handleScenarioChange = (scenario: Scenario) => {
    sendCommand("RESET", false);
    setCurrentScenario(scenario);
    setIsAttacking(false);
    setIsDefending(false);
    setBackendEvent(null);
    setLogs([]);
    addLog("info", `Scenario ${scenario} loaded`);
  };

  const handleInitiateAttack = () => {
    setIsAttacking(true);
    sendCommand("ATTACK");
  };

  const handleToggleDefense = () => {
    const newDefenseState = !isDefending;
    if (isAttacking) {
      setIsAttacking(false);
      sendCommand("RESET", newDefenseState);
      addLog("info", "🔄 Restarting attack sequence with new defense configurations...");
      setTimeout(() => {
        setIsDefending(newDefenseState);
        sendCommand(newDefenseState ? "DEFENSE_ON" : "DEFENSE_OFF", newDefenseState);
        setTimeout(() => {
          setIsAttacking(true);
          sendCommand("ATTACK", newDefenseState);
        }, 500);
      }, 500);
    } else {
      setIsDefending(newDefenseState);
      sendCommand(newDefenseState ? "DEFENSE_ON" : "DEFENSE_OFF", newDefenseState);
    }
  };

  const handleReset = () => {
    setIsAttacking(false);
    setIsDefending(false);
    setBackendEvent(null);
    sendCommand("RESET", false);
    setLogs([]);
    addLog("info", "Backend simulation reset");
  };

  return (
    <div className="size-full flex flex-col bg-slate-900 text-slate-100 overflow-hidden h-screen">
      <header className="bg-slate-950 border-b-2 border-slate-700 px-3 sm:px-6 py-3 shrink-0">
        <h1 className="text-xl font-bold mb-2">Onion Routing Simulator</h1>
        <div className="flex gap-2 overflow-x-auto">
          {[1, 2, 3].map((scenario) => (
            <button
              key={scenario}
              onClick={() => handleScenarioChange(scenario as Scenario)}
              className={`px-4 py-1.5 rounded-lg font-medium transition-all text-sm whitespace-nowrap ${
                currentScenario === scenario
                  ? "bg-blue-600 text-white shadow-lg shadow-blue-500/50"
                  : "bg-slate-800 text-slate-400 hover:bg-slate-700 hover:text-slate-200"
              }`}
            >
              {scenario === 1 && "SYN Flood"}
              {scenario === 2 && "Circuit Hijacking"}
              {scenario === 3 && "Sybil Attack"}
            </button>
          ))}
        </div>
      </header>

      <div className="flex flex-col lg:flex-row flex-1 min-h-0 relative">
        <div className="w-full lg:w-[25%] border-b lg:border-b-0 lg:border-r-2 border-slate-700 overflow-y-auto bg-slate-950">
          <ControlPanel
            currentScenario={currentScenario}
            isAttacking={isAttacking}
            isDefending={isDefending}
            onInitiateAttack={handleInitiateAttack}
            onToggleDefense={handleToggleDefense}
            onReset={handleReset}
          />
        </div>

        <div className="flex-1 bg-slate-900 relative overflow-auto">
          <div className="min-w-[800px] min-h-[600px] w-full h-full relative">
            <NetworkCanvas
              currentScenario={currentScenario}
              isAttacking={isAttacking}
              isDefending={isDefending}
              addLog={addLog}
              backendEvent={backendEvent}
            />
          </div>
        </div>
      </div>
      
      <div 
        className="h-1.5 w-full bg-slate-700 hover:bg-blue-500 cursor-row-resize z-50 transition-colors"
        onMouseDown={() => setIsDragging(true)}
      ></div>

      <div style={{ height: `${terminalHeight}px` }} className="shrink-0 flex flex-col w-full bg-black">
        <div className="h-full relative overflow-hidden flex flex-col">
           <Terminal logs={logs} onCommandSubmit={handleTerminalSubmit} />
        </div>
      </div>
    </div>
  );
}