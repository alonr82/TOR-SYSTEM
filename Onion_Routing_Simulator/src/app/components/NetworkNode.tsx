import { motion } from "motion/react";
import { Server, Shield, ShieldAlert, Lock } from "lucide-react";

interface NetworkNodeProps {
  id: string;
  x: number;
  y: number;
  label: string;
  type: "client" | "directory" | "guard" | "middle" | "exit" | "attacker" | "malicious";
  isActive?: boolean;
  isPinned?: boolean;
  backlogQueue?: number;
  maxBacklog?: number;
  showError?: boolean;
}

export function NetworkNode({
  x,
  y,
  label,
  type,
  isActive,
  isPinned,
  backlogQueue,
  maxBacklog,
  showError,
}: NetworkNodeProps) {
  const getNodeColor = () => {
    if (type === "malicious" || type === "attacker") return "bg-red-500";
    if (type === "guard" || type === "middle" || type === "exit") return "bg-green-500";
    if (type === "directory") return "bg-purple-500";
    return "bg-blue-500";
  };

  const getIcon = () => {
    if (type === "attacker") return ShieldAlert;
    if (isPinned) return Lock;
    if (type === "malicious") return ShieldAlert;
    return Server;
  };

  const Icon = getIcon();

  return (
    <motion.div
      className="absolute"
      style={{ left: x - 40, top: y - 40 }}
      initial={{ scale: 0 }}
      animate={{ scale: 1 }}
      transition={{ duration: 0.3 }}
    >
      <div className="relative flex flex-col items-center">
        <motion.div
          className={`w-20 h-20 rounded-full ${getNodeColor()} flex items-center justify-center border-4 border-gray-800 relative`}
          animate={{
            boxShadow: isActive
              ? [
                  "0 0 20px rgba(59, 130, 246, 0.5)",
                  "0 0 40px rgba(59, 130, 246, 0.8)",
                  "0 0 20px rgba(59, 130, 246, 0.5)",
                ]
              : "0 0 10px rgba(0, 0, 0, 0.5)",
          }}
          transition={{ duration: 1.5, repeat: Infinity }}
        >
          <Icon className="w-8 h-8 text-white" />

          {isPinned && (
            <motion.div
              className="absolute -top-2 -right-2 bg-yellow-500 rounded-full p-1"
              initial={{ scale: 0 }}
              animate={{ scale: 1 }}
              transition={{ type: "spring" }}
            >
              <Lock className="w-4 h-4 text-gray-900" />
            </motion.div>
          )}

          {showError && (
            <motion.div
              className="absolute -top-2 -right-2 bg-red-600 rounded-full p-1"
              initial={{ scale: 0 }}
              animate={{ scale: [1, 1.2, 1] }}
              transition={{ duration: 0.5, repeat: Infinity }}
            >
              <ShieldAlert className="w-4 h-4 text-white" />
            </motion.div>
          )}
        </motion.div>

        <div className="mt-2 text-white text-xs font-mono text-center">
          {label}
        </div>

        {backlogQueue !== undefined && maxBacklog !== undefined && (
          <motion.div
            className="mt-1 bg-gray-800 border border-gray-600 rounded px-2 py-1 text-xs font-mono text-white"
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
          >
            Queue: {backlogQueue}/{maxBacklog}
          </motion.div>
        )}
      </div>
    </motion.div>
  );
}
