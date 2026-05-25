import { useState } from 'react'
import { Watch, X } from 'lucide-react'
import { useAppStore } from '@/store/appStore'

export default function AddDeviceModal() {
  const { addDeviceModalOpen, setAddDeviceModalOpen, addDevice, devices } = useAppStore()
  const [name, setName] = useState('')
  const [mac, setMac] = useState('')

  if (!addDeviceModalOpen) return null

  const isValidMac = (str: string) => {
    const macPattern = /^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$/
    return macPattern.test(str)
  }

  const normalizeMac = (str: string) => str.toUpperCase().replace(/[-]/g, ':')

  const isValidName = name.trim().length > 0
  const normalizedMac = normalizeMac(mac.trim())
  const canAdd = isValidName && isValidMac(normalizedMac) &&
    !devices.some(d => d.mac === normalizedMac)

  const handleAdd = () => {
    if (!canAdd) return
    addDevice(name.trim(), normalizedMac)
    setName('')
    setMac('')
  }

  const handleClose = () => {
    setAddDeviceModalOpen(false)
    setName('')
    setMac('')
  }

  return (
    <div className="absolute inset-0 z-50 flex items-end justify-center" onClick={handleClose}>
      <div className="absolute inset-0 bg-black/60 backdrop-blur-sm" />
      <div
        className="relative w-full bg-base-800 rounded-t-2xl p-4 animate-slide-up border-t border-base-600/30"
        onClick={(e) => e.stopPropagation()}
      >
        <div className="flex items-center justify-between mb-3">
          <div className="flex items-center gap-2">
          <div className="w-7 h-7 rounded-lg bg-accent-green/15 flex items-center justify-center">
            <Watch size={14} className="text-accent-green" />
          </div>
          <span className="text-sm font-medium text-gray-200">添加设备</span>
        </div>
        <button
          onClick={handleClose}
          className="w-6 h-6 rounded-full bg-base-700 flex items-center justify-center hover:bg-base-600 transition-colors"
        >
          <X size={12} className="text-gray-400" />
        </button>
      </div>

        <div className="mb-3">
          <label className="text-[10px] text-gray-500 mb-1 block">设备名称</label>
          <div className="flex items-center gap-2 bg-base-700/60 rounded-lg px-3 py-2 border border-base-600/20 focus-within:border-accent-green/40 transition-colors">
            <Watch size={12} className="text-gray-500 flex-shrink-0" />
            <input
              type="text"
              value={name}
              onChange={(e) => setName(e.target.value)}
              placeholder="例如：手表-05"
              className="flex-1 bg-transparent text-[11px] text-gray-200 placeholder-gray-600 outline-none"
              autoFocus
            />
          </div>
        </div>

        <div className="mb-4">
          <label className="text-[10px] text-gray-500 mb-1 block">MAC 地址</label>
          <div className="flex items-center gap-2 bg-base-700/60 rounded-lg px-3 py-2 border border-base-600/20 focus-within:border-accent-green/40 transition-colors">
            <div className="w-3 h-3 rounded bg-gray-600 flex items-center justify-center">
              <span className="text-[8px] font-mono text-gray-400">MAC</span>
            </div>
            <input
              type="text"
              value={mac}
              onChange={(e) => setMac(e.target.value)}
              placeholder="例如：52:00:07:5C:67:13"
              className="flex-1 bg-transparent text-[11px] text-gray-200 placeholder-gray-600 outline-none"
            />
          </div>
          {mac.length > 0 && !isValidMac(normalizedMac) && (
            <span className="text-[9px] text-accent-red mt-1 block">MAC 地址格式不正确</span>
          )}
          {devices.some(d => d.mac === normalizedMac) && (
            <span className="text-[9px] text-accent-yellow mt-1 block">该设备已存在</span>
          )}
        </div>

        <button
          onClick={handleAdd}
          disabled={!canAdd}
          className={`w-full py-2.5 rounded-xl text-[11px] font-medium transition-all active:scale-[0.98] flex items-center justify-center ${
            canAdd
              ? 'bg-accent-green text-base-900 hover:brightness-110'
              : 'bg-base-700 text-gray-600 cursor-not-allowed'
          }`}
        >
          添加设备
        </button>
      </div>
    </div>
  )
}
