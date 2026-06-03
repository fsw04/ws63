import useAppStore from '@/store/appStore'
import { Wifi, WifiOff, Send, Signal } from 'lucide-react'

export default function SleConnectionPanel() {
  const sle = useAppStore((s) => s.sleStatus)

  const rssiBars = getRssiBars(sle.rssi)

  return (
    <div className="flex flex-col gap-2 px-3 py-2 rounded-lg bg-[#111827]/80 border border-[#1E3A5F]/40 h-full">
      {/* 连接状态 */}
      <div className="flex items-center gap-2">
        <div className={`w-2 h-2 rounded-full ${sle.connected ? 'bg-[#00E5A0] shadow-[0_0_6px_#00E5A0]' : 'bg-[#FF4D6A] shadow-[0_0_6px_#FF4D6A]'}`} />
        <span className="text-[#E0E7FF] text-xs font-medium">SLE</span>
        <span className={`text-[10px] ${sle.connected ? 'text-[#00E5A0]' : 'text-[#FF4D6A]'}`}>
          {sle.connected ? '已连接' : '未连接'}
        </span>
        {sle.connected ? (
          <Wifi size={12} className="text-[#00E5A0] ml-auto" />
        ) : (
          <WifiOff size={12} className="text-[#FF4D6A] ml-auto" />
        )}
      </div>

      {/* 网关 MAC */}
      {sle.connected && (
        <div className="flex items-center gap-1.5">
          <span className="text-[#6B7FA3] text-[9px]">网关</span>
          <span className="text-[#A0B4D0] text-[10px] font-mono">{sle.gatewayMac}</span>
        </div>
      )}

      {/* RSSI 信号条 */}
      {sle.connected && (
        <div className="flex items-center gap-1.5">
          <Signal size={10} className="text-[#6B7FA3]" />
          <span className="text-[#6B7FA3] text-[9px]">RSSI</span>
          <div className="flex items-end gap-[2px] ml-1">
            {rssiBars.map((active, i) => (
              <div
                key={i}
                className={`w-[3px] rounded-sm ${active ? 'bg-[#00E5A0]' : 'bg-[#1E3A5F]'}`}
                style={{ height: `${4 + i * 3}px` }}
              />
            ))}
          </div>
          <span className="text-[#A0B4D0] text-[10px] font-mono ml-1">{sle.rssi}dBm</span>
        </div>
      )}

      {/* 发送状态 */}
      <div className="flex items-center gap-1.5 mt-auto">
        <Send size={10} className={sle.lastSendSuccess ? 'text-[#00E5A0]' : 'text-[#FF4D6A]'} />
        <span className="text-[#6B7FA3] text-[9px]">发送</span>
        <span className={`text-[10px] ${sle.lastSendSuccess ? 'text-[#00E5A0]' : 'text-[#FF4D6A]'}`}>
          {sle.lastSendSuccess ? '✓' : '✗'}
        </span>
        <span className="text-[#A0B4D0] text-[10px] font-mono ml-auto">{sle.lastSendTime}</span>
      </div>
    </div>
  )
}

function getRssiBars(rssi: number): boolean[] {
  const abs = Math.abs(rssi)
  return [abs <= 80, abs <= 65, abs <= 50, abs <= 35]
}
