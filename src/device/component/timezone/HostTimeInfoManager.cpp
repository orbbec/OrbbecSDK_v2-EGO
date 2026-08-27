// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "HostTimeInfoManager.hpp"

#include "IDevice.hpp"
#include "IProperty.hpp"
#include "exception/ObException.hpp"
#include "logger/Logger.hpp"
#include "property/InternalProperty.hpp"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <unordered_map>
#include <utility>

#ifdef _WIN32
#include <Windows.h>
#elif defined(__ANDROID__)
#include <sys/system_properties.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#else
#include <unistd.h>
#endif

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

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if(first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string normalizeIanaTimeZone(std::string timezone) {
    timezone = trim(std::move(timezone));
    if(timezone == "UTC") {
        return "Etc/UTC";
    }
    if(timezone == "GMT") {
        return "Etc/GMT";
    }
    return timezone.find('/') == std::string::npos ? std::string{} : timezone;
}

#ifdef _WIN32
std::wstring getWindowsUserCountryCode() {
    wchar_t countryCode[4]{};
    const int result = GetUserDefaultGeoName(countryCode, static_cast<int>(sizeof(countryCode) / sizeof(countryCode[0])));
    return result > 1 ? std::wstring(countryCode) : std::wstring{};
}
#endif


}  // namespace

HostTimeInfoManager::HostTimeInfoManager(IDevice *owner) : DeviceComponentBase(owner) {}

void HostTimeInfoManager::setUtcOffsetToDevice() {
    const auto now           = std::chrono::system_clock::now();
    const auto offsetMinutes = getUtcOffsetMinutes(std::chrono::system_clock::to_time_t(now));
    if(offsetMinutes < kMinimumOffsetMinutes || offsetMinutes > kMaximumOffsetMinutes) {
        THROW_INVALID_DATA_EXCEPTION("Current local UTC offset is out of the supported range");
    }

    auto propertyServer = getOwner()->getPropertyServer();
    propertyServer->setPropertyValueT<int32_t>(OB_PROP_LOCAL_UTC_OFFSET_MINUTES_INT, offsetMinutes, PROP_ACCESS_INTERNAL);
}

void HostTimeInfoManager::setHostTimeInfoToDevice() {
    const auto timeInfo = buildHostTimeInfo();
    getOwner()->getPropertyServer()->setStructureDataT<OBHostTimeInfo>(OB_STRUCT_HOST_TIME_INFO, timeInfo, PROP_ACCESS_INTERNAL);
    LOG_DEBUG("Host time information sent: propertyId={}, utcTimeUs={}, timezoneOffsetMin={}, timezone={}",
              static_cast<uint32_t>(OB_STRUCT_HOST_TIME_INFO),
              timeInfo.utcTimeUs, timeInfo.timezoneOffsetMin, timeInfo.timezone);
}

int32_t HostTimeInfoManager::getUtcOffsetMinutes(std::time_t timestamp) {

    std::tm localTime{};
    std::tm utcTime{};
    if(!getLocalTime(timestamp, &localTime) || !getUtcTime(timestamp, &utcTime)) {
        THROW_IO_EXCEPTION("Failed to convert the current system time");
    }

    const auto offsetSeconds = civilTimeInSeconds(localTime) - civilTimeInSeconds(utcTime);
    if(offsetSeconds % kSecondsPerMinute != 0) {
        THROW_INVALID_DATA_EXCEPTION("Current local UTC offset is not an integral number of minutes");
    }
    return static_cast<int32_t>(offsetSeconds / kSecondsPerMinute);
}

