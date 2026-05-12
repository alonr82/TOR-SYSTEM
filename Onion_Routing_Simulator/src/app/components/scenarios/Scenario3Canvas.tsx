import { useState, useEffect, useRef } from "react";
import { motion, AnimatePresence } from "framer-motion";
import { Node } from "../Node";
import { ConnectionLine } from "../ConnectionLine";
import { AnimatedPacket } from "../AnimatedPacket";
import { ShieldCheck, ShieldAlert, Database } from "lucide-react";

export interface Scenario3CanvasProps {
  isAttacking: boolean;
  isDefending: boolean;
  backendEvent?: any; 
}

interface BackendRelay {
  ip: string;
  index: string;
  isMalicious: boolean;
  trustScore: number;
  sameSource: number;
  uptime: number;
}

const clientPos = { x: 100, y: 350 };
const directoryPos = { x: 450, y: 80 };
const targetPos = { x: 800, y: 350 };

const sleep = (ms: number) => new Promise(resolve => setTimeout(resolve, ms));

export function Scenario3Canvas({ isAttacking, isDefending, backendEvent }: Scenario3CanvasProps) {
  // המערך מבוסס כעת על כתובת ה-IP כמפתח (Key) כדי שנוכל לאתר במדויק את המטרה
  const [activeRelays, setActiveRelays] = useState<Record<string, BackendRelay>>({});
  const [pinnedGuardsLog, setPinnedGuardsLog] = useState<string[]>([]);
  const [pinnedGuardActive, setPinnedGuardActive] = useState(false);
  const [packets, setPackets] = useState<any[]>([]);
  const [circuitBroken, setCircuitBroken] = useState(false);
  const [activePath, setActivePath] = useState<{from: any, to: any, isMalicious: boolean} | null>(null);
  
  const activeRelaysRef = useRef(activeRelays);
  const backendStartRef = useRef(false);
  const targetIpRef = useRef<string>("");
  const backendResultRef = useRef<string>("none");
  const isActiveRef = useRef(false);

  useEffect(() => {
    activeRelaysRef.current = activeRelays;
  }, [activeRelays]);

  useEffect(() => {
    if (!backendEvent) return;
    const ev = backendEvent.event;

    // הרכבת הראוטרים מתוך הדפסת הלקוח
    if (ev === 'SYBIL_NODE_UPDATE') {
      setActiveRelays(prev => ({
        ...prev,
        [backendEvent.ip]: {
          ip: backendEvent.ip,
          index: backendEvent.index,
          isMalicious: backendEvent.isMalicious,
          trustScore: prev[backendEvent.ip]?.trustScore || 80, // דיפולט עד שהדוקר ישלח נתונים
          sameSource: prev[backendEvent.ip]?.sameSource || 0,
          uptime: prev[backendEvent.ip]?.uptime || 100
        }
      }));
    }

    // קליטת נתוני עומק מה-Directory
    if (ev === 'SYBIL_DIR_UPDATE') {
      setActiveRelays(prev => ({
        ...prev,
        [backendEvent.ip]: {
          ...(prev[backendEvent.ip] || { ip: backendEvent.ip, index: "99", isMalicious: false }),
          trustScore: backendEvent.trustScore,
          sameSource: backendEvent.sameSource,
          uptime: backendEvent.uptime
        }
      }));
    }
    
    if (ev === 'PINNED_GUARDS_ARRAY') {
      setPinnedGuardsLog(prev => [...prev.slice(-4), backendEvent.message]);
    }

    if (ev === 'PINNED_GUARD_ACTIVE') {
      setPinnedGuardActive(backendEvent.active);
    }

    // הלקוח ב-C מתחיל בניית מסלול ומעביר לנו את ה-IP!
    if (ev === 'REAL_CIRCUIT_START') {
        backendStartRef.current = true;
        targetIpRef.current = backendEvent.targetIp;
    }
    
    if (ev === 'REAL_MAC_FAILED_DEFENSE') backendResultRef.current = "defense_caught";
    if (ev === 'REAL_CIRCUIT_TEARDOWN') backendResultRef.current = "teardown";
    if (ev === 'CIRCUIT_BUILT') backendResultRef.current = "success";

    if (ev === 'SYBIL_RESET') {
      setActiveRelays({});
      setPinnedGuardsLog([]);
      setPinnedGuardActive(false);
    }
  }, [backendEvent]);

  useEffect(() => {
    if (!isAttacking) {
      setPackets([]);
      setActivePath(null);
      setCircuitBroken(false);
      isActiveRef.current = false;
      setActiveRelays({});
      return;
    }

    isActiveRef.current = true;

    const runSimulation = async () => {
      while (isActiveRef.current) {
        setPackets([]);
        setActivePath(null);
        setCircuitBroken(false);
        backendStartRef.current = false;
        backendResultRef.current = "none";

        // ממתינים עד שה-C ידפיס IP מטרה
        while (!backendStartRef.current && isActiveRef.current) {
          await sleep(200);
        }

        if (!isActiveRef.current) break;
        backendStartRef.current = false; 

        // אנחנו שולפים את הראוטר המדויק שה-C החליט להתחבר אליו!
        const targetIp = targetIpRef.current;
        const targetNode = activeRelaysRef.current[targetIp];
        
        let targetPosOnScreen = { x: 450, y: 250 }; 
        if (targetNode) {
          const idx = parseInt(targetNode.index);
          targetPosOnScreen = { 
            x: 250 + (idx % 4) * 120, 
            y: 250 + Math.floor(idx / 4) * 120 
          };
        }

        setActivePath({ from: clientPos, to: targetPosOnScreen, isMalicious: targetNode?.isMalicious || false });
        setPackets([{ id: Date.now(), path: [clientPos, targetPosOnScreen], type: targetNode?.isMalicious ? "corrupted" : "encrypted" }]);

        await sleep(1500);

        let waitTime = 0;
        while (backendResultRef.current === "none" && waitTime < 5000 && isActiveRef.current) {
          await sleep(200);
          waitTime += 200;
        }

        if (!isActiveRef.current) break;

        if (backendResultRef.current === "defense_caught" || backendResultRef.current === "teardown") {
           setCircuitBroken(true); 
           await sleep(3000);
        } else if (backendResultRef.current === "success") {
           setPackets([{ id: Date.now()+1, path: [targetPosOnScreen, targetPos], type: targetNode?.isMalicious ? "corrupted" : "encrypted" }]);
           await sleep(3000); 
        }
      }
    };

    runSimulation();
    return () => { isActiveRef.current = false; };
  }, [isAttacking, isDefending]);

  // סידור הראוטרים למערך עקבי שמצוייר על המסך
  const relayArray = Object.values(activeRelays).sort((a,b) => parseInt(a.index) - parseInt(b.index));

  return (
    <div className="w-full h-full relative overflow-hidden bg-slate-950">
      
      <Node {...clientPos} label="Tor Client" type="client" isError={circuitBroken} />
      <Node {...directoryPos} label="Directory Server" type="directory" />
      <Node {...targetPos} label="Destination" type="exit" />

      {activePath && (
        <ConnectionLine 
           x1={activePath.from.x} y1={activePath.from.y} 
           x2={activePath.to.x} y2={activePath.to.y} 
           isActive={!circuitBroken} 
           isBroken={circuitBroken}
           style={{ stroke: activePath.isMalicious ? "#ef4444" : (isDefending || pinnedGuardActive ? "#eab308" : "#3b82f6"), strokeWidth: (isDefending || pinnedGuardActive) ? 4 : 2 }} 
        />
      )}

      <AnimatePresence>
        {isDefending && pinnedGuardsLog.length > 0 && (
          <motion.div 
            initial={{ opacity: 0, x: -20 }} 
            animate={{ opacity: 1, x: 0 }} 
            className="absolute top-4 left-4 bg-slate-900 border border-green-800 p-3 rounded shadow-xl z-50 max-w-sm"
          >
             <h3 className="text-xs font-bold text-green-400 mb-2 flex items-center gap-2">
               <ShieldCheck size={14} /> Pinned Guards Array
             </h3>
             <div className="font-mono text-[10px] text-slate-300 space-y-1">
               {pinnedGuardsLog.map((log, i) => <div key={i}>{log}</div>)}
             </div>
          </motion.div>
        )}
      </AnimatePresence>

      <AnimatePresence>
        {relayArray.map((relay) => {
          const idx = parseInt(relay.index);
          const nodeX = 250 + (idx % 4) * 120;
          const nodeY = 250 + Math.floor(idx / 4) * 120;
          
          const isRejected = relay.trustScore < 50;

          return (
            <motion.div
              key={relay.ip}
              initial={{ opacity: 0, scale: 0 }}
              animate={{ opacity: 1, scale: 1 }}
              exit={{ opacity: 0, scale: 0 }}
              className="absolute flex flex-col items-center"
              style={{ left: nodeX - 25, top: nodeY - 25 }}
            >
              <div className={`relative p-2 rounded-full border-2 ${relay.isMalicious ? 'border-red-500 bg-red-900/20' : 'border-green-500 bg-green-900/20'} ${isRejected ? 'opacity-30 grayscale' : ''}`}>
                 <Database size={24} className={relay.isMalicious ? 'text-red-400' : 'text-green-400'} />
                 {isRejected && <ShieldAlert size={32} className="absolute -top-2 -right-2 text-red-500" />}
              </div>

              <div className="mt-2 bg-slate-900 border border-slate-700 p-1.5 rounded text-[9px] text-center whitespace-nowrap z-10 shadow-lg">
                 <div className="font-bold text-slate-300 border-b border-slate-700 pb-0.5 mb-0.5">IP: {relay.ip}</div>
                 <div className={relay.trustScore < 50 ? 'text-red-400 font-bold' : 'text-blue-400'}>
                    Trust: {relay.trustScore}
                 </div>
                 <div className={relay.sameSource > 1 ? 'text-yellow-400' : 'text-slate-400'}>
                    SameSrc: {relay.sameSource}
                 </div>
              </div>
            </motion.div>
          );
        })}
      </AnimatePresence>

      {packets.map((packet) => (
        <AnimatedPacket
          key={packet.id}
          id={packet.id}
          path={packet.path}
          type={packet.type}
          duration={0.8}
          shouldDestroy={circuitBroken}
        />
      ))}

      {isAttacking && relayArray.length === 0 && (
         <div className="absolute top-[60%] left-1/2 transform -translate-x-1/2 text-slate-500 text-sm animate-pulse border border-slate-700 p-4 rounded bg-slate-900">
           Waiting for C Engine relays list...
         </div>
      )}
    </div>
  );
}