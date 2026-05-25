import { useState } from 'react'
import { Watch, ArrowDownToLine, ArrowUpFromLine, X, User } from 'lucide-react'
import { useAppStore, type DeviceStatus } from '@/store/appStore'

const statusConfig: Record<DeviceStatus, { label: string; color: string; bgColor: string }> = {
  idle: { label: '空闲', color: 'text-accent-green', bgColor: 'bg-accent-green/15' },
  borrowed: { label: '已借出', color: 'text-accent-yellow', bgColor: 'bg-accent-yellow/15' },
  requested: { label: '申请中', color: 'text-accent-cyan', bgColor: 'bg-accent-cyan/15' },
  returning: { label: '归还中', color: 'text-accent-cyan', bgColor: 'bg-accent-cyan/15' },
}

function DeviceRow({ device }: { device: ReturnType<typeof useAppStore.getState>['devices'][0] }) {
  const { setRequestModalOpen, setReturnModalOpen, setSelectedDeviceId } = useAppStore()
  const config = statusConfig[device.status]

  const handleRequest = () => {
    setSelectedDeviceId(device.deviceId)
    setRequestModalOpen(true)
  }

  const handleReturn = () => {
    setSelectedDeviceId(device.deviceId)
    setReturnModalOpen(true)
  }

  const canRequest = device.status === 'idle' && device.connected
  const canReturn = device.status === 'borrowed'

  return (
    <div className="card flex items-center gap-2.5 py-2.5">
      <div className={`w-8 h-8 rounded-lg flex items-center justify-center ${device.connected ? 'bg-accent-green/10' : 'bg-gray-700/50'}`}>
        <Watch size={16} className={device.connected ? 'text-accent-green' : 'text-gray-600'} />
      </div>

      <div className="flex-1 min-w-0">
        <div className="flex items-center gap-1.5">
          <span className="text-[11px] font-medium text-gray-200 truncate">{device.name}</span>
          <span className={`text-[8px] px-1.5 py-0.5 rounded-full font-medium ${config.color} ${config.bgColor}`}>
            {config.label}
          </span>
        </div>
        <div className="flex items-center gap-2 mt-0.5">
          <div className="flex items-center gap-1">
            <span className={`status-dot ${device.connected ? 'status-dot-online' : 'status-dot-offline'}`} />
            <span className="text-[9px] text-gray-500">{device.connected ? '已连接' : '离线'}</span>
          </div>
          {device.borrowedBy && (
            <span className="text-[9px] text-gray-600">借用人: {device.borrowedBy}</span>
          )}
        </div>
      </div>

      <div className="flex gap-1.5 flex-shrink-0">
        {canRequest && (
          <button
            onClick={handleRequest}
            className="w-8 h-8 rounded-lg bg-accent-green/10 flex items-center justify-center hover:bg-accent-green/20 transition-colors active:scale-95"
            title="申请设备"
          >
            <ArrowDownToLine size={14} className="text-accent-green" />
          </button>
        )}
        {canReturn && (
          <button
            onClick={handleReturn}
            className="w-8 h-8 rounded-lg bg-accent-yellow/10 flex items-center justify-center hover:bg-accent-yellow/20 transition-colors active:scale-95"
            title="返还设备"
          >
            <ArrowUpFromLine size={14} className="text-accent-yellow" />
          </button>
        )}
      </div>
    </div>
  )
}

function RequestModal() {
  const { requestModalOpen, setRequestModalOpen, selectedDeviceId, devices, requestDevice, addLog } = useAppStore()
  const [borrower, setBorrower] = useState('')

  if (!requestModalOpen) return null

  const device = devices.find((d) => d.deviceId === selectedDeviceId)
  if (!device) return null

  const handleConfirm = () => {
    if (!borrower.trim()) return
    requestDevice(device.deviceId, borrower.trim())
    addLog({
      type: 'device',
      level: 'info',
      message: `${borrower.trim()} 申请了 ${device.name}`,
    })
    setBorrower('')
  }

  const handleClose = () => {
    setRequestModalOpen(false)
    setBorrower('')
  }

  return (
    <div className="absolute inset-0 z-50 flex items-end justify-center" onClick={handleClose}>
      <div className="absolute inset-0 bg-black/60 backdrop-blur-sm" />
      <div
        className="relative w-full bg-base-800 rounded-t-2xl p-4 animate-slide-up border-t border-base-600/30"
        onClick={(e) => e.stopPropagation()}
      >
        <div className="flex items-center justify-between mb-3">
          <span className="text-sm font-medium text-gray-200">申请设备</span>
          <button onClick={handleClose} className="w-6 h-6 rounded-full bg-base-700 flex items-center justify-center hover:bg-base-600 transition-colors">
            <X size={12} className="text-gray-400" />
          </button>
        </div>

        <div className="card mb-3 flex items-center gap-2.5">
          <div className="w-7 h-7 rounded-lg bg-accent-green/10 flex items-center justify-center">
            <Watch size={14} className="text-accent-green" />
          </div>
          <div>
            <div className="text-[11px] font-medium text-gray-300">{device.name}</div>
            <div className="text-[9px] text-gray-500 font-mono">{device.deviceId}</div>
          </div>
        </div>

        <div className="mb-3">
          <label className="text-[10px] text-gray-500 mb-1 block">借用人姓名</label>
          <div className="flex items-center gap-2 bg-base-700/60 rounded-lg px-3 py-2 border border-base-600/20 focus-within:border-accent-green/40 transition-colors">
            <User size={12} className="text-gray-500 flex-shrink-0" />
            <input
              type="text"
              value={borrower}
              onChange={(e) => setBorrower(e.target.value)}
              placeholder="请输入姓名"
              className="flex-1 bg-transparent text-[11px] text-gray-200 placeholder-gray-600 outline-none"
              autoFocus
            />
          </div>
        </div>

        <button
          onClick={handleConfirm}
          disabled={!borrower.trim()}
          className={`w-full py-2.5 rounded-xl text-[11px] font-medium transition-all active:scale-[0.98] ${
            borrower.trim()
              ? 'bg-accent-green text-base-900 hover:brightness-110'
              : 'bg-base-700 text-gray-600 cursor-not-allowed'
          }`}
        >
          确认申请
        </button>
      </div>
    </div>
  )
}

