#include "parsers/smcparser.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

void SmcRecord::addDataPoint(double rawValue)
{
    dataPoints.push_back(rawValue);
}

void SmcRecord::finalize()
{
    totalPoints = static_cast<int>(dataPoints.size());
}

std::string SmcParser::trim(const std::string &input)
{
    std::size_t first = 0;
    while (first < input.size() && std::isspace(static_cast<unsigned char>(input[first])) != 0)
    {
        ++first;
    }

    if (first == input.size())
    {
        return {};
    }

    std::size_t last = input.size() - 1;
    while (last > first && std::isspace(static_cast<unsigned char>(input[last])) != 0)
    {
        --last;
    }
    return input.substr(first, last - first + 1);
}

bool SmcParser::isAllDigits(const std::string &input)
{
    if (input.empty())
    {
        return false;
    }
    return std::all_of(input.begin(), input.end(),
                       [](unsigned char ch)
                       { return std::isdigit(ch) != 0; });
}

double SmcParser::parseOptionalField(const std::string &source, std::size_t start,
                                     std::size_t length)
{
    if (start >= source.size())
    {
        return 0.0;
    }

    const std::string field = trim(source.substr(start, length));
    if (field.empty())
    {
        return 0.0;
    }

    try
    {
        return std::stod(field);
    }
    catch (...)
    {
        return 0.0;
    }
}

std::array<std::string, SmcParser::kTextHeaderLineCount> SmcParser::readTextHeaders(
    std::istream &input)
{
    std::array<std::string, kTextHeaderLineCount> textLines;
    std::string line;
    for (int i = 0; i < kTextHeaderLineCount; ++i)
    {
        if (!std::getline(input, line))
        {
            throw std::runtime_error("Unexpected end of file while reading text headers");
        }
        textLines[i] = line;
    }
    return textLines;
}

std::vector<long long> SmcParser::readIntegerHeaders(std::istream &input)
{
    std::vector<long long> values;
    values.reserve(kIntegerHeaderValueCount);

    std::string line;
    for (int lineIdx = 0; lineIdx < kIntegerHeaderLineCount; ++lineIdx)
    {
        if (!std::getline(input, line))
        {
            throw std::runtime_error("Unexpected end of file while reading integer headers");
        }

        std::istringstream iss(line);
        long long value = 0;
        while (iss >> value && static_cast<int>(values.size()) < kIntegerHeaderValueCount)
        {
            values.push_back(value);
        }
    }

    if (static_cast<int>(values.size()) < kIntegerHeaderValueCount)
    {
        throw std::runtime_error("Failed to parse all integer header values");
    }

    return values;
}

std::vector<double> SmcParser::readRealHeaders(std::istream &input)
{
    std::vector<double> values;
    values.reserve(kRealHeaderValueCount);

    std::string line;
    for (int lineIdx = 0; lineIdx < kRealHeaderLineCount; ++lineIdx)
    {
        if (!std::getline(input, line))
        {
            throw std::runtime_error("Unexpected end of file while reading real headers");
        }

        std::istringstream iss(line);
        double value = 0.0;
        while (iss >> value && static_cast<int>(values.size()) < kRealHeaderValueCount)
        {
            values.push_back(value);
        }
    }

    if (static_cast<int>(values.size()) < kRealHeaderValueCount)
    {
        throw std::runtime_error("Failed to parse all real header values");
    }

    return values;
}

void SmcParser::parseTextSection(const std::array<std::string, kTextHeaderLineCount> &textLines,
                                 SmcRecord &record)
{
    {
        const std::string &line4 = textLines[3];
        std::istringstream iss(line4);

        int year = 0;
        int month = 0;
        int day = 0;
        std::string hhmm;

        if (iss >> year >> month >> day >> hhmm)
        {
            record.eventYear = year;
            record.eventMonth = month;
            record.eventDay = day;

            if (hhmm.size() == 4 && isAllDigits(hhmm))
            {
                const std::string hourStr = hhmm.substr(0, 2);
                const std::string minuteStr = hhmm.substr(2, 2);
                record.eventHour = std::stoi(hourStr);
                record.eventMinute = std::stoi(minuteStr);

                std::ostringstream ts;
                ts << year << '-';
                if (month < 10)
                {
                    ts << '0';
                }
                ts << month << '-';
                if (day < 10)
                {
                    ts << '0';
                }
                ts << day << 'T' << hourStr << ':' << minuteStr << ":00";
                record.startTime = ts.str();
            }
        }

        if (line4.size() >= 27)
        {
            record.earthquakeName = trim(line4.substr(26));
        }
    }

    {
        const std::string &line5 = textLines[4];
        if (line5.find("Moment Mag=") != std::string::npos)
        {
            record.magnitudeM = parseOptionalField(line5, 11, 9);
            record.magnitudeMs = parseOptionalField(line5, 24, 9);
            record.magnitudeMl = parseOptionalField(line5, 37, 9);
        }
    }

    {
        const std::string &line6 = textLines[5];
        const std::string stationKey = "station =";
        const std::string componentKey = "component=";

        const std::size_t stationPos = line6.find(stationKey);
        if (stationPos != std::string::npos)
        {
            const std::size_t componentPos = line6.find(componentKey);
            const std::size_t stationNameStart = stationPos + stationKey.size();

            const std::string stationPart =
                (componentPos != std::string::npos && componentPos > stationNameStart)
                    ? line6.substr(stationNameStart, componentPos - stationNameStart)
                    : line6.substr(stationNameStart);
            record.stationName = trim(stationPart);

            if (componentPos != std::string::npos)
            {
                const std::size_t directionStart = componentPos + componentKey.size();
                record.channelDirection = trim(line6.substr(directionStart));
            }
        }
    }
}

