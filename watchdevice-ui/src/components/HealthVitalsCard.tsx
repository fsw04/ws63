import useAppStore from '@/store/appStore'
import { Heart, Droplets, Ruler, Weight, Activity } from 'lucide-react'

export default function HealthVitalsCard() {
  const vitals = useAppStore((s) => s.healthVitals)

  return (
    <div className="flex flex-col h-full gap-2">
      {/* 姓名 + 电话 */}
      <div className="flex items-center gap-2 px-3 py-1.5 rounded-lg bg-[#111827]/80 border border-[#1E3A5F]/40">
        <div className="w-6 h-6 rounded-full bg-[#00E5A0]/20 flex items-center justify-center">
          <span className="text-[10px] text-[#00E5A0] font-bold">{vitals.name[0]}</span>
        </div>
        <span className="text-[#E0E7FF] text-xs font-medium">{vitals.name}</span>
        <span className="text-[#6B7FA3] text-[10px] ml-auto font-mono">{vitals.phone}</span>
      </div>

      {/* 血压大数字 */}
      <div className="flex-1 flex flex-col justify-center px-3 py-2 rounded-lg bg-[#111827]/80 border border-[#1E3A5F]/40">
        <div className="flex items-center gap-1.5 mb-1">
          <Heart size={12} className="text-[#FF4D6A]" />
          <span className="text-[#6B7FA3] text-[10px]">血压</span>
        </div>
        <div className="flex items-baseline gap-1">
          <span className="text-[#E0E7FF] text-2xl font-bold font-mono tracking-tight">
            {vitals.bloodPressure}
          </span>
          <span className="text-[#6B7FA3] text-[10px]">mmHg</span>
        </div>
      </div>

      {/* 2×2 网格 */}
      <div className="grid grid-cols-2 gap-2">
        <MetricCard
          icon={<Droplets size={10} className="text-[#00D4FF]" />}
          label="血糖"
          value={vitals.fastingBloodGlucose}
          unit="mmol/L"
        />
        <MetricCard
          icon={<Ruler size={10} className="text-[#00E5A0]" />}
          label="身高"
          value={vitals.height}
          unit="cm"
        />
        <MetricCard
          icon={<Weight size={10} className="text-[#FFB020]" />}
          label="体重"
          value={vitals.weight}
          unit="kg"
        />
        <MetricCard
          icon={<Activity size={10} className="text-[#A78BFA]" />}
          label="BMI"
          value={vitals.bmi}
          unit=""
        />
      </div>
    </div>
  )
}

function MetricCard({
  icon,
  label,
  value,
  unit,
}: {
  icon: React.ReactNode
  label: string
  value: string
  unit: string
}) {
  return (
    <div className="flex flex-col px-2 py-1.5 rounded-lg bg-[#111827]/80 border border-[#1E3A5F]/40">
      <div className="flex items-center gap-1 mb-0.5">
        {icon}
        <span className="text-[#6B7FA3] text-[9px]">{label}</span>
      </div>
      <div className="flex items-baseline gap-0.5">
        <span className="text-[#E0E7FF] text-sm font-bold font-mono">{value}</span>
        {unit && <span className="text-[#6B7FA3] text-[8px]">{unit}</span>}
      </div>
    </div>
  )
}
