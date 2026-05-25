import { create } from 'zustand'

export interface WatchDevice {
  deviceId: string
  mac: string
  name: string
  phone: string
  idCard: string
  connected: boolean
  lastUpdate: number
  vitals: {
    height: string
    weight: string
    bmi: string
    bloodPressure: string
    fastingBloodGlucose: string
  }
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
  type: 'sle' | 'mqtt' | 'wifi' | 'alert'
  level: 'info' | 'warn' | 'error'
  message: string
}

export type TabType = 'status' | 'devices' | 'logs'

interface AppStore {
  systemStatus: SystemStatus
  devices: WatchDevice[]
  logs: LogEntry[]
  activeTab: TabType
  currentDeviceIndex: number
  setActiveTab: (tab: TabType) => void
  setCurrentDeviceIndex: (index: number) => void
  addLog: (entry: Omit<LogEntry, 'id' | 'timestamp'>) => void
  updateDeviceVitals: (deviceId: string, vitals: WatchDevice['vitals']) => void
}

const mockDevices: WatchDevice[] = [
  {
    deviceId: 'watch_5200075C6713',
    mac: '52:00:07:5C:67:13',
    name: '李桂芳',
    phone: '13900000002',
    idCard: '110101196104082422',
    connected: true,
    lastUpdate: Date.now() - 30000,
    vitals: {
      height: '160 cm',
      weight: '55 kg',
      bmi: '21.5',
      bloodPressure: '120/80 mmHg',
      fastingBloodGlucose: '5.2 mmol/L',
    },
  },
  {
    deviceId: 'watch_A1B2C3D4E5F6',
    mac: 'A1:B2:C3:D4:E5:F6',
    name: '张明远',
    phone: '13900000003',
    idCard: '110101198503152315',
    connected: true,
    lastUpdate: Date.now() - 45000,
    vitals: {
      height: '172 cm',
      weight: '68 kg',
      bmi: '23.0',
      bloodPressure: '130/85 mmHg',
      fastingBloodGlucose: '5.8 mmol/L',
    },
  },
  {
    deviceId: 'watch_7A8B9C0D1E2F',
    mac: '7A:8B:9C:0D:1E:2F',
    name: '王秀英',
    phone: '13900000004',
    idCard: '110101193805201628',
    connected: false,
    lastUpdate: Date.now() - 300000,
    vitals: {
      height: '155 cm',
      weight: '48 kg',
      bmi: '20.0',
      bloodPressure: '145/92 mmHg',
      fastingBloodGlucose: '6.8 mmol/L',
    },
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
    connectedDevices: 2,
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
    type: 'mqtt',
    level: 'info',
    message: '发布数据到 watch/sensors/report',
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
    type: 'mqtt',
    level: 'info',
    message: '发布数据到 watch/sensors/report',
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
  currentDeviceIndex: 0,
  setActiveTab: (tab) => set({ activeTab: tab }),
  setCurrentDeviceIndex: (index) => set({ currentDeviceIndex: index }),
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
  updateDeviceVitals: (deviceId, vitals) =>
    set((state) => ({
      devices: state.devices.map((d) =>
        d.deviceId === deviceId
          ? { ...d, vitals, lastUpdate: Date.now() }
          : d
      ),
    })),
}))
