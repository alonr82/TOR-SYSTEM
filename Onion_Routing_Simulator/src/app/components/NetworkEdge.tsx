import { motion } from "motion/react";

interface NetworkEdgeProps {
  x1: number;
  y1: number;
  x2: number;
  y2: number;
  isActive?: boolean;
  isBroken?: boolean;
}

export function NetworkEdge({ x1, y1, x2, y2, isActive, isBroken }: NetworkEdgeProps) {
  return (
    <svg className="absolute inset-0 pointer-events-none" style={{ zIndex: 0 }}>
      <motion.line
        x1={x1}
        y1={y1}
        x2={x2}
        y2={y2}
        stroke={isBroken ? "#ef4444" : isActive ? "#3b82f6" : "#4b5563"}
        strokeWidth={isBroken ? 3 : 2}
        strokeDasharray={isBroken ? "5,5" : "0"}
        initial={{ pathLength: 0, opacity: 0 }}
        animate={{
          pathLength: 1,
          opacity: isBroken ? 0.5 : 1,
        }}
        transition={{ duration: 0.5 }}
      />
    </svg>
  );
}
