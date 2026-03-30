import { CheckCircle2, Clock3, Sparkles } from "lucide-react";
import { SEND_STAGES } from "@/data/mockMessages";

export function StatusOverlay({ sendStep }) {
  if (sendStep < 0) return null;

  return (
    <div className="fixed bottom-5 left-1/2 z-50 w-[min(840px,calc(100%-24px))] -translate-x-1/2 rounded-[1.75rem] border border-slate-800/80 bg-slate-950 px-5 py-4 text-white shadow-2xl">
      <div className="flex flex-col gap-4 md:flex-row md:items-center md:justify-between">
        <div>
          <div className="text-sm font-semibold uppercase tracking-[0.22em] text-slate-300">System status</div>
          <div className="mt-1 text-xl font-bold tracking-tight">{SEND_STAGES[sendStep]}</div>
        </div>

        <div className="grid flex-1 gap-2 md:max-w-[520px] md:grid-cols-3">
          {SEND_STAGES.map((stage, index) => {
            const active = index === sendStep;
            const done = index < sendStep;
            const Icon = done ? CheckCircle2 : active ? Sparkles : Clock3;

            return (
              <div
                key={stage}
                className={`rounded-2xl border px-3 py-3 text-sm ${
                  active
                    ? "border-blue-500 bg-blue-500/10 text-blue-300"
                    : done
                    ? "border-emerald-500/30 bg-emerald-500/10 text-emerald-300"
                    : "border-slate-700 bg-slate-900 text-slate-400"
                }`}
              >
                <div className="flex items-center gap-2 font-medium">
                  <Icon className="h-4 w-4" />
                  {stage}
                </div>
              </div>
            );
          })}
        </div>
      </div>
    </div>
  );
}