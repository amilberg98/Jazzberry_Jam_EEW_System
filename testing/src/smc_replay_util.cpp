#include "parsers/smcparser.hpp"

#include "STALTA.h"
#include "biquad.h"
#include "taucpd.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

    enum class AlertPhase
    {
        IDLE,
        CONFIRMING_STA_LTA,
        COLLECTING_TAUCPD,
    };

    struct ReplayMetrics
    {
        std::size_t samplesProcessed{};
        std::size_t collectingSamples{};
        int collectingPhaseStarts{};
        int alertDecisions{};
        bool staltaWarmupReached{};
        float maxRatioObserved{};
        float maxTauCObserved{};
        float maxPdObserved{};
        float finalSta{};
        float finalLta{};
        float finalTauC{};
        float finalPd{};
    };

    struct DevicePacket
    {
        std::size_t index{};
        unsigned long timestampMs{};
        float rawVec{};
        float accelVec{};
        float filteredX{};
        float filteredY{};
        float filteredZ{};
        float rawAccel{};
        float filteredAccel{};
        float correctedAccelVec{};
        float sta{};
        float lta{};
        float tauC{};
        float pd{};
    };

    struct ReplayResult
    {
        ReplayMetrics metrics{};
        std::vector<DevicePacket> packets;
    };

    struct AxisTriplet
    {
        std::filesystem::path xPath;
        std::filesystem::path yPath;
        std::filesystem::path zPath;
    };

    std::string escapeJson(const std::string &value)
    {
        std::string escaped;
        escaped.reserve(value.size() + 8);

        for (const char ch : value)
        {
            switch (ch)
            {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped += ch;
                break;
            }
        }

        return escaped;
    }

    ReplayResult replayMonitoringPipeline(const std::array<SmcRecord, 3> &records,
                                          const std::array<SmcRecord, 3> *correctedRecords = nullptr)
    {
        constexpr float kTauCWindowSeconds = 3.0f;
        constexpr unsigned long kStaLtaConfirmMs = 300;
        constexpr unsigned long kTaucpdEvalMs = 2000;
        constexpr unsigned long kCooldownMs = 10000;
        constexpr float kTauCThresh = 2.0f;
        constexpr float kPdThresh = 0.000100f;

        STALTA stalta;
        TauCPdEstimator taucpd(static_cast<float>(records[2].sampleRateHz), kTauCWindowSeconds);
        Biquad filterX;
        Biquad filterY;
        Biquad filterZ;

        ReplayResult result{};
        result.metrics.maxRatioObserved = 0.0f;
        result.metrics.maxTauCObserved = 0.0f;
        result.metrics.maxPdObserved = 0.0f;

        std::size_t sampleCount = std::min(
            records[0].dataPoints.size(),
            std::min(records[1].dataPoints.size(), records[2].dataPoints.size()));

        // If corrected records are provided, use the smaller count
        if (correctedRecords)
        {
            sampleCount = std::min(
                sampleCount,
                std::min(
                    (*correctedRecords)[0].dataPoints.size(),
                    std::min((*correctedRecords)[1].dataPoints.size(),
                             (*correctedRecords)[2].dataPoints.size())));
        }
        result.packets.reserve(sampleCount);

        AlertPhase phase = AlertPhase::IDLE;
        unsigned long triggerTimeMs = 0;
        unsigned long lastAlertEndTimeMs = 0;
        float maxTauCInWindow = 0.0f;
        float maxPdInWindow = 0.0f;

        const double samplePeriodMs = 1000.0 / records[2].sampleRateHz;

        for (std::size_t i = 0; i < sampleCount; ++i)
        {
            const float rawX = static_cast<float>(records[0].dataPoints[i]);
            const float rawY = static_cast<float>(records[1].dataPoints[i]);
            const float rawZ = static_cast<float>(records[2].dataPoints[i]);

            const float filteredX = filterX.process(rawX);
            const float filteredY = filterY.process(rawY);
            const float filteredZ = filterZ.process(rawZ);
            const float rawMagnitude = std::sqrt(rawX * rawX + rawY * rawY + rawZ * rawZ);
            const float filteredMagnitude =
                std::sqrt(filteredX * filteredX + filteredY * filteredY + filteredZ * filteredZ);

            // Compute corrected magnitude if corrected records are provided
            float correctedMagnitude = 0.0f;
            if (correctedRecords)
            {
                const float corrX = static_cast<float>((*correctedRecords)[0].dataPoints[i]);
                const float corrY = static_cast<float>((*correctedRecords)[1].dataPoints[i]);
                const float corrZ = static_cast<float>((*correctedRecords)[2].dataPoints[i]);
                correctedMagnitude = std::sqrt(corrX * corrX + corrY * corrY + corrZ * corrZ);
            }
            stalta.update(filteredZ);

            if (stalta.ratio > result.metrics.maxRatioObserved)
            {
                result.metrics.maxRatioObserved = stalta.ratio;
            }

            if (i + 1 >= static_cast<std::size_t>(MIN_LTA_FILL))
            {
                result.metrics.staltaWarmupReached = true;
            }

            const unsigned long nowMs =
                static_cast<unsigned long>(10001.0 + std::llround(samplePeriodMs * static_cast<double>(i)));

            if (phase == AlertPhase::IDLE &&
                (nowMs - lastAlertEndTimeMs > kCooldownMs) &&
                stalta.triggeredStatus())
            {
                phase = AlertPhase::CONFIRMING_STA_LTA;
                triggerTimeMs = nowMs;
            }

            if (phase == AlertPhase::CONFIRMING_STA_LTA)
            {
                if (!stalta.triggeredStatus())
                {
                    phase = AlertPhase::IDLE;
                }
                else if (nowMs - triggerTimeMs >= kStaLtaConfirmMs)
                {
                    phase = AlertPhase::COLLECTING_TAUCPD;
                    taucpd.reset();
                    maxTauCInWindow = 0.0f;
                    maxPdInWindow = 0.0f;
                    result.metrics.collectingPhaseStarts += 1;
                }
            }

            if (phase == AlertPhase::COLLECTING_TAUCPD)
            {
                taucpd.processSample(filteredZ);
                const float tauC = taucpd.getTauC();
                const float pd = taucpd.getPd();

                maxTauCInWindow = std::max(maxTauCInWindow, tauC);
                maxPdInWindow = std::max(maxPdInWindow, pd);
                result.metrics.maxTauCObserved = std::max(result.metrics.maxTauCObserved, tauC);
                result.metrics.maxPdObserved = std::max(result.metrics.maxPdObserved, pd);
                result.metrics.collectingSamples += 1;

                if (nowMs - triggerTimeMs >= (kStaLtaConfirmMs + kTaucpdEvalMs))
                {
                    if (maxTauCInWindow > kTauCThresh && maxPdInWindow > kPdThresh)
                    {
                        result.metrics.alertDecisions += 1;
                        lastAlertEndTimeMs = nowMs;
                    }
                    phase = AlertPhase::IDLE;
                }
            }

            DevicePacket packet{};
            packet.index = i;
            packet.timestampMs = nowMs;
            packet.rawVec = rawMagnitude;
            packet.accelVec = filteredMagnitude;
            packet.filteredX = filteredX;
            packet.filteredY = filteredY;
            packet.filteredZ = filteredZ;
            packet.rawAccel = rawMagnitude;
            packet.filteredAccel = filteredMagnitude;
            packet.correctedAccelVec = correctedMagnitude;
            packet.sta = stalta.getSTA();
            packet.lta = stalta.getLTA();
            packet.tauC = taucpd.getTauC();
            packet.pd = taucpd.getPd();
            result.packets.push_back(packet);

            result.metrics.samplesProcessed += 1;
        }

        result.metrics.finalSta = stalta.getSTA();
        result.metrics.finalLta = stalta.getLTA();
        result.metrics.finalTauC = taucpd.getTauC();
        result.metrics.finalPd = taucpd.getPd();

        return result;
    }

    void writeJsonOutput(const SmcRecord &record,
                         const ReplayResult &result,
                         const std::filesystem::path &outputPath,
                         const std::filesystem::path &inputPath)
    {
        std::filesystem::create_directories(outputPath.parent_path());

        std::ofstream out(outputPath);
        if (!out.is_open())
        {
            throw std::runtime_error("Failed to open output JSON file: " + outputPath.string());
        }

        out << std::setprecision(10);
        out << "{\n";
        out << "  \"inputFile\": \"" << escapeJson(inputPath.string()) << "\",\n";

        out << "  \"metadata\": {\n";
        out << "    \"stationName\": \"" << escapeJson(record.stationName) << "\",\n";
        out << "    \"channelDirection\": \"" << escapeJson(record.channelDirection) << "\",\n";
        out << "    \"earthquakeName\": \"" << escapeJson(record.earthquakeName) << "\",\n";
        out << "    \"startTime\": \"" << escapeJson(record.startTime) << "\",\n";
        out << "    \"eventYear\": " << record.eventYear << ",\n";
        out << "    \"eventMonth\": " << record.eventMonth << ",\n";
        out << "    \"eventDay\": " << record.eventDay << ",\n";
        out << "    \"eventHour\": " << record.eventHour << ",\n";
        out << "    \"eventMinute\": " << record.eventMinute << ",\n";
        out << "    \"samplingInterval\": " << record.samplingInterval << ",\n";
        out << "    \"sampleRateHz\": " << record.sampleRateHz << ",\n";
        out << "    \"totalPoints\": " << record.totalPoints << ",\n";
        out << "    \"earthquakeLatitude\": " << record.earthquakeLatitude << ",\n";
        out << "    \"earthquakeLongitude\": " << record.earthquakeLongitude << ",\n";
        out << "    \"earthquakeDepthKm\": " << record.earthquakeDepthKm << ",\n";
        out << "    \"magnitudeM\": " << record.magnitudeM << ",\n";
        out << "    \"magnitudeMs\": " << record.magnitudeMs << ",\n";
        out << "    \"magnitudeMl\": " << record.magnitudeMl << ",\n";
        out << "    \"stationLatitude\": " << record.stationLatitude << ",\n";
        out << "    \"stationLongitude\": " << record.stationLongitude << ",\n";
        out << "    \"stationElevationM\": " << record.stationElevationM << ",\n";
        out << "    \"epicentralDistanceKm\": " << record.epicentralDistanceKm << ",\n";
        out << "    \"epicentralAzimuthDeg\": " << record.epicentralAzimuthDeg << ",\n";
        out << "    \"vs30\": " << record.vs30 << "\n";
        out << "  },\n";

        out << "  \"timeSeries\": {\n";
        out << "    \"format\": \"ui_live_packet\",\n";
        out << "    \"fields\": [\"raw_vec\", \"accel_vec\", \"corrected_accel_vec\", \"filteredX\", \"filteredY\", \"filteredZ\", \"raw_accel\", \"filtered_accel\", \"sta\", \"lta\", \"tauc\", \"pd\"],\n";
        out << "    \"samples\": [\n";
        for (std::size_t i = 0; i < result.packets.size(); ++i)
        {
            const DevicePacket &packet = result.packets[i];
            out << "      {\"index\": " << packet.index
                << ", \"timestamp_ms\": " << packet.timestampMs
                << ", \"raw_vec\": " << packet.rawVec
                << ", \"accel_vec\": " << packet.accelVec
                << ", \"corrected_accel_vec\": " << packet.correctedAccelVec
                << ", \"filteredX\": " << packet.filteredX
                << ", \"filteredY\": " << packet.filteredY
                << ", \"filteredZ\": " << packet.filteredZ
                << ", \"raw_accel\": " << packet.rawAccel
                << ", \"filtered_accel\": " << packet.filteredAccel
                << ", \"sta\": " << packet.sta
                << ", \"lta\": " << packet.lta
                << ", \"tauc\": " << packet.tauC
                << ", \"pd\": " << packet.pd << "}";
            if (i + 1 < result.packets.size())
            {
                out << ",";
            }
            out << "\n";
        }
        out << "    ]\n";
        out << "  },\n";

        out << "  \"metrics\": {\n";
        out << "    \"samplesProcessed\": " << result.metrics.samplesProcessed << ",\n";
        out << "    \"collectingSamples\": " << result.metrics.collectingSamples << ",\n";
        out << "    \"collectingPhaseStarts\": " << result.metrics.collectingPhaseStarts << ",\n";
        out << "    \"alertDecisions\": " << result.metrics.alertDecisions << ",\n";
        out << "    \"staltaWarmupReached\": " << (result.metrics.staltaWarmupReached ? "true" : "false") << ",\n";
        out << "    \"maxRatioObserved\": " << result.metrics.maxRatioObserved << ",\n";
        out << "    \"maxTauCObserved\": " << result.metrics.maxTauCObserved << ",\n";
        out << "    \"maxPdObserved\": " << result.metrics.maxPdObserved << ",\n";
        out << "    \"finalSta\": " << result.metrics.finalSta << ",\n";
        out << "    \"finalLta\": " << result.metrics.finalLta << ",\n";
        out << "    \"finalTauC\": " << result.metrics.finalTauC << ",\n";
        out << "    \"finalPd\": " << result.metrics.finalPd << "\n";
        out << "  }\n";
        out << "}\n";
    }

    bool isSmcFile(const std::filesystem::path &path)
    {
        return path.has_extension() && path.extension() == ".smc";
    }

    std::string makeGroupKey(const std::filesystem::path &path)
    {
        const std::string stem = path.stem().string();
        const std::size_t underscore = stem.find('_');
        const std::string prefix = underscore == std::string::npos ? stem : stem.substr(0, underscore);

        if (prefix.size() >= 2)
        {
            const char component = prefix.back();
            if (component == 'a' || component == 'b' || component == 'c')
            {
                return prefix.substr(0, prefix.size() - 1) + stem.substr(underscore);
            }
        }

        return stem;
    }

    std::string makeStationKey(const std::filesystem::path &path)
    {
        const std::string stem = path.stem().string();
        const std::size_t underscore = stem.find('_');
        const std::string prefix = underscore == std::string::npos ? stem : stem.substr(0, underscore);

        if (prefix.size() < 2)
        {
            return stem;
        }

        const char component = prefix.back();
        if (component == 'a' || component == 'b' || component == 'c')
        {
            return prefix.substr(0, prefix.size() - 1);
        }

        return prefix;
    }

    std::string makeProcessingSuffix(const std::filesystem::path &path)
    {
        const std::string stem = path.stem().string();
        const std::size_t underscore = stem.find('_');
        if (underscore == std::string::npos)
        {
            return {};
        }

        return stem.substr(underscore + 1);
    }

    std::map<std::string, AxisTriplet> collectTripletsFromDirectory(const std::filesystem::path &directory)
    {
        std::map<std::string, AxisTriplet> groups;

        for (const auto &entry : std::filesystem::directory_iterator(directory))
        {
            if (!entry.is_regular_file() || !isSmcFile(entry.path()))
            {
                continue;
            }

            const std::string stem = entry.path().stem().string();
            if (stem.size() < 4)
            {
                continue;
            }

            const std::string stationKey = makeStationKey(entry.path());
            const std::string processingSuffix = makeProcessingSuffix(entry.path());
            if (stationKey.empty() || processingSuffix.empty())
            {
                continue;
            }

            const std::string componentKey = stationKey + "_" + processingSuffix;
            AxisTriplet &triplet = groups[componentKey];

            const char component = stem[stationKey.size()];
            if (component == 'a')
            {
                triplet.xPath = entry.path();
            }
            else if (component == 'b')
            {
                triplet.yPath = entry.path();
            }
            else if (component == 'c')
            {
                triplet.zPath = entry.path();
            }
        }

        return groups;
    }

    void processTriplet(const std::string &groupName, const AxisTriplet &triplet,
                        const AxisTriplet *correctedTriplet, const std::filesystem::path &outputDir)
    {
        if (triplet.xPath.empty() || triplet.yPath.empty() || triplet.zPath.empty())
        {
            std::cout << "Skipping incomplete group " << groupName << "\n";
            return;
        }

        SmcParser parser;
        std::array<SmcRecord, 3> records = {
            parser.parseFile(triplet.xPath.string()),
            parser.parseFile(triplet.yPath.string()),
            parser.parseFile(triplet.zPath.string()),
        };

        for (const auto &record : records)
        {
            if (record.totalPoints <= 0 || record.sampleRateHz <= 0.0)
            {
                throw std::runtime_error("Invalid SMC record encountered in group " + groupName);
            }
        }

        // Load corrected records if provided
        std::array<SmcRecord, 3> *correctedRecordsPtr = nullptr;
        std::array<SmcRecord, 3> correctedRecords;
        if (correctedTriplet && !correctedTriplet->xPath.empty() && !correctedTriplet->yPath.empty() && !correctedTriplet->zPath.empty())
        {
            correctedRecords = {
                parser.parseFile(correctedTriplet->xPath.string()),
                parser.parseFile(correctedTriplet->yPath.string()),
                parser.parseFile(correctedTriplet->zPath.string()),
            };
            correctedRecordsPtr = &correctedRecords;
        }

        const ReplayResult replay = replayMonitoringPipeline(records, correctedRecordsPtr);

        const std::filesystem::path outputPath = outputDir / (groupName + ".json");
        writeJsonOutput(records[2], replay, outputPath, triplet.zPath);

        std::cout << "Wrote " << outputPath << " (" << replay.metrics.samplesProcessed << " samples)\n";
    }

    std::vector<std::filesystem::path> expandInputs(int argc, char **argv)
    {
        std::vector<std::filesystem::path> inputs;
        for (int i = 1; i < argc; ++i)
        {
            inputs.emplace_back(argv[i]);
        }
        return inputs;
    }

} // namespace

