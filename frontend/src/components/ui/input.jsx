export function Input({ className = "", ...props }) {
  return (
    <input
      className={`flex w-full rounded-md border border-slate-200 bg-white px-3 py-2 text-sm outline-none placeholder:text-slate-400 focus:border-slate-300 ${className}`}
      {...props}
    />
  );
}