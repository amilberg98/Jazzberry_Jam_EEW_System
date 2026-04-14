#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"

class DefaultOutputListener : public Catch::EventListenerBase
{
public:
    using Catch::EventListenerBase::EventListenerBase;

    void testCaseStarting(Catch::TestCaseInfo const &testInfo) override
    {
        Catch::cout() << "\n";
        Catch::cout()
            << "=== " << testInfo.name
            << "  (" << testInfo.tagsAsString() << ") ===\n\n";
    }

    void testCaseEnded(Catch::TestCaseStats const &testCaseStats) override
    {
        Catch::cout()
            << "\n=== assertions: " << testCaseStats.totals.assertions.total()
            << "  failed: " << testCaseStats.totals.assertions.failed
            << " ===\n";
    }
};

CATCH_REGISTER_LISTENER(DefaultOutputListener)
