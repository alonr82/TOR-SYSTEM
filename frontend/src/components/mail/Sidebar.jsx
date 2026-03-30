import { FileText, Inbox, PenSquare, Send, Trash2 } from "lucide-react";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import { Card, CardContent } from "@/components/ui/card";
import { Progress } from "@/components/ui/progress";
import { FOLDERS } from "@/data/mockMessages";

const folderIcons = {
  Inbox,
  Sent: Send,
  Drafts: FileText,
  Trash: Trash2,
};

export function Sidebar({ folder, counts, onSelectFolder, onCompose }) {
  return (
    <aside className="border-r border-slate-200 bg-white p-4">
      <Button onClick={onCompose} className="mb-5 h-14 w-full justify-start rounded-2xl bg-blue-600 px-5 text-base font-semibold hover:bg-blue-700">
        <PenSquare className="mr-3 h-5 w-5" />
        New Message
      </Button>

      <div className="space-y-1">
        {FOLDERS.map((item) => {
          const Icon = folderIcons[item];
          const active = folder === item;

          return (
            <button
              key={item}
              onClick={() => onSelectFolder(item)}
              className={`flex w-full items-center justify-between rounded-2xl px-4 py-3 text-left transition ${
                active ? "bg-blue-50 text-blue-700" : "text-slate-700 hover:bg-slate-100"
              }`}
            >
              <div className="flex items-center gap-3">
                <Icon className="h-4 w-4" />
                <span className="font-medium">{item}</span>
              </div>
              {counts[item] > 0 && <Badge className={active ? "bg-blue-600 text-white" : "bg-slate-200 text-slate-700"}>{counts[item]}</Badge>}
            </button>
          );
        })}
      </div>

      <Card className="mt-8 rounded-3xl border-slate-200 bg-slate-50">
        <CardContent className="p-4">
          <div className="mb-3 flex items-center justify-between">
            <span className="text-sm font-semibold text-slate-700">Secure mailbox health</span>
            <span className="text-sm font-semibold text-slate-500">81%</span>
          </div>
          <Progress value={81} className="h-2" />
          <div className="mt-3 text-xs leading-5 text-slate-500">
            Live sync connected. Metadata-safe indicators are visible. Backend delivery mechanics remain hidden.
          </div>
        </CardContent>
      </Card>
    </aside>
  );
}