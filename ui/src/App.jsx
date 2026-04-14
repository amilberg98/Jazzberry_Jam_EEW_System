import { useEffect, useMemo, useRef, useState } from "react";
import {
    CartesianGrid,
    Legend,
    Line,
    LineChart,
    ReferenceArea,
    ReferenceLine,
    ResponsiveContainer,
    Tooltip,
    XAxis,
    YAxis,
} from "recharts";

import { Button } from "@/components/ui/button";
import {
    Card,
    CardContent,
    CardDescription,
    CardHeader,
    CardTitle,
} from "@/components/ui/card";
import {
    listenForUsbJsonPackets,
    monitorUsbDeviceConnections,
} from "@/lib/usbPacketListener";

const WINDOW_MS = 60_000;
const FRAME_MS = 33;
const SEGMENT_ANIMATION_MS = 260;
const USB_SAMPLE_RATE_HZ = 200;
const FFT_WINDOW_SIZE = 256;
const FFT_BAND_MIN_HZ = 1;
const FFT_BAND_MAX_HZ = 20;
const FFT_VIEW_MAX_HZ = 50;
const SHOW_TEST_BUTTON = true;

/**
 * @typedef {Object} DataPacket
 * @property {number} raw_vec
 * @property {number} accel_vec
 * @property {number} filteredX
 * @property {number} filteredY
 * @property {number} filteredZ
 * @property {number} sta
 * @property {number} lta
 * @property {number} tauc
 * @property {number} pd
 */

/** @type {((packet: DataPacket) => void) | null} */
let pushPacketHandler = null;
/** @type {null | (() => Promise<void>)} */
let stopUsbPacketListener = null;

/**
 * Entry point for live packets from external sources.
 * Example: window.pushLiveDataPacket({ raw_vec: 0.1, accel_vec: 0.2, filteredX: 0.01, filteredY: 0.02, filteredZ: 0.03, sta: 1, lta: 2, tauc: 0.3, pd: 0.01 })
 * @param {DataPacket} packet
 */
export function pushLiveDataPacket(packet) {
    if (pushPacketHandler) {
        pushPacketHandler(packet);
    }
}

function toChartPoint(packet, index) {
    const rawVec = packet.raw_vec ?? packet.raw_accel ?? 0;
    const accelVec = packet.accel_vec ?? packet.filtered_accel ?? 0;
    const filteredX = packet.filteredX ?? 0;
    const filteredY = packet.filteredY ?? 0;
    const filteredZ = packet.filteredZ ?? 0;

    return {
        index,
        timestamp: Date.now(),
        timeLabel: new Date().toLocaleTimeString(),
        raw_vec: rawVec,
        accel_vec: accelVec,
        filteredX,
        filteredY,
        filteredZ,
        sta: packet.sta,
        lta: packet.lta,
        tauc: packet.tauc,
        pd: packet.pd,
    };
}

function randomPacket(lastPoint) {
    const rawBase = lastPoint ? lastPoint.raw_vec : 0.2;
    const raw = rawBase + (Math.random() - 0.5) * 0.2;
    const accelVec = raw * 0.8 + (Math.random() - 0.5) * 0.04;
    const theta = Math.random() * Math.PI * 2;
    const phi = Math.random() * Math.PI;
    const filteredX = accelVec * Math.sin(phi) * Math.cos(theta);
    const filteredY = accelVec * Math.sin(phi) * Math.sin(theta);
    const filteredZ = accelVec * Math.cos(phi);

    return {
        raw_vec: Number(raw.toFixed(4)),
        accel_vec: Number(accelVec.toFixed(4)),
        filteredX: Number(filteredX.toFixed(4)),
        filteredY: Number(filteredY.toFixed(4)),
        filteredZ: Number(filteredZ.toFixed(4)),
        sta: Number((Math.random() * 4 + 0.1).toFixed(3)),
        lta: Number((Math.random() * 4 + 0.1).toFixed(3)),
        tauc: Number((Math.random() * 2).toFixed(4)),
        pd: Number((Math.random() * 0.2).toFixed(4)),
    };
}

