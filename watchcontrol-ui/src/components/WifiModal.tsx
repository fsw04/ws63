import { useState } from 'react'
import { Wifi, X, Eye, EyeOff, Loader2 } from 'lucide-react'
import { useAppStore } from '@/store/appStore'

export default function WifiModal() {
  const { wifiModalOpen, setWifiModalOpen, wifiConnecting, connectWifi, systemStatus } = useAppStore()
  const [ssid, setSsid] = useState('')
  const [password, setPassword] = useState('')
  const [showPassword, setShowPassword] = useState(false)

  if (!wifiModalOpen) return null

  const isEditing = systemStatus.wifi.connected
  const canConnect = ssid.trim().length > 0 && password.trim().length >= 8

  const handleConnect = () => {
    if (!canConnect || wifiConnecting) return
    connectWifi(ssid.trim(), password.trim())
    setSsid('')
    setPassword('')
    setShowPassword(false)
  }

  const handleClose = () => {
    if (wifiConnecting) return
    setWifiModalOpen(false)
    setSsid('')
    setPassword('')
    setShowPassword(false)
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
              <Wifi size={14} className="text-accent-green" />
            </div>
            <span className="text-sm font-medium text-gray-200">
              {isEditing ? '切换 WiFi' : '连接 WiFi'}
            </span>
          </div>
          <button
            onClick={handleClose}
            className="w-6 h-6 rounded-full bg-base-700 flex items-center justify-center hover:bg-base-600 transition-colors"
          >
            <X size={12} className="text-gray-400" />
          </button>
        </div>

        {isEditing && (
          <div className="mb-3 px-3 py-2 rounded-lg bg-base-700/40 border border-base-600/20">
            <span className="text-[9px] text-gray-500">当前连接</span>
            <div className="flex items-center gap-1.5 mt-0.5">
              <span className="status-dot status-dot-online" />
              <span className="text-[10px] font-mono text-gray-300">{systemStatus.wifi.ssid}</span>
              <span className="text-[9px] text-gray-600">{systemStatus.wifi.ip}</span>
            </div>
          </div>
        )}

        <div className="mb-3">
          <label className="text-[10px] text-gray-500 mb-1 block">网络名称 (SSID)</label>
          <div className="flex items-center gap-2 bg-base-700/60 rounded-lg px-3 py-2 border border-base-600/20 focus-within:border-accent-green/40 transition-colors">
            <Wifi size={12} className="text-gray-500 flex-shrink-0" />
            <input
              type="text"
              value={ssid}
              onChange={(e) => setSsid(e.target.value)}
              placeholder="输入 WiFi 名称"
              className="flex-1 bg-transparent text-[11px] text-gray-200 placeholder-gray-600 outline-none"
              autoFocus
            />
          </div>
        </div>

        <div className="mb-4">
          <label className="text-[10px] text-gray-500 mb-1 block">密码</label>
          <div className="flex items-center gap-2 bg-base-700/60 rounded-lg px-3 py-2 border border-base-600/20 focus-within:border-accent-green/40 transition-colors">
            <button
              onClick={() => setShowPassword(!showPassword)}
              className="text-gray-500 hover:text-gray-400 transition-colors flex-shrink-0"
            >
              {showPassword ? <EyeOff size={12} /> : <Eye size={12} />}
            </button>
            <input
              type={showPassword ? 'text' : 'password'}
              value={password}
              onChange={(e) => setPassword(e.target.value)}
              placeholder="输入 WiFi 密码"
              className="flex-1 bg-transparent text-[11px] text-gray-200 placeholder-gray-600 outline-none"
            />
          </div>
          {password.length > 0 && password.length < 8 && (
            <span className="text-[9px] text-accent-red mt-1 block">密码至少 8 位</span>
          )}
        </div>

        <button
          onClick={handleConnect}
          disabled={!canConnect || wifiConnecting}
          className={`w-full py-2.5 rounded-xl text-[11px] font-medium transition-all active:scale-[0.98] flex items-center justify-center gap-2 ${
            canConnect && !wifiConnecting
              ? 'bg-accent-green text-base-900 hover:brightness-110'
              : 'bg-base-700 text-gray-600 cursor-not-allowed'
          }`}
        >
          {wifiConnecting && <Loader2 size={12} className="animate-spin" />}
          {wifiConnecting ? '正在连接...' : '连接'}
        </button>
      </div>
    </div>
  )
}
