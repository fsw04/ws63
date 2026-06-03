import { create } from 'zustand'

export interface HealthVitals {
  name: string
  phone: string
  idCard: string
  height: string
  weight: string
  bmi: string
  bloodPressure: string
  fastingBloodGlucose: string
}

export interface SleStatus {
  connected: boolean
  gatewayMac: string
  rssi: number
  lastSendTime: string
  lastSendSuccess: boolean
}

export interface DeviceInfo {
  deviceId: string
  mac: string
  uptime: number
}

interface AppState {
  healthVitals: HealthVitals
  sleStatus: SleStatus
  deviceInfo: DeviceInfo

  updateHealthVitals: (v: Partial<HealthVitals>) => void
  updateSleStatus: (s: Partial<SleStatus>) => void
  updateDeviceInfo: (d: Partial<DeviceInfo>) => void
  tickUptime: () => void
}

const useAppStore = create<AppState>((set) => ({
  healthVitals: {
    name: '李桂芳',
    phone: '13900000002',
    idCard: '3101**********1234',
    height: '160',
    weight: '55',
    bmi: '21.5',
    bloodPressure: '120/80',
    fastingBloodGlucose: '5.2',
  },
  sleStatus: {
    connected: true,
    gatewayMac: 'A1:B2:C3:D4:E5:F6',
    rssi: -45,
    lastSendTime: '10:30:25',
    lastSendSuccess: true,
  },
  deviceInfo: {
    deviceId: 'watch_A1B2C3',
    mac: 'A1B2C3D4E5F6',
    uptime: 930,
  },

  updateHealthVitals: (v) =>
    set((s) => ({ healthVitals: { ...s.healthVitals, ...v } })),
  updateSleStatus: (s) =>
    set((state) => ({ sleStatus: { ...state.sleStatus, ...s } })),
  updateDeviceInfo: (d) =>
    set((s) => ({ deviceInfo: { ...s.deviceInfo, ...d } })),
  tickUptime: () =>
    set((s) => ({ deviceInfo: { ...s.deviceInfo, uptime: s.deviceInfo.uptime + 1 } })),
}))

export default useAppStore
