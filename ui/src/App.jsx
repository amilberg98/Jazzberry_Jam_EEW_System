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
import { AlertCircle, Upload, X } from "lucide-react";

const WINDOW_MS = 60_000;
const FRAME_MS = 33;
const SEGMENT_ANIMATION_MS = 260;
const USB_SAMPLE_RATE_HZ = 200;
const DISPLAY_UPDATE_INTERVAL_MS = 250; // Show a new point every 0.25 seconds
const COLLECTING_ON_LABEL_DY = 0;
const EARTHQUAKE_LABEL_DY = 14;
const COLLECTING_OFF_LABEL_DY = 28;
const EARTHQUAKE_AREA_BASE_OPACITY = 0.12;
const EARTHQUAKE_AREA_BASE_STROKE_OPACITY = 0.18;
const EARTHQUAKE_AREA_EDGE_FADE_MS = 2500;
const TRIGGER_ON_RATIO = 4.0;
const TRIGGER_OFF_RATIO = 3.0;
const STA_LTA_CONFIRM_MS = 300;
const TAUCPD_EVAL_MS = 2000;
const TAUC_ALERT_THRESHOLD = 0.6;
const PD_ALERT_THRESHOLD = 0.001;
const FILE_PLAYBACK_LTA_WARMUP_POINTS = 20;
const FILE_PLAYBACK_STARTUP_RATIO_CAP = 3.8;
const LTA_SEED_TAIL_SECONDS = 2;
const LTA_SEED_HISTORY_SECONDS = 30;
const EARTHQUAKE_FILE_CATALOG = [
    {
        name: "0528_u.json",
        stationName: "AK:Eagle River;Alaska Geol Mat",
        sampleRateHz: 200,
        sampleCount: 10000,
    },
    {
        name: "0593_u.json",
        stationName: "AK:Fairbanks; Geoph Inst, CIGO",
        sampleRateHz: 200,
        sampleCount: 46800,
    },
    {
        name: "1356_u.json",
        stationName: "AK; Anchorage; BP Bldg.",
        sampleRateHz: 200,
        sampleCount: 24200,
    },
    {
        name: "1397_u.json",
        stationName: "AK: Anchorage;NOAA Weather Fac",
        sampleRateHz: 200,
        sampleCount: 32600,
    },
    {
        name: "1713_u.json",
        stationName: "AK: Valdez; Civic Center",
        sampleRateHz: 200,
        sampleCount: 14200,
    },
    {
        name: "1727_u.json",
        stationName: "AK: Valdez; Dock Company",
        sampleRateHz: 200,
        sampleCount: 17200,
    },
    {
        name: "1728_u.json",
        stationName: "AK: Valdez; City Hall",
        sampleRateHz: 200,
        sampleCount: 34000,
    },
    {
        name: "1731_u.json",
        stationName: "AK: Anchorage; DOI OAS",
        sampleRateHz: 200,
        sampleCount: 34400,
    },
    {
        name: "1734_u.json",
        stationName: "AK: Anchorage; New FS 7",
        sampleRateHz: 200,
        sampleCount: 37600,
    },
    {
        name: "1737_u.json",
        stationName: "AK: Anchorage; New FS 1",
        sampleRateHz: 200,
        sampleCount: 33800,
    },
    {
        name: "2178_u.json",
        stationName: "AK:Fairbanks; Univ of Alaska",
        sampleRateHz: 200,
        sampleCount: 16719,
    },
    {
        name: "4005_u.json",
        stationName: "AK: Moose Creek Dam",
        sampleRateHz: 200,
        sampleCount: 10623,
    },
    {
        name: "5609_u.json",
        stationName: "AK:Fairbanks; Ester FS",
        sampleRateHz: 200,
        sampleCount: 60000,
    },
    {
        name: "k202_u.json",
        stationName: "K2-02",
        sampleRateHz: 200,
        sampleCount: 64200,
    },
    {
        name: "k203_u.json",
        stationName: "K2-03",
        sampleRateHz: 200,
        sampleCount: 56000,
    },
    {
        name: "k204_u.json",
        stationName: "K2-04",
        sampleRateHz: 200,
        sampleCount: 70200,
    },
    {
        name: "k205_u.json",
        stationName: "K2-05",
        sampleRateHz: 200,
        sampleCount: 69600,
    },
    {
        name: "k206_u.json",
        stationName: "K2-06",
        sampleRateHz: 200,
        sampleCount: 58600,
    },
];

const DEFAULT_PUBLIC_JSON_FILES = EARTHQUAKE_FILE_CATALOG.map((file) => ({
    ...file,
    label: file.stationName || file.name,
    durationSec: file.sampleCount / file.sampleRateHz,
}));

const EARTHQUAKE_FILE_INDEX = Object.fromEntries(
    DEFAULT_PUBLIC_JSON_FILES.map((file) => [file.name, file]),
);