int main(int argc, char **argv)
{
    try
    {
        const std::filesystem::path outputDir("output");
        std::vector<std::filesystem::path> inputs = expandInputs(argc, argv);

        if (inputs.empty())
        {
            inputs.emplace_back("data/20021103_2212_uncorr");
        }

        std::map<std::string, AxisTriplet> groups;
        for (const auto &input : inputs)
        {
            if (std::filesystem::is_directory(input))
            {
                const auto directoryGroups = collectTripletsFromDirectory(input);
                for (const auto &entry : directoryGroups)
                {
                    AxisTriplet &existing = groups[entry.first];
                    if (!entry.second.xPath.empty())
                        existing.xPath = entry.second.xPath;
                    if (!entry.second.yPath.empty())
                        existing.yPath = entry.second.yPath;
                    if (!entry.second.zPath.empty())
                        existing.zPath = entry.second.zPath;
                }
            }
            else if (std::filesystem::is_regular_file(input) && isSmcFile(input))
            {
                const std::string groupKey = makeGroupKey(input);
                AxisTriplet &triplet = groups[groupKey];
                const std::string stem = input.stem().string();
                const std::string stationKey = makeStationKey(input);
                const char component = stem.size() > stationKey.size() ? stem[stationKey.size()] : '\0';

                if (component == 'a')
                {
                    triplet.xPath = input;
                }
                else if (component == 'b')
                {
                    triplet.yPath = input;
                }
                else if (component == 'c')
                {
                    triplet.zPath = input;
                }
                else
                {
                    std::cout << "Skipping unsupported component file: " << input << "\n";
                }
            }
        }

        std::filesystem::create_directories(outputDir);

        // Collect corrected triplets if they exist (look for _a suffix)
        std::map<std::string, AxisTriplet> correctedGroups;
        const std::filesystem::path corrDataDir("data/20021103_2212_corr");
        if (std::filesystem::is_directory(corrDataDir))
        {
            const auto allCorrected = collectTripletsFromDirectory(corrDataDir);
            for (const auto &entry : allCorrected)
            {
                // Only keep groups with _a suffix (acceleration data)
                if (entry.first.find("_a") != std::string::npos)
                {
                    correctedGroups[entry.first] = entry.second;
                }
            }
        }

        std::size_t processedGroups = 0;
        for (const auto &entry : groups)
        {
            AxisTriplet *correctedTripletPtr = nullptr;
            // Try to find matching corrected group by replacing suffix
            std::string corrKey = entry.first;
            // Replace last suffix with _a (e.g., "0528_u" -> "0528_a")
            size_t lastUnderscore = corrKey.rfind('_');
            if (lastUnderscore != std::string::npos)
            {
                corrKey = corrKey.substr(0, lastUnderscore + 1) + "a";
                const auto corrIter = correctedGroups.find(corrKey);
                if (corrIter != correctedGroups.end())
                {
                    correctedTripletPtr = &corrIter->second;
                }
            }

            processTriplet(entry.first, entry.second, correctedTripletPtr, outputDir);
            if (!entry.second.xPath.empty() && !entry.second.yPath.empty() && !entry.second.zPath.empty())
            {
                processedGroups += 1;
            }
        }

        if (processedGroups == 0)
        {
            std::cerr << "No complete a/b/c SMC triplets were found in the input directories.\n";
            return 1;
        }

        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "smc_replay_util error: " << error.what() << '\n';
        std::cerr << "Note: Corrected data is optional. If corr folder is missing, only uncorrected data will be processed.\n";
        return 1;
    }
}