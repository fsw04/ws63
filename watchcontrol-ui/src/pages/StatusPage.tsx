import { Wifi, Cloud, Radio, Hash } from 'lucide-react'
import { useAppStore } from '@/store/appStore'

function StatusIndicator({ online, label }: { online: boolean; label: string }) {
  return (
    <div className="flex items-center gap-1.5">
      <span className={`status-dot ${online ? 'status-dot-online' : 'status-dot-offline'}`} />
      <span className={`text-[10px] font-medium ${online ? 'text-accent-green' : 'text-accent-red'}`}>
        {label}
      </span>
    </div>
  )
}

function StatusCard({
  icon,
  title,
  online,
  onlineLabel,
  offlineLabel,
  details,
}: {
  icon: React.ReactNode
  title: string
  online: boolean
  onlineLabel: string
  offlineLabel: string
  details: { label: string; value: string }[]
}) {
  return (
    <div className={`card ${online ? 'animate-glow' : ''}`}>
      <div className="flex items-center justify-between mb-2">
        <div className="flex items-center gap-2">
          <div className={`w-7 h-7 rounded-lg flex items-center justify-center ${online ? 'bg-accent-green/15' : 'bg-accent-red/15'}`}>
            {icon}
          </div>
          <span className="text-xs font-medium text-gray-300">{title}</span>
        </div>
        <StatusIndicator online={online} label={online ? onlineLabel : offlineLabel} />
      </div>
      <div className="space-y-1">
        {details.map((d, i) => (
          <div key={i} className="flex items-center justify-between">
            <span className="text-[10px] text-gray-500">{d.label}</span>
            <span className="text-[10px] font-mono text-gray-300">{d.value}</span>
          </div>
        ))}
      </div>
    </div>
  )
}

function DeviceCounter() {
  const { systemStatus, devices } = useAppStore()
  const total = devices.length
  const connected = systemStatus.sle.connectedDevices
  const percentage = total > 0 ? (connected / total) * 100 : 0

  const idleCount = devices.filter((d) => d.status === 'idle').length
  const borrowedCount = devices.filter((d) => d.status === 'borrowed').length

  const circumference = 2 * Math.PI * 28
  const strokeDashoffset = circumference - (percentage / 100) * circumference

  return (
    <div className="card flex items-center gap-4">
      <div className="relative w-16 h-16 flex-shrink-0">
        <svg className="w-16 h-16 -rotate-90" viewBox="0 0 64 64">
          <circle
            cx="32" cy="32" r="28"
            fill="none"
            stroke="rgba(36, 48, 73, 0.5)"
            strokeWidth="4"
          />
          <circle
            cx="32" cy="32" r="28"
            fill="none"
            stroke="#00E5A0"
            strokeWidth="4"
            strokeLinecap="round"
            strokeDasharray={circumference}
            strokeDashoffset={strokeDashoffset}
            className="transition-all duration-1000 ease-out"
          />
        </svg>
        <div className="absolute inset-0 flex items-center justify-center">
          <span className="text-lg font-mono font-bold text-accent-green">{connected}</span>
        </div>
      </div>
      <div className="flex-1">
        <div className="text-xs font-medium text-gray-300 mb-1">SLE 设备</div>
        <div className="text-[10px] text-gray-500 mb-2">
          {connected} 在线 / {total} 总计
        </div>
        <div className="flex items-center gap-2">
          <span className="text-[9px] text-accent-green">{idleCount} 空闲</span>
          <span className="text-[9px] text-accent-yellow">{borrowedCount} 借出</span>
        </div>
      </div>
    </div>
  )
}

export default function StatusPage() {
  const { systemStatus } = useAppStore()
  const { wifi, mqtt, sle } = systemStatus

  return (
    <div className="p-3 space-y-2.5 animate-slide-up">
      <StatusCard
        icon={<Wifi size={14} className="text-accent-green" />}
        title="WiFi"
        online={wifi.connected}
        onlineLabel="已连接"
        offlineLabel="未连接"
        details={[
          { label: 'SSID', value: wifi.ssid },
          { label: 'IP', value: wifi.ip },
          { label: '信号', value: `${wifi.signalStrength} dBm` },
        ]}
      />

      <StatusCard
        icon={<Cloud size={14} className="text-accent-cyan" />}
        title="MQTT"
        online={mqtt.connected}
        onlineLabel="已连接"
        offlineLabel="未连接"
        details={[
          { label: 'Broker', value: mqtt.broker },
          { label: 'Topic', value: mqtt.topic },
          { label: '最近上报', value: formatTime(mqtt.lastPublish) },
        ]}
      />

      <StatusCard
        icon={<Radio size={14} className="text-accent-green" />}
        title="SLE 星闪"
        online={sle.broadcasting}
        onlineLabel="广播中"
        offlineLabel="已停止"
        details={[
          { label: '服务名', value: sle.serverName },
          { label: '已连接', value: `${sle.connectedDevices} 台设备` },
        ]}
      />

      <DeviceCounter />

      <div className="card">
        <div className="flex items-center gap-2 mb-2">
          <Hash size={12} className="text-gray-500" />
          <span className="text-[10px] text-gray-500">系统信息</span>
        </div>
        <div className="space-y-1">
          <div className="flex items-center justify-between">
            <span className="text-[10px] text-gray-500">协议栈</span>
            <span className="text-[10px] font-mono text-gray-400">SSAP / SLE 1.0</span>
          </div>
          <div className="flex items-center justify-between">
            <span className="text-[10px] text-gray-500">MQTT QoS</span>
            <span className="text-[10px] font-mono text-gray-400">1</span>
          </div>
          <div className="flex items-center justify-between">
            <span className="text-[10px] text-gray-500">MTU</span>
            <span className="text-[10px] font-mono text-gray-400">1500 bytes</span>
          </div>
        </div>
      </div>
    </div>
  )
}

function formatTime(ts: number): string {
  const d = new Date(ts)
  return `${d.getHours().toString().padStart(2, '0')}:${d.getMinutes().toString().padStart(2, '0')}:${d.getSeconds().toString().padStart(2, '0')}`
}
