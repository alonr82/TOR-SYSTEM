import {
  Bell,
  Link2,
  Radio,
  Settings,
  Shield,
  ShieldCheck,
  Wifi,
  Zap,
} from "lucide-react";
import { Avatar, AvatarFallback } from "@/components/ui/avatar";
import { Button } from "@/components/ui/button";
import { SecurityPill } from "@/components/common/SecurityPill";
import { Sidebar } from "@/components/mail/Sidebar";
import { InboxPanel } from "@/components/mail/InboxPanel";
import { MessageDetail } from "@/components/mail/MessageDetail";
import { ComposeModal } from "@/components/mail/ComposeModal";
import { LiveToast } from "@/components/mail/LiveToast";

function ConnectModal({
  open,
  form,
  onClose,
  onFieldChange,
  onConnect,
}) {
  if (!open) {
    return null;
  }

  return (
    <div className="fixed inset-0 z-40 bg-slate-950/35 backdrop-blur-sm">
      <div className="mx-auto mt-8 w-[min(760px,calc(100%-24px))] overflow-hidden rounded-[2rem] border border-slate-200 bg-white shadow-2xl">
        <div className="border-b border-slate-200 bg-slate-50 px-6 py-5">
          <div className="text-2xl font-bold tracking-tight">Build Secure Route</div>
          <div className="mt-1 text-sm text-slate-500">
            Connect the local C client to a remote peer through the Tor-like route.
          </div>
        </div>

        <div className="grid gap-5 p-6 md:grid-cols-2">
          <div className="space-y-2">
            <label className="text-sm font-semibold text-slate-700">Target IP</label>
            <input
              className="h-12 w-full rounded-2xl border border-slate-200 px-4"
              value={form.ip}
              onChange={(event) => onFieldChange("ip", event.target.value)}
            />
          </div>

          <div className="space-y-2">
            <label className="text-sm font-semibold text-slate-700">Target Port</label>
            <input
              className="h-12 w-full rounded-2xl border border-slate-200 px-4"
              value={form.port}
              onChange={(event) => onFieldChange("port", event.target.value)}
            />
          </div>

          <div className="space-y-2">
            <label className="text-sm font-semibold text-slate-700">Local Listen Port</label>
            <input
              className="h-12 w-full rounded-2xl border border-slate-200 px-4"
              value={form.listenPort}
              onChange={(event) => onFieldChange("listenPort", event.target.value)}
            />
          </div>

          <div className="space-y-2">
            <label className="text-sm font-semibold text-slate-700">Vault Password</label>
            <input
              className="h-12 w-full rounded-2xl border border-slate-200 px-4"
              value={form.dbPassword}
              onChange={(event) => onFieldChange("dbPassword", event.target.value)}
            />
          </div>

          <div className="space-y-2 md:col-span-2">
            <label className="text-sm font-semibold text-slate-700">Route Mode</label>
            <select
              className="h-12 w-full rounded-2xl border border-slate-200 px-4"
              value={form.routeMode}
              onChange={(event) => onFieldChange("routeMode", event.target.value)}
            >
              <option value="default">Default</option>
              <option value="custom">Custom</option>
            </select>
          </div>

          {form.routeMode === "custom" && (
            <>
              <div className="space-y-2">
                <label className="text-sm font-semibold text-slate-700">Route Relay 0</label>
                <input
                  className="h-12 w-full rounded-2xl border border-slate-200 px-4"
                  value={form.route0}
                  onChange={(event) => onFieldChange("route0", event.target.value)}
                />
              </div>
              <div className="space-y-2">
                <label className="text-sm font-semibold text-slate-700">Route Relay 1</label>
                <input
                  className="h-12 w-full rounded-2xl border border-slate-200 px-4"
                  value={form.route1}
                  onChange={(event) => onFieldChange("route1", event.target.value)}
                />
              </div>
              <div className="space-y-2 md:col-span-2">
                <label className="text-sm font-semibold text-slate-700">Route Relay 2</label>
                <input
                  className="h-12 w-full rounded-2xl border border-slate-200 px-4"
                  value={form.route2}
                  onChange={(event) => onFieldChange("route2", event.target.value)}
                />
              </div>
            </>
          )}
        </div>

        <div className="flex justify-end gap-3 border-t border-slate-200 px-6 py-4">
          <Button variant="ghost" className="rounded-2xl" onClick={onClose}>
            Cancel
          </Button>
          <Button className="rounded-2xl bg-blue-600 hover:bg-blue-700" onClick={onConnect}>
            Connect Securely
          </Button>
        </div>
      </div>
    </div>
  );
}

