import { useEffect } from 'react'
import { Watch, X, RefreshCw, Smartphone, Radio } from 'lucide-react'
import { useAppStore } from '@/store/appStore'

function getSignalStrengthColor(rssi: number) {
  if (rssi >= -50) return 'text-accent-green'
  if (rssi >= -70) return 'text-accent-yellow'
  return 'text-accent-red'
}

function getSignalStrengthLevel(rssi: number) {
  if (rssi >= -50) return 4
  if (rssi >= -60) return 3
  if (rssi >= -75) return 2
  return 1
}

function SignalIcon({ rssi }: { rssi: number }) {
  const color = getSignalStrengthColor(rssi)
  const level = getSignalStrengthLevel(rssi)

  return (
    <div className="flex items-end gap-0.5 h-3">
      {[1, 2, 3, 4].map((i) => (
        <div
          key={i}
          className={`w-0.5 rounded-t-sm ${i <= level ? color : 'text-gray-700'} bg-current`}
          style={{ height: `${3 + i * 2}px` }}
        />
      ))}
    </div>
  )
}

function ScanningAnimation() {
  return (
    <div className="relative w-20 h-20 mx-auto">
      <div className="absolute inset-0 flex items-center justify-center">
        <div className="w-8 h-8 rounded-full bg-accent-green/30 flex items-center justify-center">
          <Watch size={18} className="text-accent-green" />
        </div>
      </div>
      <div className="absolute inset-0 animate-ping">
        <div className="w-full h-full rounded-full border-2 border-accent-green/30" />
      </div>
      <div className="absolute inset-2 animate-ping" style={{ animationDelay: '0.3s' }}>
        <div className="w-full h-full rounded-full border-2 border-accent-green/20" />
      </div>
      <div className="absolute inset-4 animate-ping" style={{ animationDelay: '0.6s' }}>
        <div className="w-full h-full rounded-full border-2 border-accent-green/10" />
      </div>
    </div>
  )
}

