#include "catch_amalgamated.hpp"
#include "algorithms/stalta.hpp"
#include "parsers/smcparser.hpp"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

TEST_CASE("STA/LTA", "[unit][stalta]")
{
    const SmcParser parser;

    const std::vector<std::string> files = {
        "data/20021103_2212_uncorr/0528a_u.smc",
        "data/20021103_2212_uncorr/0528b_u.smc",
        "data/20021103_2212_uncorr/0528c_u.smc",
        "data/20021103_2212_uncorr/0593a_u.smc",
        "data/20021103_2212_uncorr/0593b_u.smc",
        "data/20021103_2212_uncorr/0593c_u.smc",
        "data/20021103_2212_uncorr/1356a_u.smc",
        "data/20021103_2212_uncorr/1356b_u.smc",
        "data/20021103_2212_uncorr/1356c_u.smc",
        "data/20021103_2212_uncorr/1356d_u.smc",
        "data/20021103_2212_uncorr/1356e_u.smc",
        "data/20021103_2212_uncorr/1356f_u.smc",
        "data/20021103_2212_uncorr/1356g_u.smc",
        "data/20021103_2212_uncorr/1356h_u.smc",
        "data/20021103_2212_uncorr/1356i_u.smc",
        "data/20021103_2212_uncorr/1356j_u.smc",
        "data/20021103_2212_uncorr/1356k_u.smc",
        "data/20021103_2212_uncorr/1356l_u.smc",
        "data/20021103_2212_uncorr/1397a_u.smc",
        "data/20021103_2212_uncorr/1397b_u.smc",
    };

    for (const auto &path : files)
    {
        INFO("Processing file: " << path);
        std::cout << "STA/LTA unit_test on: " << path << '\n';

        SmcRecord rec = parser.parseFile(path);

        REQUIRE(rec.totalPoints > 0);

        // first set LTA seed based on time values in first 2 seconds
        // then start STA/LTA calculation at 2 seconds
        // find triggers and print them

        // verify that the triggers are correct

        // Placeholder until STA/LTA is added
        REQUIRE(true);
    }
}
