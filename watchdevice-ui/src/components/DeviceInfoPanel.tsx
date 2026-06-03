import useAppStore from '@/store/appStore'
import { Watch, Cpu, Clock } from 'lucide-react'

export default function DeviceInfoPanel() {
  const dev = useAppStore((s) => s.deviceInfo)

  const uptime = formatUptime(dev.uptime)

  return (
    <div className="flex flex-col gap-2 px-3 py-2 rounded-lg bg-[#111827]/80 border border-[#1E3A5F]/40 h-full">
      <div className="flex items-center gap-1.5">
        <Watch size={10} className="text-[#00D4FF]" />
        <span className="text-[#6B7FA3] text-[9px]">设备</span>
        <span className="text-[#A0B4D0] text-[10px] font-mono ml-auto">{dev.deviceId}</span>
      </div>
      <div className="flex items-center gap-1.5">
        <Cpu size={10} className="text-[#6B7FA3]" />
        <span className="text-[#6B7FA3] text-[9px]">MAC</span>
        <span className="text-[#A0B4D0] text-[10px] font-mono ml-auto">{dev.mac}</span>
      </div>
      <div className="flex items-center gap-1.5">
        <Clock size={10} className="text-[#6B7FA3]" />
        <span className="text-[#6B7FA3] text-[9px]">运行</span>
        <span className="text-[#A0B4D0] text-[10px] font-mono ml-auto">{uptime}</span>
      </div>
    </div>
  )
}

function formatUptime(seconds: number): string {
  const h = Math.floor(seconds / 3600)
  const m = Math.floor((seconds % 3600) / 60)
  const s = seconds % 60
  return `${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`
}
