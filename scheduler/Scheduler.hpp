#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <string>
#include "graph/Graph.hpp"
#include "models/TimeSlotPool.hpp"
#include "Timetable.hpp"

struct ScheduleResult {
    Timetable timetable;
    bool success;        
    int slotsUsed;       
    double runtimeMs;    

    explicit ScheduleResult(const Timetable& tt)
        : timetable(tt), success(false), slotsUsed(0), runtimeMs(0.0) {}
};

class Scheduler {
public:
    virtual ~Scheduler() = default;

    virtual ScheduleResult schedule(const Graph& graph, const TimeSlotPool& pool) = 0;

    virtual std::string name() const = 0;
};

#endif 
