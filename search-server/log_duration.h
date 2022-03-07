﻿#pragma once

#include <chrono>
#include <string>
#include <iostream>

#define PROFILE_CONCAT_INTERNAL(X, Y) X##Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(profileGuard, __LINE__)
#define LOG_DURATION(x) LogDuration UNIQUE_VAR_NAME_PROFILE(x)
#define LOG_DURATION_STREAM(x, y) LogDuration UNIQUE_VAR_NAME_PROFILE(x, y)

class LogDuration
{
public:
    using Clock = std::chrono::steady_clock;

    LogDuration(const std::string& id, std::ostream& stream);
    ~LogDuration();

private:
    const std::string id_;
    std::ostream& os;
    const Clock::time_point start_time_ = Clock::now();
};