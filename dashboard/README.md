# Dashboard Skripsi — Smart Buoy Illegal Fishing Monitoring

Real-time monitoring dashboard for acoustic Smart Buoy (LoRa & ESP32) with mock data simulation.

## Stack

- **Next.js 15** (App Router)
- **Tailwind CSS 4**
- **recharts** — charts
- **lucide-react** — icons
- shadcn-style **Card** & **Badge** components

## Getting started

```bash
npm install
npm run dev
```

Open [http://localhost:3000](http://localhost:3000).

Mock data updates every **2 seconds**, simulating a boat approaching (rising SPL, low-frequency FFT spikes).

## Project structure

- `src/app/page.tsx` — main dashboard (mock generator + UI)
- `src/components/ui/` — Card, Badge
- `src/lib/utils.ts` — `cn()` helper

## Status logic (mock)

| Status | Condition |
|--------|-----------|
| **AMAN** | Low SPL, no significant low-frequency FFT |
| **INDIKASI KAPAL** | SPL ≥ 62 dB or FFT spike in 50–200 Hz |
| **ILLEGAL FISHING DETECTED** | SPL ≥ 78 dB and low-frequency FFT above threshold |
