import { motion } from "motion/react";
import { Server, User, Shield, ShieldAlert, Lock } from "lucide-react";

interface NodeProps {
  x: number;
  y: number;
  label: string;
  type: "client" | "guard" | "middle" | "exit" | "attacker" | "directory" | "malicious" | "legitimate";
  hasShield?: boolean;
  hasLock?: boolean;
  isError?: boolean;
  isDimmed?: boolean;
  backlogProgress?: number;
  isPulsing?: boolean;
}

export function Node({
  x,
  y,
  label,
  type,
  hasShield,
  hasLock,
  isError,
  isDimmed,
  backlogProgress,
  isPulsing,
}: NodeProps) {
  const getColor = () => {
    if (type === "malicious") return isDimmed ? "bg-red-900/30" : "bg-red-600";
    if (type === "legitimate" || type === "guard" || type === "exit") return "bg-green-500";
    if (type === "attacker") return "bg-red-600";
    if (type === "directory") return "bg-purple-600";
    if (type === "middle") return "bg-red-600";
    return "bg-blue-500";
  };

  const getIcon = () => {
    if (type === "client") return User;
    if (type === "attacker") return ShieldAlert;
    return Server;
  };

  const Icon = getIcon();

  return (
    <motion.div
      className="absolute"
      style={{
        left: x - 40,
        top: y - 40,
        zIndex: isDimmed ? 1 : 10,
      }}
      initial={{ scale: 0, opacity: 0 }}
      animate={{
        scale: 1,
        opacity: isDimmed ? 0.3 : 1,
      }}
      transition={{ duration: 0.3 }}
    >
      <div className="relative flex flex-col items-center">
        {/* Shield Effect */}
        {hasShield && (
          <motion.div
            className="absolute inset-0 w-20 h-20"
            initial={{ scale: 1 }}
            animate={{
              scale: [1, 1.3, 1],
              opacity: [0.8, 0.4, 0.8],
            }}
            transition={{
              duration: 2,
              repeat: Infinity,
            }}
          >
            <div className="w-full h-full rounded-full border-4 border-blue-400 shadow-lg shadow-blue-400/50" />
          </motion.div>
        )}

        {/* Main Node */}
        <motion.div
          className={`w-20 h-20 rounded-full ${getColor()} flex items-center justify-center border-4 ${
            isError ? "border-red-600" : isPulsing ? "border-yellow-500" : "border-slate-700"
          } shadow-lg relative`}
          animate={
            isError
              ? {
                  scale: [1, 1.1, 1],
                  borderColor: ["#dc2626", "#ef4444", "#dc2626"],
                }
              : isPulsing
              ? {
                  scale: [1, 1.15, 1],
                  borderColor: ["#eab308", "#fbbf24", "#eab308"],
                  boxShadow: [
                    "0 0 20px rgba(234, 179, 8, 0.5)",
                    "0 0 40px rgba(234, 179, 8, 0.8)",
                    "0 0 20px rgba(234, 179, 8, 0.5)",
                  ],
                }
              : {}
          }
          transition={{ duration: isPulsing ? 1.5 : 0.5, repeat: (isError || isPulsing) ? Infinity : 0 }}
        >
          <Icon className="w-8 h-8 text-white" />

          {/* Lock Icon */}
          {hasLock && (
            <motion.div
              className="absolute -top-3 -right-3 bg-yellow-500 rounded-full p-2 border-2 border-slate-900 shadow-lg shadow-yellow-500/50"
              initial={{ scale: 0 }}
              animate={{ scale: 1 }}
              transition={{ type: "spring", stiffness: 200 }}
            >
              <Lock className="w-5 h-5 text-slate-900" />
            </motion.div>
          )}
        </motion.div>

        {/* Label */}
        <div className={`mt-2 text-xs font-bold ${isDimmed ? "text-slate-600" : "text-slate-100"}`}>
          {label}
        </div>

        {/* Backlog Progress Bar */}
        {backlogProgress !== undefined && (
          <motion.div
            className="absolute -bottom-8 bg-slate-950 border-2 border-slate-700 rounded px-3 py-1"
            initial={{ opacity: 0, y: -10 }}
            animate={{ opacity: 1, y: 0 }}
          >
            <div className="text-[10px] font-bold text-slate-300 mb-1">
              TCP Backlog
            </div>
            <div className="w-32 h-3 bg-slate-800 rounded-full overflow-hidden border border-slate-600">
              <motion.div
                className={`h-full ${
                  backlogProgress >= 100 ? "bg-red-600" : "bg-yellow-500"
                }`}
                initial={{ width: "0%" }}
                animate={{
                  width: `${backlogProgress}%`,
                  backgroundColor:
                    backlogProgress >= 100
                      ? ["#dc2626", "#ef4444", "#dc2626"]
                      : "#eab308",
                }}
                transition={{
                  width: { duration: 0.3 },
                  backgroundColor:
                    backlogProgress >= 100
                      ? { duration: 0.5, repeat: Infinity }
                      : { duration: 0 },
                }}
              />
            </div>
            <div className="text-[10px] font-bold text-slate-400 text-center mt-1">
              {Math.round((backlogProgress / 100) * 128)}/128
            </div>
          </motion.div>
        )}
      </div>
    </motion.div>
  );
}
