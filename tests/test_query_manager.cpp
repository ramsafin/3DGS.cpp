#include "vulkan/QueryManager.h"

#include <gtest/gtest.h>

#include <stdexcept>

TEST(QueryManager, ConvertsTimestampTicksUsingDevicePeriod) {
    EXPECT_DOUBLE_EQ(timestampTicksToMilliseconds(2'000, 5.0f), 0.01);
}

TEST(QueryManager, RejectsInvalidTimestampPeriod) {
    EXPECT_THROW(timestampTicksToMilliseconds(1, 0.0f), std::runtime_error);
}
