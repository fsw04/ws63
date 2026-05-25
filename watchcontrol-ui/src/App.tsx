import { BrowserRouter as Router, Routes, Route } from 'react-router-dom'
import Header from '@/components/Header'
import TabBar from '@/components/TabBar'
import StatusPage from '@/pages/StatusPage'
import DevicesPage from '@/pages/DevicesPage'
import LogsPage from '@/pages/LogsPage'
import { useAppStore } from '@/store/appStore'

function MainLayout() {
  const { activeTab } = useAppStore()

  return (
    <div className="w-[320px] h-[480px] bg-base-900 flex flex-col relative overflow-hidden">
      <Header />
      <div className="flex-1 overflow-y-auto pb-14">
        {activeTab === 'status' && <StatusPage />}
        {activeTab === 'devices' && <DevicesPage />}
        {activeTab === 'logs' && <LogsPage />}
      </div>
      <TabBar />
    </div>
  )
}

export default function App() {
  return (
    <Router>
      <Routes>
        <Route path="/*" element={<MainLayout />} />
      </Routes>
    </Router>
  )
}
