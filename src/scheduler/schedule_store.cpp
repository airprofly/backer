#include "scheduler/schedule_store.h"

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <spdlog/spdlog.h>

namespace backer {

// ══════════════════════════════════════════════════════════════════════════════
// Internal: minimal JSON reader / writer for the schedule format.
//
// Handles exactly the schema we need — no general-purpose JSON library.
// ══════════════════════════════════════════════════════════════════════════════

namespace {

/// Escape a string for JSON output.
std::string jsonEscape(std::string const& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\t': out += "\\t";  break;
            case '\r': out += "\\r";  break;
            default:   out += c;
        }
    }
    return out;
}

/// Serialise a time_point to ISO 8601 (UTC).
std::string timeToIso(std::chrono::system_clock::time_point tp) {
    auto t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#if BACKER_PLATFORM_WINDOWS
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::ostringstream os;
    os << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return os.str();
}

/// Parse ISO 8601 ("2026-07-01T10:00:00Z") to time_point.
std::chrono::system_clock::time_point isoToTime(std::string const& s) {
    std::tm tm{};
    char sep;
    std::istringstream is(s);
    is >> tm.tm_year >> sep >> tm.tm_mon >> sep >> tm.tm_mday
       >> tm.tm_hour >> sep >> tm.tm_min >> sep >> tm.tm_sec;
    if (is.fail()) {
        return std::chrono::system_clock::time_point{};
    }
    tm.tm_year -= 1900;
    tm.tm_mon  -= 1;
    tm.tm_isdst = 0; // UTC
    auto t = timegm(&tm);
    return std::chrono::system_clock::from_time_t(t);
}

// ── JSON serialisation ──────────────────────────────────────────────────────

void writeJsonString(std::ostream& os, std::string const& val, int indent) {
    os << std::string(indent, ' ') << '"' << jsonEscape(val) << '"';
}

void writeJsonBool(std::ostream& os, bool val, int indent) {
    os << std::string(indent, ' ') << (val ? "true" : "false");
}

void writeJsonInt(std::ostream& os, int val, int indent) {
    os << std::string(indent, ' ') << val;
}

void serializeJob(std::ostream& os, ScheduleJob const& job, int indent) {
    os << std::string(indent, ' ') << "{\n";

    auto e = [&](int i) { return std::string(indent + 2, ' '); };
    auto comma = [i = 0]() mutable { return i++ ? ",\n" : "\n"; };

    int f = 0;
    os << e(1) << '"' << "id"       << "\": " << '"' << jsonEscape(job.id)             << '"'; f++;
    os << ",\n" << e(1) << '"' << "name"     << "\": " << '"' << jsonEscape(job.name)           << '"'; f++;
    os << ",\n" << e(1) << '"' << "cron"     << "\": " << '"' << jsonEscape(job.cronExpression) << '"'; f++;
    os << ",\n" << e(1) << '"' << "source"   << "\": " << '"' << jsonEscape(job.source.string()) << '"'; f++;
    os << ",\n" << e(1) << '"' << "dest"     << "\": " << '"' << jsonEscape(job.destination.string()) << '"'; f++;
    os << ",\n" << e(1) << '"' << "enabled"  << "\": " << (job.enabled ? "true" : "false"); f++;

    // options object
    os << ",\n" << e(1) << '"' << "options" << "\": {\n";
    auto const& o = job.options;
    int of = 0;
    auto ek = [&](int i) { return std::string(indent + 4, ' '); };
    #define OPT_FIELD(name, field) \
        if (of++) os << ",\n"; \
        os << ek(2) << '"' << (name) << "\": " << '"' << jsonEscape(o.field) << '"'
    #define OPT_FIELD_INT(name, field) \
        if (of++) os << ",\n"; \
        os << ek(2) << '"' << (name) << "\": " << o.field

    if (!o.compressAlgo.empty())    { if (of++) os << ",\n"; os << ek(2) << '"' << "compress"    << "\": " << '"' << jsonEscape(o.compressAlgo) << '"'; }
    if (o.compressLevel != 0)       { if (of++) os << ",\n"; os << ek(2) << '"' << "compressLevel"  << "\": " << o.compressLevel; }
    if (!o.encryptAlgo.empty())     { if (of++) os << ",\n"; os << ek(2) << '"' << "encrypt"     << "\": " << '"' << jsonEscape(o.encryptAlgo) << '"'; }
    if (!o.password.empty())        { if (of++) os << ",\n"; os << ek(2) << '"' << "password"    << "\": " << '"' << jsonEscape(o.password) << '"'; }
    if (!o.packFormat.empty())      { if (of++) os << ",\n"; os << ek(2) << '"' << "pack"        << "\": " << '"' << jsonEscape(o.packFormat) << '"'; }
    if (o.retainCount > 0)          { if (of++) os << ",\n"; os << ek(2) << '"' << "retainCount" << "\": " << o.retainCount; }
    if (o.retainDays > 0)           { if (of++) os << ",\n"; os << ek(2) << '"' << "retainDays"  << "\": " << o.retainDays; }
    if (!o.preserveMetadata)        { if (of++) os << ",\n"; os << ek(2) << '"' << "noMetadata"  << "\": true"; }
    if (!o.handleSpecial)           { if (of++) os << ",\n"; os << ek(2) << '"' << "skipSpecial" << "\": true"; }

    #undef OPT_FIELD
    #undef OPT_FIELD_INT
    if (!of) os << ek(2) << '"' << "empty" << "\": true";
    os << '\n' << ek(1) << "}"; // close options

    // createdAt
    os << ",\n" << e(1) << '"' << "createdAt" << "\": " << '"' << timeToIso(job.createdAt) << '"';

    os << '\n' << std::string(indent, ' ') << "}";
}