function ReturnModal() {
  const { returnModalOpen, setReturnModalOpen, selectedDeviceId, devices, returnDevice, addLog } = useAppStore()

  if (!returnModalOpen) return null

  const device = devices.find((d) => d.deviceId === selectedDeviceId)
  if (!device) return null

  const handleConfirm = () => {
    const borrowerName = device.borrowedBy
    returnDevice(device.deviceId)
    addLog({
      type: 'device',
      level: 'info',
      message: `${borrowerName} 返还了 ${device.name}`,
    })
  }

  const handleClose = () => {
    setReturnModalOpen(false)
  }

  return (
    <div className="absolute inset-0 z-50 flex items-end justify-center" onClick={handleClose}>
      <div className="absolute inset-0 bg-black/60 backdrop-blur-sm" />
      <div
        className="relative w-full bg-base-800 rounded-t-2xl p-4 animate-slide-up border-t border-base-600/30"
        onClick={(e) => e.stopPropagation()}
      >
        <div className="flex items-center justify-between mb-3">
          <span className="text-sm font-medium text-gray-200">返还设备</span>
          <button onClick={handleClose} className="w-6 h-6 rounded-full bg-base-700 flex items-center justify-center hover:bg-base-600 transition-colors">
            <X size={12} className="text-gray-400" />
          </button>
        </div>

        <div className="card mb-3 flex items-center gap-2.5">
          <div className="w-7 h-7 rounded-lg bg-accent-yellow/10 flex items-center justify-center">
            <Watch size={14} className="text-accent-yellow" />
          </div>
          <div>
            <div className="text-[11px] font-medium text-gray-300">{device.name}</div>
            <div className="text-[9px] text-gray-500">借用人: {device.borrowedBy}</div>
          </div>
        </div>

        <p className="text-[10px] text-gray-500 mb-3">
          确认返还该设备？设备状态将恢复为空闲。
        </p>

        <div className="flex gap-2">
          <button
            onClick={handleClose}
            className="flex-1 py-2.5 rounded-xl text-[11px] font-medium bg-base-700 text-gray-400 hover:bg-base-600 transition-colors active:scale-[0.98]"
          >
            取消
          </button>
          <button
            onClick={handleConfirm}
            className="flex-1 py-2.5 rounded-xl text-[11px] font-medium bg-accent-yellow text-base-900 hover:brightness-110 transition-all active:scale-[0.98]"
          >
            确认返还
          </button>
        </div>
      </div>
    </div>
  )
}

export default function DevicesPage() {
  const { devices } = useAppStore()

  const idleCount = devices.filter((d) => d.status === 'idle').length
  const borrowedCount = devices.filter((d) => d.status === 'borrowed').length
  const requestedCount = devices.filter((d) => d.status === 'requested').length

  return (
    <div className="p-3 h-full flex flex-col relative">
      <div className="flex items-center justify-between mb-2">
        <span className="text-xs font-medium text-gray-400">设备列表</span>
        <div className="flex items-center gap-2">
          <span className="text-[9px] text-accent-green">{idleCount} 空闲</span>
          <span className="text-[9px] text-accent-yellow">{borrowedCount} 借出</span>
          {requestedCount > 0 && (
            <span className="text-[9px] text-accent-cyan">{requestedCount} 申请中</span>
          )}
        </div>
      </div>

      <div className="flex-1 overflow-y-auto space-y-2">
        {devices.map((device) => (
          <DeviceRow key={device.deviceId} device={device} />
        ))}
      </div>

      <RequestModal />
      <ReturnModal />
    </div>
  )
}
