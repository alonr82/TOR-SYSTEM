import { motion } from "motion/react";
import { Mail, AlertTriangle } from "lucide-react";

interface PacketProps {
  id: string;
  path: { x: number; y: number }[];
  type: "normal" | "syn" | "encrypted" | "tampered";
  onComplete?: () => void;
  encryptionLayers?: number;
  duration?: number;
}

export function Packet({ path, type, onComplete, encryptionLayers = 0, duration = 2 }: PacketProps) {
  const getPacketColor = () => {
    if (type === "syn") return "bg-red-500";
    if (type === "tampered") return "bg-orange-500";
    return "bg-green-500";
  };

  const getIcon = () => {
    if (type === "syn" || type === "tampered") return AlertTriangle;
    return Mail;
  };

  const Icon = getIcon();

  return (
    <motion.div
      className={`absolute w-8 h-8 rounded-full ${getPacketColor()} flex items-center justify-center border-2 border-white shadow-lg`}
      style={{ zIndex: 10 }}
      initial={{ x: path[0].x - 16, y: path[0].y - 16 }}
      animate={{
        x: path.map((p) => p.x - 16),
        y: path.map((p) => p.y - 16),
      }}
      transition={{
        duration,
        ease: "linear",
        times: path.map((_, i) => i / (path.length - 1)),
      }}
      onAnimationComplete={onComplete}
    >
      <Icon className="w-4 h-4 text-white" />

      {encryptionLayers > 0 && (
        <>
          {[...Array(encryptionLayers)].map((_, i) => (
            <motion.div
              key={i}
              className="absolute rounded-full border-2"
              style={{
                width: 32 + (i + 1) * 8,
                height: 32 + (i + 1) * 8,
                borderColor: i === 0 ? "#3b82f6" : i === 1 ? "#8b5cf6" : "#ec4899",
              }}
              initial={{ scale: 0, opacity: 0 }}
              animate={{ scale: 1, opacity: 0.6 }}
              transition={{ delay: i * 0.1 }}
            />
          ))}
        </>
      )}
    </motion.div>
  );
}
