import { useEffect, useMemo, useRef, useState } from "react";
import {
    CartesianGrid,
    Legend,
    Line,
    LineChart,
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

const WINDOW_MS = 30_000;
const FRAME_MS = 33;
const SEGMENT_ANIMATION_MS = 260;
const SHOW_TEST_BUTTON = true;

/**
 * @typedef {Object} DataPacket
 * @property {number} raw_accel
 * @property {number} filtered_accel
 * @property {number} sta
 * @property {number} lta
 * @property {number} tauc
 * @property {number} pd
 */

/** @type {((packet: DataPacket) => void) | null} */
let pushPacketHandler = null;

/**
 * Entry point for live packets from external sources.
 * Example: window.pushLiveDataPacket({ raw_accel: 0.1, filtered_accel: 0.2, sta: 1, lta: 2, tauc: 0.3, pd: 0.01 })
 * @param {DataPacket} packet
 */
export function pushLiveDataPacket(packet) {
    if (pushPacketHandler) {
        pushPacketHandler(packet);
    }
}

function toChartPoint(packet, index) {
    return {
        index,
        timestamp: Date.now(),
        timeLabel: new Date().toLocaleTimeString(),
        raw_accel: packet.raw_accel,
        filtered_accel: packet.filtered_accel,
        sta: packet.sta,
        lta: packet.lta,
        tauc: packet.tauc,
        pd: packet.pd,
    };
}

function randomPacket(lastPoint) {
    const rawBase = lastPoint ? lastPoint.raw_accel : 0.2;
    const raw = rawBase + (Math.random() - 0.5) * 0.2;
    const filtered = raw * 0.8 + (Math.random() - 0.5) * 0.04;

    return {
        raw_accel: Number(raw.toFixed(4)),
        filtered_accel: Number(filtered.toFixed(4)),
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
        raw_accel: 0,
        filtered_accel: 0,
        sta: 0,
        lta: 0,
        tauc: 0,
        pd: 0,
        isSeed: true,
    };
}

function App() {
    const initialTimestamp = Date.now();
    const [series, setSeries] = useState(() => [
        createZeroPoint(initialTimestamp),
    ]);
    const [now, setNow] = useState(initialTimestamp);
    const [yScaleMax, setYScaleMax] = useState(1);
    const [animatedPoint, setAnimatedPoint] = useState(null);
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
                raw_accel: mix(
                    animation.from.raw_accel,
                    animation.to.raw_accel,
                ),
                filtered_accel: mix(
                    animation.from.filtered_accel,
                    animation.to.filtered_accel,
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

        return () => {
            if (pushPacketHandler === appendPacket) {
                pushPacketHandler = null;
            }
            if (window.pushLiveDataPacket === pushLiveDataPacket) {
                delete window.pushLiveDataPacket;
            }
        };
    }, []);

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

    useEffect(() => {
        if (chartData.length === 0) {
            return;
        }

        const maxAbs = chartData.reduce((acc, point) => {
            return Math.max(
                acc,
                Math.abs(point.raw_accel),
                Math.abs(point.filtered_accel),
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

                <Card className="py-0 border border-slate-900/10 bg-slate-900/5 transition-shadow duration-300 hover:shadow-sm dark:border-blue-100/15 dark:bg-blue-950/25">
                    <CardHeader className="border-b py-4">
                        <CardTitle>Acceleration Over Time</CardTitle>
                        <CardDescription>
                            Live stream of raw and filtered acceleration values.
                        </CardDescription>
                    </CardHeader>
                    <CardContent className="h-[68vh] px-2 pb-4 pt-5 sm:px-4">
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
                                    domain={[xAxisEnd - WINDOW_MS, xAxisEnd]}
                                    tickLine
                                    axisLine
                                    stroke="var(--muted-foreground)"
                                    tickMargin={8}
                                    minTickGap={24}
                                    tickFormatter={(value) =>
                                        new Date(value).toLocaleTimeString([], {
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
                                        new Date(label).toLocaleTimeString([], {
                                            minute: "2-digit",
                                            second: "2-digit",
                                        })
                                    }
                                />
                                <Legend verticalAlign="top" height={28} />
                                <Line
                                    type="linear"
                                    dataKey="raw_accel"
                                    name="Raw Accel"
                                    stroke="#94a3b8"
                                    strokeWidth={1.75}
                                    strokeDasharray="4 4"
                                    dot={false}
                                    isAnimationActive={false}
                                />
                                <Line
                                    type="linear"
                                    dataKey="filtered_accel"
                                    name="Filtered Accel"
                                    stroke="#3b82f6"
                                    strokeWidth={2.75}
                                    dot={false}
                                    isAnimationActive={false}
                                />
                            </LineChart>
                        </ResponsiveContainer>
                    </CardContent>
                </Card>
            </div>

            {SHOW_TEST_BUTTON && (
                <div className="fixed inset-x-0 bottom-4 z-30 flex justify-center px-4">
                    <Button
                        variant="secondary"
                        className="w-full max-w-sm border border-border/70 bg-muted/40 text-foreground hover:bg-muted/70"
                        onClick={handleAddRandomPoint}
                    >
                        Add Random Test Point
                    </Button>
                </div>
            )}
        </main>
    );
}

export default App;
