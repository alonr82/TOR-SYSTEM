import { useState, useEffect, useRef } from "react";
import { Node } from "../Node";
import { ConnectionLine } from "../ConnectionLine";
import { AnimatedPacket } from "../AnimatedPacket";
import { PacketState, LogEntry } from "../../types";

export interface Scenario1CanvasProps {
  isAttacking: boolean;
  isDefending: boolean;
  addLog: (type: LogEntry["type"], message: string) => void;
  backendEvent?: any; 
}

const clientPos = { x: 100, y: 500 };
const directoryPos = { x: 400, y: 100 };

export function Scenario1Canvas({ isAttacking, isDefending, addLog, backendEvent }: Scenario1CanvasProps) {
  const [packets, setPackets] = useState<PacketState[]>([]);
  const [relays, setRelays] = useState<Array<{ id: string; x: number; y: number; label: string; backlog: number }>>([]);
  const [currentTargetIndex, setCurrentTargetIndex] = useState(0);
  const [hasInitialized, setHasInitialized] = useState(false);

  const addLogRef = useRef(addLog);
  useEffect(() => {
    addLogRef.current = addLog;
  }, [addLog]);

  useEffect(() => {
    if (backendEvent?.event === 'REAL_BACKLOG_UPDATE') {
      setRelays(prev => prev.map(r => 
        r.id === backendEvent.relay ? { ...r, backlog: backendEvent.count } : r
      ));
    }
  }, [backendEvent]);

  useEffect(() => {
    if (!isAttacking) {
      setPackets([]);
      setRelays([]);
      setCurrentTargetIndex(0);
      setHasInitialized(false);
      return;
    }

    if (!hasInitialized) {
      const realDockerRelays = [
        { id: `tor_relay_legit_1`, x: 200, y: 300, label: `tor_relay_legit_1`, backlog: 0 },
        { id: `tor_relay_legit_2`, x: 400, y: 300, label: `tor_relay_legit_2`, backlog: 0 },
        { id: `tor_relay_legit_3`, x: 600, y: 300, label: `tor_relay_legit_3`, backlog: 0 },
      ];
      
      setRelays(realDockerRelays);
      setHasInitialized(true);

      setTimeout(() => {
        const listPacketId = `list-request-${Date.now()}`;
        setPackets([{ id: listPacketId, path: [clientPos, directoryPos], type: "normal" }]);

        setTimeout(() => {
          setPackets([]);
          addLogRef.current("info", "Directory Server sent relay list");
          
          setTimeout(() => {
            const responsePacketId = `list-response-${Date.now()}`;
            setPackets([{ id: responsePacketId, path: [directoryPos, clientPos], type: "normal" }]);

            setTimeout(() => {
              setPackets([]);
            }, 800);
          }, 500);
        }, 800);
      }, 500);
    }
  }, [isAttacking, hasInitialized]); // ללא addLog ב-Dependencies

  useEffect(() => {
    if (!isAttacking || !hasInitialized) return;

    const staticRelayPositions = [
      { id: `tor_relay_legit_1`, x: 200, y: 300 },
      { id: `tor_relay_legit_2`, x: 400, y: 300 },
      { id: `tor_relay_legit_3`, x: 600, y: 300 },
    ];

    let localTargetIndex = 0;

    const interval = setInterval(() => {
      const targetRelay = staticRelayPositions[localTargetIndex];

      if (!isDefending) {
        for (let i = 0; i < 3; i++) {
          setTimeout(() => {
            const synPacketId = `syn-${Date.now()}-${Math.random()}`;
            setPackets((prev) => [...prev, { id: synPacketId, path: [clientPos, { x: targetRelay.x, y: targetRelay.y }], type: "syn" }]);

            setTimeout(() => {
              setPackets((prev) => prev.filter((p) => p.id !== synPacketId));
            }, 400);
          }, i * 150);
        }
      } else {
        for (let i = 0; i < 2; i++) {
          setTimeout(() => {
            const synPacketId = `syn-defense-${Date.now()}-${Math.random()}`;
            setPackets((prev) => [...prev, { id: synPacketId, path: [clientPos, { x: targetRelay.x, y: targetRelay.y }], type: "syn" }]);

            setTimeout(() => {
              setPackets((prev) => prev.filter((p) => p.id !== synPacketId));
            }, 300);
          }, i * 150);
        }

        setTimeout(() => {
          const legitPacketId = `legit-${Date.now()}`;
          setPackets((prev) => [...prev, { id: legitPacketId, path: [clientPos, { x: targetRelay.x, y: targetRelay.y }], type: "normal" }]);

          setTimeout(() => {
            setPackets((prev) => prev.filter((p) => p.id !== legitPacketId));
          }, 600);
        }, 300);
      }

      localTargetIndex = (localTargetIndex + 1) % 3;
      setCurrentTargetIndex(localTargetIndex);
      
    }, 1200);

    return () => clearInterval(interval);
  }, [isAttacking, isDefending, hasInitialized]);

  return (
    <div className="w-full h-full relative">
      
      {isAttacking && relays.length > 0 && (
        <div className="absolute top-4 right-4 bg-slate-950 border-2 border-slate-700 rounded-lg p-3 w-64 shadow-2xl z-20">
          <h3 className="text-[11px] font-bold text-slate-300 mb-3 flex items-center gap-2 uppercase tracking-wider">
            <span className={`w-2 h-2 rounded-full animate-pulse ${isDefending ? 'bg-green-500' : 'bg-blue-500'}`}></span>
            Real-Time TCP Queues
          </h3>
          <div className="space-y-3">
            {relays.map((r) => {
              const percent = (r.backlog / 128) * 100;
              let color = "bg-blue-500";
              let textColor = "text-blue-400";
              
              if (!isDefending) {
                if (percent >= 100) { color = "bg-red-500"; textColor = "text-red-400"; }
                else if (percent >= 60) { color = "bg-yellow-500"; textColor = "text-yellow-400"; }
              }
              
              return (
                <div key={r.id} className="text-[10px]">
                  <div className="flex justify-between text-slate-400 mb-1">
                    <span className="font-mono">{r.label}</span>
                    <span className={`font-mono font-bold ${isDefending ? 'text-green-400' : textColor}`}>
                      {isDefending ? "PROTECTED (Cookies)" : `${r.backlog}/128`}
                    </span>
                  </div>
                  <div className="w-full bg-slate-800 rounded-full h-1.5 overflow-hidden">
                    <div 
                      className={`h-1.5 rounded-full transition-all duration-300 ${isDefending ? 'bg-green-500' : color}`} 
                      style={{ width: isDefending ? '100%' : `${Math.min(percent, 100)}%` }}
                    ></div>
                  </div>
                </div>
              );
            })}
          </div>
        </div>
      )}

      <ConnectionLine x1={clientPos.x} y1={clientPos.y} x2={directoryPos.x} y2={directoryPos.y} />
      <Node {...directoryPos} label="tor_directory" type="directory" />
      <Node {...clientPos} label="tor_client_1 (Attacker)" type="attacker" />

      {relays.map((relay, idx) => (
        <Node
          key={relay.id}
          x={relay.x}
          y={relay.y}
          label={relay.label}
          type="guard"
          hasShield={isDefending && idx === currentTargetIndex}
          backlogProgress={!isDefending ? (relay.backlog / 128) * 100 : undefined}
        />
      ))}

      {packets.map((packet) => (
        <AnimatedPacket
          key={packet.id}
          id={packet.id}
          path={packet.path}
          type={packet.type}
          duration={packet.type === "syn" ? 0.4 : 0.8}
        />
      ))}
    </div>
  );
}