function createZeroPoint(timestamp, index = 1) {
    return {
        index,
        timestamp,
        timeLabel: new Date(timestamp).toLocaleTimeString(),
        raw_vec: 0,
        accel_vec: 0,
        filteredX: 0,
        filteredY: 0,
        filteredZ: 0,
        sta: 0,
        lta: 0,
        tauc: 0,
        pd: 0,
        isSeed: true,
    };
}

function buildMagnitudeSpectrum(samples, sampleRateHz) {
    const n = samples.length;
    if (n < 32 || !Number.isFinite(sampleRateHz) || sampleRateHz <= 0) {
        return [];
    }

    const mean = samples.reduce((sum, value) => sum + value, 0) / n;
    const windowed = samples.map((value, index) => {
        const hann = 0.5 - 0.5 * Math.cos((2 * Math.PI * index) / (n - 1));
        return (value - mean) * hann;
    });

    const half = Math.floor(n / 2);
    const bins = [];

    for (let k = 0; k <= half; k += 1) {
        let real = 0;
        let imag = 0;

        for (let t = 0; t < n; t += 1) {
            const angle = (2 * Math.PI * k * t) / n;
            real += windowed[t] * Math.cos(angle);
            imag -= windowed[t] * Math.sin(angle);
        }

        bins.push((2 / n) * Math.hypot(real, imag));
    }

    return bins;
}

