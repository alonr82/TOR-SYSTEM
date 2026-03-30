import { CalendarDays, Filter, Paperclip, Search, UserCircle2 } from "lucide-react";
import { Badge } from "@/components/ui/badge";
import { Input } from "@/components/ui/input";
import { DATE_FILTERS } from "@/data/mockMessages";

export function InboxPanel({
  folder,
  search,
  senderFilter,
  dateFilter,
  senderOptions,
  messages,
  selectedId,
  onSearchChange,
  onSenderFilterChange,
  onDateFilterChange,
  onSelectMessage,
}) {
  return (
    <section className="border-r border-slate-200 bg-white">
      <div className="border-b border-slate-200 p-4">
        <div className="relative">
          <Search className="absolute left-4 top-1/2 h-4 w-4 -translate-y-1/2 text-slate-400" />
          <Input
            value={search}
            onChange={(event) => onSearchChange(event.target.value)}
            placeholder="Search in secure mail..."
            className="h-12 rounded-2xl border-slate-200 bg-slate-50 pl-11"
          />
        </div>

        <div className="mt-4 grid gap-3 sm:grid-cols-2">
          <div className="rounded-2xl border border-slate-200 bg-slate-50 px-3 py-2">
            <div className="mb-1 flex items-center gap-2 text-xs font-semibold uppercase tracking-wide text-slate-500">
              <UserCircle2 className="h-3.5 w-3.5" /> Sender
            </div>
            <select
              value={senderFilter}
              onChange={(event) => onSenderFilterChange(event.target.value)}
              className="w-full bg-transparent text-sm font-medium outline-none"
              disabled={folder !== "Inbox"}
            >
              {senderOptions.map((option) => (
                <option key={option} value={option}>{option}</option>
              ))}
            </select>
          </div>

          <div className="rounded-2xl border border-slate-200 bg-slate-50 px-3 py-2">
            <div className="mb-1 flex items-center gap-2 text-xs font-semibold uppercase tracking-wide text-slate-500">
              <CalendarDays className="h-3.5 w-3.5" /> Date
            </div>
            <select
              value={dateFilter}
              onChange={(event) => onDateFilterChange(event.target.value)}
              className="w-full bg-transparent text-sm font-medium outline-none"
              disabled={folder !== "Inbox"}
            >
              {DATE_FILTERS.map((option) => (
                <option key={option} value={option}>{option}</option>
              ))}
            </select>
          </div>
        </div>

        <div className="mt-4 flex items-center justify-between">
          <div>
            <div className="text-sm font-semibold text-slate-700">{folder}</div>
            <div className="text-sm text-slate-500">{messages.length} messages match your current view</div>
          </div>
          <div className="flex items-center gap-2 rounded-full bg-slate-100 px-3 py-2 text-xs font-medium text-slate-600">
            <Filter className="h-3.5 w-3.5" /> Filtered view
          </div>
        </div>
      </div>

      {/* כאן מתחיל החלק שהושלם */}
      <div className="max-h-[calc(100vh-245px)] overflow-auto">
        {messages.length === 0 ? (
          <div className="flex flex-col items-center justify-center p-8 text-center text-slate-500">
            <div className="mb-2 rounded-full bg-slate-100 p-3">
              <Search className="h-6 w-6 text-slate-400" />
            </div>
            <p className="text-sm">No messages found</p>
          </div>
        ) : (
          messages.map((message) => {
            const isSelected = message.id === selectedId;
            return (
              <div
                key={message.id}
                onClick={() => onSelectMessage(message.id)}
                className={`cursor-pointer border-b border-slate-100 p-4 transition-colors hover:bg-slate-50 ${
                  isSelected ? "border-l-4 border-l-blue-600 bg-blue-50/40" : "border-l-4 border-l-transparent"
                }`}
              >
                <div className="mb-1 flex items-center justify-between">
                  <div className="flex items-center gap-2">
                    <span className={`text-sm font-semibold ${message.unread ? "text-slate-900" : "text-slate-700"}`}>
                      {message.fromName}
                    </span>
                    {message.unread && (
                      <span className="h-2 w-2 rounded-full bg-blue-600"></span>
                    )}
                  </div>
                  <span className="text-xs font-medium text-slate-500">{message.dateLabel}</span>
                </div>
                
                <div className={`mb-1 truncate text-sm ${message.unread ? "font-semibold text-slate-900" : "text-slate-600"}`}>
                  {message.subject}
                </div>
                
                <div className="line-clamp-2 text-xs leading-relaxed text-slate-500">
                  {message.preview}
                </div>

                {message.attachments && message.attachments.length > 0 && (
                  <div className="mt-3 flex items-center gap-1.5 text-xs font-medium text-slate-400">
                    <Paperclip className="h-3.5 w-3.5" />
                    <span>{message.attachments.length} Attachment{message.attachments.length > 1 ? 's' : ''}</span>
                  </div>
                )}
              </div>
            );
          })
        )}
      </div>
    </section>
  );
}