/**
 * @typedef {Object} DataPacket
 * @property {number} raw_vec
 * @property {number} accel_vec
 * @property {number=} corrected_accel_vec
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
    const correctedAccelVec =
        packet.corrected_accel_vec ?? packet.correctedAccelVec ?? null;
    const state = typeof packet.state === "string" ? packet.state : "M";
    const filteredX = packet.filteredX ?? 0;
    const filteredY = packet.filteredY ?? 0;
    const filteredZ = packet.filteredZ ?? 0;

    return {
        index,
        timestamp: Date.now(),
        timeLabel: new Date().toLocaleTimeString(),
        raw_vec: rawVec,
        accel_vec: accelVec,
        corrected_accel_vec: correctedAccelVec,
        state,
        filteredX,
        filteredY,
        filteredZ,
        sta: packet.sta,
        lta: packet.lta,
        tauc: packet.tauc,
        pd: packet.pd,
    };
}

function createZeroPoint(timestamp, index = 1) {
    return {
        index,
        timestamp,
        timeLabel: new Date(timestamp).toLocaleTimeString(),
        raw_vec: 0,
        accel_vec: 0,
        corrected_accel_vec: null,
        filteredX: 0,
        filteredY: 0,
        filteredZ: 0,
        state: "M",
        sta: 0,
        lta: 0,
        tauc: 0,
        pd: 0,
        isSeed: true,
    };
}

function estimateLtaSeedFromTail(samples, sampleRateHz) {
    const safeSampleRate =
        Number.isFinite(sampleRateHz) && sampleRateHz > 0
            ? sampleRateHz
            : USB_SAMPLE_RATE_HZ;
    const tailCount = Math.max(
        2,
        Math.round(safeSampleRate * LTA_SEED_TAIL_SECONDS),
    );
    const tailSamples = samples.slice(-tailCount);

    const ltaCandidates = tailSamples
        .map((sample) => {
            const lta = Number(sample?.lta);
            if (Number.isFinite(lta) && lta > 0) {
                return lta;
            }

            const accel = Number(
                sample?.accel_vec ??
                    sample?.filtered_accel ??
                    sample?.raw_vec ??
                    0,
            );
            return Number.isFinite(accel) ? Math.abs(accel) : null;
        })
        .filter((value) => Number.isFinite(value) && value > 0);

    if (ltaCandidates.length === 0) {
        return 1;
    }

    const historyLength = Math.max(
        ltaCandidates.length,
        Math.round(safeSampleRate * LTA_SEED_HISTORY_SECONDS),
    );

    let baseline = ltaCandidates[0];
    for (let i = 0; i < historyLength; i += 1) {
        const sample = ltaCandidates[i % ltaCandidates.length];
        baseline = baseline * 0.96 + sample * 0.04;
    }

    return Math.max(baseline, 0.05);
}

function normalizeAvailableFiles(files) {
    const source = Array.isArray(files) ? files : [];

    return source
        .map((entry) =>
            typeof entry === "string" ? { name: entry } : entry || {},
        )
        .filter((entry) => typeof entry.name === "string")
        .map((entry) => {
            const catalog = EARTHQUAKE_FILE_INDEX[entry.name] || {};
            const sampleRateHz = Number(
                entry.sampleRateHz ?? catalog.sampleRateHz,
            );
            const sampleCount = Number(
                entry.sampleCount ?? catalog.sampleCount,
            );

            const durationSec =
                Number.isFinite(sampleRateHz) &&
                sampleRateHz > 0 &&
                Number.isFinite(sampleCount) &&
                sampleCount > 0
                    ? sampleCount / sampleRateHz
                    : Number(entry.durationSec ?? catalog.durationSec);

            return {
                name: entry.name,
                label: entry.label ?? catalog.label ?? entry.name,
                stationName: entry.stationName ?? catalog.stationName ?? "",
                sampleRateHz: Number.isFinite(sampleRateHz)
                    ? sampleRateHz
                    : null,
                sampleCount: Number.isFinite(sampleCount) ? sampleCount : null,
                durationSec: Number.isFinite(durationSec) ? durationSec : null,
            };
        })
        .sort((a, b) => a.name.localeCompare(b.name));
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
    const [isUsbAvailable, setIsUsbAvailable] = useState(false);
    const [isUsbConnecting, setIsUsbConnecting] = useState(false);
    const [usbConnectionError, setUsbConnectionError] = useState("");
    const [isPlayingFile, setIsPlayingFile] = useState(false);
    const [filePlaybackProgress, setFilePlaybackProgress] = useState(0);
    const [fileMetadata, setFileMetadata] = useState(null);
    const [filePlaybackError, setFilePlaybackError] = useState("");
    const [showFileSelector, setShowFileSelector] = useState(false);
    const [availableFiles, setAvailableFiles] = useState([]);
    const [loadingFiles, setLoadingFiles] = useState(false);
    const activeAnimationRef = useRef(null);
    const filePlaybackRef = useRef(null);

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
                corrected_accel_vec: mix(
                    Number.isFinite(animation.from.corrected_accel_vec)
                        ? animation.from.corrected_accel_vec
                        : animation.from.accel_vec,
                    Number.isFinite(animation.to.corrected_accel_vec)
                        ? animation.to.corrected_accel_vec
                        : animation.to.accel_vec,
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

        const syncAvailableState = async () => {
            if (!serial || typeof serial.getPorts !== "function") {
                return;
            }

            try {
                const ports = await serial.getPorts();
                if (isMounted) {
                    setIsUsbAvailable(ports.length > 0);
                }
            } catch (error) {
                console.warn("Unable to query USB connection state.", error);
            }
        };

        const handleConnect = () => {
            if (isMounted) {
                setIsUsbAvailable(true);
            }
        };

        const handleDisconnect = async () => {
            if (!isMounted) {
                return;
            }

            setIsUsbAvailable(false);
            setIsUsbConnected(false);
            if (stopUsbPacketListener) {
                await stopUsbPacketListener();
                stopUsbPacketListener = null;
            }
        };

        if (serial) {
            serial.addEventListener("connect", handleConnect);
            serial.addEventListener("disconnect", handleDisconnect);
            syncAvailableState();
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
        window.startUsbPacketListener = async (baudRate = 115200) => {
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
                setIsUsbAvailable(true);
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
                syncAvailableState();
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

    const handleDisconnectDevice = async () => {
        if (isUsbConnecting) {
            return;
        }

        try {
            if (window.stopUsbPacketListener) {
                await window.stopUsbPacketListener();
            }
        } catch (error) {
            console.error("Unable to stop USB packet listener.", error);
            setUsbConnectionError("Unable to disconnect device.");
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

    const hasCorrectedSeries = useMemo(() => {
        return chartData.some((point) =>
            Number.isFinite(point.corrected_accel_vec),
        );
    }, [chartData]);

    const xAxisEnd = useMemo(() => {
        if (chartData.length === 0) {
            return now;
        }

        return Math.max(now, chartData[chartData.length - 1].timestamp);
    }, [chartData, now]);

    const yDomain = useMemo(() => {
        return [-yScaleMax, yScaleMax];
    }, [yScaleMax]);

    const staLtaRatioData = useMemo(() => {
        return chartData.map((point) => {
            const safeRatio =
                Number.isFinite(point.lta) && Math.abs(point.lta) > 1e-6
                    ? point.sta / point.lta
                    : null;

            return {
                ...point,
                sta_lta_ratio: Number.isFinite(safeRatio) ? safeRatio : null,
            };
        });
    }, [chartData]);

    const chartDisplayData = useMemo(() => {
        if (staLtaRatioData.length <= 1) {
            return staLtaRatioData;
        }

        const sampled = [staLtaRatioData[0]];
        let lastDisplayedTimestamp = staLtaRatioData[0].timestamp;
        let previousRatio = staLtaRatioData[0].sta_lta_ratio;
        let previousState = staLtaRatioData[0].state;

        for (let i = 1; i < staLtaRatioData.length; i += 1) {
            const point = staLtaRatioData[i];
            const ratio = point.sta_lta_ratio;
            const stateChanged = point.state !== previousState;
            const triggeredOn =
                Number.isFinite(previousRatio) &&
                Number.isFinite(ratio) &&
                previousRatio < TRIGGER_ON_RATIO &&
                ratio >= TRIGGER_ON_RATIO;
            const triggeredOff =
                Number.isFinite(previousRatio) &&
                Number.isFinite(ratio) &&
                previousRatio > TRIGGER_OFF_RATIO &&
                ratio <= TRIGGER_OFF_RATIO;
            const isLastPoint = i === staLtaRatioData.length - 1;
            const isTimeToSample =
                point.timestamp - lastDisplayedTimestamp >=
                DISPLAY_UPDATE_INTERVAL_MS;

            if (
                stateChanged ||
                triggeredOn ||
                triggeredOff ||
                isTimeToSample ||
                isLastPoint
            ) {
                sampled.push(point);
                lastDisplayedTimestamp = point.timestamp;
            }

            previousRatio = ratio;
            previousState = point.state;
        }

        return sampled;
    }, [staLtaRatioData]);

    const staLtaRatioYMax = useMemo(() => {
        if (chartDisplayData.length === 0) {
            return 1;
        }

        const maxValue = chartDisplayData.reduce((acc, point) => {
            if (!Number.isFinite(point.sta_lta_ratio)) {
                return acc;
            }

            return Math.max(acc, point.sta_lta_ratio);
        }, 0);

        return Math.max(maxValue * 1.1, 0.2);
    }, [chartDisplayData]);

    const collectingEvents = useMemo(() => {
        /** @type {Array<{ timestamp: number; type: "on" | "off" }>} */
        const events = [];
        let isTriggered = false;
        let previousRatio = null;

        for (const point of staLtaRatioData) {
            const ratio = point.sta_lta_ratio;
            if (!Number.isFinite(ratio)) {
                previousRatio = null;
                continue;
            }

            if (previousRatio === null) {
                isTriggered = ratio >= TRIGGER_ON_RATIO;
                previousRatio = ratio;
                continue;
            }

            if (
                !isTriggered &&
                previousRatio < TRIGGER_ON_RATIO &&
                ratio >= TRIGGER_ON_RATIO
            ) {
                events.push({ timestamp: point.timestamp, type: "on" });
                isTriggered = true;
            }

            if (
                isTriggered &&
                previousRatio > TRIGGER_OFF_RATIO &&
                ratio <= TRIGGER_OFF_RATIO
            ) {
                events.push({ timestamp: point.timestamp, type: "off" });
                isTriggered = false;
            }

            previousRatio = ratio;
        }

        return events;
    }, [staLtaRatioData]);

    const earthquakeDetectedEvents = useMemo(() => {
        const events = [];
        const detectionWindowMs = STA_LTA_CONFIRM_MS + TAUCPD_EVAL_MS;

        let previousState = null;

        for (const point of chartData) {
            const currentState =
                typeof point.state === "string" ? point.state : null;

            if (currentState === "A" && previousState !== "A") {
                events.push({ timestamp: point.timestamp, type: "detected" });
            }

            previousState = currentState;
        }

        for (let i = 0; i < collectingEvents.length; i += 1) {
            const event = collectingEvents[i];
            if (event.type !== "on") {
                continue;
            }

            const confirmEnd = event.timestamp + STA_LTA_CONFIRM_MS;
            const evalEnd = event.timestamp + detectionWindowMs;

            const confirmPoints = chartData.filter(
                (point) =>
                    point.timestamp >= event.timestamp &&
                    point.timestamp <= confirmEnd &&
                    Number.isFinite(point.sta_lta_ratio),
            );

            if (
                confirmPoints.some(
                    (point) => point.sta_lta_ratio < TRIGGER_ON_RATIO,
                )
            ) {
                continue;
            }

            const evalPoints = chartData.filter(
                (point) =>
                    point.timestamp >= event.timestamp &&
                    point.timestamp <= evalEnd,
            );

            const maxTauC = evalPoints.reduce((max, point) => {
                return Number.isFinite(point.tauc)
                    ? Math.max(max, point.tauc)
                    : max;
            }, 0);
            const maxPd = evalPoints.reduce((max, point) => {
                return Number.isFinite(point.pd)
                    ? Math.max(max, point.pd)
                    : max;
            }, 0);

            if (maxTauC > TAUC_ALERT_THRESHOLD && maxPd > PD_ALERT_THRESHOLD) {
                events.push({ timestamp: evalEnd, type: "detected" });
            }
        }

        return Array.from(
            new Map(events.map((event) => [event.timestamp, event])).values(),
        );
    }, [chartData, collectingEvents]);

    const visibleCollectingEvents = useMemo(() => {
        const windowStart = xAxisEnd - WINDOW_MS;
        return collectingEvents.filter(
            (event) =>
                event.timestamp >= windowStart && event.timestamp <= xAxisEnd,
        );
    }, [collectingEvents, xAxisEnd]);

    const visibleEarthquakeAlertAreas = useMemo(() => {
        const windowStart = xAxisEnd - WINDOW_MS;
        const windowEnd = xAxisEnd;
        const alertDurationMs = 10_000;

        const mergedIntervals = [];
        const sortedIntervals = earthquakeDetectedEvents
            .map((event) => ({
                start: event.timestamp,
                end: event.timestamp + alertDurationMs,
            }))
            .sort((a, b) => a.start - b.start);

        for (const interval of sortedIntervals) {
            const lastInterval = mergedIntervals[mergedIntervals.length - 1];

            if (!lastInterval || interval.start > lastInterval.end) {
                mergedIntervals.push({ ...interval });
                continue;
            }

            lastInterval.end = Math.max(lastInterval.end, interval.end);
        }

        return mergedIntervals
            .map((interval) => {
                const x1 = Math.max(interval.start, windowStart);
                const x2 = Math.min(interval.end, windowEnd);
                const fadeInScale = Math.min(
                    1,
                    (x2 - windowStart) / EARTHQUAKE_AREA_EDGE_FADE_MS,
                );
                const fadeOutScale = Math.min(
                    1,
                    (windowEnd - x1) / EARTHQUAKE_AREA_EDGE_FADE_MS,
                );
                const opacityScale = Math.max(
                    0,
                    Math.min(fadeInScale, fadeOutScale),
                );

                return {
                    x1,
                    x2,
                    fillOpacity: EARTHQUAKE_AREA_BASE_OPACITY * opacityScale,
                    strokeOpacity:
                        EARTHQUAKE_AREA_BASE_STROKE_OPACITY * opacityScale,
                    showLabel: opacityScale >= 0.45,
                };
            })
            .filter((interval) => interval.x2 > interval.x1);
    }, [earthquakeDetectedEvents, xAxisEnd]);

    useEffect(() => {
        if (chartData.length === 0) {
            return;
        }

        const maxAbs = chartData.reduce((acc, point) => {
            const corrected = Number.isFinite(point.corrected_accel_vec)
                ? Math.abs(point.corrected_accel_vec)
                : 0;

            return Math.max(
                acc,
                Math.abs(point.raw_vec),
                Math.abs(point.accel_vec),
                corrected,
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

    const stopFilePlayback = async () => {
        if (filePlaybackRef.current) {
            filePlaybackRef.current.shouldStop = true;
            filePlaybackRef.current = null;
        }
        setIsPlayingFile(false);
        setFilePlaybackProgress(0);
    };

    const handleFileFromPublic = async (fileName) => {
        setFilePlaybackError("");
        setShowFileSelector(false);

        try {
            // Stop any existing file playback
            await stopFilePlayback();

            // Disconnect USB device
            if (stopUsbPacketListener) {
                await stopUsbPacketListener();
                stopUsbPacketListener = null;
            }
            setIsUsbConnected(false);

            // Fetch JSON file from public folder
            const response = await fetch(`/${fileName}`);
            if (!response.ok) {
                throw new Error(
                    `Failed to load ${fileName}: ${response.statusText}`,
                );
            }
            const jsonData = await response.json();

            if (
                !jsonData.timeSeries?.samples ||
                !Array.isArray(jsonData.timeSeries.samples)
            ) {
                throw new Error(
                    "Invalid JSON file format: missing timeSeries.samples",
                );
            }

            const samples = jsonData.timeSeries.samples;
            if (samples.length === 0) {
                throw new Error("No samples in JSON file");
            }

            const jsonSampleRate = jsonData.metadata?.sampleRateHz || 200;
            const seededLtaBaseline = estimateLtaSeedFromTail(
                samples,
                jsonSampleRate,
            );

            setFileMetadata(jsonData.metadata);
            setIsPlayingFile(true);

            // Create a playback context
            const playbackContext = {
                shouldStop: false,
                currentIndex: 0,
                displayedPointCount: 0,
                startTime: Date.now(),
                lastPacketTime: Date.now(),
                ltaBaseline: seededLtaBaseline,
                samples: samples,
            };
            filePlaybackRef.current = playbackContext;

            // Clear existing series and start fresh
            setSeries([createZeroPoint(Date.now())]);

            // Start playback loop
            const playbackLoop = () => {
                if (!playbackContext || playbackContext.shouldStop) {
                    setIsPlayingFile(false);
                    return;
                }

                const now = Date.now();
                const elapsedMs = now - playbackContext.startTime;
                const usbSampleRate = USB_SAMPLE_RATE_HZ;

                // Calculate downsample ratio: how many JSON samples per USB sample
                const downsampleRatio = Math.max(
                    1,
                    Math.round(jsonSampleRate / usbSampleRate),
                );

                // Time per USB sample (at our target rate)
                const msPerUsbSample = 1000 / usbSampleRate;

                // Calculate how many USB samples we should have delivered by now
                let targetUsbSampleCount = Math.floor(
                    elapsedMs / msPerUsbSample,
                );
                targetUsbSampleCount = Math.min(
                    targetUsbSampleCount,
                    Math.ceil(samples.length / downsampleRatio) - 1,
                );

                // Track the latest sample to display
                let latestSample = null;

                // Advance through samples, but only display at throttled intervals
                while (
                    playbackContext.currentIndex <= targetUsbSampleCount &&
                    !playbackContext.shouldStop
                ) {
                    // Get the corresponding JSON sample (skip samples based on downsample ratio)
                    const jsonSampleIndex =
                        playbackContext.currentIndex * downsampleRatio;
                    const sample =
                        samples[Math.min(jsonSampleIndex, samples.length - 1)];

                    if (sample) {
                        latestSample = sample;
                    }
                    playbackContext.currentIndex++;
                }

                // Only push a packet every 0.25 seconds
                if (
                    latestSample &&
                    now - playbackContext.lastPacketTime >=
                        DISPLAY_UPDATE_INTERVAL_MS
                ) {
                    const staValue = Number(latestSample.sta ?? 0);
                    const rawLtaValue = Number(latestSample.lta ?? 0);
                    const staSafe = Number.isFinite(staValue) ? staValue : 0;
                    const ltaCandidate =
                        Number.isFinite(rawLtaValue) && rawLtaValue > 0
                            ? rawLtaValue
                            : null;

                    if (ltaCandidate !== null) {
                        playbackContext.ltaBaseline =
                            playbackContext.ltaBaseline * 0.96 +
                            ltaCandidate * 0.04;
                    } else {
                        const staForBaseline = Math.max(staSafe, 0.05);
                        playbackContext.ltaBaseline =
                            playbackContext.ltaBaseline * 0.98 +
                            staForBaseline * 0.02;
                    }

                    let safeLta = ltaCandidate ?? playbackContext.ltaBaseline;

                    // During startup, guard against tiny LTAs that would instantly exceed trigger-on.
                    if (
                        playbackContext.displayedPointCount <
                        FILE_PLAYBACK_LTA_WARMUP_POINTS
                    ) {
                        const startupMinLta =
                            Math.max(staSafe, 0.05) /
                            FILE_PLAYBACK_STARTUP_RATIO_CAP;
                        safeLta = Math.max(safeLta, startupMinLta);
                    }

                    const packet = {
                        raw_vec:
                            latestSample.raw_vec ?? latestSample.raw_accel ?? 0,
                        accel_vec:
                            latestSample.accel_vec ??
                            latestSample.filtered_accel ??
                            0,
                        corrected_accel_vec:
                            latestSample.corrected_accel_vec ?? null,
                        filteredX: latestSample.filteredX ?? 0,
                        filteredY: latestSample.filteredY ?? 0,
                        filteredZ: latestSample.filteredZ ?? 0,
                        sta: staSafe,
                        lta: safeLta,
                        tauc: latestSample.tauc ?? 0,
                        pd: latestSample.pd ?? 0,
                    };

                    pushLiveDataPacket(packet);
                    playbackContext.lastPacketTime = now;
                    playbackContext.displayedPointCount += 1;
                }

                // Update progress based on actual JSON samples consumed
                const jsonSamplesConsumed =
                    playbackContext.currentIndex * downsampleRatio;
                const progress = (jsonSamplesConsumed / samples.length) * 100;
                setFilePlaybackProgress(progress);

                // Check if we're done
                const totalUsbSamples = Math.ceil(
                    samples.length / downsampleRatio,
                );
                if (
                    playbackContext.currentIndex >= totalUsbSamples &&
                    !playbackContext.shouldStop
                ) {
                    setIsPlayingFile(false);
                    playbackContext.shouldStop = true;
                    return;
                }

                // Schedule next iteration
                requestAnimationFrame(playbackLoop);
            };

            requestAnimationFrame(playbackLoop);
        } catch (error) {
            console.error("Error loading file:", error);
            setFilePlaybackError(error.message || "Failed to load file");
            setIsPlayingFile(false);
        }
    };

    const handleLoadFileClick = async () => {
        setShowFileSelector(true);
        if (availableFiles.length === 0) {
            setLoadingFiles(true);
            try {
                // Fetch list of available JSON files from public folder
                const response = await fetch("/api/files.json");
                if (response.ok) {
                    const data = await response.json();
                    const files = normalizeAvailableFiles(data.files);
                    setAvailableFiles(
                        files.length > 0 ? files : DEFAULT_PUBLIC_JSON_FILES,
                    );
                } else {
                    // Fallback: hardcoded list
                    setAvailableFiles(DEFAULT_PUBLIC_JSON_FILES);
                }
            } catch (error) {
                console.warn(
                    "Could not fetch file list, using defaults",
                    error,
                );
                setAvailableFiles(DEFAULT_PUBLIC_JSON_FILES);
            } finally {
                setLoadingFiles(false);
            }
        }
    };

    return (
        <main className="flex min-h-screen items-center bg-gradient-to-b from-background via-background to-muted/20 px-4 py-6 md:px-8">
            {showFileSelector && (
                <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 backdrop-blur-sm">
                    <Card className="h-[88vh] w-[min(96vw,1100px)] border border-slate-900/10 bg-background shadow-xl">
                        <CardHeader className="border-b">
                            <CardTitle>Load JSON File</CardTitle>
                            <CardDescription>
                                Browse and select an earthquake dataset for
                                playback.
                            </CardDescription>
                        </CardHeader>
                        <CardContent className="flex min-h-0 flex-1 flex-col py-4">
                            {loadingFiles ? (
                                <p className="text-center text-sm text-muted-foreground py-4">
                                    Loading available files...
                                </p>
                            ) : availableFiles.length > 0 ? (
                                <>
                                    <div className="mb-3 flex items-center justify-between px-1 text-xs text-muted-foreground">
                                        <span>
                                            {availableFiles.length} datasets
                                        </span>
                                        <span>
                                            Scroll to view all earthquakes
                                        </span>
                                    </div>
                                    <div className="grid grid-cols-[1.8fr_0.7fr_0.8fr_0.8fr] gap-2 rounded-md border border-slate-900/10 bg-slate-900/5 px-3 py-2 text-[11px] font-semibold uppercase tracking-wide text-muted-foreground dark:border-blue-100/15 dark:bg-blue-950/25">
                                        <span>Station</span>
                                        <span>Rate</span>
                                        <span>Samples</span>
                                        <span>Duration</span>
                                    </div>
                                    <div className="mt-2 flex-1 space-y-2 overflow-y-auto pr-1">
                                        {availableFiles.map((file) => (
                                            <button
                                                key={file.name}
                                                onClick={() =>
                                                    handleFileFromPublic(
                                                        file.name,
                                                    )
                                                }
                                                className="w-full rounded-lg border border-slate-900/10 bg-slate-900/5 px-3 py-2 text-left transition-colors hover:bg-blue-950/15 dark:border-blue-100/15 dark:bg-blue-950/25 dark:hover:bg-blue-900/40"
                                            >
                                                <div className="grid grid-cols-[1.8fr_0.7fr_0.8fr_0.8fr] gap-2 text-sm text-foreground">
                                                    <div className="min-w-0">
                                                        <div className="truncate font-medium">
                                                            {file.stationName ||
                                                                file.label ||
                                                                file.name}
                                                        </div>
                                                        <div className="truncate text-xs text-muted-foreground">
                                                            {file.name}
                                                        </div>
                                                    </div>
                                                    <div className="text-xs text-muted-foreground">
                                                        {Number.isFinite(
                                                            file.sampleRateHz,
                                                        )
                                                            ? `${file.sampleRateHz} Hz`
                                                            : "-"}
                                                    </div>
                                                    <div className="text-xs text-muted-foreground">
                                                        {Number.isFinite(
                                                            file.sampleCount,
                                                        )
                                                            ? file.sampleCount.toLocaleString()
                                                            : "-"}
                                                    </div>
                                                    <div className="text-xs text-muted-foreground">
                                                        {Number.isFinite(
                                                            file.durationSec,
                                                        )
                                                            ? `${(
                                                                  file.durationSec /
                                                                  60
                                                              ).toFixed(1)} min`
                                                            : "-"}
                                                    </div>
                                                </div>
                                            </button>
                                        ))}
                                    </div>
                                </>
                            ) : (
                                <p className="text-center text-sm text-destructive py-4">
                                    No JSON files found
                                </p>
                            )}
                        </CardContent>
                        <div className="flex justify-end gap-2 border-t px-6 py-3">
                            <Button
                                variant="outline"
                                onClick={() => setShowFileSelector(false)}
                            >
                                Cancel
                            </Button>
                        </div>
                    </Card>
                </div>
            )}
            <div className="mx-auto flex min-h-[calc(100vh-6rem)] w-full max-w-7xl flex-col justify-center gap-4">
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
                                    data={chartDisplayData}
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
                                    {visibleEarthquakeAlertAreas.map((area) => (
                                        <ReferenceArea
                                            key={`${area.x1}-${area.x2}`}
                                            x1={area.x1}
                                            x2={area.x2}
                                            y1={yDomain[0]}
                                            y2={yDomain[1]}
                                            fill="#dc2626"
                                            fillOpacity={area.fillOpacity}
                                            stroke="#dc2626"
                                            strokeOpacity={area.strokeOpacity}
                                            label={
                                                area.showLabel
                                                    ? {
                                                          value: "Earthquake Detected",
                                                          position:
                                                              "insideTopRight",
                                                          fill: "#dc2626",
                                                          fontSize: 11,
                                                          dy: EARTHQUAKE_LABEL_DY,
                                                      }
                                                    : undefined
                                            }
                                        />
                                    ))}
                                    {visibleCollectingEvents.map((event) => (
                                        <ReferenceLine
                                            key={`${event.type}-${event.timestamp}`}
                                            x={event.timestamp}
                                            stroke={
                                                event.type === "on"
                                                    ? "#22c55e"
                                                    : "#f59e0b"
                                            }
                                            strokeDasharray="6 4"
                                            strokeWidth={1.4}
                                            label={{
                                                value:
                                                    event.type === "on"
                                                        ? "Collecting On"
                                                        : "Collecting Off",
                                                position: "insideTopRight",
                                                fill:
                                                    event.type === "on"
                                                        ? "#22c55e"
                                                        : "#f59e0b",
                                                fontSize: 10,
                                                dy:
                                                    event.type === "on"
                                                        ? COLLECTING_ON_LABEL_DY
                                                        : COLLECTING_OFF_LABEL_DY,
                                            }}
                                        />
                                    ))}
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
                                        name="Raw Acceleration"
                                        stroke="#94a3b8"
                                        strokeWidth={1.75}
                                        strokeDasharray="4 4"
                                        dot={false}
                                        isAnimationActive={false}
                                    />
                                    <Line
                                        type="linear"
                                        dataKey="accel_vec"
                                        name="Filtered Acceleration"
                                        stroke="#3b82f6"
                                        strokeWidth={2.75}
                                        dot={false}
                                        isAnimationActive={false}
                                    />
                                    {hasCorrectedSeries ? (
                                        <Line
                                            type="linear"
                                            dataKey="corrected_accel_vec"
                                            name="Station Corrected Acceleration"
                                            stroke="#f59e0b"
                                            strokeWidth={1.6}
                                            strokeDasharray="4 4"
                                            strokeOpacity={0.6}
                                            dot={false}
                                            isAnimationActive={false}
                                        />
                                    ) : null}
                                </LineChart>
                            </ResponsiveContainer>
                        </CardContent>
                    </Card>

                    <div className="flex h-[76vh] flex-col gap-4 lg:col-span-2">
                        <div className="grid grid-cols-2 gap-3">
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

                        <Card className="py-0 border border-slate-900/10 bg-slate-900/5 transition-shadow duration-300 hover:shadow-sm dark:border-blue-100/15 dark:bg-blue-950/25 flex flex-1 flex-col min-h-0">
                            <CardHeader className="border-b py-4">
                                <CardTitle>STA/LTA Ratio Over Time</CardTitle>
                                <CardDescription>
                                    Live ratio of STA divided by LTA.
                                </CardDescription>
                            </CardHeader>
                            <CardContent className="min-h-0 flex flex-1 flex-col px-2 pb-4 pt-5 sm:px-4">
                                <ResponsiveContainer width="100%" height="100%">
                                    <LineChart
                                        data={chartDisplayData}
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
                                            type="number"
                                            domain={[
                                                0,
                                                Math.max(
                                                    staLtaRatioYMax,
                                                    TRIGGER_ON_RATIO * 1.05,
                                                ),
                                            ]}
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
                                                new Date(
                                                    label,
                                                ).toLocaleTimeString([], {
                                                    minute: "2-digit",
                                                    second: "2-digit",
                                                })
                                            }
                                        />
                                        <ReferenceLine
                                            y={TRIGGER_ON_RATIO}
                                            stroke="#ef4444"
                                            strokeDasharray="6 4"
                                            strokeWidth={1.5}
                                            label={{
                                                value: `Collecting On (${TRIGGER_ON_RATIO.toFixed(1)})`,
                                                position: "insideTopRight",
                                                fill: "#ef4444",
                                                fontSize: 11,
                                            }}
                                        />
                                        <ReferenceLine
                                            y={TRIGGER_OFF_RATIO}
                                            stroke="#f59e0b"
                                            strokeDasharray="6 4"
                                            strokeWidth={1.5}
                                            label={{
                                                value: `Collecting Off (${TRIGGER_OFF_RATIO.toFixed(1)})`,
                                                position: "insideTopRight",
                                                fill: "#f59e0b",
                                                fontSize: 11,
                                            }}
                                        />
                                        <Legend
                                            verticalAlign="top"
                                            height={24}
                                        />
                                        <Line
                                            type="linear"
                                            dataKey="sta_lta_ratio"
                                            name="STA/LTA Ratio"
                                            stroke="#10b981"
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
                                    : isUsbAvailable
                                      ? "text-amber-500"
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
                                    : isUsbAvailable
                                      ? "bg-amber-500"
                                      : "bg-slate-400"
                            }`}
                        />
                        <span>
                            {isUsbConnected
                                ? "Connected"
                                : isUsbAvailable
                                  ? "Available"
                                  : "Disconnected"}
                        </span>
                        <button
                            type="button"
                            className="ml-1 font-medium text-blue-600 transition-colors hover:text-blue-500 dark:text-blue-400 dark:hover:text-blue-300"
                            onClick={
                                isUsbConnected
                                    ? handleDisconnectDevice
                                    : handleConnectDevice
                            }
                            disabled={isUsbConnecting}
                        >
                            {isUsbConnecting
                                ? "Connecting..."
                                : isUsbConnected
                                  ? "Disconnect"
                                  : "Connect"}
                        </button>
                    </div>

                    <div className="flex gap-2 flex-wrap sm:flex-nowrap">
                        {!isPlayingFile ? (
                            <Button
                                variant="default"
                                className="flex items-center gap-2"
                                onClick={handleLoadFileClick}
                            >
                                <Upload className="h-4 w-4" />
                                Load JSON File
                            </Button>
                        ) : (
                            <Button
                                variant="destructive"
                                className="flex items-center gap-2"
                                onClick={stopFilePlayback}
                            >
                                <X className="h-4 w-4" />
                                Stop Playback
                            </Button>
                        )}
                    </div>
                </div>

                {filePlaybackError ? (
                    <div className="flex items-center gap-2 rounded-lg border border-destructive/50 bg-destructive/10 p-3 text-sm text-destructive">
                        <AlertCircle className="h-4 w-4 flex-shrink-0" />
                        <span>{filePlaybackError}</span>
                    </div>
                ) : null}

                {isPlayingFile && (
                    <div className="space-y-2">
                        <div className="flex items-center justify-between text-sm">
                            <span className="font-medium text-foreground">
                                {fileMetadata?.stationName || "File Playback"}
                            </span>
                            <span className="text-muted-foreground">
                                {Math.round(filePlaybackProgress)}%
                            </span>
                        </div>
                        <div className="h-2 w-full rounded-full bg-slate-900/20 overflow-hidden dark:bg-blue-950/30">
                            <div
                                className="h-full bg-gradient-to-r from-blue-500 to-blue-600 transition-all duration-300"
                                style={{ width: `${filePlaybackProgress}%` }}
                            />
                        </div>
                    </div>
                )}

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
