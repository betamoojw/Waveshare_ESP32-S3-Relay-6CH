import { AlertTriangle, CheckCircle2, Circle, LoaderCircle, WifiOff } from 'lucide-react'

type Tone = 'success' | 'neutral' | 'warning' | 'danger' | 'info'
const icons = { success: CheckCircle2, neutral: Circle, warning: AlertTriangle, danger: WifiOff, info: LoaderCircle }

export function StatusBadge({ label, tone = 'neutral' }: { label: string; tone?: Tone }) {
  const Icon = icons[tone]
  return <span className={`status-badge status-badge--${tone}`}><Icon aria-hidden="true" size={14} strokeWidth={2.2} />{label}</span>
}