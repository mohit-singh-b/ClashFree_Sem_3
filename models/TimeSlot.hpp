#ifndef TIMESLOT_H
#define TIMESLOT_H

enum class Day {MON, TUE, WED, THU, FRI, SAT};

class TimeSlot {
private:
    Day day;
    int start_time; 

public:
    TimeSlot(Day day, int start_time) : day(day), start_time(start_time) {}

    Day getDay() const { return day; }
    int getPeriod() const { return start_time; }

    bool operator==(const TimeSlot& other) const {
        return day == other.day && start_time == other.start_time;
    }

    bool operator>(const TimeSlot& other) const {
        if ( day == other.day) return start_time > other.start_time;
        return day > other.day;
    }
    bool operator<(const TimeSlot& other) const {
        if ( day == other.day) return start_time < other.start_time;
        return day < other.day;
    }

};

#endif 