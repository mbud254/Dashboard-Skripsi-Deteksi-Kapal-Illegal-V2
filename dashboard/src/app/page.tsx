"use client";

import { useEffect, useMemo, useRef, useState } from "react";
import {
  Activity,
  Anchor,
  Battery,
  MapPin,
  Radio,
  Signal,
  Volume2,
  Waves,
  Wifi,
} from "lucide-react";
import {
  Bar,
  BarChart,
  CartesianGrid,
  Cell,
  Legend,
  Line,
  LineChart,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from "recharts";
import { Badge } from "@/components/ui/badge";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { cn } from "@/lib/utils";

// ——— Types & constants ———

type MonitoringStatus = "AMAN" | "INDIKASI KAPAL" | "ILLEGAL FISHING DETECTED";

interface SplPoint {
  time: string;
  spl: number;
  ema: number;
}

interface LoraPoint {
  time: string;
  rssi: number;
  snr: number;
}

interface FftBand {
  frequency: string;
  energy: number;
  hz: number;
}

const FFT_FREQUENCIES = [50, 100, 150, 200, 250, 300, 350, 400, 450, 500];
const FFT_ENERGY_THRESHOLD = 72;
const MAX_HISTORY = 36;
const EMA_ALPHA = 0.25;

const GPS_COORDS = { lat: -6.123, lng: 106.456 };

// ——— Helpers ———

function formatTime(date: Date): string {
  return date.toLocaleTimeString("id-ID", {
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
  });
}

function computeEma(current: number, previousEma: number): number {
  return EMA_ALPHA * current + (1 - EMA_ALPHA) * previousEma;
}

function evaluateStatus(
  spl: number,
  fftBands: FftBand[]
): MonitoringStatus {
  const lowFreqSpike = fftBands
    .filter((b) => b.hz <= 200)
    .some((b) => b.energy > FFT_ENERGY_THRESHOLD);

  if (spl >= 78 && lowFreqSpike) {
    return "ILLEGAL FISHING DETECTED";
  }
  if (spl >= 62 || lowFreqSpike) {
    return "INDIKASI KAPAL";
  }
  return "AMAN";
}

function statusVariant(
  status: MonitoringStatus
): "safe" | "warning" | "danger" {
  switch (status) {
    case "AMAN":
      return "safe";
    case "INDIKASI KAPAL":
      return "warning";
    case "ILLEGAL FISHING DETECTED":
      return "danger";
  }
}

function clamp(value: number, min: number, max: number): number {
  return Math.min(max, Math.max(min, value));
}

// Simulates boat approach: SPL rises, low-frequency FFT energy grows over ticks
function generateMockSnapshot(tick: number) {
  const approach = Math.min(1, tick / 45);
  const noise = () => (Math.random() - 0.5) * 2;

  const spl = clamp(48 + approach * 38 + noise() * 3, 42, 92);
  const rssi = clamp(-88 + approach * 10 + noise() * 2, -95, -68);
  const snr = clamp(6 + approach * 6 + noise(), 4, 14);
  const pdr = clamp(97 - approach * 4 + noise() * 0.5, 88, 99.5);
  const battery = clamp(87 - tick * 0.02, 62, 100);

  const fftBands: FftBand[] = FFT_FREQUENCIES.map((hz) => {
    const lowFreqBoost = hz <= 200 ? (1 - hz / 250) * approach * 55 : 0;
    const base = 18 + Math.random() * 12;
    const energy = clamp(base + lowFreqBoost + noise() * 4, 5, 98);
    return {
      frequency: `${hz}Hz`,
      energy: Math.round(energy * 10) / 10,
      hz,
    };
  });

  return { spl, rssi, snr, pdr, battery, fftBands };
}

// ——— Sub-components ———

function SummaryCard({
  title,
  value,
  unit,
  icon: Icon,
  accent = "cyan",
}: {
  title: string;
  value: string | number;
  unit: string;
  icon: React.ComponentType<{ className?: string }>;
  accent?: "cyan" | "emerald" | "violet" | "amber";
}) {
  const accentMap = {
    cyan: "text-cyan-400 bg-cyan-500/10 border-cyan-500/20",
    emerald: "text-emerald-400 bg-emerald-500/10 border-emerald-500/20",
    violet: "text-violet-400 bg-violet-500/10 border-violet-500/20",
    amber: "text-amber-400 bg-amber-500/10 border-amber-500/20",
  };

  return (
    <Card className="overflow-hidden">
      <CardContent className="p-5">
        <div className="flex items-start justify-between">
          <div>
            <p className="text-xs font-medium uppercase tracking-wider text-slate-400">
              {title}
            </p>
            <p className="mt-2 font-mono text-3xl font-bold text-white tabular-nums">
              {value}
              <span className="ml-1 text-lg font-normal text-slate-400">
                {unit}
              </span>
            </p>
          </div>
          <div
            className={cn(
              "rounded-lg border p-2.5",
              accentMap[accent]
            )}
          >
            <Icon className="h-5 w-5" />
          </div>
        </div>
      </CardContent>
    </Card>
  );
}

function BatteryIndicator({ percent }: { percent: number }) {
  const level =
    percent > 60 ? "high" : percent > 30 ? "mid" : "low";
  const color =
    level === "high"
      ? "text-emerald-400"
      : level === "mid"
        ? "text-amber-400"
        : "text-red-400";

  return (
    <div className="flex items-center gap-3 rounded-lg border border-cyan-900/40 bg-slate-900/80 px-4 py-2">
      <Battery className={cn("h-6 w-6", color)} />
      <div>
        <p className="text-[10px] uppercase tracking-wider text-slate-500">
          Baterai Pelampung
        </p>
        <p className={cn("font-mono text-lg font-bold tabular-nums", color)}>
          {percent.toFixed(1)}%
        </p>
      </div>
      <div className="ml-2 h-2 w-24 overflow-hidden rounded-full bg-slate-800">
        <div
          className={cn(
            "h-full rounded-full transition-all duration-500",
            level === "high" && "bg-emerald-500",
            level === "mid" && "bg-amber-500",
            level === "low" && "bg-red-500"
          )}
          style={{ width: `${percent}%` }}
        />
      </div>
    </div>
  );
}

const chartTooltipStyle = {
  contentStyle: {
    background: "rgba(15, 23, 42, 0.95)",
    border: "1px solid rgba(6, 182, 212, 0.3)",
    borderRadius: "8px",
    fontSize: "12px",
  },
  labelStyle: { color: "#94a3b8" },
};

// ——— Main page ———

export default function DashboardPage() {
  const tickRef = useRef(0);
  const emaRef = useRef(50);

  const [status, setStatus] = useState<MonitoringStatus>("AMAN");
  const [spl, setSpl] = useState(50);
  const [rssi, setRssi] = useState(-85);
  const [snr, setSnr] = useState(8);
  const [pdr, setPdr] = useState(98);
  const [battery, setBattery] = useState(87);
  const [splHistory, setSplHistory] = useState<SplPoint[]>([]);
  const [loraHistory, setLoraHistory] = useState<LoraPoint[]>([]);
  const [fftBands, setFftBands] = useState<FftBand[]>(() =>
    FFT_FREQUENCIES.map((hz) => ({
      frequency: `${hz}Hz`,
      energy: 25,
      hz,
    }))
  );
  const [lastUpdate, setLastUpdate] = useState<string>("—");

  useEffect(() => {
    const pushData = () => {
      tickRef.current += 1;
      const tick = tickRef.current;
      const now = new Date();
      const timeLabel = formatTime(now);

      const mock = generateMockSnapshot(tick);
      const newEma = computeEma(mock.spl, emaRef.current);
      emaRef.current = newEma;

      setSpl(Math.round(mock.spl * 10) / 10);
      setRssi(Math.round(mock.rssi * 10) / 10);
      setSnr(Math.round(mock.snr * 10) / 10);
      setPdr(Math.round(mock.pdr * 10) / 10);
      setBattery(Math.round(mock.battery * 10) / 10);
      setFftBands(mock.fftBands);
      setStatus(evaluateStatus(mock.spl, mock.fftBands));
      setLastUpdate(timeLabel);

      setSplHistory((prev) => {
        const next = [
          ...prev,
          { time: timeLabel, spl: mock.spl, ema: newEma },
        ];
        return next.slice(-MAX_HISTORY);
      });

      setLoraHistory((prev) => {
        const next = [
          ...prev,
          { time: timeLabel, rssi: mock.rssi, snr: mock.snr },
        ];
        return next.slice(-MAX_HISTORY);
      });
    };

    pushData();
    const interval = setInterval(pushData, 2000);
    return () => clearInterval(interval);
  }, []);

  const statusIcon = useMemo(() => {
    switch (status) {
      case "AMAN":
        return <Anchor className="h-4 w-4" />;
      case "INDIKASI KAPAL":
        return <Activity className="h-4 w-4" />;
      case "ILLEGAL FISHING DETECTED":
        return <Waves className="h-4 w-4" />;
    }
  }, [status]);

  return (
    <div className="dashboard-bg min-h-screen">
      <div className="mx-auto max-w-[1600px] space-y-6 p-4 md:p-6 lg:p-8">
        {/* ——— TOP HEADER ——— */}
        <header className="flex flex-col gap-4 border-b border-cyan-900/30 pb-6 lg:flex-row lg:items-center lg:justify-between">
          <div className="flex items-start gap-4">
            <div className="hidden rounded-xl border border-cyan-500/30 bg-cyan-500/10 p-3 sm:block">
              <Radio className="h-8 w-8 text-cyan-400" />
            </div>
            <div>
              <p className="text-xs font-medium uppercase tracking-[0.2em] text-cyan-500/80">
                Smart Buoy · Maritime Acoustic Monitoring
              </p>
              <h1 className="mt-1 max-w-3xl text-xl font-bold leading-tight text-white md:text-2xl lg:text-[1.65rem]">
                Sistem Pemantauan Akustik Illegal Fishing (LoRa & ESP32)
              </h1>
              <p className="mt-1 font-mono text-xs text-slate-500">
                Pembaruan terakhir: {lastUpdate} · Interval 2s (mock)
              </p>
            </div>
          </div>

          <div className="flex flex-wrap items-center gap-3 lg:justify-end">
            <Badge
              variant={statusVariant(status)}
              className="gap-2 px-5 py-2 text-sm"
            >
              {statusIcon}
              {status}
            </Badge>
            <BatteryIndicator percent={battery} />
          </div>
        </header>

        {/* ——— SUMMARY CARDS ——— */}
        <section className="grid grid-cols-1 gap-4 sm:grid-cols-2 xl:grid-cols-4">
          <SummaryCard
            title="Rata-rata SPL"
            value={spl.toFixed(1)}
            unit="dB"
            icon={Volume2}
            accent="cyan"
          />
          <SummaryCard
            title="RSSI LoRa"
            value={rssi.toFixed(1)}
            unit="dBm"
            icon={Signal}
            accent="violet"
          />
          <SummaryCard
            title="SNR LoRa"
            value={snr.toFixed(1)}
            unit="dB"
            icon={Wifi}
            accent="emerald"
          />
          <SummaryCard
            title="Packet Delivery Ratio"
            value={pdr.toFixed(1)}
            unit="%"
            icon={Radio}
            accent="amber"
          />
        </section>

        {/* ——— MAIN CHARTS ——— */}
        <section className="grid grid-cols-1 gap-6 xl:grid-cols-2">
          <Card>
            <CardHeader>
              <CardTitle className="flex items-center gap-2">
                <Volume2 className="h-4 w-4 text-cyan-400" />
                SPL & Pola Temporal
              </CardTitle>
              <p className="text-xs text-slate-500">
                Garis SPL (dB) dan EMA — indikasi peningkatan suara mesin kapal
              </p>
            </CardHeader>
            <CardContent>
              <div className="h-[320px] w-full">
                <ResponsiveContainer width="100%" height="100%">
                  <LineChart
                    data={splHistory}
                    margin={{ top: 8, right: 12, left: 0, bottom: 0 }}
                  >
                    <CartesianGrid
                      strokeDasharray="3 3"
                      stroke="rgba(6, 182, 212, 0.1)"
                    />
                    <XAxis
                      dataKey="time"
                      tick={{ fill: "#64748b", fontSize: 10 }}
                      tickLine={false}
                      axisLine={{ stroke: "rgba(6, 182, 212, 0.2)" }}
                      interval="preserveStartEnd"
                    />
                    <YAxis
                      domain={[40, 100]}
                      tick={{ fill: "#64748b", fontSize: 11 }}
                      tickLine={false}
                      axisLine={{ stroke: "rgba(6, 182, 212, 0.2)" }}
                      label={{
                        value: "dB",
                        angle: -90,
                        position: "insideLeft",
                        fill: "#64748b",
                        fontSize: 11,
                      }}
                    />
                    <Tooltip {...chartTooltipStyle} />
                    <Legend
                      wrapperStyle={{ fontSize: "12px", paddingTop: "8px" }}
                    />
                    <Line
                      type="monotone"
                      dataKey="spl"
                      name="SPL (dB)"
                      stroke="#22d3ee"
                      strokeWidth={2}
                      dot={false}
                      activeDot={{ r: 4, fill: "#22d3ee" }}
                    />
                    <Line
                      type="monotone"
                      dataKey="ema"
                      name="EMA"
                      stroke="#a78bfa"
                      strokeWidth={2}
                      strokeDasharray="6 4"
                      dot={false}
                    />
                  </LineChart>
                </ResponsiveContainer>
              </div>
            </CardContent>
          </Card>

          <Card>
            <CardHeader>
              <CardTitle className="flex items-center gap-2">
                <Activity className="h-4 w-4 text-cyan-400" />
                FFT Frequency Energy
              </CardTitle>
              <p className="text-xs text-slate-500">
                Band 50–500 Hz · merah jika energi &gt; {FFT_ENERGY_THRESHOLD}{" "}
                (signature mesin/propeller)
              </p>
            </CardHeader>
            <CardContent>
              <div className="h-[320px] w-full">
                <ResponsiveContainer width="100%" height="100%">
                  <BarChart
                    data={fftBands}
                    margin={{ top: 8, right: 12, left: 0, bottom: 0 }}
                  >
                    <CartesianGrid
                      strokeDasharray="3 3"
                      stroke="rgba(6, 182, 212, 0.1)"
                      vertical={false}
                    />
                    <XAxis
                      dataKey="frequency"
                      tick={{ fill: "#64748b", fontSize: 10 }}
                      tickLine={false}
                      axisLine={{ stroke: "rgba(6, 182, 212, 0.2)" }}
                    />
                    <YAxis
                      domain={[0, 100]}
                      tick={{ fill: "#64748b", fontSize: 11 }}
                      tickLine={false}
                      axisLine={{ stroke: "rgba(6, 182, 212, 0.2)" }}
                      label={{
                        value: "Energi",
                        angle: -90,
                        position: "insideLeft",
                        fill: "#64748b",
                        fontSize: 11,
                      }}
                    />
                    <Tooltip {...chartTooltipStyle} />
                    <Bar dataKey="energy" name="Energi" radius={[4, 4, 0, 0]}>
                      {fftBands.map((entry, index) => (
                        <Cell
                          key={`cell-${index}`}
                          fill={
                            entry.energy > FFT_ENERGY_THRESHOLD
                              ? "#ef4444"
                              : "#0891b2"
                          }
                        />
                      ))}
                    </Bar>
                  </BarChart>
                </ResponsiveContainer>
              </div>
            </CardContent>
          </Card>
        </section>

        {/* ——— LORA HEALTH & LOCATION ——— */}
        <section className="grid grid-cols-1 gap-6 xl:grid-cols-2">
          <Card>
            <CardHeader>
              <CardTitle className="flex items-center gap-2">
                <Signal className="h-4 w-4 text-cyan-400" />
                LoRa Signal Quality
              </CardTitle>
              <p className="text-xs text-slate-500">
                RSSI & SNR over time — stabilitas transmisi
              </p>
            </CardHeader>
            <CardContent>
              <div className="h-[280px] w-full">
                <ResponsiveContainer width="100%" height="100%">
                  <LineChart
                    data={loraHistory}
                    margin={{ top: 8, right: 12, left: 0, bottom: 0 }}
                  >
                    <CartesianGrid
                      strokeDasharray="3 3"
                      stroke="rgba(6, 182, 212, 0.1)"
                    />
                    <XAxis
                      dataKey="time"
                      tick={{ fill: "#64748b", fontSize: 10 }}
                      tickLine={false}
                      axisLine={{ stroke: "rgba(6, 182, 212, 0.2)" }}
                      interval="preserveStartEnd"
                    />
                    <YAxis
                      yAxisId="rssi"
                      domain={[-100, -60]}
                      tick={{ fill: "#a78bfa", fontSize: 11 }}
                      tickLine={false}
                      axisLine={{ stroke: "rgba(167, 139, 250, 0.3)" }}
                      label={{
                        value: "dBm",
                        angle: -90,
                        position: "insideLeft",
                        fill: "#a78bfa",
                        fontSize: 10,
                      }}
                    />
                    <YAxis
                      yAxisId="snr"
                      orientation="right"
                      domain={[0, 16]}
                      tick={{ fill: "#34d399", fontSize: 11 }}
                      tickLine={false}
                      axisLine={{ stroke: "rgba(52, 211, 153, 0.3)" }}
                      label={{
                        value: "SNR dB",
                        angle: 90,
                        position: "insideRight",
                        fill: "#34d399",
                        fontSize: 10,
                      }}
                    />
                    <Tooltip {...chartTooltipStyle} />
                    <Legend wrapperStyle={{ fontSize: "12px" }} />
                    <Line
                      yAxisId="rssi"
                      type="monotone"
                      dataKey="rssi"
                      name="RSSI (dBm)"
                      stroke="#a78bfa"
                      strokeWidth={2}
                      dot={false}
                    />
                    <Line
                      yAxisId="snr"
                      type="monotone"
                      dataKey="snr"
                      name="SNR (dB)"
                      stroke="#34d399"
                      strokeWidth={2}
                      dot={false}
                    />
                  </LineChart>
                </ResponsiveContainer>
              </div>
            </CardContent>
          </Card>

          <Card className="overflow-hidden">
            <CardHeader>
              <CardTitle className="flex items-center gap-2">
                <MapPin className="h-4 w-4 text-cyan-400" />
                GPS Location
              </CardTitle>
              <p className="text-xs text-slate-500">
                Placeholder peta — react-leaflet akan ditambahkan
              </p>
            </CardHeader>
            <CardContent>
              <div className="relative flex h-[280px] flex-col items-center justify-center overflow-hidden rounded-lg border border-dashed border-cyan-700/40 bg-gradient-to-br from-slate-900 via-slate-950 to-cyan-950/30">
                <div
                  className="pointer-events-none absolute inset-0 opacity-30"
                  style={{
                    backgroundImage: `
                      linear-gradient(rgba(6,182,212,0.15) 1px, transparent 1px),
                      linear-gradient(90deg, rgba(6,182,212,0.15) 1px, transparent 1px)
                    `,
                    backgroundSize: "24px 24px",
                  }}
                />
                <div className="relative z-10 flex flex-col items-center gap-4 text-center">
                  <div className="rounded-full border border-cyan-500/40 bg-cyan-500/10 p-4">
                    <MapPin className="h-10 w-10 text-cyan-400" />
                  </div>
                  <div>
                    <p className="text-sm font-medium text-slate-300">
                      Koordinat Pelampung
                    </p>
                    <p className="mt-2 font-mono text-lg text-cyan-300">
                      Lat: {GPS_COORDS.lat.toFixed(3)}, Lng:{" "}
                      {GPS_COORDS.lng.toFixed(3)}
                    </p>
                    <p className="mt-1 text-xs text-slate-500">
                      Wilayah pemantauan perairan · Smart Buoy #01
                    </p>
                  </div>
                  <span className="rounded-full border border-cyan-800/50 bg-slate-900/80 px-3 py-1 text-[10px] uppercase tracking-widest text-cyan-600">
                    Peta interaktif — coming soon
                  </span>
                </div>
              </div>
            </CardContent>
          </Card>
        </section>

        <footer className="border-t border-cyan-900/20 pt-4 text-center text-xs text-slate-600">
          Dashboard Skripsi · Smart Buoy Acoustic Monitoring · Mock data ESP32 /
          LoRa
        </footer>
      </div>
    </div>
  );
}