std::string HostTimeInfoManager::getCurrentIanaTimeZone() {
#ifdef _WIN32
    DYNAMIC_TIME_ZONE_INFORMATION timeZoneInfo{};
    if(GetDynamicTimeZoneInformation(&timeZoneInfo) == TIME_ZONE_ID_INVALID) {
        THROW_IO_EXCEPTION("Failed to query the current Windows time zone");
    }

    static const std::unordered_map<std::wstring, std::string> kWindowsToIanaTimeZone = {
        { L"Dateline Standard Time", "Etc/GMT+12" },
        { L"UTC-11", "Etc/GMT+11" },
        { L"Aleutian Standard Time", "America/Adak" },
        { L"Hawaiian Standard Time", "Pacific/Honolulu" },
        { L"Marquesas Standard Time", "Pacific/Marquesas" },
        { L"Alaskan Standard Time", "America/Anchorage" },
        { L"UTC-09", "Etc/GMT+9" },
        { L"Pacific Standard Time (Mexico)", "America/Tijuana" },
        { L"UTC-08", "Etc/GMT+8" },
        { L"Pacific Standard Time", "America/Los_Angeles" },
        { L"US Mountain Standard Time", "America/Phoenix" },
        { L"Mountain Standard Time (Mexico)", "America/Mazatlan" },
        { L"Mountain Standard Time", "America/Denver" },
        { L"Yukon Standard Time", "America/Whitehorse" },
        { L"Central America Standard Time", "America/Guatemala" },
        { L"Central Standard Time", "America/Chicago" },
        { L"Easter Island Standard Time", "Pacific/Easter" },
        { L"Central Standard Time (Mexico)", "America/Mexico_City" },
        { L"Canada Central Standard Time", "America/Regina" },
        { L"SA Pacific Standard Time", "America/Bogota" },
        { L"Eastern Standard Time (Mexico)", "America/Cancun" },
        { L"Eastern Standard Time", "America/New_York" },
        { L"Haiti Standard Time", "America/Port-au-Prince" },
        { L"Cuba Standard Time", "America/Havana" },
        { L"US Eastern Standard Time", "America/Indianapolis" },
        { L"Turks And Caicos Standard Time", "America/Grand_Turk" },
        { L"Paraguay Standard Time", "America/Asuncion" },
        { L"Atlantic Standard Time", "America/Halifax" },
        { L"Venezuela Standard Time", "America/Caracas" },
        { L"Central Brazilian Standard Time", "America/Cuiaba" },
        { L"SA Western Standard Time", "America/La_Paz" },
        { L"Pacific SA Standard Time", "America/Santiago" },
        { L"Newfoundland Standard Time", "America/St_Johns" },
        { L"Tocantins Standard Time", "America/Araguaina" },
        { L"E. South America Standard Time", "America/Sao_Paulo" },
        { L"SA Eastern Standard Time", "America/Cayenne" },
        { L"Argentina Standard Time", "America/Buenos_Aires" },
        { L"Greenland Standard Time", "America/Godthab" },
        { L"Montevideo Standard Time", "America/Montevideo" },
        { L"Magallanes Standard Time", "America/Punta_Arenas" },
        { L"Saint Pierre Standard Time", "America/Miquelon" },
        { L"Bahia Standard Time", "America/Bahia" },
        { L"UTC-02", "Etc/GMT+2" },
        { L"Azores Standard Time", "Atlantic/Azores" },
        { L"Cape Verde Standard Time", "Atlantic/Cape_Verde" },
        { L"China Standard Time", "Asia/Shanghai" },
        { L"UTC", "Etc/UTC" },
        { L"GMT Standard Time", "Europe/London" },
        { L"Greenwich Standard Time", "Atlantic/Reykjavik" },
        { L"Sao Tome Standard Time", "Africa/Sao_Tome" },
        { L"Morocco Standard Time", "Africa/Casablanca" },
        { L"W. Europe Standard Time", "Europe/Berlin" },
        { L"Central Europe Standard Time", "Europe/Budapest" },
        { L"Romance Standard Time", "Europe/Paris" },
        { L"Central European Standard Time", "Europe/Warsaw" },
        { L"W. Central Africa Standard Time", "Africa/Lagos" },
        { L"Jordan Standard Time", "Asia/Amman" },
        { L"GTB Standard Time", "Europe/Bucharest" },
        { L"Middle East Standard Time", "Asia/Beirut" },
        { L"Egypt Standard Time", "Africa/Cairo" },
        { L"E. Europe Standard Time", "Europe/Chisinau" },
        { L"Syria Standard Time", "Asia/Damascus" },
        { L"West Bank Standard Time", "Asia/Hebron" },
        { L"South Africa Standard Time", "Africa/Johannesburg" },
        { L"FLE Standard Time", "Europe/Kiev" },
        { L"Israel Standard Time", "Asia/Jerusalem" },
        { L"South Sudan Standard Time", "Africa/Juba" },
        { L"Kaliningrad Standard Time", "Europe/Kaliningrad" },
        { L"Sudan Standard Time", "Africa/Khartoum" },
        { L"Libya Standard Time", "Africa/Tripoli" },
        { L"Namibia Standard Time", "Africa/Windhoek" },
        { L"Arabic Standard Time", "Asia/Baghdad" },
        { L"Turkey Standard Time", "Europe/Istanbul" },
        { L"Arab Standard Time", "Asia/Riyadh" },
        { L"Belarus Standard Time", "Europe/Minsk" },
        { L"Russian Standard Time", "Europe/Moscow" },
        { L"E. Africa Standard Time", "Africa/Nairobi" },
        { L"Iran Standard Time", "Asia/Tehran" },
        { L"Arabian Standard Time", "Asia/Dubai" },
        { L"Astrakhan Standard Time", "Europe/Astrakhan" },
        { L"Azerbaijan Standard Time", "Asia/Baku" },
        { L"Russia Time Zone 3", "Europe/Samara" },
        { L"Mauritius Standard Time", "Indian/Mauritius" },
        { L"Saratov Standard Time", "Europe/Saratov" },
        { L"Georgian Standard Time", "Asia/Tbilisi" },
        { L"Volgograd Standard Time", "Europe/Volgograd" },
        { L"Caucasus Standard Time", "Asia/Yerevan" },
        { L"Afghanistan Standard Time", "Asia/Kabul" },
        { L"West Asia Standard Time", "Asia/Tashkent" },
        { L"Ekaterinburg Standard Time", "Asia/Yekaterinburg" },
        { L"Pakistan Standard Time", "Asia/Karachi" },
        { L"Qyzylorda Standard Time", "Asia/Qyzylorda" },
        { L"India Standard Time", "Asia/Kolkata" },
        { L"Sri Lanka Standard Time", "Asia/Colombo" },
        { L"Nepal Standard Time", "Asia/Katmandu" },
        { L"Central Asia Standard Time", "Asia/Bishkek" },
        { L"Bangladesh Standard Time", "Asia/Dhaka" },
        { L"Omsk Standard Time", "Asia/Omsk" },
        { L"Myanmar Standard Time", "Asia/Rangoon" },
        { L"SE Asia Standard Time", "Asia/Bangkok" },
        { L"Altai Standard Time", "Asia/Barnaul" },
        { L"W. Mongolia Standard Time", "Asia/Hovd" },
        { L"North Asia Standard Time", "Asia/Krasnoyarsk" },
        { L"N. Central Asia Standard Time", "Asia/Novosibirsk" },
        { L"Tomsk Standard Time", "Asia/Tomsk" },
        { L"North Asia East Standard Time", "Asia/Irkutsk" },
        { L"Singapore Standard Time", "Asia/Singapore" },
        { L"W. Australia Standard Time", "Australia/Perth" },
        { L"Taipei Standard Time", "Asia/Taipei" },
        { L"Ulaanbaatar Standard Time", "Asia/Ulaanbaatar" },
        { L"Aus Central W. Standard Time", "Australia/Eucla" },
        { L"Transbaikal Standard Time", "Asia/Chita" },
        { L"Tokyo Standard Time", "Asia/Tokyo" },
        { L"North Korea Standard Time", "Asia/Pyongyang" },
        { L"Korea Standard Time", "Asia/Seoul" },
        { L"Yakutsk Standard Time", "Asia/Yakutsk" },
        { L"Cen. Australia Standard Time", "Australia/Adelaide" },
        { L"AUS Central Standard Time", "Australia/Darwin" },
        { L"E. Australia Standard Time", "Australia/Brisbane" },
        { L"AUS Eastern Standard Time", "Australia/Sydney" },
        { L"West Pacific Standard Time", "Pacific/Port_Moresby" },
        { L"Tasmania Standard Time", "Australia/Hobart" },
        { L"Vladivostok Standard Time", "Asia/Vladivostok" },
        { L"Lord Howe Standard Time", "Australia/Lord_Howe" },
        { L"Bougainville Standard Time", "Pacific/Bougainville" },
        { L"Russia Time Zone 10", "Asia/Srednekolymsk" },
        { L"Magadan Standard Time", "Asia/Magadan" },
        { L"Norfolk Standard Time", "Pacific/Norfolk" },
        { L"Sakhalin Standard Time", "Asia/Sakhalin" },
        { L"Central Pacific Standard Time", "Pacific/Guadalcanal" },
        { L"Russia Time Zone 11", "Asia/Kamchatka" },
        { L"New Zealand Standard Time", "Pacific/Auckland" },
        { L"UTC+12", "Etc/GMT-12" },
        { L"Fiji Standard Time", "Pacific/Fiji" },
        { L"Chatham Islands Standard Time", "Pacific/Chatham" },
        { L"UTC+13", "Etc/GMT-13" },
        { L"Tonga Standard Time", "Pacific/Tongatapu" },
        { L"Samoa Standard Time", "Pacific/Apia" },
        { L"Line Islands Standard Time", "Pacific/Kiritimati" },
    };

    static const std::unordered_map<std::wstring, std::string> kWindowsToIanaTimeZoneByRegion = {
{ L"UTC-11|AS", "Pacific/Pago_Pago" },
        { L"UTC-11|NU", "Pacific/Niue" },
        { L"UTC-11|UM", "Pacific/Midway" },
        { L"Aleutian Standard Time|US", "America/Adak" },
        { L"Hawaiian Standard Time|CK", "Pacific/Rarotonga" },
        { L"Hawaiian Standard Time|PF", "Pacific/Tahiti" },
        { L"Hawaiian Standard Time|US", "Pacific/Honolulu" },
        { L"Marquesas Standard Time|PF", "Pacific/Marquesas" },
        { L"Alaskan Standard Time|US", "America/Anchorage" },
        { L"UTC-09|PF", "Pacific/Gambier" },
        { L"Pacific Standard Time (Mexico)|MX", "America/Tijuana" },
        { L"UTC-08|PN", "Pacific/Pitcairn" },
        { L"Pacific Standard Time|CA", "America/Vancouver" },
        { L"Pacific Standard Time|US", "America/Los_Angeles" },
        { L"US Mountain Standard Time|CA", "America/Creston" },
        { L"US Mountain Standard Time|MX", "America/Hermosillo" },
        { L"US Mountain Standard Time|US", "America/Phoenix" },
        { L"Mountain Standard Time (Mexico)|MX", "America/Mazatlan" },
        { L"Mountain Standard Time|CA", "America/Edmonton" },
        { L"Mountain Standard Time|MX", "America/Ciudad_Juarez" },
        { L"Mountain Standard Time|US", "America/Denver" },
        { L"Yukon Standard Time|CA", "America/Whitehorse" },
        { L"Central America Standard Time|BZ", "America/Belize" },
        { L"Central America Standard Time|CR", "America/Costa_Rica" },
        { L"Central America Standard Time|EC", "Pacific/Galapagos" },
        { L"Central America Standard Time|GT", "America/Guatemala" },
        { L"Central America Standard Time|HN", "America/Tegucigalpa" },
        { L"Central America Standard Time|NI", "America/Managua" },
        { L"Central America Standard Time|SV", "America/El_Salvador" },
        { L"Central Standard Time|CA", "America/Winnipeg" },
        { L"Central Standard Time|MX", "America/Matamoros" },
        { L"Central Standard Time|US", "America/Chicago" },
        { L"Easter Island Standard Time|CL", "Pacific/Easter" },
        { L"Central Standard Time (Mexico)|MX", "America/Mexico_City" },
        { L"Canada Central Standard Time|CA", "America/Regina" },
        { L"SA Pacific Standard Time|BR", "America/Rio_Branco" },
        { L"SA Pacific Standard Time|CA", "America/Coral_Harbour" },
        { L"SA Pacific Standard Time|CO", "America/Bogota" },
        { L"SA Pacific Standard Time|EC", "America/Guayaquil" },
        { L"SA Pacific Standard Time|JM", "America/Jamaica" },
        { L"SA Pacific Standard Time|KY", "America/Cayman" },
        { L"SA Pacific Standard Time|PA", "America/Panama" },
        { L"SA Pacific Standard Time|PE", "America/Lima" },
        { L"Eastern Standard Time (Mexico)|MX", "America/Cancun" },
        { L"Eastern Standard Time|BS", "America/Nassau" },
        { L"Eastern Standard Time|CA", "America/Toronto" },
        { L"Eastern Standard Time|US", "America/New_York" },
        { L"Haiti Standard Time|HT", "America/Port-au-Prince" },
        { L"Cuba Standard Time|CU", "America/Havana" },
        { L"US Eastern Standard Time|US", "America/Indianapolis" },
        { L"Turks And Caicos Standard Time|TC", "America/Grand_Turk" },
        { L"Paraguay Standard Time|PY", "America/Asuncion" },
        { L"Atlantic Standard Time|BM", "Atlantic/Bermuda" },
        { L"Atlantic Standard Time|CA", "America/Halifax" },
        { L"Atlantic Standard Time|GL", "America/Thule" },
        { L"Venezuela Standard Time|VE", "America/Caracas" },
        { L"Central Brazilian Standard Time|BR", "America/Cuiaba" },
        { L"SA Western Standard Time|AG", "America/Antigua" },
        { L"SA Western Standard Time|AI", "America/Anguilla" },
        { L"SA Western Standard Time|AW", "America/Aruba" },
        { L"SA Western Standard Time|BB", "America/Barbados" },
        { L"SA Western Standard Time|BL", "America/St_Barthelemy" },
        { L"SA Western Standard Time|BO", "America/La_Paz" },
        { L"SA Western Standard Time|BQ", "America/Kralendijk" },
        { L"SA Western Standard Time|BR", "America/Manaus" },
        { L"SA Western Standard Time|CA", "America/Blanc-Sablon" },
        { L"SA Western Standard Time|CW", "America/Curacao" },
        { L"SA Western Standard Time|DM", "America/Dominica" },
        { L"SA Western Standard Time|DO", "America/Santo_Domingo" },
        { L"SA Western Standard Time|GD", "America/Grenada" },
        { L"SA Western Standard Time|GP", "America/Guadeloupe" },
        { L"SA Western Standard Time|GY", "America/Guyana" },
        { L"SA Western Standard Time|KN", "America/St_Kitts" },
        { L"SA Western Standard Time|LC", "America/St_Lucia" },
        { L"SA Western Standard Time|MF", "America/Marigot" },
        { L"SA Western Standard Time|MQ", "America/Martinique" },
        { L"SA Western Standard Time|MS", "America/Montserrat" },
        { L"SA Western Standard Time|PR", "America/Puerto_Rico" },
        { L"SA Western Standard Time|SX", "America/Lower_Princes" },
        { L"SA Western Standard Time|TT", "America/Port_of_Spain" },
        { L"SA Western Standard Time|VC", "America/St_Vincent" },
        { L"SA Western Standard Time|VG", "America/Tortola" },
        { L"SA Western Standard Time|VI", "America/St_Thomas" },
        { L"Pacific SA Standard Time|CL", "America/Santiago" },
        { L"Newfoundland Standard Time|CA", "America/St_Johns" },
        { L"Tocantins Standard Time|BR", "America/Araguaina" },
        { L"E. South America Standard Time|BR", "America/Sao_Paulo" },
        { L"SA Eastern Standard Time|AQ", "Antarctica/Rothera" },
        { L"SA Eastern Standard Time|BR", "America/Fortaleza" },
        { L"SA Eastern Standard Time|FK", "Atlantic/Stanley" },
        { L"SA Eastern Standard Time|GF", "America/Cayenne" },
        { L"SA Eastern Standard Time|SR", "America/Paramaribo" },
        { L"Argentina Standard Time|AR", "America/Buenos_Aires" },
        { L"Greenland Standard Time|GL", "America/Godthab" },
        { L"Montevideo Standard Time|UY", "America/Montevideo" },
        { L"Magallanes Standard Time|CL", "America/Punta_Arenas" },
        { L"Saint Pierre Standard Time|PM", "America/Miquelon" },
        { L"Bahia Standard Time|BR", "America/Bahia" },
        { L"UTC-02|BR", "America/Noronha" },
        { L"UTC-02|GS", "Atlantic/South_Georgia" },
        { L"Azores Standard Time|GL", "America/Scoresbysund" },
        { L"Azores Standard Time|PT", "Atlantic/Azores" },
        { L"Cape Verde Standard Time|CV", "Atlantic/Cape_Verde" },
        { L"GMT Standard Time|ES", "Atlantic/Canary" },
        { L"GMT Standard Time|FO", "Atlantic/Faeroe" },
        { L"GMT Standard Time|GB", "Europe/London" },
        { L"GMT Standard Time|GG", "Europe/Guernsey" },
        { L"GMT Standard Time|IE", "Europe/Dublin" },
        { L"GMT Standard Time|IM", "Europe/Isle_of_Man" },
        { L"GMT Standard Time|JE", "Europe/Jersey" },
        { L"GMT Standard Time|PT", "Europe/Lisbon" },
        { L"Greenwich Standard Time|BF", "Africa/Ouagadougou" },
        { L"Greenwich Standard Time|CI", "Africa/Abidjan" },
        { L"Greenwich Standard Time|GH", "Africa/Accra" },
        { L"Greenwich Standard Time|GL", "America/Danmarkshavn" },
        { L"Greenwich Standard Time|GM", "Africa/Banjul" },
        { L"Greenwich Standard Time|GN", "Africa/Conakry" },
        { L"Greenwich Standard Time|GW", "Africa/Bissau" },
        { L"Greenwich Standard Time|IS", "Atlantic/Reykjavik" },
        { L"Greenwich Standard Time|LR", "Africa/Monrovia" },
        { L"Greenwich Standard Time|ML", "Africa/Bamako" },
        { L"Greenwich Standard Time|MR", "Africa/Nouakchott" },
        { L"Greenwich Standard Time|SH", "Atlantic/St_Helena" },
        { L"Greenwich Standard Time|SL", "Africa/Freetown" },
        { L"Greenwich Standard Time|SN", "Africa/Dakar" },
        { L"Greenwich Standard Time|TG", "Africa/Lome" },
        { L"Sao Tome Standard Time|ST", "Africa/Sao_Tome" },
        { L"Morocco Standard Time|EH", "Africa/El_Aaiun" },
        { L"Morocco Standard Time|MA", "Africa/Casablanca" },
        { L"W. Europe Standard Time|AD", "Europe/Andorra" },
        { L"W. Europe Standard Time|AT", "Europe/Vienna" },
        { L"W. Europe Standard Time|CH", "Europe/Zurich" },
        { L"W. Europe Standard Time|DE", "Europe/Berlin" },
        { L"W. Europe Standard Time|GI", "Europe/Gibraltar" },
        { L"W. Europe Standard Time|IT", "Europe/Rome" },
        { L"W. Europe Standard Time|LI", "Europe/Vaduz" },
        { L"W. Europe Standard Time|LU", "Europe/Luxembourg" },
        { L"W. Europe Standard Time|MC", "Europe/Monaco" },
        { L"W. Europe Standard Time|MT", "Europe/Malta" },
        { L"W. Europe Standard Time|NL", "Europe/Amsterdam" },
        { L"W. Europe Standard Time|NO", "Europe/Oslo" },
        { L"W. Europe Standard Time|SE", "Europe/Stockholm" },
        { L"W. Europe Standard Time|SJ", "Arctic/Longyearbyen" },
        { L"W. Europe Standard Time|SM", "Europe/San_Marino" },
        { L"W. Europe Standard Time|VA", "Europe/Vatican" },
        { L"Central Europe Standard Time|AL", "Europe/Tirane" },
        { L"Central Europe Standard Time|CZ", "Europe/Prague" },
        { L"Central Europe Standard Time|HU", "Europe/Budapest" },
        { L"Central Europe Standard Time|ME", "Europe/Podgorica" },
        { L"Central Europe Standard Time|RS", "Europe/Belgrade" },
        { L"Central Europe Standard Time|SI", "Europe/Ljubljana" },
        { L"Central Europe Standard Time|SK", "Europe/Bratislava" },
        { L"Romance Standard Time|BE", "Europe/Brussels" },
        { L"Romance Standard Time|DK", "Europe/Copenhagen" },
        { L"Romance Standard Time|ES", "Europe/Madrid" },
        { L"Romance Standard Time|FR", "Europe/Paris" },
        { L"Central European Standard Time|BA", "Europe/Sarajevo" },
        { L"Central European Standard Time|HR", "Europe/Zagreb" },
        { L"Central European Standard Time|MK", "Europe/Skopje" },
        { L"Central European Standard Time|PL", "Europe/Warsaw" },
        { L"W. Central Africa Standard Time|AO", "Africa/Luanda" },
        { L"W. Central Africa Standard Time|BJ", "Africa/Porto-Novo" },
        { L"W. Central Africa Standard Time|CD", "Africa/Kinshasa" },
        { L"W. Central Africa Standard Time|CF", "Africa/Bangui" },
        { L"W. Central Africa Standard Time|CG", "Africa/Brazzaville" },
        { L"W. Central Africa Standard Time|CM", "Africa/Douala" },
        { L"W. Central Africa Standard Time|DZ", "Africa/Algiers" },
        { L"W. Central Africa Standard Time|GA", "Africa/Libreville" },
        { L"W. Central Africa Standard Time|GQ", "Africa/Malabo" },
        { L"W. Central Africa Standard Time|NE", "Africa/Niamey" },
        { L"W. Central Africa Standard Time|NG", "Africa/Lagos" },
        { L"W. Central Africa Standard Time|TD", "Africa/Ndjamena" },
        { L"W. Central Africa Standard Time|TN", "Africa/Tunis" },
        { L"Jordan Standard Time|JO", "Asia/Amman" },
        { L"GTB Standard Time|CY", "Asia/Nicosia" },
        { L"GTB Standard Time|GR", "Europe/Athens" },
        { L"GTB Standard Time|RO", "Europe/Bucharest" },
        { L"Middle East Standard Time|LB", "Asia/Beirut" },
        { L"Egypt Standard Time|EG", "Africa/Cairo" },
        { L"E. Europe Standard Time|MD", "Europe/Chisinau" },
        { L"Syria Standard Time|SY", "Asia/Damascus" },
        { L"West Bank Standard Time|PS", "Asia/Hebron" },
        { L"South Africa Standard Time|BI", "Africa/Bujumbura" },
        { L"South Africa Standard Time|BW", "Africa/Gaborone" },
        { L"South Africa Standard Time|CD", "Africa/Lubumbashi" },
        { L"South Africa Standard Time|LS", "Africa/Maseru" },
        { L"South Africa Standard Time|MW", "Africa/Blantyre" },
        { L"South Africa Standard Time|MZ", "Africa/Maputo" },
        { L"South Africa Standard Time|RW", "Africa/Kigali" },
        { L"South Africa Standard Time|SZ", "Africa/Mbabane" },
        { L"South Africa Standard Time|ZA", "Africa/Johannesburg" },
        { L"South Africa Standard Time|ZM", "Africa/Lusaka" },
        { L"South Africa Standard Time|ZW", "Africa/Harare" },
        { L"FLE Standard Time|AX", "Europe/Mariehamn" },
        { L"FLE Standard Time|BG", "Europe/Sofia" },
        { L"FLE Standard Time|EE", "Europe/Tallinn" },
        { L"FLE Standard Time|FI", "Europe/Helsinki" },
        { L"FLE Standard Time|LT", "Europe/Vilnius" },
        { L"FLE Standard Time|LV", "Europe/Riga" },
        { L"FLE Standard Time|UA", "Europe/Kiev" },
        { L"Israel Standard Time|IL", "Asia/Jerusalem" },
        { L"South Sudan Standard Time|SS", "Africa/Juba" },
        { L"Kaliningrad Standard Time|RU", "Europe/Kaliningrad" },
        { L"Sudan Standard Time|SD", "Africa/Khartoum" },
        { L"Libya Standard Time|LY", "Africa/Tripoli" },
        { L"Namibia Standard Time|NA", "Africa/Windhoek" },
        { L"Arabic Standard Time|IQ", "Asia/Baghdad" },
        { L"Turkey Standard Time|TR", "Europe/Istanbul" },
        { L"Arab Standard Time|BH", "Asia/Bahrain" },
        { L"Arab Standard Time|KW", "Asia/Kuwait" },
        { L"Arab Standard Time|QA", "Asia/Qatar" },
        { L"Arab Standard Time|SA", "Asia/Riyadh" },
        { L"Arab Standard Time|YE", "Asia/Aden" },
        { L"Belarus Standard Time|BY", "Europe/Minsk" },
        { L"Russian Standard Time|RU", "Europe/Moscow" },
        { L"Russian Standard Time|UA", "Europe/Simferopol" },
        { L"E. Africa Standard Time|AQ", "Antarctica/Syowa" },
        { L"E. Africa Standard Time|DJ", "Africa/Djibouti" },
        { L"E. Africa Standard Time|ER", "Africa/Asmera" },
        { L"E. Africa Standard Time|ET", "Africa/Addis_Ababa" },
        { L"E. Africa Standard Time|KE", "Africa/Nairobi" },
        { L"E. Africa Standard Time|KM", "Indian/Comoro" },
        { L"E. Africa Standard Time|MG", "Indian/Antananarivo" },
        { L"E. Africa Standard Time|SO", "Africa/Mogadishu" },
        { L"E. Africa Standard Time|TZ", "Africa/Dar_es_Salaam" },
        { L"E. Africa Standard Time|UG", "Africa/Kampala" },
        { L"E. Africa Standard Time|YT", "Indian/Mayotte" },
        { L"Iran Standard Time|IR", "Asia/Tehran" },
        { L"Arabian Standard Time|AE", "Asia/Dubai" },
        { L"Arabian Standard Time|OM", "Asia/Muscat" },
        { L"Astrakhan Standard Time|RU", "Europe/Astrakhan" },
        { L"Azerbaijan Standard Time|AZ", "Asia/Baku" },
        { L"Russia Time Zone 3|RU", "Europe/Samara" },
        { L"Mauritius Standard Time|MU", "Indian/Mauritius" },
        { L"Mauritius Standard Time|RE", "Indian/Reunion" },
        { L"Mauritius Standard Time|SC", "Indian/Mahe" },
        { L"Saratov Standard Time|RU", "Europe/Saratov" },
        { L"Georgian Standard Time|GE", "Asia/Tbilisi" },
        { L"Volgograd Standard Time|RU", "Europe/Volgograd" },
        { L"Caucasus Standard Time|AM", "Asia/Yerevan" },
        { L"Afghanistan Standard Time|AF", "Asia/Kabul" },
        { L"West Asia Standard Time|AQ", "Antarctica/Mawson" },
        { L"West Asia Standard Time|KZ", "Asia/Oral" },
        { L"West Asia Standard Time|MV", "Indian/Maldives" },
        { L"West Asia Standard Time|TF", "Indian/Kerguelen" },
        { L"West Asia Standard Time|TJ", "Asia/Dushanbe" },
        { L"West Asia Standard Time|TM", "Asia/Ashgabat" },
        { L"West Asia Standard Time|UZ", "Asia/Tashkent" },
        { L"Ekaterinburg Standard Time|RU", "Asia/Yekaterinburg" },
        { L"Pakistan Standard Time|PK", "Asia/Karachi" },
        { L"Qyzylorda Standard Time|KZ", "Asia/Qyzylorda" },
        { L"India Standard Time|IN", "Asia/Calcutta" },
        { L"Sri Lanka Standard Time|LK", "Asia/Colombo" },
        { L"Nepal Standard Time|NP", "Asia/Katmandu" },
        { L"Central Asia Standard Time|AQ", "Antarctica/Vostok" },
        { L"Central Asia Standard Time|CN", "Asia/Urumqi" },
        { L"Central Asia Standard Time|IO", "Indian/Chagos" },
        { L"Central Asia Standard Time|KG", "Asia/Bishkek" },
        { L"Bangladesh Standard Time|BD", "Asia/Dhaka" },
        { L"Bangladesh Standard Time|BT", "Asia/Thimphu" },
        { L"Omsk Standard Time|RU", "Asia/Omsk" },
        { L"Myanmar Standard Time|CC", "Indian/Cocos" },
        { L"Myanmar Standard Time|MM", "Asia/Rangoon" },
        { L"SE Asia Standard Time|AQ", "Antarctica/Davis" },
        { L"SE Asia Standard Time|CX", "Indian/Christmas" },
        { L"SE Asia Standard Time|ID", "Asia/Jakarta" },
        { L"SE Asia Standard Time|KH", "Asia/Phnom_Penh" },
        { L"SE Asia Standard Time|LA", "Asia/Vientiane" },
        { L"SE Asia Standard Time|TH", "Asia/Bangkok" },
        { L"SE Asia Standard Time|VN", "Asia/Saigon" },
        { L"Altai Standard Time|RU", "Asia/Barnaul" },
        { L"W. Mongolia Standard Time|MN", "Asia/Hovd" },
        { L"North Asia Standard Time|RU", "Asia/Krasnoyarsk" },
        { L"N. Central Asia Standard Time|RU", "Asia/Novosibirsk" },
        { L"Tomsk Standard Time|RU", "Asia/Tomsk" },
        { L"China Standard Time|CN", "Asia/Shanghai" },
        { L"China Standard Time|HK", "Asia/Hong_Kong" },
        { L"China Standard Time|MO", "Asia/Macau" },
        { L"North Asia East Standard Time|RU", "Asia/Irkutsk" },
        { L"Singapore Standard Time|BN", "Asia/Brunei" },
        { L"Singapore Standard Time|ID", "Asia/Makassar" },
        { L"Singapore Standard Time|MY", "Asia/Kuala_Lumpur" },
        { L"Singapore Standard Time|PH", "Asia/Manila" },
        { L"Singapore Standard Time|SG", "Asia/Singapore" },
        { L"W. Australia Standard Time|AU", "Australia/Perth" },
        { L"Taipei Standard Time|TW", "Asia/Taipei" },
        { L"Ulaanbaatar Standard Time|MN", "Asia/Ulaanbaatar" },
        { L"Aus Central W. Standard Time|AU", "Australia/Eucla" },
        { L"Transbaikal Standard Time|RU", "Asia/Chita" },
        { L"Tokyo Standard Time|ID", "Asia/Jayapura" },
        { L"Tokyo Standard Time|JP", "Asia/Tokyo" },
        { L"Tokyo Standard Time|PW", "Pacific/Palau" },
        { L"Tokyo Standard Time|TL", "Asia/Dili" },
        { L"North Korea Standard Time|KP", "Asia/Pyongyang" },
        { L"Korea Standard Time|KR", "Asia/Seoul" },
        { L"Yakutsk Standard Time|RU", "Asia/Yakutsk" },
        { L"Cen. Australia Standard Time|AU", "Australia/Adelaide" },
        { L"AUS Central Standard Time|AU", "Australia/Darwin" },
        { L"E. Australia Standard Time|AU", "Australia/Brisbane" },
        { L"AUS Eastern Standard Time|AU", "Australia/Sydney" },
        { L"West Pacific Standard Time|AQ", "Antarctica/DumontDUrville" },
        { L"West Pacific Standard Time|FM", "Pacific/Truk" },
        { L"West Pacific Standard Time|GU", "Pacific/Guam" },
        { L"West Pacific Standard Time|MP", "Pacific/Saipan" },
        { L"West Pacific Standard Time|PG", "Pacific/Port_Moresby" },
        { L"Tasmania Standard Time|AU", "Australia/Hobart" },
        { L"Vladivostok Standard Time|RU", "Asia/Vladivostok" },
        { L"Lord Howe Standard Time|AU", "Australia/Lord_Howe" },
        { L"Bougainville Standard Time|PG", "Pacific/Bougainville" },
        { L"Russia Time Zone 10|RU", "Asia/Srednekolymsk" },
        { L"Magadan Standard Time|RU", "Asia/Magadan" },
        { L"Norfolk Standard Time|NF", "Pacific/Norfolk" },
        { L"Sakhalin Standard Time|RU", "Asia/Sakhalin" },
        { L"Central Pacific Standard Time|AQ", "Antarctica/Casey" },
        { L"Central Pacific Standard Time|FM", "Pacific/Ponape" },
        { L"Central Pacific Standard Time|NC", "Pacific/Noumea" },
        { L"Central Pacific Standard Time|SB", "Pacific/Guadalcanal" },
        { L"Central Pacific Standard Time|VU", "Pacific/Efate" },
        { L"Russia Time Zone 11|RU", "Asia/Kamchatka" },
        { L"New Zealand Standard Time|AQ", "Antarctica/McMurdo" },
        { L"New Zealand Standard Time|NZ", "Pacific/Auckland" },
        { L"UTC+12|KI", "Pacific/Tarawa" },
        { L"UTC+12|MH", "Pacific/Majuro" },
        { L"UTC+12|NR", "Pacific/Nauru" },
        { L"UTC+12|TV", "Pacific/Funafuti" },
        { L"UTC+12|UM", "Pacific/Wake" },
        { L"UTC+12|WF", "Pacific/Wallis" },
        { L"Fiji Standard Time|FJ", "Pacific/Fiji" },
        { L"Chatham Islands Standard Time|NZ", "Pacific/Chatham" },
        { L"UTC+13|KI", "Pacific/Enderbury" },
        { L"UTC+13|TK", "Pacific/Fakaofo" },
        { L"Tonga Standard Time|TO", "Pacific/Tongatapu" },
        { L"Samoa Standard Time|WS", "Pacific/Apia" },
        { L"Line Islands Standard Time|KI", "Pacific/Kiritimati" },
    };

    const auto countryCode = getWindowsUserCountryCode();
    if(!countryCode.empty()) {
        const auto regionalIter = kWindowsToIanaTimeZoneByRegion.find(std::wstring(timeZoneInfo.TimeZoneKeyName) + L"|" + countryCode);
        if(regionalIter != kWindowsToIanaTimeZoneByRegion.end()) {
            return regionalIter->second;
        }
    }

    const auto iter = kWindowsToIanaTimeZone.find(timeZoneInfo.TimeZoneKeyName);
    if(iter == kWindowsToIanaTimeZone.end()) {
        THROW_UNSUPPORTED_OPERATION_EXCEPTION("The current Windows time zone cannot be mapped to an IANA time zone");
    }
    return iter->second;
#elif defined(__ANDROID__)
    char tzProp[PROP_VALUE_MAX]{};
    if(__system_property_get("persist.sys.timezone", tzProp) > 0) {
        const std::string timezone = normalizeIanaTimeZone(tzProp);
        if(!timezone.empty()) {
            return timezone;
        }
    }
    THROW_UNSUPPORTED_OPERATION_EXCEPTION("The current Android time zone cannot be mapped to an IANA time zone");
#elif defined(__APPLE__)
    CFTimeZoneRef timeZone = CFTimeZoneCopySystem();
    if(timeZone == nullptr) {
        THROW_IO_EXCEPTION("Failed to query the current Apple system time zone");
    }

    const auto name = CFTimeZoneGetName(timeZone);
    char       timezone[64]{};
    const bool converted = name != nullptr && CFStringGetCString(name, timezone, sizeof(timezone), kCFStringEncodingUTF8);
    CFRelease(timeZone);

    if(converted) {
        const std::string ianaTimeZone = normalizeIanaTimeZone(timezone);
        if(!ianaTimeZone.empty()) {
            return ianaTimeZone;
        }
    }
    THROW_UNSUPPORTED_OPERATION_EXCEPTION("The current Apple system time zone cannot be mapped to an IANA time zone");
#else
    const auto tz = std::getenv("TZ");
    if(tz && tz[0] != '\0') {
        const std::string timezone = normalizeIanaTimeZone(tz[0] == ':' ? tz + 1 : tz);
        if(!timezone.empty()) {
            return timezone;
        }
    }

    std::ifstream timezoneFile("/etc/timezone");
    std::string   timezone;
    if(std::getline(timezoneFile, timezone)) {
        timezone = normalizeIanaTimeZone(std::move(timezone));
        if(!timezone.empty()) {
            return timezone;
        }
    }

    // Covers Linux distros that use an /etc/localtime symlink (RHEL/Fedora/Arch).
    char linkTarget[512]{};
    const ssize_t len = readlink("/etc/localtime", linkTarget, sizeof(linkTarget) - 1);
    if(len > 0) {
        const std::string path(linkTarget, static_cast<size_t>(len));
        const std::string marker = "zoneinfo/";
        const auto        pos    = path.find(marker);
        if(pos != std::string::npos) {
            const std::string tz2 = normalizeIanaTimeZone(path.substr(pos + marker.size()));
            if(!tz2.empty()) {
                return tz2;
            }
        }
    }

    THROW_UNSUPPORTED_OPERATION_EXCEPTION("The current system time zone cannot be mapped to an IANA time zone");
#endif
}

