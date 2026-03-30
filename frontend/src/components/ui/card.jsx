export function Card({ className = "", children }) {
  return (
    <div className={`rounded-xl border bg-white text-slate-950 shadow-sm ${className}`}>
      {children}
    </div>
  );
}

export function CardContent({ className = "", children }) {
  return <div className={className}>{children}</div>;
}