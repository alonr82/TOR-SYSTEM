import { ArrowRight, KeyRound, Lock, Shield, ShieldCheck, User, Wifi } from "lucide-react";
import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import { Card, CardContent } from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { Separator } from "@/components/ui/separator";
import { SecurityPill } from "@/components/common/SecurityPill";

export function LoginScreen({ onLogin }) {
  return (
    <div className="min-h-screen bg-[radial-gradient(circle_at_top,_rgba(59,130,246,0.08),_transparent_30%),linear-gradient(180deg,#f8fafc_0%,#eef2ff_100%)] text-slate-900">
      <div className="mx-auto flex min-h-screen max-w-7xl flex-col px-6 py-6">
        <header className="flex items-center justify-between border-b border-slate-200/80 pb-6">
          <div className="flex items-center gap-3">
            <div className="flex h-12 w-12 items-center justify-center rounded-2xl bg-blue-600 text-white shadow-lg shadow-blue-600/20">
              <Shield className="h-6 w-6" />
            </div>
            <div>
              <div className="text-3xl font-bold tracking-tight">SAMS</div>
              <div className="text-sm font-medium uppercase tracking-[0.28em] text-blue-600">Secure Anonymous Mail System</div>
            </div>
          </div>
          <SecurityPill icon={ShieldCheck} tone="success">Private Session Ready</SecurityPill>
        </header>

        <div className="grid flex-1 gap-8 py-10 lg:grid-cols-[1.1fr_0.9fr] lg:items-center">
          <div className="space-y-8">
            <div className="space-y-4">
              <Badge className="rounded-full bg-blue-50 px-4 py-1.5 text-blue-700 hover:bg-blue-50">Clean secure mail UX</Badge>
              <h1 className="max-w-2xl text-5xl font-bold leading-tight tracking-tight text-slate-950">
                Familiar email workflow. Private delivery underneath.
              </h1>
              <p className="max-w-2xl text-lg leading-8 text-slate-600">
                SAMS presents a polished inbox, message detail view, attachment handling, real-time updates, and a secure compose flow. Encryption setup, private routing, and backend delivery orchestration stay completely invisible to the user.
              </p>
            </div>

            <div className="grid gap-4 md:grid-cols-3">
              {[
                { title: "Protected inbox", text: "Security is communicated through clear trust signals, not technical jargon.", Icon: Lock },
                { title: "Live synchronization", text: "Real-time delivery updates appear naturally through the mailbox interface.", Icon: Wifi },
                { title: "No routing exposure", text: "The user sees status like Encrypting and Routing, never circuit paths or session keys.", Icon: Shield },
              ].map(({ title, text, Icon }) => (
                <Card key={title} className="rounded-3xl border-slate-200/80 bg-white/80 shadow-sm backdrop-blur">
                  <CardContent className="p-5">
                    <Icon className="mb-3 h-5 w-5 text-blue-600" />
                    <div className="text-sm font-semibold">{title}</div>
                    <p className="mt-2 text-sm leading-6 text-slate-600">{text}</p>
                  </CardContent>
                </Card>
              ))}
            </div>
          </div>

          <Card className="mx-auto w-full max-w-md overflow-hidden rounded-[2rem] border-slate-200/80 bg-white/95 shadow-2xl shadow-slate-200/80 backdrop-blur">
            <CardContent className="p-8">
              <div className="mb-8 space-y-2 text-center">
                <div className="mx-auto flex h-14 w-14 items-center justify-center rounded-2xl bg-blue-600 text-white shadow-lg shadow-blue-600/20">
                  <ShieldCheck className="h-7 w-7" />
                </div>
                <h2 className="text-3xl font-bold tracking-tight">Welcome back</h2>
                <p className="text-slate-500">Access your encrypted communication workspace.</p>
              </div>

              <div className="space-y-5">
                <div className="space-y-2">
                  <label className="text-sm font-semibold text-slate-700">Username / ID</label>
                  <div className="relative">
                    <User className="absolute left-4 top-1/2 h-4 w-4 -translate-y-1/2 text-slate-400" />
                    <Input className="h-12 rounded-2xl border-slate-200 pl-11" placeholder="Enter your secure identity" />
                  </div>
                </div>
                <div className="space-y-2">
                  <label className="text-sm font-semibold text-slate-700">Passphrase</label>
                  <div className="relative">
                    <KeyRound className="absolute left-4 top-1/2 h-4 w-4 -translate-y-1/2 text-slate-400" />
                    <Input type="password" className="h-12 rounded-2xl border-slate-200 pl-11" placeholder="Enter your secure passphrase" />
                  </div>
                </div>

                <Button 
                  onClick={onLogin} 
                  className="mt-2 h-12 w-full rounded-2xl bg-blue-600 text-white hover:bg-blue-700"
                >
                  Access Workspace
                  <ArrowRight className="ml-2 h-4 w-4" />
                </Button>

                <div className="flex items-center justify-center pt-2">
                  <span className="text-xs text-slate-400">End-to-end encrypted session</span>
                </div>
              </div>
            </CardContent>
          </Card>
        </div>
      </div>
    </div>
  );
}