OBHostTimeInfo HostTimeInfoManager::buildHostTimeInfo() {
    const auto now       = std::chrono::system_clock::now();
    const auto utcTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
    if(utcTimeUs < 0) {
        LOG_WARN("Current system UTC time is before Unix epoch; falling back to 0");
    }

    OBHostTimeInfo timeInfo{};
    timeInfo.utcTimeUs = utcTimeUs < 0 ? 0 : static_cast<uint64_t>(utcTimeUs);

    int32_t offsetMin = 0;
    BEGIN_TRY_EXECUTE({ offsetMin = getUtcOffsetMinutes(std::chrono::system_clock::to_time_t(now)); })
    CATCH_EXCEPTION_AND_EXECUTE({
        LOG_WARN("Failed to obtain UTC offset; falling back to 0");
    })
    if(offsetMin < kMinimumOffsetMinutes || offsetMin > kMaximumOffsetMinutes) {
        LOG_WARN("UTC offset {} is out of range; falling back to 0", offsetMin);
        offsetMin = 0;
    }
    timeInfo.timezoneOffsetMin = offsetMin;

    std::string timezone;
    BEGIN_TRY_EXECUTE({ timezone = getCurrentIanaTimeZone(); })
    CATCH_EXCEPTION_AND_EXECUTE({
        LOG_WARN("Failed to obtain the current IANA time zone; falling back to Etc/UTC");
        timezone = "Etc/UTC";
    })

    if(timezone.empty() || timezone.size() >= sizeof(timeInfo.timezone)) {
        LOG_WARN("Current IANA time zone is invalid or too long; falling back to Etc/UTC");
        timezone = "Etc/UTC";
    }
    std::memcpy(timeInfo.timezone, timezone.data(), timezone.size());
    return timeInfo;
}

}  // namespace libobsensor
