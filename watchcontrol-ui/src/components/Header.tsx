import { Radio } from 'lucide-react'

export default function Header() {
  return (
    <div className="h-11 flex items-center justify-between px-3 bg-base-800/80 backdrop-blur-sm border-b border-base-600/20">
      <div className="flex items-center gap-2">
        <div className="w-6 h-6 rounded-md bg-accent-green/20 flex items-center justify-center">
          <Radio size={14} className="text-accent-green" />
        </div>
        <span className="text-xs font-medium text-gray-200 font-sans">
          WatchControl
        </span>
      </div>
      <div className="flex items-center gap-1.5">
        <span className="status-dot status-dot-online" />
        <span className="text-[10px] text-gray-400 font-mono">运行中</span>
      </div>
    </div>
  )
}
