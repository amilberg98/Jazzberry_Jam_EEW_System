#include "catch_amalgamated.hpp"
#include "parsers/smcparser.hpp"

#include <cmath>
#include <iostream>

TEST_CASE("SMC Parser Metadata", "[smcparser]")
{
    const std::string filePath = "data/20021103_2212_uncorr/0528a_u.smc";
    const SmcParser parser;

    SmcRecord rec = parser.parseFile(filePath);

    std::cout << "Station: " << rec.stationName << '\n';
    std::cout << "Channel: " << rec.channelDirection << '\n';
    std::cout << "Start time: " << rec.startTime << '\n';
    std::cout << "Sampling interval (s): " << rec.samplingInterval << '\n';

    std::cout << "Number of points: " << rec.totalPoints << '\n';

    // find the highest absolute value in the data points
    double maxValue = 0.0;
    for (const auto &value : rec.dataPoints)
    {
        if (std::abs(value) > std::abs(maxValue))
        {
            maxValue = value;
        }
    }
    std::cout << "Max data point: " << maxValue << '\n';

    std::cout << "First data points (index : value):\n";
    const std::size_t maxToShow = 5;
    for (std::size_t i = 0; i < rec.dataPoints.size() && i < maxToShow; ++i)
    {
        std::cout << "  " << i << " : " << rec.dataPoints[i] << '\n';
    }

    REQUIRE(rec.totalPoints == static_cast<int>(rec.dataPoints.size()));
    REQUIRE(rec.totalPoints > 0);
}
