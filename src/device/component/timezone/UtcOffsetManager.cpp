// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "UtcOffsetManager.hpp"

#include "IDevice.hpp"
#include "IProperty.hpp"
#include "exception/ObException.hpp"
#include "property/InternalProperty.hpp"

#include <cstdint>
#include <ctime>

namespace libobsensor {
namespace {

constexpr int64_t kSecondsPerMinute     = 60;
constexpr int64_t kSecondsPerHour       = 60 * kSecondsPerMinute;
constexpr int64_t kSecondsPerDay        = 24 * kSecondsPerHour;
constexpr int32_t kMinimumOffsetMinutes = -12 * 60;
constexpr int32_t kMaximumOffsetMinutes = 14 * 60;

bool getLocalTime(std::time_t time, std::tm *result) {
#ifdef _WIN32
    return localtime_s(result, &time) == 0;
#else
    return localtime_r(&time, result) != nullptr;
#endif
}

bool getUtcTime(std::time_t time, std::tm *result) {
#ifdef _WIN32
    return gmtime_s(result, &time) == 0;
#else
    return gmtime_r(&time, result) != nullptr;
#endif
}

// Days since 1970-01-01 for a Gregorian civil date. Comparing civil times avoids
// interpreting a UTC tm as local time, which can be wrong near a DST transition.
int64_t daysFromCivil(int32_t year, uint32_t month, uint32_t day) {
    year -= static_cast<int32_t>(month <= 2);
    const int32_t  era = (year >= 0 ? year : year - 399) / 400;
    const uint32_t yoe = static_cast<uint32_t>(year - era * 400);
    const int32_t  monthPrime = static_cast<int32_t>(month) + (month > 2 ? -3 : 9);
    const uint32_t doy        = static_cast<uint32_t>((153 * monthPrime + 2) / 5) + day - 1;
    const uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;
}

int64_t civilTimeInSeconds(const std::tm &time) {
    const auto days = daysFromCivil(time.tm_year + 1900, static_cast<uint32_t>(time.tm_mon + 1), static_cast<uint32_t>(time.tm_mday));
    return days * kSecondsPerDay + static_cast<int64_t>(time.tm_hour) * kSecondsPerHour
           + static_cast<int64_t>(time.tm_min) * kSecondsPerMinute + time.tm_sec;
}

}  // namespace

UtcOffsetManager::UtcOffsetManager(IDevice *owner) : DeviceComponentBase(owner) {}

void UtcOffsetManager::syncFromSystem() {
    const auto offsetMinutes = getCurrentLocalUtcOffsetMinutes();
    if(offsetMinutes < kMinimumOffsetMinutes || offsetMinutes > kMaximumOffsetMinutes) {
        THROW_INVALID_DATA_EXCEPTION("Current local UTC offset is out of the supported range");
    }

    auto propertyServer = getOwner()->getPropertyServer();
    propertyServer->setPropertyValueT<int32_t>(OB_PROP_LOCAL_UTC_OFFSET_MINUTES_INT, offsetMinutes, PROP_ACCESS_INTERNAL);
}

int32_t UtcOffsetManager::getCurrentLocalUtcOffsetMinutes() {
    const auto now = std::time(nullptr);
    if(now == static_cast<std::time_t>(-1)) {
        THROW_IO_EXCEPTION("Failed to get the current system time");
    }

    std::tm localTime{};
    std::tm utcTime{};
    if(!getLocalTime(now, &localTime) || !getUtcTime(now, &utcTime)) {
        THROW_IO_EXCEPTION("Failed to convert the current system time");
    }

    const auto offsetSeconds = civilTimeInSeconds(localTime) - civilTimeInSeconds(utcTime);
    if(offsetSeconds % kSecondsPerMinute != 0) {
        THROW_INVALID_DATA_EXCEPTION("Current local UTC offset is not an integral number of minutes");
    }
    return static_cast<int32_t>(offsetSeconds / kSecondsPerMinute);
}

}  // namespace libobsensor
