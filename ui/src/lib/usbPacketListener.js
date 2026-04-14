/**
 * @typedef {Object} LivePacket
 * @property {number} raw_vec
 * @property {number} accel_vec
 * @property {number} filteredX
 * @property {number} filteredY
 * @property {number} filteredZ
 * @property {number} raw_accel
 * @property {number} filtered_accel
 * @property {number} sta
 * @property {number} lta
 * @property {number} tauc
 * @property {number} pd
 */

/**
 * @typedef {Object} UsbPacketListenerOptions
 * @property {number} [baudRate]
 * @property {(packet: LivePacket) => void} onPacket
 * @property {(error: unknown) => void} [onError]
 * @property {(rawChunk: string) => void} [onRawData]
 */

function formatPortLabel(usbPort) {
    const info = usbPort.getInfo();
    const vendor =
        typeof info.usbVendorId === "number"
            ? `0x${info.usbVendorId.toString(16)}`
            : "unknown-vendor";
    const product =
        typeof info.usbProductId === "number"
            ? `0x${info.usbProductId.toString(16)}`
            : "unknown-product";
    return `${vendor}:${product}`;
}

/**
 * Start console logging for USB serial connect/disconnect events.
 * @returns {() => void} cleanup function
 */
export function monitorUsbDeviceConnections() {
    if (!("serial" in navigator)) {
        console.warn("Web Serial API is not available in this browser.");
        return () => {};
    }

    const serial = navigator.serial;
    const onConnect = (event) => {
        if (event?.port) {
            console.info(
                "USB serial device detected:",
                formatPortLabel(event.port),
            );
            return;
        }

        console.info("USB serial device detected.");
    };

    const onDisconnect = (event) => {
        if (event?.port) {
            console.info(
                "USB serial device disconnected:",
                formatPortLabel(event.port),
            );
            return;
        }

        console.info("USB serial device disconnected.");
    };

    serial.addEventListener("connect", onConnect);
    serial.addEventListener("disconnect", onDisconnect);

    console.info("USB serial connection monitor is active.");
    serial
        .getPorts()
        .then((ports) => {
            for (const port of ports) {
                console.info(
                    "USB serial device already available:",
                    formatPortLabel(port),
                );
            }
        })
        .catch((error) => {
            console.warn("Unable to query existing USB serial devices.", error);
        });

    return () => {
        serial.removeEventListener("connect", onConnect);
        serial.removeEventListener("disconnect", onDisconnect);
    };
}

/**
 * Listen on a USB serial device for newline-delimited JSON packets.
 * Expected payload fields (new schema):
 * raw_vec, accel_vec, filteredX, filteredY, filteredZ, sta, lta, tauc, pd
 *
 * Also supports legacy fields: raw, filtered
 *
 * Example payload (new schema):
 * {"raw_vec":0.1,"accel_vec":0.08,"filteredX":0.01,"filteredY":0.02,"filteredZ":0.03,"sta":1.2,"lta":0.9,"tauc":0.01,"pd":0.3}
 *
 * @param {UsbPacketListenerOptions} options
 * @returns {Promise<() => Promise<void>>} async cleanup function
 */
