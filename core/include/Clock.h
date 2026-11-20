#ifndef SERCOM_CLOCK_H
#define SERCOM_CLOCK_H

#include <chrono>

namespace sercom::Clock {

class Clock
{
public:
    /*
     * Source of the current time, injected so that tests can control the time.
     */
    Clock() = default;
    virtual ~Clock();

    // delete copy constructor and copy assignment operator
    Clock(const Clock &) = delete;
    Clock &operator=(const Clock &) = delete;

    // delete move constructor and move assignment operator
    Clock(Clock &&) = delete;
    Clock &operator=(Clock &&) = delete;

public:
    /*
     * Get the current time in milliseconds since an arbitrary start point.
     */
    [[nodiscard]] virtual std::chrono::milliseconds getTime() const = 0;
};

} // namespace sercom::Clock

#endif // SERCOM_CLOCK_H
