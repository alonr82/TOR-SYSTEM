import { motion } from "motion/react";
import { X } from "lucide-react";

interface ConnectionLineProps {
  x1: number;
  y1: number;
  x2: number;
  y2: number;
  isActive?: boolean;
  isBroken?: boolean;
  style?: React.CSSProperties;
}

export function ConnectionLine({
  x1,
  y1,
  x2,
  y2,
  isActive,
  isBroken,
  style,
}: ConnectionLineProps) {
  const midX = (x1 + x2) / 2;
  const midY = (y1 + y2) / 2;

  return (
    <>
      <svg className="absolute inset-0 pointer-events-none" style={{ zIndex: 0 }}>
        <line
          x1={x1}
          y1={y1}
          x2={x2}
          y2={y2}
          stroke={style?.stroke || (isBroken ? "#dc2626" : isActive ? "#3b82f6" : "#475569")}
          strokeWidth={style?.strokeWidth || 3}
          strokeDasharray={style?.strokeDasharray || (isBroken ? "10,10" : "0")}
          opacity={isBroken ? 0.6 : 1}
          className={isBroken ? "animate-dash" : ""}
        />
      </svg>

      <style>{`
        @keyframes dash {
          to {
            stroke-dashoffset: 20;
          }
        }
        .animate-dash {
          animation: dash 1s linear infinite;
        }
      `}</style>

      {/* Large X for broken connection */}
      {isBroken && (
        <motion.div
          className="absolute bg-red-600 rounded-full p-3 border-4 border-slate-900 shadow-lg shadow-red-600/50"
          style={{
            left: midX - 25,
            top: midY - 25,
            zIndex: 100,
          }}
          initial={{ scale: 0, rotate: 0 }}
          animate={{
            scale: [0, 1.2, 1],
            rotate: [0, 180, 180],
          }}
          transition={{ duration: 0.5 }}
        >
          <X className="w-8 h-8 text-white" strokeWidth={4} />
        </motion.div>
      )}
    </>
  );
}
