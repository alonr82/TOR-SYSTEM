import { motion } from "motion/react";
import { Mail, AlertTriangle, X } from "lucide-react";

interface AnimatedPacketProps {
  id: string;
  path: { x: number; y: number }[];
  type: "normal" | "syn" | "encrypted" | "corrupted";
  duration?: number;
  onComplete?: () => void;
  encryptionLayers?: number;
  shouldBounce?: boolean;
  shouldDestroy?: boolean;
}

export function AnimatedPacket({
  path,
  type,
  duration = 1,
  onComplete,
  encryptionLayers = 0,
  shouldBounce,
  shouldDestroy,
}: AnimatedPacketProps) {
  // Validate path to prevent non-finite values
  if (!path || path.length === 0) return null;

  const validPath = path.filter(p =>
    typeof p.x === 'number' &&
    typeof p.y === 'number' &&
    isFinite(p.x) &&
    isFinite(p.y)
  );

  if (validPath.length === 0) return null;

  const getColor = () => {
    if (type === "syn") return "bg-red-500";
    if (type === "corrupted") return "bg-orange-600";
    return "bg-green-500";
  };

  const getIcon = () => {
    if (shouldDestroy) return X;
    if (type === "syn") return AlertTriangle;
    return Mail;
  };

  const Icon = getIcon();

  const finalPath = shouldBounce
    ? [...validPath, { x: validPath[validPath.length - 1].x - 50, y: validPath[validPath.length - 1].y + 30 }]
    : validPath;

  const times = finalPath.length > 1
    ? finalPath.map((_, i) => i / (finalPath.length - 1))
    : [0];

  const animate = shouldDestroy
    ? {
        scale: [1, 1.5, 0],
        opacity: [1, 0.5, 0],
      }
    : {
        x: finalPath.map((p) => p.x - 20),
        y: finalPath.map((p) => p.y - 20),
        scale: 1,
        opacity: 1,
      };

  const transition = shouldDestroy
    ? {
        duration: 0.8,
        ease: "easeOut",
      }
    : {
        duration: shouldBounce ? duration * 1.2 : duration,
        ease: shouldBounce ? "easeOut" : "linear",
        times: times,
      };

  return (
    <motion.div
      className={`absolute w-10 h-10 rounded-full ${getColor()} flex items-center justify-center border-2 border-white shadow-xl`}
      style={{ zIndex: 50 }}
      initial={{ x: finalPath[0].x - 20, y: finalPath[0].y - 20, scale: 1, opacity: 1 }}
      animate={animate}
      transition={transition}
      onAnimationComplete={onComplete}
    >
      <Icon className="w-5 h-5 text-white" />

      {/* Encryption Layers */}
      {encryptionLayers > 0 && type !== "corrupted" && (
        <>
          {[...Array(encryptionLayers)].map((_, i) => (
            <motion.div
              key={i}
              className="absolute rounded-full border-3"
              style={{
                width: 40 + (i + 1) * 12,
                height: 40 + (i + 1) * 12,
                borderWidth: 3,
                borderColor:
                  i === 0 ? "#3b82f6" : i === 1 ? "#8b5cf6" : "#ec4899",
              }}
              initial={{ scale: 0, opacity: 0 }}
              animate={{ scale: 1, opacity: 0.7 }}
              transition={{ delay: 0.1 }}
            />
          ))}
        </>
      )}

      {/* Corrupted Visual Effect */}
      {type === "corrupted" && (
        <motion.div
          className="absolute inset-0 rounded-full"
          animate={{
            boxShadow: [
              "0 0 10px #ea580c",
              "0 0 30px #ea580c",
              "0 0 10px #ea580c",
            ],
          }}
          transition={{ duration: 0.5, repeat: Infinity }}
        />
      )}
    </motion.div>
  );
}
