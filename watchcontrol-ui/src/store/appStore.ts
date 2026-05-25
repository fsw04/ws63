import { create } from 'zustand'

export type DeviceStatus = 'idle' | 'borrowed' | 'requested' | 'returning'

export interface WatchDevice {
  deviceId: string
  mac: string
  name: string
  connected: boolean
  status: DeviceStatus
  borrowedBy: string
  borrowedAt: number
}

export interface SystemStatus {
  wifi: {
    connected: boolean
    ssid: string
    ip: string
    signalStrength: number
  }
  mqtt: {
    connected: boolean
    broker: string
    lastPublish: number
    topic: string
  }
  sle: {
    broadcasting: boolean
    connectedDevices: number
    serverName: string
  }
}

export interface LogEntry {
  id: string
  timestamp: number
  type: 'sle' | 'mqtt' | 'wifi' | 'alert' | 'device'
  level: 'info' | 'warn' | 'error'
  message: string
}

export type TabType = 'status' | 'devices' | 'logs'

interface AppStore {
  systemStatus: SystemStatus
  devices: WatchDevice[]
  logs: LogEntry[]
  activeTab: TabType
  requestModalOpen: boolean
  returnModalOpen: boolean
  wifiModalOpen: boolean
  wifiConnecting: boolean
  selectedDeviceId: string | null
  setActiveTab: (tab: TabType) => void
  setRequestModalOpen: (open: boolean) => void
  setReturnModalOpen: (open: boolean) => void
  setWifiModalOpen: (open: boolean) => void
  setSelectedDeviceId: (id: string | null) => void
  addLog: (entry: Omit<LogEntry, 'id' | 'timestamp'>) => void
  requestDevice: (deviceId: string, borrower: string) => void
  returnDevice: (deviceId: string) => void
  connectWifi: (ssid: string, password: string) => void
  disconnectWifi: () => void
}

const mockDevices: WatchDevice[] = [
  {
    deviceId: 'watch_5200075C6713',
    mac: '52:00:07:5C:67:13',
    name: '手表-01',
    connected: true,
    status: 'idle',
    borrowedBy: '',
    borrowedAt: 0,
  },
  {
    deviceId: 'watch_A1B2C3D4E5F6',
    mac: 'A1:B2:C3:D4:E5:F6',
    name: '手表-02',
    connected: true,
    status: 'borrowed',
    borrowedBy: '李桂芳',
    borrowedAt: Date.now() - 86400000,
  },
  {
    deviceId: 'watch_7A8B9C0D1E2F',
    mac: '7A:8B:9C:0D:1E:2F',
    name: '手表-03',
    connected: false,
    status: 'idle',
    borrowedBy: '',
    borrowedAt: 0,
  },
  {
    deviceId: 'watch_D4E5F6A7B8C9',
    mac: 'D4:E5:F6:A7:B8:C9',
    name: '手表-04',
    connected: true,
    status: 'requested',
    borrowedBy: '张明远',
    borrowedAt: 0,
  },
]

const mockStatus: SystemStatus = {
  wifi: {
    connected: true,
    ssid: 'FSW',
    ip: '192.168.43.110',
    signalStrength: -42,
  },
  mqtt: {
    connected: true,
    broker: '192.168.43.110:1883',
    lastPublish: Date.now() - 5000,
    topic: 'watch/sensors/report',
  },
  sle: {
    broadcasting: true,
    connectedDevices: 3,
    serverName: 'sle_speed_server',
  },
}