export async function listenForUsbJsonPackets(options) {
    const { baudRate = 9600, onPacket, onError, onRawData } = options ?? {};
    console.info("USB listener: start requested.");

    if (typeof onPacket !== "function") {
        throw new Error(
            "listenForUsbJsonPackets requires an onPacket callback.",
        );
    }

    if (!("serial" in navigator)) {
        throw new Error("Web Serial API is not available in this browser.");
    }

    const serial = navigator.serial;
    console.info("USB listener: requesting user-selected port...");

    const port = await serial.requestPort();
    console.info("USB serial device selected:", formatPortLabel(port));
    console.info("USB listener: opening selected port...");
    await port.open({ baudRate });
    console.info(`USB serial device detected and opened at ${baudRate} baud.`);
    if (typeof port.setSignals === "function") {
        try {
            await port.setSignals({
                dataTerminalReady: true,
                requestToSend: true,
            });
            console.info("USB listener: serial control signals set (DTR/RTS).");
        } catch (error) {
            console.warn("USB listener: unable to set DTR/RTS signals.", error);
        }
    }

    const decoder = new TextDecoder();
    const MAX_BUFFER_REMAINDER = 8192;
    let buffer = "";
    let isClosing = false;
    /** @type {ReadableStreamDefaultReader<Uint8Array> | null} */
    let reader = null;

    const toNumber = (value) => {
        if (typeof value === "number") {
            return Number.isFinite(value) ? value : null;
        }

        if (typeof value === "string" && value.trim().length > 0) {
            const parsed = Number(value);
            return Number.isFinite(parsed) ? parsed : null;
        }

        return null;
    };

    const normalizeDeviceJson = (text) => {
        // Accept non-strict numbers emitted by some devices (e.g. .09, -.5).
        return text.replace(/(:\s*)(-?)\.(\d+)/g, "$1$20.$3");
    };

    const parsePacketLine = (line) => {
        let payload;

        try {
            payload = JSON.parse(line);
        } catch (error) {
            const normalized = normalizeDeviceJson(line);
            if (normalized === line) {
                throw error;
            }

            console.warn(
                "USB RX payload required JSON normalization before parsing.",
                { original: line, normalized },
            );
            payload = JSON.parse(normalized);
        }

        const raw = toNumber(payload.raw_vec);
        const accelVec = toNumber(payload.filteredZ);
        // const accelVec = toNumber(payload.accel_vec);
        const filteredLegacy = toNumber(payload.filtered);
        const filteredX = toNumber(payload.filteredX);
        const filteredY = toNumber(payload.filteredY);
        const filteredZ = toNumber(payload.filteredZ);
        const filteredMagnitude =
            filteredX !== null && filteredY !== null && filteredZ !== null
                ? Math.hypot(filteredX, filteredY, filteredZ)
                : null;
        const filtered = accelVec ?? filteredLegacy ?? filteredMagnitude;
        const sta = toNumber(payload.sta);
        const lta = toNumber(payload.lta);
        const tauc = toNumber(payload.tauc);
        const pd = toNumber(payload.pd);

        if (
            raw === null ||
            filtered === null ||
            filteredX === null ||
            filteredY === null ||
            filteredZ === null ||
            sta === null ||
            lta === null ||
            tauc === null ||
            pd === null
        ) {
            throw new Error("Packet missing required numeric fields.");
        }

        return {
            raw_vec: raw,
            accel_vec: filtered,
            filteredX,
            filteredY,
            filteredZ,
            raw_accel: raw,
            filtered_accel: filtered,
            sta,
            lta,
            tauc,
            pd,
        };
    };

    const extractJsonObjects = (textBuffer) => {
        const packets = [];
        let start = -1;
        let depth = 0;
        let inString = false;
        let escaped = false;
        let lastObjectStart = -1;

        for (let i = 0; i < textBuffer.length; i += 1) {
            const ch = textBuffer[i];

            if (start === -1) {
                if (ch === "{") {
                    start = i;
                    lastObjectStart = i;
                    depth = 1;
                    inString = false;
                    escaped = false;
                }
                continue;
            }

            if (inString) {
                if (escaped) {
                    escaped = false;
                    continue;
                }

                if (ch === "\\") {
                    escaped = true;
                    continue;
                }

                if (ch === '"') {
                    inString = false;
                }

                continue;
            }

            if (ch === '"') {
                inString = true;
                continue;
            }

            if (ch === "{") {
                depth += 1;
                lastObjectStart = i;
                continue;
            }

            if (ch === "}") {
                depth -= 1;

                if (depth === 0) {
                    packets.push(textBuffer.slice(start, i + 1));
                    start = -1;
                }
            }
        }

        if (start === -1) {
            return { packets, remainder: "" };
        }

        // If we are stuck in an unterminated object and have seen a newer
        // object start, keep only the newest start to avoid parser deadlock.
        if (lastObjectStart > start) {
            return { packets, remainder: textBuffer.slice(lastObjectStart) };
        }

        return { packets, remainder: textBuffer.slice(start) };
    };

    const cleanup = async () => {
        console.info("USB listener: cleanup requested.");
        if (isClosing) {
            console.info("USB listener: cleanup skipped (already closing).");
            return;
        }

        isClosing = true;

        try {
            if (reader) {
                await reader.cancel();
                reader.releaseLock();
            }
        } catch {
            // Ignore cancellation errors during shutdown.
        }

        try {
            await port.close();
            console.info("USB listener: port closed.");
        } catch {
            // Ignore close errors during shutdown.
        }
    };

    const readLoop = async () => {
        console.info("USB listener: read loop started.");
        while (!isClosing && port.readable) {
            console.info("USB listener: acquiring stream reader...");
            reader = port.readable.getReader();

            try {
                let waitLogTimer = 0;
                while (!isClosing) {
                    const now = Date.now();
                    if (waitLogTimer === 0 || now - waitLogTimer >= 3000) {
                        console.info(
                            "USB listener: waiting for serial data...",
                        );
                        waitLogTimer = now;
                    }

                    const { value, done } = await reader.read();
                    if (done) {
                        console.warn(
                            "USB reader reported done=true; stream ended.",
                        );
                        break;
                    }

                    waitLogTimer = 0;

                    const decodedChunk = decoder.decode(value, {
                        stream: true,
                    });
                    if (decodedChunk.length > 0) {
                        if (typeof onRawData === "function") {
                            onRawData(decodedChunk);
                        }
                    }

                    const sanitizedChunk = decodedChunk.replace(/\u0000+/g, "");
                    if (
                        sanitizedChunk.length === 0 &&
                        decodedChunk.length > 0
                    ) {
                        console.warn(
                            "USB RX chunk was only NUL bytes; nothing to parse.",
                        );
                    }

                    buffer += sanitizedChunk;
                    const previousBuffer = buffer;
                    const { packets, remainder } = extractJsonObjects(buffer);
                    buffer = remainder;

                    if (
                        packets.length === 0 &&
                        previousBuffer.length > 0 &&
                        remainder.length > 0 &&
                        previousBuffer.length !== remainder.length
                    ) {
                        console.warn(
                            "USB RX parser resynchronized to a newer JSON object start.",
                        );
                    }

                    if (buffer.length > MAX_BUFFER_REMAINDER) {
                        const lastObjectStart = buffer.lastIndexOf("{");
                        if (lastObjectStart >= 0) {
                            buffer = buffer.slice(lastObjectStart);
                        } else {
                            buffer = "";
                        }

                        console.warn(
                            "USB RX parser buffer was resynchronized after exceeding max remainder size.",
                        );
                    }

                    for (const packetText of packets) {
                        try {
                            const packet = parsePacketLine(packetText);
                            console.log("USB RX JSON object:", {
                                raw_vec: packet.raw_vec,
                                accel_vec: packet.accel_vec,
                                filteredX: packet.filteredX,
                                filteredY: packet.filteredY,
                                filteredZ: packet.filteredZ,
                                sta: packet.sta,
                                lta: packet.lta,
                                tauc: packet.tauc,
                                pd: packet.pd,
                            });
                            onPacket(packet);
                        } catch (error) {
                            console.warn(
                                "USB RX payload could not be parsed as expected packet:",
                                packetText,
                                error,
                            );
                            if (onError) {
                                onError(error);
                            }
                        }
                    }
                }
            } catch (error) {
                console.error("USB read loop error:", error);
                if (!isClosing && onError) {
                    onError(error);
                }
            } finally {
                reader.releaseLock();
                reader = null;
                console.info("USB listener: reader released.");
            }
        }

        if (!isClosing) {
            console.warn(
                "USB read loop exited because port is no longer readable. Reconnect and restart listener if needed.",
            );
            return;
        }

        console.info("USB listener: read loop ended by cleanup.");
    };

    readLoop().catch((error) => {
        console.error("USB listener: unhandled read loop rejection:", error);
        if (!isClosing && onError) {
            onError(error);
        }
    });

    return cleanup;
}
