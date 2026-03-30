export function Button({
  className = "",
  variant = "default",
  size = "default",
  type = "button",
  children,
  ...props
}) {
  const variantClasses = {
    default: "bg-slate-900 text-white hover:bg-slate-800",
    ghost: "bg-transparent hover:bg-slate-100 text-slate-900",
    outline: "border border-slate-200 bg-white text-slate-900 hover:bg-slate-50",
  };

  const sizeClasses = {
    default: "h-10 px-4 py-2",
    icon: "h-10 w-10",
  };

  return (
    <button
      type={type}
      className={`inline-flex items-center justify-center rounded-md text-sm font-medium transition disabled:cursor-not-allowed disabled:opacity-50 ${
        variantClasses[variant] || variantClasses.default
      } ${sizeClasses[size] || sizeClasses.default} ${className}`}
      {...props}
    >
      {children}
    </button>
  );
}