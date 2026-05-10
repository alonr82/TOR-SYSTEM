import { Shield, Play, RotateCcw } from "lucide-react";
import { Scenario } from "../App";

interface ControlPanelProps {
  currentScenario: Scenario;
  isAttacking: boolean;
  isDefending: boolean;
  onInitiateAttack: () => void;
  onToggleDefense: () => void;
  onReset: () => void;
}

export function ControlPanel({
  currentScenario,
  isAttacking,
  isDefending,
  onInitiateAttack,
  onToggleDefense,
  onReset,
}: ControlPanelProps) {
  const scenarioData = {
    1: {
      title: "Balanced SYN Flood Attack",
      description:
        "Client sends connection requests in balanced round-robin rotation to all 10 relays, systematically filling their TCP backlogs (max 128). SYN Cookies defense allows relays to validate connections cryptographically without storing state, bypassing the backlog entirely.",
      defense: "SYN Cookies",
    },
    2: {
      title: "Circuit Hijacking During Construction",
      description:
        "A malicious middle relay intercepts the Diffie-Hellman key exchange during circuit construction and hijacks the shared secret to attacker-controlled servers. ChaCha20-Poly1305 authentication tags detect tampering in the handshake and immediately abort circuit construction.",
      defense: "Authenticated Key Exchange",
    },
    3: {
      title: "Sybil Attack with Collusion",
      description:
        "An attacker infiltrates the network with 15 malicious relays. Two of them collude to de-anonymize traffic by correlating entry and exit points. Guard pinning locks the client to a trusted guard, preventing malicious relays from being selected.",
      defense: "Guard Pinning",
    },
  };

  const data = scenarioData[currentScenario];

  return (
    <div className="h-full flex flex-col p-3 sm:p-6 space-y-3 sm:space-y-6">
      {/* Scenario Description */}
      <div className="bg-slate-800 rounded-lg p-3 sm:p-4 border border-slate-700">
        <h3 className="text-sm sm:text-lg font-bold text-blue-400 mb-2">{data.title}</h3>
        <p className="text-xs sm:text-sm text-slate-300 leading-relaxed">{data.description}</p>
      </div>

      {/* Defense Info */}
      <div className="bg-slate-800 rounded-lg p-3 sm:p-4 border border-slate-700">
        <div className="flex items-center gap-2 mb-2">
          <Shield className="w-4 h-4 sm:w-5 sm:h-5 text-green-400" />
          <h4 className="text-sm sm:text-base font-bold text-green-400">Defense Mechanism</h4>
        </div>
        <p className="text-xs sm:text-sm text-slate-300">{data.defense}</p>
      </div>

      {/* Controls */}
      <div className="space-y-2 sm:space-y-3 flex-1">
        <button
          onClick={onInitiateAttack}
          disabled={isAttacking}
          className={`w-full py-2.5 sm:py-3 rounded-lg font-bold flex items-center justify-center gap-2 transition-all text-sm sm:text-base ${
            isAttacking
              ? "bg-red-900 text-red-300 cursor-not-allowed"
              : "bg-red-600 hover:bg-red-700 text-white shadow-lg shadow-red-600/50"
          }`}
        >
          <Play className="w-4 h-4 sm:w-5 sm:h-5" />
          {isAttacking ? "Attack in Progress" : "Initiate Attack"}
        </button>

        <button
          onClick={onToggleDefense}
          className={`w-full py-2.5 sm:py-3 rounded-lg font-bold flex items-center justify-center gap-2 transition-all text-sm sm:text-base ${
            isDefending
              ? "bg-green-600 hover:bg-green-700 text-white shadow-lg shadow-green-600/50"
              : "bg-blue-600 hover:bg-blue-700 text-white shadow-lg shadow-blue-600/50"
          }`}
        >
          <Shield className="w-4 h-4 sm:w-5 sm:h-5" />
          {isDefending ? "Defense Active" : "Toggle Defense"}
        </button>

        <button
          onClick={onReset}
          className="w-full py-2.5 sm:py-3 rounded-lg font-bold flex items-center justify-center gap-2 bg-slate-700 hover:bg-slate-600 text-white transition-all text-sm sm:text-base"
        >
          <RotateCcw className="w-4 h-4 sm:w-5 sm:h-5" />
          Reset
        </button>
      </div>

      {/* Status Indicators */}
      <div className="bg-slate-950 rounded-lg p-3 sm:p-4 border border-slate-700 space-y-2">
        <div className="flex items-center justify-between">
          <span className="text-xs sm:text-sm text-slate-400">Attack Status:</span>
          <span
            className={`text-xs sm:text-sm font-bold ${
              isAttacking ? "text-red-400" : "text-slate-500"
            }`}
          >
            {isAttacking ? "ACTIVE" : "IDLE"}
          </span>
        </div>
        <div className="flex items-center justify-between">
          <span className="text-xs sm:text-sm text-slate-400">Defense Status:</span>
          <span
            className={`text-xs sm:text-sm font-bold ${
              isDefending ? "text-green-400" : "text-slate-500"
            }`}
          >
            {isDefending ? "ENABLED" : "DISABLED"}
          </span>
        </div>
      </div>
    </div>
  );
}