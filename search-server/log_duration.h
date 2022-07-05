#pragma once
#include <chrono>
#include <iostream>

#define PROFILE_CONCAT_INTERNAL(X, Y) X##Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(profileGuard, LINE)
#define LOG_DURATION(x,y) LogDuration UNIQUE_VAR_NAME_PROFILE(x,y)
#define LOG_DURATION_STREAM(x,y) LOG_DURATION(x,y)

using namespace std;

class LogDuration {
public:
    using Clock = std::chrono::steady_clock;

    LogDuration(const string& id, ostream& out)
            : id_(id), out(out) {

    }

    ~LogDuration() {
        using namespace chrono;
        using namespace literals;

        const auto end_time = Clock::now();
        const auto dur = end_time - start_time_;
        out << id_ << ": "s << duration_cast<milliseconds>(dur).count() << " ms"s << endl;
    }

private:
    const string id_;
    const Clock::time_point start_time_ = Clock::now();
    ostream& out = cerr;
};