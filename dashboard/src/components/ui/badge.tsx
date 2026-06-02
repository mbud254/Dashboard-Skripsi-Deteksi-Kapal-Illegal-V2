import * as React from "react";
import { cva, type VariantProps } from "class-variance-authority";
import { cn } from "@/lib/utils";

const badgeVariants = cva(
  "inline-flex items-center rounded-full border px-4 py-1.5 text-xs font-bold uppercase tracking-widest transition-colors",
  {
    variants: {
      variant: {
        safe: "border-emerald-500/50 bg-emerald-500/15 text-emerald-400 shadow-[0_0_20px_rgba(16,185,129,0.25)]",
        warning:
          "border-amber-500/50 bg-amber-500/15 text-amber-400 shadow-[0_0_20px_rgba(245,158,11,0.25)]",
        danger:
          "border-red-500/50 bg-red-500/15 text-red-400 shadow-[0_0_20px_rgba(239,68,68,0.35)] animate-pulse",
      },
    },
    defaultVariants: {
      variant: "safe",
    },
  }
);

export interface BadgeProps
  extends React.HTMLAttributes<HTMLDivElement>,
    VariantProps<typeof badgeVariants> {}

function Badge({ className, variant, ...props }: BadgeProps) {
  return (
    <div className={cn(badgeVariants({ variant }), className)} {...props} />
  );
}

export { Badge, badgeVariants };
