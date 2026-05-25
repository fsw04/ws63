import { Radio, Cloud, Wifi, AlertTriangle, Watch } from 'lucide-react'
import { useAppStore, type LogEntry } from '@/store/appStore'

const typeConfig: Record<string, { icon: React.ReactNode; color: string; label: string }> = {
  sle: { icon: <Radio size={10} />, color: 'text-accent-green', label: 'SLE' },
  mqtt: { icon: <Cloud size={10} />, color: 'text-accent-cyan', label: 'MQTT' },
  wifi: { icon: <Wifi size={10} />, color: 'text-accent-yellow', label: 'WiFi' },
  alert: { icon: <AlertTriangle size={10} />, color: 'text-accent-red', label: '告警' },
  device: { icon: <Watch size={10} />, color: 'text-accent-green', label: '设备' },
}

const levelColors: Record<string, string> = {
  info: 'bg-accent-cyan/20 text-accent-cyan',
  warn: 'bg-accent-yellow/20 text-accent-yellow',
  error: 'bg-accent-red/20 text-accent-red',
}

function LogItem({ entry }: { entry: LogEntry }) {
  const config = typeConfig[entry.type] || typeConfig.sle
  const time = new Date(entry.timestamp)
  const timeStr = `${time.getHours().toString().padStart(2, '0')}:${time.getMinutes().toString().padStart(2, '0')}:${time.getSeconds().toString().padStart(2, '0')}`

  return (
    <div className="flex gap-2 py-1.5 animate-slide-in">
      <div className="flex flex-col items-center w-6 flex-shrink-0">
        <div className={`w-5 h-5 rounded flex items-center justify-center ${config.color} bg-base-700`}>
          {config.icon}
        </div>
        <div className="w-px flex-1 bg-base-600/30 mt-1" />
      </div>
      <div className="flex-1 min-w-0">
        <div className="flex items-center gap-1.5 mb-0.5">
          <span className={`text-[8px] px-1 py-0.5 rounded font-mono ${levelColors[entry.level]}`}>
            {config.label}
          </span>
          <span className="text-[9px] font-mono text-gray-600">{timeStr}</span>
        </div>
        <p className={`text-[10px] leading-tight ${entry.level === 'error' ? 'text-accent-red' : entry.level === 'warn' ? 'text-accent-yellow' : 'text-gray-400'}`}>
          {entry.message}
        </p>
      </div>
    </div>
  )
}

export default function LogsPage() {
  const { logs } = useAppStore()
  const hasAlert = logs.some((l) => l.level === 'error')

  return (
    <div className="p-3 h-full flex flex-col">
      {hasAlert && (
        <div className="mb-2 px-3 py-2 rounded-lg bg-accent-red/10 border border-accent-red/30 flex items-center gap-2 animate-slide-up">
          <AlertTriangle size={12} className="text-accent-red animate-blink" />
          <span className="text-[10px] text-accent-red font-medium">存在异常告警，请关注</span>
        </div>
      )}

      <div className="flex items-center justify-between mb-2">
        <span className="text-xs font-medium text-gray-400">通信日志</span>
        <span className="text-[9px] text-gray-600 font-mono">{logs.length} 条记录</span>
      </div>

      <div className="flex-1 overflow-y-auto space-y-0">
        {[...logs].reverse().map((entry) => (
          <LogItem key={entry.id} entry={entry} />
        ))}
      </div>
    </div>
  )
}