const mockLogs: LogEntry[] = [
  {
    id: '1',
    timestamp: Date.now() - 60000,
    type: 'wifi',
    level: 'info',
    message: 'WiFi 连接成功, IP: 192.168.43.110',
  },
  {
    id: '2',
    timestamp: Date.now() - 55000,
    type: 'mqtt',
    level: 'info',
    message: 'MQTT 连接成功, Broker: 192.168.43.110:1883',
  },
  {
    id: '3',
    timestamp: Date.now() - 50000,
    type: 'sle',
    level: 'info',
    message: 'SLE 广播已开启, 等待设备连接...',
  },
  {
    id: '4',
    timestamp: Date.now() - 45000,
    type: 'sle',
    level: 'info',
    message: '设备 watch_5200075C6713 已连接',
  },
  {
    id: '5',
    timestamp: Date.now() - 40000,
    type: 'sle',
    level: 'info',
    message: '设备 watch_A1B2C3D4E5F6 已连接',
  },
  {
    id: '6',
    timestamp: Date.now() - 35000,
    type: 'device',
    level: 'info',
    message: '李桂芳 申请了 手表-02',
  },
  {
    id: '7',
    timestamp: Date.now() - 30000,
    type: 'sle',
    level: 'warn',
    message: '设备 watch_7A8B9C0D1E2F 连接超时',
  },
  {
    id: '8',
    timestamp: Date.now() - 25000,
    type: 'device',
    level: 'info',
    message: '张明远 申请了 手表-04，待审批',
  },
  {
    id: '9',
    timestamp: Date.now() - 20000,
    type: 'alert',
    level: 'error',
    message: '设备 watch_7A8B9C0D1E2F 离线',
  },
  {
    id: '10',
    timestamp: Date.now() - 10000,
    type: 'mqtt',
    level: 'info',
    message: '订阅 watch/commands 成功',
  },
]

export const useAppStore = create<AppStore>((set) => ({
  systemStatus: mockStatus,
  devices: mockDevices,
  logs: mockLogs,
  activeTab: 'status',
  requestModalOpen: false,
  returnModalOpen: false,
  wifiModalOpen: false,
  wifiConnecting: false,
  selectedDeviceId: null,
  setActiveTab: (tab) => set({ activeTab: tab }),
  setRequestModalOpen: (open) => set({ requestModalOpen: open }),
  setReturnModalOpen: (open) => set({ returnModalOpen: open }),
  setWifiModalOpen: (open) => set({ wifiModalOpen: open }),
  setSelectedDeviceId: (id) => set({ selectedDeviceId: id }),
  addLog: (entry) =>
    set((state) => ({
      logs: [
        ...state.logs,
        {
          ...entry,
          id: String(state.logs.length + 1),
          timestamp: Date.now(),
        },
      ],
    })),
  requestDevice: (deviceId, borrower) =>
    set((state) => ({
      devices: state.devices.map((d) =>
        d.deviceId === deviceId
          ? { ...d, status: 'borrowed' as DeviceStatus, borrowedBy: borrower, borrowedAt: Date.now() }
          : d
      ),
      requestModalOpen: false,
      selectedDeviceId: null,
    })),
  returnDevice: (deviceId) =>
    set((state) => ({
      devices: state.devices.map((d) =>
        d.deviceId === deviceId
          ? { ...d, status: 'idle' as DeviceStatus, borrowedBy: '', borrowedAt: 0 }
          : d
      ),
      returnModalOpen: false,
      selectedDeviceId: null,
    })),
  connectWifi: (ssid, password) => {
    set({ wifiConnecting: true })
    setTimeout(() => {
      set((state) => ({
        systemStatus: {
          ...state.systemStatus,
          wifi: {
            connected: true,
            ssid,
            ip: '192.168.43.' + Math.floor(Math.random() * 254 + 1),
            signalStrength: -Math.floor(Math.random() * 30 + 30),
          },
        },
        wifiConnecting: false,
        wifiModalOpen: false,
      }))
    }, 1500)
    set((state) => ({
      logs: [
        ...state.logs,
        {
          id: String(state.logs.length + 1),
          timestamp: Date.now(),
          type: 'wifi' as const,
          level: 'info' as const,
          message: `正在连接 WiFi: ${ssid}...`,
        },
      ],
    }))
    setTimeout(() => {
      set((state) => ({
        logs: [
          ...state.logs,
          {
            id: String(state.logs.length + 1),
            timestamp: Date.now(),
            type: 'wifi' as const,
            level: 'info' as const,
            message: `WiFi 连接成功, IP: ${ssid}`,
          },
        ],
      }))
    }, 1600)
  },
  disconnectWifi: () =>
    set((state) => ({
      systemStatus: {
        ...state.systemStatus,
        wifi: {
          connected: false,
          ssid: '',
          ip: '',
          signalStrength: 0,
        },
      },
      logs: [
        ...state.logs,
        {
          id: String(state.logs.length + 1),
          timestamp: Date.now(),
          type: 'wifi' as const,
          level: 'warn' as const,
          message: 'WiFi 已断开连接',
        },
      ],
    })),
}))
