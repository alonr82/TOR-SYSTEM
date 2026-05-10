import { useState, useEffect, useRef } from "react";
import { motion, AnimatePresence } from "framer-motion";
import { Node } from "../Node";
import { ConnectionLine } from "../ConnectionLine";
import { AnimatedPacket } from "../AnimatedPacket";
import { PacketState, LogEntry } from "../../types";
import { ShieldCheck, ShieldAlert, Lock, Skull, ScanLine, XOctagon } from "lucide-react";

export interface Scenario2CanvasProps {
  isAttacking: boolean;
  isDefending: boolean;
  addLog: (type: LogEntry["type"], message: string) => void;
  backendEvent?: any; 
}

const clientPos = { x: 120, y: 250 };
const guardPos = { x: 300, y: 250 };
const middlePos = { x: 480, y: 250 };
const exitPos = { x: 660, y: 250 };
const targetAPos = { x: 480, y: 450 };

const sleep = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));

export function Scenario2Canvas({
  isAttacking,
  isDefending,
  addLog,
  backendEvent
}: Scenario2CanvasProps) {
  const [packets, setPackets] = useState<PacketState[]>([]);
  const [circuitBroken, setCircuitBroken] = useState(false);
  
  const [layer1Active, setLayer1Active] = useState(false);
  const [layer1Broken, setLayer1Broken] = useState(false); 
  const [layer2Active, setLayer2Active] = useState(false);
  const [layer3Hijacked, setLayer3Hijacked] = useState(false);
  const [showHijackPath, setShowHijackPath] = useState(false); 
  
  const [showVerification, setShowVerification] = useState(false);
  const [verificationStatus, setVerificationStatus] = useState<"checking" | "failed" | "success" | null>(null);
  const [frozenPacket, setFrozenPacket] = useState<boolean>(false);
  
  const isActiveRef = useRef(false);
  // התיקון: תעודת זהות ייחודית לכל ריצת אנימציה כדי למנוע דריסות כפולות
  const runIdRef = useRef(0);
  
  const backendStartRef = useRef(false);
  const backendMacResult = useRef<string>("none");
  const backendTeardown = useRef(false);

  const addLogRef = useRef(addLog);
  useEffect(() => {
    addLogRef.current = addLog;
  }, [addLog]);

  useEffect(() => {
    if (!backendEvent) return;
    
    const ev = backendEvent.event;
    if (ev === 'REAL_CIRCUIT_START') {
      backendStartRef.current = true;
    }
    if (ev === 'REAL_MAC_FAILED_DEFENSE') backendMacResult.current = "defense_caught";
    if (ev === 'REAL_MAC_FAILED_ATTACK') backendMacResult.current = "attack_caught";
    if (ev === 'CIRCUIT_BUILT') backendMacResult.current = "valid";
    if (ev === 'REAL_CIRCUIT_TEARDOWN') backendTeardown.current = true;
  }, [backendEvent]);

  useEffect(() => {
    if (!isAttacking) {
      runIdRef.current += 1; // מחסל כל אנימציה שרצה ברקע
      setPackets([]);
      setCircuitBroken(false);
      setLayer1Active(false);
      setLayer1Broken(false);
      setLayer2Active(false);
      setLayer3Hijacked(false);
      setShowHijackPath(false);
      setShowVerification(false);
      setVerificationStatus(null);
      setFrozenPacket(false);
      
      backendStartRef.current = false;
      backendMacResult.current = "none";
      backendTeardown.current = false;
      isActiveRef.current = false;
      return;
    }

    isActiveRef.current = true;
    runIdRef.current += 1;
    const currentRunId = runIdRef.current;

    // פונקציית עזר לבדיקה האם צריך לעצור את הריצה הנוכחית
    const isAborted = () => !isActiveRef.current || runIdRef.current !== currentRunId;

    const runSimulation = async () => {
      while (isActiveRef.current && runIdRef.current === currentRunId) {
        setLayer1Active(false);
        setLayer1Broken(false);
        setLayer2Active(false);
        setLayer3Hijacked(false);
        setShowHijackPath(false);
        setCircuitBroken(false);
        setShowVerification(false);
        setVerificationStatus(null);
        setFrozenPacket(false);
        setPackets([]); // חובה לאפס חבילות בתחילת כל סייקל!
        
        backendStartRef.current = false;
        backendMacResult.current = "none";
        backendTeardown.current = false;

        addLogRef.current("info", "Visualizer: Waiting for C Engine to trigger circuit build...");
        
        while (!backendStartRef.current && !isAborted()) {
            await sleep(200);
        }

        if (isAborted()) return; // יציאה מיידית אם התבקשנו להפסיק
        backendStartRef.current = false;

        addLogRef.current("success", "Visualizer: Circuit initialization detected! Animating...");

        setPackets([{ id: `p-${Date.now()}`, path: [clientPos, guardPos], type: "normal" }]);
        await sleep(800);
        if (isAborted()) return;

        setPackets([{ id: `p-${Date.now()}`, path: [guardPos, clientPos], type: "normal" }]);
        await sleep(800);
        if (isAborted()) return;

        setLayer1Active(true);
        await sleep(800);
        if (isAborted()) return;

        setPackets([{ id: `p-${Date.now()}`, path: [clientPos, guardPos], type: "encrypted", encryptionLayers: 1 }]);
        await sleep(800);
        if (isAborted()) return;
        
        setPackets([{ id: `p-${Date.now()}`, path: [guardPos, middlePos], type: "normal" }]);
        await sleep(800);
        if (isAborted()) return;

        setPackets([{ id: `p-${Date.now()}`, path: [middlePos, guardPos], type: "normal" }]);
        await sleep(800);
        if (isAborted()) return;
        
        setPackets([{ id: `p-${Date.now()}`, path: [guardPos, clientPos], type: "encrypted", encryptionLayers: 1 }]);
        await sleep(800);
        if (isAborted()) return;

        setLayer2Active(true);
        await sleep(800);
        if (isAborted()) return;

        setPackets([{ id: `p-${Date.now()}`, path: [clientPos, guardPos], type: "encrypted", encryptionLayers: 2 }]);
        await sleep(800);
        if (isAborted()) return;
        
        setPackets([{ id: `p-${Date.now()}`, path: [guardPos, middlePos], type: "encrypted", encryptionLayers: 1 }]);
        await sleep(800);
        if (isAborted()) return;

        setShowHijackPath(true); 
        setPackets([{ id: `p-${Date.now()}`, path: [middlePos, targetAPos], type: "corrupted" }]);
        await sleep(800);
        if (isAborted()) return;

        setPackets([{ id: `p-${Date.now()}`, path: [targetAPos, middlePos], type: "corrupted" }]);
        await sleep(800);
        if (isAborted()) return;
        
        setPackets([{ id: `p-${Date.now()}`, path: [middlePos, guardPos], type: "encrypted", encryptionLayers: 1 }]);
        await sleep(800);
        if (isAborted()) return;
        
        setPackets([{ id: `p-${Date.now()}`, path: [guardPos, clientPos], type: "encrypted", encryptionLayers: 2 }]);
        await sleep(800);
        if (isAborted()) return;

        if (!isDefending) {
          setLayer3Hijacked(true);
          await sleep(1000);
          if (isAborted()) return;

          setPackets([{ id: `d1-${Date.now()}`, path: [clientPos, guardPos], type: "encrypted", encryptionLayers: 3 }]);
          await sleep(800);
          if (isAborted()) return;
          
          setPackets([{ id: `d2-${Date.now()}`, path: [guardPos, middlePos], type: "encrypted", encryptionLayers: 2 }]);
          await sleep(800);
          if (isAborted()) return;
          
          setPackets([{ id: `d3-${Date.now()}`, path: [middlePos, targetAPos], type: "corrupted" }]);
          
          await sleep(10000);

        } else {
          setFrozenPacket(true);
          setShowVerification(true);
          setVerificationStatus("checking");
          addLogRef.current("info", "Visualizer: Awaiting C Engine MAC Validation...");
          
          await sleep(2000);
          if (isAborted()) return;

          let waitTime = 0;
          while (backendMacResult.current === "none" && waitTime < 15000 && !isAborted()) {
              await sleep(200);
              waitTime += 200;
          }

          if (isAborted()) return;

          if (backendMacResult.current === "defense_caught") {
             setVerificationStatus("failed");
             addLogRef.current("error", "Visualizer: MAC Signature Mismatch! Circuit Hijacking blocked.");
             await sleep(2000);
             if (isAborted()) return;

             setShowVerification(false);
             setFrozenPacket(false);
             
             setCircuitBroken(true);
             await sleep(1000);
             if (isAborted()) return;
             
             setLayer2Active(false);
             setShowHijackPath(false);
             
             await sleep(1000);
             if (isAborted()) return;
             
             setLayer1Broken(true);
             await sleep(1000);
             setLayer1Active(false);
             
             await sleep(6000); 
          } else if (backendMacResult.current === "valid") {
             setVerificationStatus("success");
             await sleep(1500);
             if (isAborted()) return;
             
             setShowVerification(false);
             setFrozenPacket(false);
             addLogRef.current("success", "Visualizer: MAC Signature Valid. No malicious tampering detected.");
             await sleep(4000);
          } else {
             setShowVerification(false);
             setFrozenPacket(false);
             await sleep(3000);
          }
        }
      }
    };

    runSimulation();

    return () => {
      isActiveRef.current = false;
      runIdRef.current += 1; // מבטיח יציאה מיידית בניקוי
    };
  }, [isAttacking, isDefending]);

  return (
    <div className="w-full h-full relative">
      <ConnectionLine x1={clientPos.x} y1={clientPos.y} x2={guardPos.x} y2={guardPos.y} isActive={layer1Active || layer1Broken} isBroken={layer1Broken} />
      <ConnectionLine x1={guardPos.x} y1={guardPos.y} x2={middlePos.x} y2={middlePos.y} isActive={layer2Active || circuitBroken} isBroken={circuitBroken} />
      <ConnectionLine x1={middlePos.x} y1={middlePos.y} x2={exitPos.x} y2={exitPos.y} isBroken={true} style={{ opacity: 0.2 }} />

      <AnimatePresence>
        {showHijackPath && (
          <motion.div initial={{ opacity: 0 }} animate={{ opacity: 1 }} exit={{ opacity: 0 }}>
            <ConnectionLine x1={middlePos.x} y1={middlePos.y} x2={targetAPos.x} y2={targetAPos.y} isActive={layer3Hijacked || (showVerification && !circuitBroken)} style={{ stroke: "#ef4444", strokeDasharray: "6,6" }} />
          </motion.div>
        )}
      </AnimatePresence>

      {(layer1Active && !layer1Broken) && <motion.div initial={{ scale: 0 }} animate={{ scale: 1 }} className="absolute" style={{ left: (clientPos.x + guardPos.x)/2 - 12, top: clientPos.y - 25 }}><Lock size={16} className="text-green-400" /></motion.div>}
      {(layer2Active && !circuitBroken) && <motion.div initial={{ scale: 0 }} animate={{ scale: 1 }} className="absolute" style={{ left: (guardPos.x + middlePos.x)/2 - 12, top: guardPos.y - 25 }}><Lock size={16} className="text-green-400" /></motion.div>}
      {layer3Hijacked && <motion.div initial={{ scale: 0 }} animate={{ scale: 1 }} className="absolute" style={{ left: (middlePos.x + targetAPos.x)/2 - 12, top: (middlePos.y + targetAPos.y)/2 - 12 }}><Skull size={20} className="text-red-500" /></motion.div>}

      <Node {...clientPos} label="tor_client_1" type="client" isError={circuitBroken || layer1Broken} />
      <Node {...guardPos} label="tor_relay_legit_1" type="guard" isError={layer1Broken} />
      <Node {...middlePos} label="tor_relay_malicious_1" type="middle" isPulsing={layer3Hijacked || (showVerification && !circuitBroken)} />
      <Node {...exitPos} label="tor_relay_legit_3" type="exit" isDimmed={true} />
      <Node {...targetAPos} label="Target Server A" type="malicious" />

      {packets.map((packet) => (
        <AnimatedPacket
          key={packet.id}
          id={packet.id}
          path={packet.path}
          type={packet.type}
          encryptionLayers={packet.encryptionLayers}
          duration={0.8}
          shouldDestroy={circuitBroken || layer1Broken}
        />
      ))}

      <AnimatePresence>
        {frozenPacket && (
          <motion.div
            className="absolute z-40 flex flex-col items-center justify-center"
            style={{ left: clientPos.x + 50, top: clientPos.y - 15 }}
            initial={{ opacity: 0, scale: 0.5 }}
            animate={
              verificationStatus === "failed" 
                ? { x: [-4, 4, -4, 4, 0], scale: 1.1, opacity: 1 } 
                : { opacity: 1, scale: 1 }
            }
            exit={{ opacity: 0, scale: 0, rotate: 45 }}
            transition={{ duration: 0.3 }}
          >
            <div className={`relative w-10 h-10 border-2 rounded flex items-center justify-center bg-slate-900 overflow-hidden
                ${verificationStatus === "failed" ? "border-red-500 shadow-[0_0_15px_rgba(239,68,68,0.6)]" : 
                  verificationStatus === "success" ? "border-green-500 shadow-[0_0_15px_rgba(34,197,94,0.6)]" :
                  "border-blue-500"}`}>
              {verificationStatus === "checking" && (
                <motion.div 
                  className="absolute w-full h-1 bg-blue-400/80 shadow-[0_0_8px_rgba(96,165,250,1)]"
                  animate={{ top: ["0%", "90%", "0%"] }}
                  transition={{ duration: 1.2, repeat: Infinity, ease: "linear" }}
                />
              )}
              {verificationStatus === "checking" ? (
                <ScanLine size={20} className="text-blue-400 opacity-50" />
              ) : verificationStatus === "failed" ? (
                <XOctagon size={24} className="text-red-500 drop-shadow-[0_0_8px_rgba(239,68,68,0.8)]" />
              ) : (
                <ShieldCheck size={24} className="text-green-500 drop-shadow-[0_0_8px_rgba(34,197,94,0.8)]" />
              )}
            </div>
          </motion.div>
        )}
      </AnimatePresence>

      <AnimatePresence>
        {showVerification && (
          <motion.div
            className="absolute bg-slate-950 border-2 rounded-lg p-4 shadow-[0_0_30px_rgba(0,0,0,0.8)] w-[340px] z-50"
            style={{ borderColor: verificationStatus === "failed" ? "#ef4444" : verificationStatus === "success" ? "#22c55e" : "#3b82f6", left: 20, top: 20 }}
            initial={{ opacity: 0, x: -30, scale: 0.9 }}
            animate={{ opacity: 1, x: 0, scale: 1 }}
            exit={{ opacity: 0, scale: 0.9 }}
            transition={{ type: "spring", damping: 25 }}
          >
            <div className="flex items-center gap-3 mb-3">
              {verificationStatus === "checking" ? (
                <>
                  <motion.div animate={{ rotate: 360 }} transition={{ duration: 1.5, repeat: Infinity, ease: "linear" }}>
                    <ShieldCheck className="w-7 h-7 text-blue-400" />
                  </motion.div>
                  <div>
                    <h4 className="font-bold text-blue-400 text-sm">Attempting to Unlock</h4>
                    <p className="text-xs text-slate-400">Verifying MAC signature...</p>
                  </div>
                </>
              ) : verificationStatus === "failed" ? (
                <>
                  <ShieldAlert className="w-7 h-7 text-red-500" />
                  <div>
                    <h4 className="font-bold text-red-400 text-sm">Unlock Failed</h4>
                    <p className="text-xs text-slate-400">Invalid cryptographic seal.</p>
                  </div>
                </>
              ) : (
                <>
                  <ShieldCheck className="w-7 h-7 text-green-500" />
                  <div>
                    <h4 className="font-bold text-green-400 text-sm">Unlock Successful</h4>
                    <p className="text-xs text-slate-400">Cryptographic seal is valid.</p>
                  </div>
                </>
              )}
            </div>
            
            {verificationStatus === "failed" && (
              <motion.div className="space-y-2" initial={{ opacity: 0 }} animate={{ opacity: 1 }} transition={{ delay: 0.2 }}>
                <div className="bg-slate-900 rounded p-2 border border-slate-700">
                  <div className="text-slate-500 text-[10px] uppercase tracking-wider mb-1">Expected Signature (Exit Key):</div>
                  <div className="font-mono text-green-400 text-xs">0x7A8F3E2C91B4...</div>
                </div>
                <div className="bg-slate-900 rounded p-2 border border-red-900/50">
                  <div className="text-slate-500 text-[10px] uppercase tracking-wider mb-1">Received Signature (Forged):</div>
                  <div className="font-mono text-red-400 text-xs line-through opacity-80">0x4B2D1C9F88A1...</div>
                </div>
              </motion.div>
            )}
          </motion.div>
        )}
      </AnimatePresence>
    </div>
  );
}