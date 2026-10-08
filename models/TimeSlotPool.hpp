#ifndef TIMESLOTPOOL_H
#define TIMESLOTPOOL_H

#include <vector>
#include <stdexcept>
#include "TimeSlot.hpp"


class TimeSlotPool {
private:
    std::vector<TimeSlot> slots;
    int numDays;
    int periodsPerDay;

public:
    TimeSlotPool(int days, int periodsPerDay)
        : numDays(days), periodsPerDay(periodsPerDay) {
        if (days < 1 || days > 6) {
            throw std::invalid_argument("days must be between 1 and 6");
        }
        if (periodsPerDay < 1) {
            throw std::invalid_argument("periodsPerDay must be at least 1");
        }
        slots.reserve(days * periodsPerDay);
        for (int d = 0; d < days; d++) {
            for (int p = 1; p <= periodsPerDay; p++) {
                slots.emplace_back(static_cast<Day>(d), p);
            }
        }
    }

    const std::vector<TimeSlot>& all() const { return slots; }

    // number of colors (K)
    int size() const { return static_cast<int>(slots.size()); }

    int getNumDays() const { return numDays; }
    int getPeriodsPerDay() const { return periodsPerDay; }

    const TimeSlot& at(int index) const { return slots.at(index); }

    int indexOf(const TimeSlot& s) const {
        for (size_t i = 0; i < slots.size(); i++) {
            if (slots[i] == s) return static_cast<int>(i);
        }
        return -1;
    }

    bool contains(const TimeSlot& s) const { return indexOf(s) != -1; }
};

#endif 