function App() {
    const initialTimestamp = Date.now();
    const [series, setSeries] = useState(() => [
        createZeroPoint(initialTimestamp),
    ]);
    const [now, setNow] = useState(initialTimestamp);
    const [yScaleMax, setYScaleMax] = useState(1);
    const [animatedPoint, setAnimatedPoint] = useState(null);
    const [isUsbConnected, setIsUsbConnected] = useState(false);
    const [isUsbConnecting, setIsUsbConnecting] = useState(false);
    const [usbConnectionError, setUsbConnectionError] = useState("");
    const activeAnimationRef = useRef(null);

    useEffect(() => {
        const interval = setInterval(() => {
            const currentTime = Date.now();
            const cutoff = currentTime - WINDOW_MS;

            setNow(currentTime);
            setSeries((prev) => {
                const filtered = prev.filter(
                    (point) => point.timestamp >= cutoff,
                );
                return filtered.length === prev.length ? prev : filtered;
            });

            const animation = activeAnimationRef.current;
            if (!animation) {
                return;
            }

            const elapsed = performance.now() - animation.startTime;
            const progress = Math.min(elapsed / SEGMENT_ANIMATION_MS, 1);
            const mix = (from, to) => from + (to - from) * progress;
            const interpolated = {
                index: animation.to.index,
                timestamp: mix(
                    animation.from.timestamp,
                    animation.to.timestamp,
                ),
                timeLabel: animation.to.timeLabel,
                raw_vec: mix(animation.from.raw_vec, animation.to.raw_vec),
                accel_vec: mix(
                    animation.from.accel_vec,
                    animation.to.accel_vec,
                ),
                filteredX: mix(
                    animation.from.filteredX,
                    animation.to.filteredX,
                ),
                filteredY: mix(
                    animation.from.filteredY,
                    animation.to.filteredY,
                ),
                filteredZ: mix(
                    animation.from.filteredZ,
                    animation.to.filteredZ,
                ),
                sta: mix(animation.from.sta, animation.to.sta),
                lta: mix(animation.from.lta, animation.to.lta),
                tauc: mix(animation.from.tauc, animation.to.tauc),
                pd: mix(animation.from.pd, animation.to.pd),
            };

            setAnimatedPoint(interpolated);

            if (progress >= 1) {
                const completedPoint = animation.to;
                activeAnimationRef.current = null;
                setAnimatedPoint(null);
                setSeries((prev) => {
                    const next = [...prev, completedPoint];
                    return next.filter((point) => point.timestamp >= cutoff);
                });
            }
        }, FRAME_MS);

        return () => clearInterval(interval);
    }, []);

    useEffect(() => {
        if (series.length > 0 || animatedPoint || activeAnimationRef.current) {
            return;
        }

        setSeries([createZeroPoint(Date.now())]);
    }, [series, animatedPoint]);

    useEffect(() => {
        const stopUsbConnectionMonitor = monitorUsbDeviceConnections();

        const serial = navigator.serial;
        let isMounted = true;

        const syncConnectedState = async () => {
            if (!serial || typeof serial.getPorts !== "function") {
                return;
            }

            try {
                const ports = await serial.getPorts();
                if (isMounted) {
                    setIsUsbConnected(ports.length > 0);
                }
            } catch (error) {
                console.warn("Unable to query USB connection state.", error);
            }
        };

        const handleConnect = () => {
            if (isMounted) {
                setIsUsbConnected(true);
            }
        };

        const handleDisconnect = async () => {
            if (!isMounted) {
                return;
            }

            setIsUsbConnected(false);
            if (stopUsbPacketListener) {
                await stopUsbPacketListener();
                stopUsbPacketListener = null;
            }
        };

        if (serial) {
            serial.addEventListener("connect", handleConnect);
            serial.addEventListener("disconnect", handleDisconnect);
            syncConnectedState();
        }

        const appendPacket = (packet) => {
            if (!packet) {
                return;
            }

            setSeries((prev) => {
                const isSeedOnly = prev.length === 1 && prev[0].isSeed;
                const nextPoint = {
                    ...toChartPoint(packet, isSeedOnly ? 1 : prev.length + 1),
                    isSeed: false,
                };

                if (isSeedOnly) {
                    activeAnimationRef.current = null;
                    setAnimatedPoint(null);
                    const cutoff = Date.now() - WINDOW_MS;
                    return [nextPoint].filter(
                        (point) => point.timestamp >= cutoff,
                    );
                }

                const previousPoint = prev[prev.length - 1];

                if (!previousPoint) {
                    const cutoff = Date.now() - WINDOW_MS;
                    return [...prev, nextPoint].filter(
                        (point) => point.timestamp >= cutoff,
                    );
                }

                if (activeAnimationRef.current) {
                    const forcedPoint = activeAnimationRef.current.to;
                    prev = [...prev, forcedPoint];
                }

                activeAnimationRef.current = {
                    from: previousPoint,
                    to: nextPoint,
                    startTime: performance.now(),
                };

                setAnimatedPoint({
                    ...previousPoint,
                    index: nextPoint.index,
                    timestamp: previousPoint.timestamp,
                });

                const cutoff = Date.now() - WINDOW_MS;
                return prev.filter((point) => point.timestamp >= cutoff);
            });
        };

        pushPacketHandler = appendPacket;
        window.pushLiveDataPacket = pushLiveDataPacket;
        window.startUsbPacketListener = async (baudRate = 9600) => {
            if (stopUsbPacketListener) {
                await stopUsbPacketListener();
                stopUsbPacketListener = null;
            }

            stopUsbPacketListener = await listenForUsbJsonPackets({
                baudRate,
                onPacket: (packet) => pushLiveDataPacket(packet),
                onError: (error) => {
                    console.error("USB packet listener error", error);
                },
            });

            if (isMounted) {
                setIsUsbConnected(true);
                setUsbConnectionError("");
            }

            return stopUsbPacketListener;
        };
        window.stopUsbPacketListener = async () => {
            if (!stopUsbPacketListener) {
                return;
            }

            await stopUsbPacketListener();
            stopUsbPacketListener = null;
            if (isMounted) {
                setIsUsbConnected(false);
            }
        };

        return () => {
            isMounted = false;

            if (serial) {
                serial.removeEventListener("connect", handleConnect);
                serial.removeEventListener("disconnect", handleDisconnect);
            }

            if (pushPacketHandler === appendPacket) {
                pushPacketHandler = null;
            }
            if (window.pushLiveDataPacket === pushLiveDataPacket) {
                delete window.pushLiveDataPacket;
            }
            if (window.startUsbPacketListener) {
                delete window.startUsbPacketListener;
            }
            if (window.stopUsbPacketListener) {
                delete window.stopUsbPacketListener;
            }

            if (stopUsbPacketListener) {
                stopUsbPacketListener();
                stopUsbPacketListener = null;
            }

            stopUsbConnectionMonitor();
        };
    }, []);

    const handleConnectDevice = async () => {
        if (isUsbConnecting) {
            return;
        }

        setIsUsbConnecting(true);
        setUsbConnectionError("");

        try {
            await window.startUsbPacketListener();
        } catch (error) {
            if (error?.name === "NotFoundError") {
                console.info("USB connect prompt was dismissed by the user.");
            } else if (error?.name === "SecurityError") {
                console.warn(
                    "USB connect prompt must be triggered from a user gesture.",
                    error,
                );
                setUsbConnectionError("Connect from a user gesture.");
            } else {
                console.error("Unable to start USB packet listener.", error);
                setUsbConnectionError("Unable to connect device.");
            }
        } finally {
            setIsUsbConnecting(false);
        }
    };

    const latest = useMemo(() => {
        if (animatedPoint) {
            return {
                sta: animatedPoint.sta,
                lta: animatedPoint.lta,
                tauc: animatedPoint.tauc,
                pd: animatedPoint.pd,
            };
        }

        if (series.length === 0) {
            return { sta: 0, lta: 0, tauc: 0, pd: 0 };
        }
        const last = series[series.length - 1];
        return { sta: last.sta, lta: last.lta, tauc: last.tauc, pd: last.pd };
    }, [series, animatedPoint]);

    const chartData = useMemo(() => {
        if (!animatedPoint) {
            return series;
        }

        return [...series, animatedPoint];
    }, [series, animatedPoint]);

    const xAxisEnd = useMemo(() => {
        if (chartData.length === 0) {
            return now;
        }

        return Math.max(now, chartData[chartData.length - 1].timestamp);
    }, [chartData, now]);

    const yDomain = useMemo(() => {
        return [-yScaleMax, yScaleMax];
    }, [yScaleMax]);

    const fftData = useMemo(() => {
        const maxSamples = Math.min(FFT_WINDOW_SIZE, chartData.length);
        if (maxSamples < 32) {
            return [];
        }

        const fftSize = 2 ** Math.floor(Math.log2(maxSamples));
        if (fftSize < 32) {
            return [];
        }

        const windowPoints = chartData.slice(-fftSize);
        const sampleRateHz = USB_SAMPLE_RATE_HZ;
        if (!Number.isFinite(sampleRateHz) || sampleRateHz <= 0) {
            return [];
        }

        const rawSamples = windowPoints.map((point) => point.raw_vec);
        const filteredSamples = windowPoints.map((point) => point.accel_vec);

        const rawSpectrum = buildMagnitudeSpectrum(rawSamples, sampleRateHz);
        const filteredSpectrum = buildMagnitudeSpectrum(
            filteredSamples,
            sampleRateHz,
        );
        const bins = Math.min(rawSpectrum.length, filteredSpectrum.length);

        const data = [];
        for (let k = 0; k < bins; k += 1) {
            const frequencyHz = (k * sampleRateHz) / fftSize;
            if (frequencyHz > FFT_VIEW_MAX_HZ) {
                break;
            }

            data.push({
                frequencyHz,
                raw_fft: rawSpectrum[k],
                filtered_fft: filteredSpectrum[k],
            });
        }

        return data;
    }, [chartData]);

    const fftYMax = useMemo(() => {
        if (fftData.length === 0) {
            return 1;
        }

        const maxValue = fftData.reduce((acc, point) => {
            return Math.max(acc, point.raw_fft, point.filtered_fft);
        }, 0);

        return Math.max(maxValue * 1.1, 0.01);
    }, [fftData]);

    useEffect(() => {
        if (chartData.length === 0) {
            return;
        }

        const maxAbs = chartData.reduce((acc, point) => {
            return Math.max(
                acc,
                Math.abs(point.raw_vec),
                Math.abs(point.accel_vec),
                Math.abs(point.filteredX),
                Math.abs(point.filteredY),
                Math.abs(point.filteredZ),
            );
        }, 0);

        const target = Math.max(maxAbs * 1.2, 0.2);

        setYScaleMax((prev) => {
            if (target > prev) {
                return target;
            }

            return Math.max(target, prev * 0.92);
        });
    }, [chartData]);

    const handleAddRandomPoint = () => {
        const packet = randomPacket(series[series.length - 1]);
        pushLiveDataPacket(packet);
    };

    return (
        <main className="flex min-h-screen items-center bg-gradient-to-b from-background via-background to-muted/20 px-4 py-6 md:px-8">
            <div className="mx-auto flex min-h-[calc(100vh-6rem)] w-full max-w-7xl flex-col justify-center gap-4 pb-20">
                <div className="grid grid-cols-2 gap-3 md:grid-cols-4">
                    <div className="rounded-lg border border-slate-900/10 bg-slate-900/5 p-3 transition-colors duration-300 hover:bg-blue-950/10 dark:border-blue-100/15 dark:bg-blue-950/25 dark:hover:bg-blue-900/35">
                        <p className="text-xs uppercase tracking-wide text-muted-foreground">
                            STA
                        </p>
                        <p className="text-xl font-semibold">
                            {latest.sta.toFixed(3)}
                        </p>
                    </div>
                    <div className="rounded-lg border border-slate-900/10 bg-slate-900/5 p-3 transition-colors duration-300 hover:bg-blue-950/10 dark:border-blue-100/15 dark:bg-blue-950/25 dark:hover:bg-blue-900/35">
                        <p className="text-xs uppercase tracking-wide text-muted-foreground">
                            LTA
                        </p>
                        <p className="text-xl font-semibold">
                            {latest.lta.toFixed(3)}
                        </p>
                    </div>
                    <div className="rounded-lg border border-slate-900/10 bg-slate-900/5 p-3 transition-colors duration-300 hover:bg-blue-950/10 dark:border-blue-100/15 dark:bg-blue-950/25 dark:hover:bg-blue-900/35">
                        <p className="text-xs uppercase tracking-wide text-muted-foreground">
                            TAUC
                        </p>
                        <p className="text-xl font-semibold">
                            {latest.tauc.toFixed(4)}
                        </p>
                    </div>
                    <div className="rounded-lg border border-slate-900/10 bg-slate-900/5 p-3 transition-colors duration-300 hover:bg-blue-950/10 dark:border-blue-100/15 dark:bg-blue-950/25 dark:hover:bg-blue-900/35">
                        <p className="text-xs uppercase tracking-wide text-muted-foreground">
                            PD
                        </p>
                        <p className="text-xl font-semibold">
                            {latest.pd.toFixed(4)}
                        </p>
                    </div>
                </div>

                <div className="grid gap-4 lg:grid-cols-5">
                    <Card className="h-[76vh] py-0 border border-slate-900/10 bg-slate-900/5 transition-shadow duration-300 hover:shadow-sm dark:border-blue-100/15 dark:bg-blue-950/25 lg:col-span-3 flex flex-col">
                        <CardHeader className="border-b py-4">
                            <CardTitle>Acceleration Over Time</CardTitle>
                            <CardDescription>
                                Live stream of raw and accel vector values.
                            </CardDescription>
                        </CardHeader>
                        <CardContent className="min-h-0 flex-1 px-2 pb-4 pt-5 sm:px-4">
                            <ResponsiveContainer width="100%" height="100%">
                                <LineChart
                                    data={chartData}
                                    margin={{
                                        top: 16,
                                        right: 16,
                                        left: 8,
                                        bottom: 8,
                                    }}
                                >
                                    <CartesianGrid
                                        strokeDasharray="3 3"
                                        stroke="var(--muted)"
                                    />
                                    <XAxis
                                        dataKey="timestamp"
                                        type="number"
                                        domain={[
                                            xAxisEnd - WINDOW_MS,
                                            xAxisEnd,
                                        ]}
                                        tickLine
                                        axisLine
                                        stroke="var(--muted-foreground)"
                                        tickMargin={8}
                                        minTickGap={24}
                                        tickFormatter={(value) =>
                                            new Date(value).toLocaleTimeString(
                                                [],
                                                {
                                                    minute: "2-digit",
                                                    second: "2-digit",
                                                },
                                            )
                                        }
                                    />
                                    <YAxis
                                        domain={yDomain}
                                        tickLine
                                        axisLine
                                        stroke="var(--muted-foreground)"
                                        tickMargin={8}
                                        width={64}
                                        tickFormatter={(value) =>
                                            Number(value).toFixed(3)
                                        }
                                    />
                                    <ReferenceLine
                                        y={0}
                                        stroke="var(--muted-foreground)"
                                        strokeOpacity={0.45}
                                    />
                                    <Tooltip
                                        cursor={{
                                            stroke: "var(--ring)",
                                            strokeWidth: 1,
                                        }}
                                        contentStyle={{
                                            borderRadius: "0.75rem",
                                            borderColor: "var(--border)",
                                            boxShadow:
                                                "0 1px 2px rgba(15, 23, 42, 0.08)",
                                        }}
                                        labelFormatter={(label) =>
                                            new Date(label).toLocaleTimeString(
                                                [],
                                                {
                                                    minute: "2-digit",
                                                    second: "2-digit",
                                                },
                                            )
                                        }
                                    />
                                    <Legend verticalAlign="top" height={28} />
                                    <Line
                                        type="linear"
                                        dataKey="raw_vec"
                                        name="Raw Vector"
                                        stroke="#94a3b8"
                                        strokeWidth={1.75}
                                        strokeDasharray="4 4"
                                        dot={false}
                                        isAnimationActive={false}
                                    />
                                    <Line
                                        type="linear"
                                        dataKey="accel_vec"
                                        name="Accel Vector"
                                        stroke="#3b82f6"
                                        strokeWidth={2.75}
                                        dot={false}
                                        isAnimationActive={false}
                                    />
                                </LineChart>
                            </ResponsiveContainer>
                        </CardContent>
                    </Card>

                    <div className="flex h-[76vh] flex-col gap-4 lg:col-span-2">
                        <Card className="py-0 border border-slate-900/10 bg-slate-900/5 transition-shadow duration-300 hover:shadow-sm dark:border-blue-100/15 dark:bg-blue-950/25 flex flex-1 flex-col min-h-0">
                            <CardHeader className="border-b py-4">
                                <CardTitle>Filtered Axes</CardTitle>
                                <CardDescription>
                                    Filtered X, Y, and Z components.
                                </CardDescription>
                            </CardHeader>
                            <CardContent className="min-h-0 flex-1 px-2 pb-4 pt-5 sm:px-4">
                                <ResponsiveContainer width="100%" height="100%">
                                    <LineChart
                                        data={chartData}
                                        margin={{
                                            top: 8,
                                            right: 8,
                                            left: 6,
                                            bottom: 8,
                                        }}
                                    >
                                        <CartesianGrid
                                            strokeDasharray="3 3"
                                            stroke="var(--muted)"
                                        />
                                        <XAxis
                                            dataKey="timestamp"
                                            type="number"
                                            domain={[
                                                xAxisEnd - WINDOW_MS,
                                                xAxisEnd,
                                            ]}
                                            tickLine
                                            axisLine
                                            stroke="var(--muted-foreground)"
                                            tickMargin={6}
                                            minTickGap={20}
                                            tickFormatter={(value) =>
                                                new Date(
                                                    value,
                                                ).toLocaleTimeString([], {
                                                    minute: "2-digit",
                                                    second: "2-digit",
                                                })
                                            }
                                        />
                                        <YAxis
                                            domain={yDomain}
                                            tickLine
                                            axisLine
                                            stroke="var(--muted-foreground)"
                                            tickMargin={6}
                                            width={52}
                                            tickFormatter={(value) =>
                                                Number(value).toFixed(3)
                                            }
                                        />
                                        <ReferenceLine
                                            y={0}
                                            stroke="var(--muted-foreground)"
                                            strokeOpacity={0.45}
                                        />
                                        <Tooltip
                                            cursor={{
                                                stroke: "var(--ring)",
                                                strokeWidth: 1,
                                            }}
                                            contentStyle={{
                                                borderRadius: "0.75rem",
                                                borderColor: "var(--border)",
                                                boxShadow:
                                                    "0 1px 2px rgba(15, 23, 42, 0.08)",
                                            }}
                                            labelFormatter={(label) =>
                                                new Date(
                                                    label,
                                                ).toLocaleTimeString([], {
                                                    minute: "2-digit",
                                                    second: "2-digit",
                                                })
                                            }
                                        />
                                        <Legend
                                            verticalAlign="top"
                                            height={24}
                                        />
                                        <Line
                                            type="linear"
                                            dataKey="filteredX"
                                            name="Filtered X"
                                            stroke="#f9a8d4"
                                            strokeWidth={2}
                                            dot={false}
                                            isAnimationActive={false}
                                        />
                                        <Line
                                            type="linear"
                                            dataKey="filteredY"
                                            name="Filtered Y"
                                            stroke="#86efac"
                                            strokeWidth={2}
                                            dot={false}
                                            isAnimationActive={false}
                                        />
                                        <Line
                                            type="linear"
                                            dataKey="filteredZ"
                                            name="Filtered Z"
                                            stroke="#93c5fd"
                                            strokeWidth={2}
                                            dot={false}
                                            isAnimationActive={false}
                                        />
                                    </LineChart>
                                </ResponsiveContainer>
                            </CardContent>
                        </Card>

                        <Card className="py-0 border border-slate-900/10 bg-slate-900/5 transition-shadow duration-300 hover:shadow-sm dark:border-blue-100/15 dark:bg-blue-950/25 flex flex-1 flex-col min-h-0">
                            <CardHeader className="border-b py-4">
                                <CardTitle>FFT: Raw vs Filtered</CardTitle>
                                <CardDescription>
                                    Frequency spectrum.
                                </CardDescription>
                            </CardHeader>
                            <CardContent className="min-h-0 flex flex-1 flex-col px-2 pb-4 pt-5 sm:px-4">
                                <ResponsiveContainer width="100%" height="100%">
                                    <LineChart
                                        data={fftData}
                                        margin={{
                                            top: 8,
                                            right: 8,
                                            left: 6,
                                            bottom: 8,
                                        }}
                                    >
                                        <CartesianGrid
                                            strokeDasharray="3 3"
                                            stroke="var(--muted)"
                                        />
                                        <ReferenceArea
                                            x1={FFT_BAND_MIN_HZ}
                                            x2={FFT_BAND_MAX_HZ}
                                            y1={0}
                                            y2={fftYMax}
                                            fill="#86efac"
                                            fillOpacity={0.12}
                                        />
                                        <XAxis
                                            dataKey="frequencyHz"
                                            type="number"
                                            domain={[0, FFT_VIEW_MAX_HZ]}
                                            tickLine
                                            axisLine
                                            stroke="var(--muted-foreground)"
                                            tickMargin={6}
                                            minTickGap={20}
                                            tickFormatter={(value) =>
                                                `${value.toFixed(0)} Hz`
                                            }
                                        />
                                        <YAxis
                                            type="number"
                                            domain={[0, fftYMax]}
                                            tickLine
                                            axisLine
                                            stroke="var(--muted-foreground)"
                                            tickMargin={6}
                                            width={52}
                                            tickFormatter={(value) =>
                                                Number(value).toFixed(3)
                                            }
                                        />
                                        <Tooltip
                                            cursor={{
                                                stroke: "var(--ring)",
                                                strokeWidth: 1,
                                            }}
                                            contentStyle={{
                                                borderRadius: "0.75rem",
                                                borderColor: "var(--border)",
                                                boxShadow:
                                                    "0 1px 2px rgba(15, 23, 42, 0.08)",
                                            }}
                                            labelFormatter={(label) =>
                                                `${Number(label).toFixed(2)} Hz`
                                            }
                                        />
                                        <Legend
                                            verticalAlign="top"
                                            height={24}
                                        />
                                        <Line
                                            type="linear"
                                            dataKey="raw_fft"
                                            name="Raw FFT"
                                            stroke="#94a3b8"
                                            strokeWidth={1.8}
                                            dot={false}
                                            isAnimationActive={false}
                                        />
                                        <Line
                                            type="linear"
                                            dataKey="filtered_fft"
                                            name="Filtered FFT"
                                            stroke="#3b82f6"
                                            strokeWidth={2.2}
                                            dot={false}
                                            isAnimationActive={false}
                                        />
                                    </LineChart>
                                </ResponsiveContainer>
                            </CardContent>
                        </Card>
                    </div>
                </div>

                <div className="flex flex-col gap-3 sm:flex-row sm:items-center sm:justify-between">
                    <div className="flex items-center gap-3 text-sm text-muted-foreground">
                        <svg
                            aria-hidden="true"
                            viewBox="0 0 24 24"
                            className={`h-5 w-5 ${
                                isUsbConnected
                                    ? "text-emerald-500"
                                    : "text-slate-400"
                            }`}
                            fill="none"
                            stroke="currentColor"
                            strokeWidth="1.8"
                            strokeLinecap="round"
                            strokeLinejoin="round"
                        >
                            <path d="M8 7h8v6a4 4 0 0 1-8 0V7Z" />
                            <path d="M10 3v4M14 3v4" />
                            <path d="M12 17v4" />
                        </svg>
                        <span className="font-medium text-foreground">
                            USB Device
                        </span>
                        <span
                            className={`h-2.5 w-2.5 rounded-full ${
                                isUsbConnected
                                    ? "bg-emerald-500"
                                    : "bg-slate-400"
                            }`}
                        />
                        <span>
                            {isUsbConnected ? "Connected" : "Disconnected"}
                        </span>
                        {!isUsbConnected ? (
                            <button
                                type="button"
                                className="ml-1 font-medium text-blue-600 transition-colors hover:text-blue-500 dark:text-blue-400 dark:hover:text-blue-300"
                                onClick={handleConnectDevice}
                                disabled={isUsbConnecting}
                            >
                                {isUsbConnecting ? "Connecting..." : "Connect"}
                            </button>
                        ) : null}
                    </div>

                    {SHOW_TEST_BUTTON && (
                        <Button
                            variant="secondary"
                            className="w-full sm:w-auto border border-border/70 bg-muted/40 text-foreground hover:bg-muted/70"
                            onClick={handleAddRandomPoint}
                        >
                            Add Random Test Point
                        </Button>
                    )}
                </div>

                {usbConnectionError ? (
                    <p className="text-xs text-destructive">
                        {usbConnectionError}
                    </p>
                ) : null}
            </div>
        </main>
    );
}

export default App;
