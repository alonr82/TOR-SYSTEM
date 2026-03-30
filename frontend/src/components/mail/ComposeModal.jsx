import { ChevronRight, PenSquare, Shield, Wifi, X } from "lucide-react";
import { Button } from "@/components/ui/button";
import { Card, CardContent } from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { Textarea } from "@/components/ui/textarea";

export function ComposeModal({ open, form, sendStep, onClose, onFieldChange, onSend }) {
  if (!open) return null;

  const isDisabled = !form.recipient.trim() || !form.message.trim() || sendStep >= 0;

  return (
    <div className="fixed inset-0 z-40 bg-slate-950/35 backdrop-blur-sm flex items-center justify-center p-4">
      <div className="w-full max-w-[980px] overflow-hidden rounded-[2rem] border border-slate-200 bg-white shadow-2xl">
        <div className="flex items-center justify-between border-b border-slate-200 bg-slate-50 px-6 py-5">
          <div>
            <div className="flex items-center gap-2 text-2xl font-bold tracking-tight">
              <PenSquare className="h-5 w-5 text-blue-600" /> New Secure Message
            </div>
            <div className="mt-1 text-sm text-slate-500">
              Familiar compose experience with private delivery and simple progress states.
            </div>
          </div>
          <Button variant="ghost" size="icon" className="rounded-2xl" onClick={onClose} disabled={sendStep >= 0}>
            <X className="h-5 w-5" />
          </Button>
        </div>

        <div className="grid gap-6 p-6 lg:grid-cols-[1fr_280px]">
          <div className="space-y-5">
            <div className="space-y-2">
              <label className="text-sm font-semibold uppercase tracking-[0.22em] text-slate-500">Recipient</label>
              <Input
                value={form.recipient}
                onChange={(event) => onFieldChange("recipient", event.target.value)}
                className="h-12 rounded-2xl border-slate-200"
                placeholder="Recipient ID or secure address"
                disabled={sendStep >= 0}
              />
            </div>

            <div className="space-y-2">
              <label className="text-sm font-semibold uppercase tracking-[0.22em] text-slate-500">Subject</label>
              <Input
                value={form.subject}
                onChange={(event) => onFieldChange("subject", event.target.value)}
                className="h-12 rounded-2xl border-slate-200"
                placeholder="Enter message subject"
                disabled={sendStep >= 0}
              />
            </div>

            <div className="space-y-2">
              <label className="text-sm font-semibold uppercase tracking-[0.22em] text-slate-500">Message</label>
              <Textarea
                value={form.message}
                onChange={(event) => onFieldChange("message", event.target.value)}
                className="min-h-[280px] rounded-[1.5rem] border-slate-200 px-4 py-4 text-base leading-7 resize-none"
                placeholder="Type your secure message here... all private delivery mechanics stay hidden from the user."
                disabled={sendStep >= 0}
              />
            </div>

            <div className="flex flex-wrap items-center justify-between gap-3">
              <div className="text-sm text-slate-500">File upload is intentionally not supported in this compose flow.</div>
              <Button onClick={onSend} disabled={isDisabled} className="rounded-2xl bg-blue-600 px-6 py-6 text-base font-semibold hover:bg-blue-700">
                Secure Send
                <ChevronRight className="ml-2 h-4 w-4" />
              </Button>
            </div>
          </div>

          {/* כאן מתחיל החלק שהיה חסר - עמודת הסטטוס / אבטחה */}
          <div className="flex flex-col gap-6 rounded-[1.5rem] border border-slate-200 bg-slate-50 p-6">
            <div className="flex items-center gap-2 font-semibold text-slate-900">
              <Shield className="h-5 w-5 text-blue-600" />
              Delivery Status
            </div>
            
            <div className="flex-1 space-y-6">
              <div className={`flex items-center gap-3 transition-opacity ${sendStep >= 0 ? "opacity-100" : "opacity-40"}`}>
                <div className={`h-3 w-3 rounded-full ${sendStep >= 0 ? "bg-blue-600 animate-pulse" : "bg-slate-300"}`} />
                <span className={`text-sm font-medium ${sendStep >= 0 ? "text-blue-700" : "text-slate-500"}`}>Encrypting...</span>
              </div>
              
              <div className={`flex items-center gap-3 transition-opacity ${sendStep >= 1 ? "opacity-100" : "opacity-40"}`}>
                <div className={`h-3 w-3 rounded-full ${sendStep >= 1 ? "bg-blue-600 animate-pulse" : "bg-slate-300"}`} />
                <span className={`text-sm font-medium ${sendStep >= 1 ? "text-blue-700" : "text-slate-500"}`}>Routing...</span>
              </div>
              
              <div className={`flex items-center gap-3 transition-opacity ${sendStep >= 2 ? "opacity-100" : "opacity-40"}`}>
                <div className={`h-3 w-3 rounded-full ${sendStep >= 2 ? "bg-green-500" : "bg-slate-300"}`} />
                <span className={`text-sm font-medium ${sendStep >= 2 ? "text-green-600" : "text-slate-500"}`}>Sent Securely</span>
              </div>
            </div>

            <div className="mt-auto pt-6 border-t border-slate-200">
              <div className="flex items-center gap-2 text-xs font-medium text-slate-500">
                <Wifi className="h-4 w-4" />
                End-to-end encrypted session
              </div>
            </div>
          </div>
          
        </div>
      </div>
    </div>
  );
}