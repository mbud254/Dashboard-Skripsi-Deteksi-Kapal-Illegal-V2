export type VesselStatus = "SAFE" | "ILLEGAL VESSEL DETECTED";

interface SensorData {
  spl: number;
  fft: number;
  ema: number;
  status: VesselStatus;
  updatedAt: number;
}

let latestSensorData: SensorData | null = null;

function computeStatus(spl: number, fft: number, ema: number): VesselStatus {
  if (spl > 70 && fft > 5 && ema > 65) {
    return "ILLEGAL VESSEL DETECTED";
  }
  return "SAFE";
}

export async function POST(request: Request) {
  try {
    const body = await request.json();

    if (
      typeof body.spl !== "number" ||
      typeof body.fft !== "number" ||
      typeof body.ema !== "number"
    ) {
      return Response.json(
        { error: "Expected { spl: number, fft: number, ema: number }" },
        { status: 400 }
      );
    }

    latestSensorData = {
      spl: body.spl,
      fft: body.fft,
      ema: body.ema,
      status: computeStatus(body.spl, body.fft, body.ema),
      updatedAt: Date.now(),
    };

    return Response.json({ success: true, data: latestSensorData });
  } catch {
    return Response.json({ error: "Invalid JSON body" }, { status: 400 });
  }
}

export async function GET() {
  if (!latestSensorData) {
    return Response.json({
      spl: null,
      fft: null,
      ema: null,
      status: null,
      updatedAt: null,
    });
  }

  return Response.json(latestSensorData);
}
