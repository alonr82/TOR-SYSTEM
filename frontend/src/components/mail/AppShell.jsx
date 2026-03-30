import { Bell, Settings, Shield, ShieldCheck, Wifi } from "lucide-react";
import { Avatar, AvatarFallback } from "@/components/ui/avatar";
import { Button } from "@/components/ui/button";
import { SecurityPill } from "@/components/common/SecurityPill";
import { Sidebar } from "@/components/mail/Sidebar";
import { InboxPanel } from "@/components/mail/InboxPanel";
import { MessageDetail } from "@/components/mail/MessageDetail";
import { ComposeModal } from "@/components/mail/ComposeModal";
import { StatusOverlay } from "@/components/mail/StatusOverlay";
import { LiveToast } from "@/components/mail/LiveToast";

export function AppShell({ state, actions }) {
  return (
    <div className="min-h-screen bg-slate-50 text-slate-950">
      <div className="flex min-h-screen flex-col">
        <header className="sticky top-0 z-30 border-b border-slate-200 bg-white/90 backdrop-blur-xl">
          <div className="flex items-center justify-between gap-4 px-4 py-4 lg:px-6">
            <div className="flex items-center gap-3">
              <div className="flex h-11 w-11 items-center justify-center rounded-2xl bg-blue-600 text-white shadow-md shadow-blue-600/20">
                <Shield className="h-5 w-5" />
              </div>
              <div>
                <div className="text-2xl font-bold tracking-tight">SAMS</div>
                <div className="text-xs font-semibold uppercase tracking-[0.24em] text-blue-600">Secure Anonymous Mail System</div>
              </div>
            </div>

            <div className="hidden items-center gap-3 md:flex">
              <SecurityPill icon={ShieldCheck} tone="success">Protected delivery active</SecurityPill>
              <SecurityPill icon={Wifi} tone="primary">WebSocket live sync</SecurityPill>
            </div>

            <div className="flex items-center gap-3">
              <Button variant="ghost" size="icon" className="rounded-2xl border border-slate-200 bg-slate-50">
                <Bell className="h-4 w-4" />
              </Button>
              <Button variant="ghost" size="icon" className="rounded-2xl border border-slate-200 bg-slate-50">
                <Settings className="h-4 w-4" />
              </Button>
              <div className="hidden items-center gap-3 rounded-2xl border border-slate-200 bg-white px-3 py-2 md:flex">
                <Avatar className="h-10 w-10">
                  <AvatarFallback className="bg-blue-100 text-blue-700">AR</AvatarFallback>
                </Avatar>
                <div>
                  <div className="text-sm font-semibold">Alex Rivera</div>
                  <div className="text-xs text-slate-500">secure@sams.io</div>
                </div>
              </div>
            </div>
          </div>
        </header>

        <div className="grid flex-1 lg:grid-cols-[280px_420px_minmax(0,1fr)]">
          <Sidebar
            folder={state.folder}
            counts={state.folderCounts}
            onSelectFolder={actions.setFolder}
            onCompose={() => actions.setComposeOpen(true)}
          />

          <InboxPanel
            folder={state.folder}
            search={state.search}
            senderFilter={state.senderFilter}
            dateFilter={state.dateFilter}
            senderOptions={state.senderOptions}
            messages={state.filteredMessages}
            selectedId={state.selectedId}
            onSearchChange={actions.setSearch}
            onSenderFilterChange={actions.setSenderFilter}
            onDateFilterChange={actions.setDateFilter}
            onSelectMessage={actions.setSelectedId}
          />

          <MessageDetail message={state.selectedMessage} />
        </div>

        <ComposeModal
          open={state.composeOpen}
          form={state.composeForm}
          sendStep={state.sendStep}
          onClose={() => actions.setComposeOpen(false)}
          onFieldChange={(field, value) => 
            actions.setComposeForm({ ...state.composeForm, [field]: value })
          }
          onSend={actions.handleSend}
        />

        <StatusOverlay step={state.sendStep} />
        
        <LiveToast 
          visible={state.liveToastVisible} 
          onClose={() => actions.setLiveToastVisible(false)} 
        />
        
      </div>
    </div>
  );
}