void SmcParser::parseIntegerSection(const std::vector<long long> &intHeaders, SmcRecord &record)
{
    record.intNullValue = static_cast<int>(intHeaders[0]);
    record.headerNumComments = static_cast<int>(intHeaders[15]);
    record.headerNumTimeSeries = static_cast<int>(intHeaders[16]);
    record.problemFlag = static_cast<int>(intHeaders[17]);
    record.structureType = static_cast<int>(intHeaders[18]);
    record.structureNumber = static_cast<int>(intHeaders[19]);
    record.transducerNumber = static_cast<int>(intHeaders[20]);
    record.totalTransducerChannels = static_cast<int>(intHeaders[21]);

    record.recorderSerialNumber = static_cast<int>(intHeaders[7]);
    record.channelNumber = static_cast<int>(intHeaders[8]);
    record.totalChannelsOnRecord = static_cast<int>(intHeaders[9]);
    record.totalChannelsAtStation = static_cast<int>(intHeaders[10]);
    record.sensorSerialNumber = static_cast<int>(intHeaders[11]);
    record.verticalOrientationDeg = static_cast<double>(intHeaders[12]);
    record.horizontalOrientationDeg = static_cast<double>(intHeaders[13]);
    record.sensorTypeCode = static_cast<int>(intHeaders[14]);
}

void SmcParser::parseRealSection(const std::vector<double> &realHeaders, SmcRecord &record)
{
    record.realNullValue = realHeaders[0];

    const auto isDefined = [&](int index)
    {
        return realHeaders[index] != record.realNullValue;
    };

    if (isDefined(1) && realHeaders[1] > 0.0)
    {
        record.sampleRateHz = realHeaders[1];
        record.samplingInterval = 1.0 / record.sampleRateHz;
    }
    else
    {
        throw std::runtime_error(
            "SMC real header cell #2 (sampling rate) is undefined or non-positive");
    }

    if (isDefined(2))
        record.earthquakeLatitude = realHeaders[2];
    if (isDefined(3))
        record.earthquakeLongitude = realHeaders[3];
    if (isDefined(4))
        record.earthquakeDepthKm = realHeaders[4];
    if (isDefined(5))
        record.magnitudeM = realHeaders[5];
    if (isDefined(6))
        record.magnitudeMs = realHeaders[6];
    if (isDefined(7))
        record.magnitudeMl = realHeaders[7];
    if (isDefined(8))
        record.magnitudeOther = realHeaders[8];

    if (isDefined(10))
        record.stationLatitude = realHeaders[10];
    if (isDefined(11))
        record.stationLongitude = realHeaders[11];
    if (isDefined(12))
        record.stationElevationM = realHeaders[12];
    if (isDefined(16))
        record.epicentralDistanceKm = realHeaders[16];
    if (isDefined(17))
        record.epicentralAzimuthDeg = realHeaders[17];

    if (isDefined(18))
        record.digitizingConstant = realHeaders[18];
    if (isDefined(19))
        record.antiAliasCornerHz = realHeaders[19];
    if (isDefined(20))
        record.antiAliasPoles = realHeaders[20];
    if (isDefined(21))
        record.naturalFrequencyHz = realHeaders[21];
    if (isDefined(22))
        record.sensorDamping = realHeaders[22];
    if (isDefined(23))
        record.amplifierGainDb = realHeaders[23];
    if (isDefined(24))
        record.preampGainDb = realHeaders[24];

    if (isDefined(28))
        record.headerMaxTimeSec = realHeaders[28];
    if (isDefined(29))
        record.headerMaxValue = realHeaders[29];
    if (isDefined(30))
        record.headerMinTimeSec = realHeaders[30];
    if (isDefined(31))
        record.headerMinValue = realHeaders[31];

    if (isDefined(39))
        record.vs30 = realHeaders[39];
}

void SmcParser::readComments(std::istream &input, SmcRecord &record)
{
    record.comments.clear();
    record.comments.reserve(static_cast<std::size_t>(std::max(0, record.headerNumComments)));

    std::string line;
    for (int i = 0; i < record.headerNumComments; ++i)
    {
        if (!std::getline(input, line))
        {
            throw std::runtime_error("Unexpected end of file while reading comments");
        }
        record.comments.push_back(line);
    }
}

void SmcParser::readDataPoints(std::istream &input, SmcRecord &record)
{
    std::string line;
    while (std::getline(input, line))
    {
        if (line.empty())
        {
            continue;
        }

        std::istringstream iss(line);
        double rawValue = 0.0;
        while (iss >> rawValue)
        {
            record.addDataPoint(rawValue);
        }
    }
}

SmcRecord SmcParser::parseFile(const std::string &filePath) const
{
    std::ifstream input(filePath);
    if (!input.is_open())
    {
        throw std::runtime_error("Failed to open SMC file: " + filePath);
    }

    SmcRecord record;

    const std::array<std::string, kTextHeaderLineCount> textHeaders = readTextHeaders(input);
    parseTextSection(textHeaders, record);

    const std::vector<long long> intHeaders = readIntegerHeaders(input);
    parseIntegerSection(intHeaders, record);

    const std::vector<double> realHeaders = readRealHeaders(input);
    parseRealSection(realHeaders, record);

    readComments(input, record);
    readDataPoints(input, record);
    record.finalize();

    return record;
}
