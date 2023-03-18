

#include "Timestamp.h"

static_assert(sizeof(Timestamp) == sizeof(int64_t), "Timestamp should be same size as int64_t");

Timestamp::Timestamp(std::chrono::system_clock::time_point tp) : Timestamp(std::chrono::time_point_cast<std::chrono::microseconds>(tp).time_since_epoch().count())
{
}

Timestamp::~Timestamp()
{
}

std::string Timestamp::toString() const
{
    char buf[32] = {0};
    int64_t seconds = microSecondsSinceEpoch_ / kMicroSecondsPerSecond;
    int64_t microseconds = microSecondsSinceEpoch_ % kMicroSecondsPerSecond;
    snprintf(buf, sizeof(buf), "%" PRId64 ".%06" PRId64 "", seconds, microseconds);
    return buf;
}

std::string Timestamp::toFormattedString(bool showMicroseconds) const
{
    char buf[64] = {0};
    // time_t seconds = static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond);
    int64_t seconds = microSecondsSinceEpoch_ / kMicroSecondsPerSecond;
    int64_t microseconds = microSecondsSinceEpoch_ % kMicroSecondsPerSecond;
    snprintf(buf, sizeof(buf), "%" PRId64 ".%06" PRId64 "", seconds, microseconds);
    return buf;
}

Timestamp Timestamp::now()
{
    return Timestamp(std::chrono::system_clock::now());
}

DateTime::DateTime(const struct tm &t)
    : year(t.tm_year + 1900), month(t.tm_mon + 1), day(t.tm_mday),
      hour(t.tm_hour), minute(t.tm_min), second(t.tm_sec)
{
}