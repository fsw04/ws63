import { Wifi, Radio, Cloud, Activity } from 'lucide-react'
import { useAppStore, type TabType } from '@/store/appStore'

const tabs: { key: TabType; label: string; icon: React.ReactNode }[] = [
  { key: 'status', label: '状态', icon: <Wifi size={16} /> },
  { key: 'devices', label: '设备', icon: <Activity size={16} /> },
  { key: 'logs', label: '日志', icon: <Radio size={16} /> },
]

export default function TabBar() {
  const { activeTab, setActiveTab, systemStatus } = useAppStore()

  const onlineCount = systemStatus.sle.connectedDevices

  return (
    <div className="absolute bottom-0 left-0 right-0 h-14 bg-base-800/95 backdrop-blur-sm border-t border-base-600/30 flex items-center justify-around px-2">
      {tabs.map((tab) => {
        const isActive = activeTab === tab.key
        const showBadge = tab.key === 'devices' && onlineCount > 0

        return (
          <button
            key={tab.key}
            onClick={() => setActiveTab(tab.key)}
            className={`
              flex flex-col items-center justify-center gap-0.5 w-16 h-11 rounded-lg
              transition-all duration-200 relative
              ${isActive ? 'tab-active' : 'tab-inactive hover:text-gray-400'}
            `}
          >
            {isActive && (
              <div className="absolute top-0 left-1/2 -translate-x-1/2 w-6 h-0.5 bg-accent-green rounded-full" />
            )}
            <div className="relative">
              {tab.icon}
              {showBadge && (
                <span className="absolute -top-1.5 -right-2 w-3.5 h-3.5 bg-accent-green rounded-full text-[8px] font-mono font-bold text-base-900 flex items-center justify-center">
                  {onlineCount}
                </span>
              )}
            </div>
            <span className="text-[10px] font-medium">{tab.label}</span>
          </button>
        )
      })}

      <button
        onClick={() => setActiveTab('status')}
        className={`
          flex flex-col items-center justify-center gap-0.5 w-16 h-11 rounded-lg
          transition-all duration-200
          ${activeTab === 'status' ? 'tab-active' : 'tab-inactive'}
        `}
      >
        <Cloud size={16} />
        <span className="text-[10px] font-medium">云端</span>
      </button>
    </div>
  )
}