function ConnectionsPanel({
  connections,
  activeConnectionId,
  onSelectConnection,
  onPing,
  onDisconnect,
}) {
  return (
    <div className="border-t border-slate-200 bg-white px-4 py-4">
      <div className="mb-3 flex items-center gap-2 text-sm font-bold uppercase tracking-[0.22em] text-slate-500">
        <Radio className="h-4 w-4" />
        Active Connections
      </div>

      <div className="space-y-3">
        {connections.length === 0 && (
          <div className="rounded-2xl border border-dashed border-slate-200 p-4 text-sm text-slate-500">
            No active connections yet.
          </div>
        )}

        {connections.map((item) => {
          const active = item.connectionId === activeConnectionId;

          return (
            <div
              key={item.connectionId}
              className={`rounded-2xl border p-4 ${
                active ? "border-blue-300 bg-blue-50" : "border-slate-200 bg-slate-50"
              }`}
            >
              <div className="flex items-center justify-between gap-4">
                <button
                  className="text-left"
                  onClick={() => onSelectConnection(item.connectionId)}
                >
                  <div className="font-semibold text-slate-900">{item.label}</div>
                  <div className="text-sm text-slate-500">{item.status}</div>
                </button>

                <div className="flex gap-2">
                  <Button
                    variant="ghost"
                    className="rounded-2xl border border-slate-200"
                    onClick={() => onPing(item.connectionId)}
                  >
                    Ping
                  </Button>
                  <Button
                    variant="ghost"
                    className="rounded-2xl border border-slate-200"
                    onClick={() => onDisconnect(item.connectionId)}
                  >
                    Disconnect
                  </Button>
                </div>
              </div>
            </div>
          );
        })}
      </div>
    </div>
  );
}

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
                <div className="text-xs font-semibold uppercase tracking-[0.24em] text-blue-600">
                  Secure Anonymous Mail System
                </div>
              </div>
            </div>

            <div className="hidden items-center gap-3 md:flex">
              <SecurityPill icon={ShieldCheck} tone="success">
                Protected delivery active
              </SecurityPill>
              <SecurityPill icon={Wifi} tone="primary">
                {state.serviceStatus}
              </SecurityPill>
              <SecurityPill icon={Zap}>
                {state.lastAck || "Waiting for commands"}
              </SecurityPill>
            </div>

            <div className="flex items-center gap-3">
              <Button
                variant="ghost"
                className="rounded-2xl border border-slate-200 bg-slate-50"
                onClick={actions.openConnectModal}
              >
                <Link2 className="mr-2 h-4 w-4" />
                Connect
              </Button>
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
            onCompose={actions.openComposeModal}
          />

          <div className="border-r border-slate-200 bg-white">
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

            <ConnectionsPanel
              connections={state.connections}
              activeConnectionId={state.activeConnectionId}
              onSelectConnection={actions.selectConnection}
              onPing={actions.requestPing}
              onDisconnect={actions.requestDisconnect}
            />
          </div>

          <section className="min-w-0 bg-slate-50">
            <MessageDetail message={state.selectedMessage} />
          </section>
        </div>
      </div>

      <ComposeModal
        open={state.composeOpen}
        form={state.composeForm}
        sendStep={state.sendStep}
        onClose={() => actions.setComposeOpen(false)}
        onFieldChange={actions.updateComposeField}
        onSend={actions.startSecureSend}
      />

      <ConnectModal
        open={state.connectOpen}
        form={state.connectForm}
        onClose={() => actions.setConnectOpen(false)}
        onFieldChange={actions.updateConnectField}
        onConnect={actions.startConnect}
      />

      <LiveToast visible={state.liveToastVisible} />
    </div>
  );
}