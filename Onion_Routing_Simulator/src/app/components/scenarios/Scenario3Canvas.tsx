import { useState, useEffect } from "react";
import { Node } from "../Node";
import { ConnectionLine } from "../ConnectionLine";
import { AnimatedPacket } from "../AnimatedPacket";
import { NodePosition, PacketState, LogEntry } from "../../types";

export interface Scenario3CanvasProps {
  isAttacking: boolean;
  isDefending: boolean;
  addLog: (type: LogEntry["type"], message: string) => void;
  backendEvent?: any; 
}

const clientPos = { x: 150, y: 500 };
const directoryPos = { x: 400, y: 100 };

export function Scenario3Canvas({
  isAttacking,
  isDefending,
  addLog,
  backendEvent
}: Scenario3CanvasProps) {
  const [maliciousNodes, setMaliciousNodes] = useState<NodePosition[]>([]);
  const [legitimateGuards, setLegitimateGuards] = useState<NodePosition[]>([]);
  const [packets, setPackets] = useState<PacketState[]>([]);
  const [hasSpawned, setHasSpawned] = useState(false);
  const [guardList, setGuardList] = useState<string[]>([]);
  const [colludingNodes, setColludingNodes] = useState<[NodePosition, NodePosition] | null>(null);
  const [showCollusion, setShowCollusion] = useState(false);

  useEffect(() => {
    if (!isAttacking) {
      setMaliciousNodes([]);
      setLegitimateGuards([]);
      setPackets([]);
      setHasSpawned(false);
      setGuardList([]);
      setColludingNodes(null);
      setShowCollusion(false);
      return;
    }

    if (!hasSpawned) {
      const legitimate: NodePosition[] = [
        { id: "tor_relay_legit_1", x: 200, y: 300, label: "tor_relay_legit_1", type: "legitimate" },
        { id: "tor_relay_legit_2", x: 350, y: 300, label: "tor_relay_legit_2", type: "legitimate" },
        { id: "tor_relay_legit_3", x: 500, y: 300, label: "tor_relay_legit_3", type: "legitimate" },
      ];
      setLegitimateGuards(legitimate);

      const malicious: NodePosition[] = [];
      for (let i = 0; i < 15; i++) {
        const col = i % 5;
        const row = Math.floor(i / 5);
        malicious.push({
          id: `malicious-${i}`,
          x: 200 + col * 80,
          y: 350 + row * 60,
          label: `Spoofed_Node_${i + 1}`,
          type: "malicious",
        });
      }
      setMaliciousNodes(malicious);
      setHasSpawned(true);
      addLog("alert", `Sybil attack: tor_relay_malicious_1 spawned 15 spoofed relays into the network`);
    }
  }, [isAttacking, hasSpawned, isDefending, addLog]);

  useEffect(() => {
    if (!isAttacking || !hasSpawned || maliciousNodes.length === 0) return;

    if (!isDefending) {
      const node1 = maliciousNodes[Math.floor(Math.random() * maliciousNodes.length)];
      const node2 = maliciousNodes.filter(n => n.id !== node1.id)[Math.floor(Math.random() * (maliciousNodes.length - 1))];
      setColludingNodes([node1, node2]);

      setTimeout(() => {
        setShowCollusion(true);
        addLog("alert", `${node1.label} and ${node2.label} are colluding to de-anonymize traffic!`);
      }, 500);

      setGuardList(["tor_relay_legit_1", "tor_relay_legit_2", "tor_relay_legit_3", ...maliciousNodes.map((m) => m.label)]);
    } else {
      setColludingNodes(null);
      setShowCollusion(false);
      setGuardList(["tor_relay_legit_1 (Pinned)", "tor_relay_legit_2", "tor_relay_legit_3"]);
    }
  }, [isDefending, isAttacking, hasSpawned, maliciousNodes, addLog]);

  useEffect(() => {
    if (!isAttacking || (maliciousNodes.length === 0 && legitimateGuards.length === 0)) {
      return;
    }

    const interval = setInterval(() => {
      if (!isDefending) {
        const allNodes = [...legitimateGuards, ...maliciousNodes];
        const randomNode = allNodes[Math.floor(Math.random() * allNodes.length)];

        if (randomNode) {
          const packetId = `sybil-${Date.now()}`;
          setPackets([{ id: packetId, path: [clientPos, { x: randomNode.x, y: randomNode.y }], type: "normal" }]);

          setTimeout(() => {
            setPackets([]);
            const isMalicious = randomNode.type === "malicious";
            const isColluding = colludingNodes && (randomNode.id === colludingNodes[0].id || randomNode.id === colludingNodes[1].id);

            if (isColluding) {
              addLog("error", `Client connected to ${randomNode.label} (COLLUDING NODE - Traffic being monitored!)`);
            } else {
              addLog(isMalicious ? "alert" : "success", `Client connected to ${randomNode.label} ${isMalicious ? "(MALICIOUS)" : "(Legitimate)"}`);
            }
          }, 1200);
        }
      } else {
        const pinnedGuard = legitimateGuards[0];
        if (pinnedGuard) {
          const packetId = `pinned-${Date.now()}`;
          setPackets([{ id: packetId, path: [clientPos, { x: pinnedGuard.x, y: pinnedGuard.y }], type: "normal" }]);

          setTimeout(() => {
            setPackets([]);
            addLog("success", `Using pinned ${pinnedGuard.label}. Ignored 15 untrusted relays`);
          }, 1200);
        }
      }
    }, 3000);

    return () => clearInterval(interval);
  }, [isAttacking, isDefending, maliciousNodes, legitimateGuards, colludingNodes, addLog]);

  return (
    <div className="w-full h-full relative">
      {isDefending && legitimateGuards.length > 0 && (
        <ConnectionLine x1={clientPos.x} y1={clientPos.y} x2={legitimateGuards[0].x} y2={legitimateGuards[0].y} isActive />
      )}

      {showCollusion && colludingNodes && (
        <ConnectionLine x1={colludingNodes[0].x} y1={colludingNodes[0].y} x2={colludingNodes[1].x} y2={colludingNodes[1].y} isActive style={{ stroke: "#ef4444", strokeWidth: 3, strokeDasharray: "5,5" }} />
      )}

      <Node {...directoryPos} label="tor_directory" type="directory" />

      {guardList.length > 0 && (
        <div className="absolute bg-slate-950 border-2 border-slate-600 rounded-lg p-2 sm:p-3 w-40 sm:w-52" style={{ left: 10, top: 10 }}>
          <div className="text-[10px] sm:text-xs font-bold text-blue-400 mb-2">Available Guards:</div>
          <div className="max-h-32 sm:max-h-40 overflow-y-auto space-y-1">
            {guardList.slice(0, 8).map((guard, idx) => (
              <div key={idx} className={`text-[9px] sm:text-[10px] px-1.5 sm:px-2 py-1 rounded ${guard.startsWith("Spoofed") ? isDefending ? "bg-red-900/30 text-red-600 line-through" : "bg-red-900 text-red-300" : guard.includes("Pinned") ? "bg-yellow-600 text-white font-bold" : "bg-green-900 text-green-300"}`}>
                {guard} {isDefending && guard.startsWith("Spoofed") && "⏳"}
              </div>
            ))}
            {guardList.length > 8 && <div className="text-[9px] sm:text-[10px] text-slate-500 px-1.5 sm:px-2">+{guardList.length - 8} more...</div>}
          </div>
        </div>
      )}

      {legitimateGuards.map((node) => (
        <Node key={node.id} x={node.x} y={node.y} label={node.label} type="legitimate" hasLock={isDefending && node.id === "tor_relay_legit_1"} />
      ))}

      {maliciousNodes.map((node) => {
        const isColluding = !!(colludingNodes && showCollusion && (node.id === colludingNodes[0].id || node.id === colludingNodes[1].id));
        return <Node key={node.id} x={node.x} y={node.y} label={node.label} type="malicious" isDimmed={isDefending} isPulsing={isColluding} />;
      })}

      <Node {...clientPos} label="tor_client_1" type="client" hasLock={isDefending} />

      {packets.map((packet) => (
        <AnimatedPacket key={packet.id} id={packet.id} path={packet.path} type={packet.type} duration={1.2} />
      ))}
    </div>
  );
};