std::string serializeJobs(std::vector<ScheduleJob> const& jobs) {
    std::ostringstream os;
    os << "{\n  \"jobs\": [\n";
    for (std::size_t i = 0; i < jobs.size(); ++i) {
        if (i > 0) os << ",\n";
        serializeJob(os, jobs[i], 4);
    }
    os << "\n  ]\n}\n";
    return os.str();
}

// ── JSON deserialisation ────────────────────────────────────────────────────

/// Very basic JSON parser that extracts string/bool/int values by key.
/// Only handles the exact structure we write above.  Not a general parser.

std::string trim(std::string const& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

/// Find the value of a top-level key in a JSON-like string.
/// Handles "key": "string", "key": true/false, "key": 123.
/// Does NOT handle nested objects (we process those separately).
std::string extractStr(std::string const& json, std::string const& key) {
    auto k = '"' + key + '"';
    auto pos = json.find(k);
    if (pos == std::string::npos) return {};
    pos = json.find(':', pos + k.size());
    if (pos == std::string::npos) return {};
    pos++; // skip ':'
    // Skip whitespace
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
                                 json[pos] == '\n' || json[pos] == '\r')) pos++;
    if (pos >= json.size()) return {};
    if (json[pos] == '"') {
        // String value
        pos++;
        std::string val;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\') { pos++; if (pos >= json.size()) break; }
            val += json[pos++];
        }
        return val;
    }
    // Number or boolean — return up to next comma or brace
    auto end = json.find_first_of(",}]", pos);
    if (end == std::string::npos) end = json.size();
    return trim(json.substr(pos, end - pos));
}

int extractInt(std::string const& json, std::string const& key) {
    auto s = extractStr(json, key);
    return s.empty() ? 0 : std::stoi(s);
}

bool extractBool(std::string const& json, std::string const& key, bool def = false) {
    auto s = extractStr(json, key);
    if (s.empty()) return def;
    return s == "true";
}

/// Extract the content of a nested object for a given key.
/// e.g. extractObject("... \"options\": { ... } ...", "options")
/// Returns the content between the outer braces.
std::string extractObject(std::string const& json, std::string const& key) {
    auto k = '"' + key + '"';
    auto pos = json.find(k);
    if (pos == std::string::npos) return {};
    pos = json.find(':', pos + k.size());
    if (pos == std::string::npos) return {};
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
                                 json[pos] == '\n' || json[pos] == '\r')) pos++;
    if (pos >= json.size() || json[pos] != '{') return {};

    int depth = 0;
    std::size_t start = pos;
    while (pos < json.size()) {
        if (json[pos] == '{') depth++;
        else if (json[pos] == '}') { depth--; if (depth == 0) break; }
        pos++;
    }
    if (depth != 0) return {};
    return json.substr(start, pos - start + 1);
}

