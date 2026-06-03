import HealthVitalsCard from '@/components/HealthVitalsCard'
import SleConnectionPanel from '@/components/SleConnectionPanel'
import DeviceInfoPanel from '@/components/DeviceInfoPanel'

export default function Home() {
  return (
    <div className="w-[480px] h-[320px] bg-[#0A0E1A] text-white overflow-hidden flex gap-2 p-2">
      {/* 左侧：健康体征 */}
      <div className="flex-1 min-w-0">
        <HealthVitalsCard />
      </div>

      {/* 右侧：连接状态 + 设备信息 */}
      <div className="w-[170px] flex flex-col gap-2 shrink-0">
        <div className="flex-1 min-h-0">
          <SleConnectionPanel />
        </div>
        <div className="flex-1 min-h-0">
          <DeviceInfoPanel />
        </div>
      </div>
    </div>
  )
}
