// COSMOS/SMC accelerogram parser and record model.

#pragma once

#include <array>
#include <istream>
#include <string>
#include <vector>

class SmcRecord
{
public:
    // Sampling
    double samplingInterval{}; // seconds
    double sampleRateHz{};     // Hz (from real header cell #2)
    int totalPoints{};         // number of samples in dataPoints
    int headerNumTimeSeries{};
    int headerNumComments{};

    // Text header fields
    std::string stationName;
    std::string channelDirection;
    std::string earthquakeName;
    std::string startTime; // ISO-like start time of the record

    int eventYear{};
    int eventMonth{};
    int eventDay{};
    int eventHour{};
    int eventMinute{};

    // Earthquake metadata
    double earthquakeLatitude{};
    double earthquakeLongitude{};
    double earthquakeDepthKm{};
    double magnitudeM{};
    double magnitudeMs{};
    double magnitudeMl{};
    double magnitudeOther{};

    // Station / site
    double stationLatitude{};
    double stationLongitude{};
    double stationElevationM{};
    double epicentralDistanceKm{};
    double epicentralAzimuthDeg{};
    double vs30{}; // m/s

    // Instrument / channel
    int recorderSerialNumber{};
    int channelNumber{};
    int totalChannelsOnRecord{};
    int totalChannelsAtStation{};
    int sensorSerialNumber{};
    double verticalOrientationDeg{};
    double horizontalOrientationDeg{};
    int sensorTypeCode{};

    double digitizingConstant{};
    double antiAliasCornerHz{};
    double antiAliasPoles{};
    double naturalFrequencyHz{};
    double sensorDamping{};
    double amplifierGainDb{};
    double preampGainDb{};

    // Header extrema
    double headerMaxTimeSec{};
    double headerMaxValue{};
    double headerMinTimeSec{};
    double headerMinValue{};

    // Flags / bookkeeping
    int problemFlag{};
    int structureType{};
    int structureNumber{};
    int transducerNumber{};
    int totalTransducerChannels{};

    int intNullValue{};
    double realNullValue{};

    std::vector<std::string> comments;

    // Time series values in native acceleration units (usually cm/s^2)
    std::vector<double> dataPoints;

    void addDataPoint(double rawValue);
    void finalize();
};

class SmcParser final
{
public:
    [[nodiscard]] SmcRecord parseFile(const std::string &filePath) const;

private:
    static constexpr int kTextHeaderLineCount = 11;
    static constexpr int kIntegerHeaderLineCount = 6;
    static constexpr int kIntegerHeaderValueCount = 48;
    static constexpr int kRealHeaderLineCount = 10;
    static constexpr int kRealHeaderValueCount = 50;

    static std::string trim(const std::string &input);
    static bool isAllDigits(const std::string &input);
    static double parseOptionalField(const std::string &source, std::size_t start, std::size_t length);

    static std::array<std::string, kTextHeaderLineCount> readTextHeaders(std::istream &input);
    static std::vector<long long> readIntegerHeaders(std::istream &input);
    static std::vector<double> readRealHeaders(std::istream &input);

    static void parseTextSection(const std::array<std::string, kTextHeaderLineCount> &textLines,
                                 SmcRecord &record);
    static void parseIntegerSection(const std::vector<long long> &intHeaders, SmcRecord &record);
    static void parseRealSection(const std::vector<double> &realHeaders, SmcRecord &record);
    static void readComments(std::istream &input, SmcRecord &record);
    static void readDataPoints(std::istream &input, SmcRecord &record);
};