export default function AddDeviceModal() {
  const { 
    addDeviceModalOpen, 
    setAddDeviceModalOpen, 
    discoveredDevices, 
    sleScanning, 
    startSleScan, 
    stopSleScan,
    connectDiscoveredDevice,
    devices 
  } = useAppStore()

  useEffect(() => {
    if (addDeviceModalOpen && !sleScanning) {
      startSleScan()
    }
    return () => {
      if (sleScanning) {
        stopSleScan()
      }
    }
  }, [addDeviceModalOpen, sleScanning, startSleScan, stopSleScan])

  if (!addDeviceModalOpen) return null

  const handleClose = () => {
    setAddDeviceModalOpen(false)
  }

  const handleConnect = (mac: string) => {
    connectDiscoveredDevice(mac)
  }

  const existingDeviceMacs = devices.map(d => d.mac)

  return (
    <div className="absolute inset-0 z-50 flex items-end justify-center" onClick={handleClose}>
      <div className="absolute inset-0 bg-black/60 backdrop-blur-sm" />
      <div
        className="relative w-full bg-base-800 rounded-t-2xl animate-slide-up border-t border-base-600/30 flex flex-col"
        style={{ maxHeight: '75%' }}
        onClick={(e) => e.stopPropagation()}
      >
        <div className="flex items-center justify-between p-3 border-b border-base-600/20">
          <div className="flex items-center gap-2">
            <div className={`w-7 h-7 rounded-lg flex items-center justify-center ${sleScanning ? 'bg-accent-green/15' : 'bg-base-700'}`}>
              <Radio size={14} className={sleScanning ? 'text-accent-green' : 'text-gray-500'} />
            </div>
            <span className="text-sm font-medium text-gray-200">SLE 设备扫描</span>
          </div>
          <div className="flex items-center gap-2">
            <button
              onClick={sleScanning ? stopSleScan : startSleScan}
              className="w-7 h-7 rounded-lg bg-base-700 flex items-center justify-center hover:bg-base-600 transition-colors"
              title={sleScanning ? '停止扫描' : '开始扫描'}
            >
              <RefreshCw size={12} className={`text-gray-400 ${sleScanning ? 'animate-spin' : ''}`} />
            </button>
            <button
              onClick={handleClose}
              className="w-7 h-7 rounded-full bg-base-700 flex items-center justify-center hover:bg-base-600 transition-colors"
            >
              <X size={12} className="text-gray-400" />
            </button>
          </div>
        </div>

        <div className="flex-1 overflow-y-auto p-3">
          {discoveredDevices.length === 0 ? (
            <div className="flex flex-col items-center justify-center py-8">
              {sleScanning ? (
                <>
                  <ScanningAnimation />
                  <p className="text-[11px] text-gray-500 mt-4">正在扫描附近的SLE设备...</p>
                </>
              ) : (
                <>
                  <Smartphone size={32} className="text-gray-700 mb-3" />
                  <p className="text-[11px] text-gray-500 mb-3">未发现设备</p>
                  <button
                    onClick={startSleScan}
                    className="px-4 py-1.5 rounded-lg bg-accent-green/10 text-[11px] text-accent-green hover:bg-accent-green/20 transition-colors"
                  >
                    重新扫描
                  </button>
                </>
              )}
            </div>
          ) : (
            <div className="space-y-2">
              {discoveredDevices.map((device) => {
                const isAlreadyAdded = existingDeviceMacs.includes(device.mac)
                return (
                  <div
                    key={device.mac}
                    className="card flex items-center justify-between gap-2.5"
                  >
                    <div className="flex items-center gap-2.5">
                      <div className="w-8 h-8 rounded-lg bg-accent-green/10 flex items-center justify-center">
                        <Watch size={16} className="text-accent-green" />
                      </div>
                      <div>
                        <div className="flex items-center gap-2">
                          <span className="text-[11px] font-medium text-gray-200">{device.name}</span>
                          {isAlreadyAdded && (
                            <span className="text-[9px] px-1.5 py-0.5 rounded-full bg-accent-cyan/15 text-accent-cyan">
                              已添加
                            </span>
                          )}
                        </div>
                        <div className="flex items-center gap-3 mt-0.5">
                          <span className="text-[9px] text-gray-600 font-mono">{device.mac}</span>
                          <div className="flex items-center gap-1">
                            <SignalIcon rssi={device.rssi} />
                            <span className={`text-[9px] ${getSignalStrengthColor(device.rssi)}`}>
                              {device.rssi} dBm
                            </span>
                          </div>
                        </div>
                      </div>
                    </div>
                    <button
                      onClick={() => handleConnect(device.mac)}
                      disabled={isAlreadyAdded}
                      className={`px-3 py-1.5 rounded-lg text-[10px] font-medium transition-all active:scale-95 ${
                        isAlreadyAdded
                          ? 'bg-base-700 text-gray-600 cursor-not-allowed'
                          : 'bg-accent-green text-base-900 hover:brightness-110'
                      }`}
                    >
                      {isAlreadyAdded ? '已添加' : '连接'}
                    </button>
                  </div>
                )
              })}
            </div>
          )}
        </div>

        <div className="p-3 border-t border-base-600/20">
          <div className="flex items-center justify-between text-[9px] text-gray-600">
            <div className="flex items-center gap-3">
              <div className="flex items-center gap-1">
                <span className="w-1.5 h-1.5 rounded-full bg-accent-green" />
                <span>强</span>
              </div>
              <div className="flex items-center gap-1">
                <span className="w-1.5 h-1.5 rounded-full bg-accent-yellow" />
                <span>中</span>
              </div>
              <div className="flex items-center gap-1">
                <span className="w-1.5 h-1.5 rounded-full bg-accent-red" />
                <span>弱</span>
              </div>
            </div>
            <span>
              发现 {discoveredDevices.length} 个设备
            </span>
          </div>
        </div>
      </div>
    </div>
  )
}
