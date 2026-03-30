import { CheckCircle2, Clock3, Download, File, Trash2 } from "lucide-react";
import { Avatar, AvatarFallback } from "@/components/ui/avatar";
import { Button } from "@/components/ui/button";
import { Card, CardContent } from "@/components/ui/card";
import { SecurityPill } from "@/components/common/SecurityPill";
import { formatLongDate } from "@/utils/mail";

export function MessageDetail({ message }) {
  if (!message) {
    return (
      <div className="flex h-full items-center justify-center px-6">
        <div className="max-w-md text-center">
          <h3 className="text-3xl font-bold tracking-tight">Select a message</h3>
          <p className="mt-3 text-lg leading-8 text-slate-500">
            Choose an item from the secure inbox to view protected content, sender confidence, and attachment details.
          </p>
        </div>
      </div>
    );
  }

  const initials = message.fromName
    .split(" ")
    .map((part) => part[0])
    .slice(0, 2)
    .join("");

  return (
    <div className="flex h-full flex-col">
      <div className="border-b border-slate-200 bg-white px-6 py-5">
        <div className="flex flex-wrap items-start justify-between gap-4">
          <div className="space-y-3">
            <div className="flex items-center gap-3">
              <Avatar className="h-12 w-12">
                <AvatarFallback className="bg-blue-100 text-blue-700">{initials}</AvatarFallback>
              </Avatar>
              <div>
                <div className="text-2xl font-bold tracking-tight">{message.subject}</div>
                <div className="mt-1 text-sm text-slate-500">
                  From <span className="font-medium text-slate-700">{message.fromName}</span> &lt;{message.fromId}&gt;
                </div>
                <div className="text-sm text-slate-500">To {message.to} • {formatLongDate(message.timestamp)}</div>
              </div>
            </div>

            <div className="flex flex-wrap gap-2">
              <SecurityPill icon={CheckCircle2} tone="success">End-to-end protected</SecurityPill>
              <SecurityPill icon={CheckCircle2}>Verified sender</SecurityPill>
              <SecurityPill icon={Clock3} tone="primary">Delivered in real time</SecurityPill>
            </div>
          </div>

          <div className="flex items-center gap-2">
            <Button variant="ghost" size="icon" className="rounded-2xl border border-slate-200 bg-white">
              <Download className="h-4 w-4" />
            </Button>
            <Button variant="ghost" size="icon" className="rounded-2xl border border-slate-200 bg-white">
              <Trash2 className="h-4 w-4" />
            </Button>
          </div>
        </div>
      </div>

      <div className="flex-1 overflow-auto px-6 py-6">
        <Card className="rounded-[2rem] border-slate-200 bg-white shadow-sm">
          <CardContent className="p-8">
            {/* כאן מתחיל החלק שהושלם - גוף ההודעה */}
            <div className="space-y-6 text-base leading-8 text-slate-700">
              {message.body && message.body.map((paragraph, index) => (
                <p key={index}>{paragraph}</p>
              ))}
            </div>

            {/* אזור הקבצים המצורפים */}
            {message.attachments && message.attachments.length > 0 && (
              <div className="mt-10 border-t border-slate-100 pt-8">
                <h4 className="mb-4 text-sm font-semibold uppercase tracking-wider text-slate-500">
                  Attachments ({message.attachments.length})
                </h4>
                <div className="grid gap-4 sm:grid-cols-2 lg:grid-cols-3">
                  {message.attachments.map((file) => (
                    <div
                      key={file.id}
                      className="group flex cursor-pointer items-center gap-4 rounded-2xl border border-slate-200 bg-slate-50 p-4 transition-all hover:border-blue-300 hover:bg-blue-50/50 hover:shadow-md"
                    >
                      <div className="flex h-10 w-10 shrink-0 items-center justify-center rounded-xl bg-blue-100 text-blue-600 transition-colors group-hover:bg-blue-600 group-hover:text-white">
                        <File className="h-5 w-5" />
                      </div>
                      <div className="min-w-0 flex-1">
                        <p className="truncate text-sm font-semibold text-slate-900">
                          {file.name}
                        </p>
                        <p className="text-xs font-medium text-slate-500">
                          {file.size} • {file.kind}
                        </p>
                      </div>
                      <Button
                        variant="ghost"
                        size="icon"
                        className="h-8 w-8 shrink-0 text-slate-400 opacity-0 transition-opacity hover:text-blue-600 group-hover:opacity-100"
                      >
                        <Download className="h-4 w-4" />
                      </Button>
                    </div>
                  ))}
                </div>
              </div>
            )}
          </CardContent>
        </Card>
      </div>
    </div>
  );
}