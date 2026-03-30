export function Progress({ value = 0, className = "" }) {
  return (
    <div className={`relative w-full overflow-hidden rounded-full bg-slate-200 ${className}`}>
      <div
        className="h-full bg-slate-900 transition-all"
        style={{ width: `${value}%` }}
      />
    </div>
  );
}