ScheduleJob parseOneJob(std::string const& objJson) {
    ScheduleJob job;
    job.id             = extractStr(objJson, "id");
    job.name           = extractStr(objJson, "name");
    job.cronExpression = extractStr(objJson, "cron");
    job.source         = extractStr(objJson, "source");
    job.destination    = extractStr(objJson, "dest");
    job.enabled        = extractBool(objJson, "enabled", true);

    auto iso = extractStr(objJson, "createdAt");
    if (!iso.empty()) job.createdAt = isoToTime(iso);

    // Parse options sub-object
    auto optJson = extractObject(objJson, "options");
    if (!optJson.empty()) {
        auto& o = job.options;
        o.compressAlgo     = extractStr(optJson, "compress");
        o.compressLevel    = extractInt(optJson, "compressLevel");
        o.encryptAlgo      = extractStr(optJson, "encrypt");
        o.password         = extractStr(optJson, "password");
        o.packFormat       = extractStr(optJson, "pack");
        o.retainCount      = extractInt(optJson, "retainCount");
        o.retainDays       = extractInt(optJson, "retainDays");
        o.preserveMetadata = !extractBool(optJson, "noMetadata", false);
        o.handleSpecial    = !extractBool(optJson, "skipSpecial", false);
    }

    return job;
}

std::vector<ScheduleJob> deserializeJobs(std::string const& json) {
    std::vector<ScheduleJob> result;

    // Find the "jobs" array
    auto arrPos = json.find("\"jobs\"");
    if (arrPos == std::string::npos) return result;

    // Find the '[' after "jobs":
    arrPos = json.find('[', arrPos);
    if (arrPos == std::string::npos) return result;

    // Extract each object from the array
    std::size_t i = arrPos;
    while (i < json.size()) {
        auto ob = json.find('{', i);
        if (ob == std::string::npos || ob > json.find(']', i)) break;

        int depth = 0;
        std::size_t end = ob;
        while (end < json.size()) {
            if (json[end] == '{') depth++;
            else if (json[end] == '}') { depth--; if (depth == 0) break; }
            end++;
        }
        if (depth != 0) break;

        auto jobJson = json.substr(ob, end - ob + 1);
        result.push_back(parseOneJob(jobJson));

        i = end + 1;
    }

    return result;
}

} // anonymous namespace

// ══════════════════════════════════════════════════════════════════════════════
// ScheduleStore implementation
// ══════════════════════════════════════════════════════════════════════════════

std::filesystem::path ScheduleStore::defaultPath() {
    auto const* home = std::getenv("HOME");
    if (!home) {
        home = std::getenv("USERPROFILE"); // Windows
    }
    if (!home) {
        spdlog::warn("HOME not set, using ./schedule.json");
        return "schedule.json";
    }
    return std::filesystem::path(home) / ".config" / "backer" / "schedule.json";
}

ScheduleStore::ScheduleStore(std::filesystem::path filePath)
    : filePath_(filePath.empty() ? defaultPath() : std::move(filePath))
{
}

Expected<void, ErrorCode>
ScheduleStore::save(std::vector<ScheduleJob> const& jobs) {
    // Ensure the parent directory exists
    auto parent = filePath_.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            spdlog::error("Cannot create schedule directory {}: {}",
                          parent.string(), ec.message());
            return ErrorCode::kWriteFailed;
        }
    }

    auto json = serializeJobs(jobs);

    std::ofstream out(filePath_, std::ios::trunc);
    if (!out) {
        spdlog::error("Cannot open {} for writing", filePath_.string());
        return ErrorCode::kWriteFailed;
    }

    out << json;
    if (!out) {
        spdlog::error("Failed to write schedule file {}", filePath_.string());
        return ErrorCode::kWriteFailed;
    }

    spdlog::debug("Saved {} job(s) to {}", jobs.size(), filePath_.string());
    return {};
}

Expected<std::vector<ScheduleJob>, ErrorCode>
ScheduleStore::load() {
    std::ifstream in(filePath_);
    if (!in) {
        // File doesn't exist yet — first run, return empty list
        if (!std::filesystem::exists(filePath_)) {
            spdlog::info("Schedule file not found (first run): {}", filePath_.string());
            return std::vector<ScheduleJob>{};
        }
        spdlog::error("Cannot open schedule file {}", filePath_.string());
        return ErrorCode::kReadFailed;
    }

    std::string json((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());

    if (json.empty()) {
        return std::vector<ScheduleJob>{};
    }

    auto jobs = deserializeJobs(json);
    spdlog::debug("Loaded {} job(s) from {}", jobs.size(), filePath_.string());
    return jobs;
}

} // namespace backer
