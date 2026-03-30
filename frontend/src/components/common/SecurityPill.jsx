export function SecurityPill({ icon: Icon, children, tone = "default" }) {
  const toneClasses = {
    default: "bg-slate-100 text-slate-700 border-slate-200",
    success: "bg-emerald-50 text-emerald-700 border-emerald-200",
    primary: "bg-blue-50 text-blue-700 border-blue-200",
  };

  return (
    <div className={`inline-flex items-center gap-2 rounded-full border px-3 py-1.5 text-xs font-medium ${toneClasses[tone]}`}>
      <Icon className="h-3.5 w-3.5" />
      <span>{children}</span>
    </div>
  );
}