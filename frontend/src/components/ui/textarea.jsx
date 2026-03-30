export function Textarea({ className = "", ...props }) {
  return (
    <textarea
      className={`flex min-h-[80px] w-full rounded-md border border-slate-200 bg-white px-3 py-2 text-sm outline-none placeholder:text-slate-400 focus:border-slate-300 ${className}`}
      {...props}
    />
  );
}