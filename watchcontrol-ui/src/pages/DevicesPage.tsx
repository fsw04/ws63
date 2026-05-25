import { ChevronLeft, ChevronRight, Heart, Ruler, Weight, Activity, Droplets, User, Phone, Clock } from 'lucide-react'
import { useAppStore } from '@/store/appStore'

function VitalItem({ icon, label, value, unit, warn }: {
  icon: React.ReactNode
  label: string
  value: string
  unit: string
  warn?: boolean
}) {
  return (
    <div className={`vital-card ${warn ? 'border-accent-yellow/30' : ''}`}>
      <div className="flex items-center gap-1.5 mb-1">
        <div className={`w-5 h-5 rounded flex items-center justify-center ${warn ? 'bg-accent-yellow/15' : 'bg-accent-cyan/10'}`}>
          {icon}
        </div>
        <span className="text-[9px] text-gray-500">{label}</span>
      </div>
      <div className="flex items-baseline gap-1">
        <span className={`text-sm font-mono font-bold ${warn ? 'text-accent-yellow' : 'text-gray-200'}`}>
          {value}
        </span>
        <span className="text-[8px] text-gray-500">{unit}</span>
      </div>
    </div>
  )
}

function DeviceCard({ device }: { device: ReturnType<typeof useAppStore.getState>['devices'][0] }) {
  const bpParts = device.vitals.bloodPressure.split(' ')[0].split('/')
  const systolic = parseInt(bpParts[0])
  const bpWarn = systolic >= 140

  const glucoseVal = parseFloat(device.vitals.fastingBloodGlucose)
  const glucoseWarn = glucoseVal >= 6.1

  return (
    <div className="animate-slide-up">
      <div className="card mb-2">
        <div className="flex items-center justify-between mb-2">
          <div className="flex items-center gap-2">
            <div className={`w-8 h-8 rounded-full flex items-center justify-center ${device.connected ? 'bg-accent-green/15' : 'bg-accent-red/15'}`}>
              <User size={16} className={device.connected ? 'text-accent-green' : 'text-accent-red'} />
            </div>
            <div>
              <div className="text-sm font-medium text-gray-200">{device.name}</div>
              <div className="text-[9px] text-gray-500 font-mono">{device.deviceId}</div>
            </div>
          </div>
          <div className="flex items-center gap-1">
            <span className={`status-dot ${device.connected ? 'status-dot-online' : 'status-dot-offline'}`} />
            <span className={`text-[9px] ${device.connected ? 'text-accent-green' : 'text-accent-red'}`}>
              {device.connected ? '在线' : '离线'}
            </span>
          </div>
        </div>

        <div className="flex items-center gap-3 text-[9px] text-gray-500">
          <div className="flex items-center gap-1">
            <Phone size={9} />
            <span>{device.phone}</span>
          </div>
          <div className="flex items-center gap-1">
            <Clock size={9} />
            <span>{formatTime(device.lastUpdate)}</span>
          </div>
        </div>
      </div>

      <div className="grid grid-cols-2 gap-2">
        <VitalItem
          icon={<Heart size={11} className="text-accent-red" />}
          label="血压"
          value={device.vitals.bloodPressure.split(' ')[0]}
          unit="mmHg"
          warn={bpWarn}
        />
        <VitalItem
          icon={<Droplets size={11} className="text-accent-cyan" />}
          label="空腹血糖"
          value={device.vitals.fastingBloodGlucose.split(' ')[0]}
          unit="mmol/L"
          warn={glucoseWarn}
        />
        <VitalItem
          icon={<Ruler size={11} className="text-accent-green" />}
          label="身高"
          value={device.vitals.height.split(' ')[0]}
          unit="cm"
        />
        <VitalItem
          icon={<Weight size={11} className="text-accent-yellow" />}
          label="体重"
          value={device.vitals.weight.split(' ')[0]}
          unit="kg"
        />
      </div>

      <div className="vital-card mt-2 flex items-center justify-between">
        <div className="flex items-center gap-1.5">
          <div className="w-5 h-5 rounded flex items-center justify-center bg-accent-green/10">
            <Activity size={11} className="text-accent-green" />
          </div>
          <span className="text-[9px] text-gray-500">BMI</span>
        </div>
        <div className="flex items-baseline gap-1">
          <span className="text-sm font-mono font-bold text-gray-200">{device.vitals.bmi}</span>
          <span className="text-[8px] text-gray-500">kg/m²</span>
        </div>
      </div>

      <div className="mt-2 flex items-center justify-between px-1">
        <span className="text-[9px] text-gray-600">MQTT 上报</span>
        <div className="flex items-center gap-1">
          <span className={`status-dot ${device.connected ? 'status-dot-online' : 'status-dot-offline'}`} />
          <span className="text-[9px] font-mono text-gray-500">
            {device.connected ? 'watch/sensors/report' : '未上报'}
          </span>
        </div>
      </div>
    </div>
  )
}

export default function DevicesPage() {
  const { devices, currentDeviceIndex, setCurrentDeviceIndex } = useAppStore()
  const device = devices[currentDeviceIndex]

  if (!device) return null

  const canPrev = currentDeviceIndex > 0
  const canNext = currentDeviceIndex < devices.length - 1

  return (
    <div className="p-3 h-full flex flex-col">
      <div className="flex items-center justify-between mb-2">
        <button
          onClick={() => canPrev && setCurrentDeviceIndex(currentDeviceIndex - 1)}
          className={`w-7 h-7 rounded-lg flex items-center justify-center transition-colors ${canPrev ? 'bg-base-700 text-gray-400 hover:text-accent-green' : 'bg-base-800 text-gray-700 cursor-not-allowed'}`}
          disabled={!canPrev}
        >
          <ChevronLeft size={14} />
        </button>

        <div className="flex items-center gap-1.5">
          {devices.map((_, i) => (
            <button
              key={i}
              onClick={() => setCurrentDeviceIndex(i)}
              className={`w-1.5 h-1.5 rounded-full transition-all duration-300 ${
                i === currentDeviceIndex
                  ? 'w-4 bg-accent-green'
                  : 'bg-gray-600 hover:bg-gray-500'
              }`}
            />
          ))}
        </div>

        <button
          onClick={() => canNext && setCurrentDeviceIndex(currentDeviceIndex + 1)}
          className={`w-7 h-7 rounded-lg flex items-center justify-center transition-colors ${canNext ? 'bg-base-700 text-gray-400 hover:text-accent-green' : 'bg-base-800 text-gray-700 cursor-not-allowed'}`}
          disabled={!canNext}
        >
          <ChevronRight size={14} />
        </button>
      </div>

      <div className="flex-1 overflow-y-auto">
        <DeviceCard device={device} />
      </div>
    </div>
  )
}

function formatTime(ts: number): string {
  const d = new Date(ts)
  return `${d.getHours().toString().padStart(2, '0')}:${d.getMinutes().toString().padStart(2, '0')}`
}
