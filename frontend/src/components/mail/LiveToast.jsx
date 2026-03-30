import { Wifi } from "lucide-react";
import { Badge } from "@/components/ui/badge";

export function LiveToast({ visible }) {
  if (!visible) return null;

  return (
    <div className="fixed right-5 top-24 z-40 w-[360px] rounded-[1.75rem] border border-blue-200 bg-white p-4 shadow-2xl">
      <div className="flex items-start gap-3">
        <div className="mt-1 flex h-10 w-10 items-center justify-center rounded-2xl bg-blue-100 text-blue-700">
          <Wifi className="h-5 w-5" />
        </div>
        <div className="min-w-0 flex-1">
          <div className="flex items-center justify-between gap-3">
            <div className="font-semibold">New secure message received</div>
            <Badge className="rounded-full bg-blue-50 text-blue-700 hover:bg-blue-50">Live</Badge>
          </div>
          <p className="mt-1 text-sm leading-6 text-slate-500">
            The inbox updated in real time. Delivery state is visible, while backend routing remains hidden.
          </p>
        </div>
      </div>
    </div>
  );
}