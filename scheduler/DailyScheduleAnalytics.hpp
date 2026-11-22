#ifndef DAILY_SCHEDULE_ANALYTICS_HPP
#define DAILY_SCHEDULE_ANALYTICS_HPP

#include <iostream>
#include <vector>

#include "graph/Graph.hpp"
#include "Timetable.hpp"

namespace daily_schedule_analytics {

inline const char* dayName(int dayIndex) {
    static const char* names[] = {"MON", "TUE", "WED", "THU", "FRI", "SAT"};
    if (dayIndex < 0 || dayIndex >= 6) return "UNKNOWN";
    return names[dayIndex];
}

inline void printDailyScheduleAnalytics(
    const Graph& graph,
    const Timetable& timetable,
    std::ostream& out = std::cout
) {
    const TimeSlotPool& pool = timetable.getPool();
    const int numberOfDays = pool.getNumDays();
    const int periodsPerDay = pool.getPeriodsPerDay();
    const int totalGridPeriods = pool.size();
    std::vector<int> sessionsPerDay(static_cast<std::size_t>(numberOfDays), 0);
    std::vector<bool> occupiedPeriods(static_cast<std::size_t>(totalGridPeriods), false);
    int assignedSessions = 0;

    for (const auto& nodePtr : graph.getNodes()) {
        if (!nodePtr) continue;
        const int nodeId = nodePtr->getId();
        if (nodeId < 0 || nodeId >= timetable.nodeCount()) continue;
        if (!timetable.isAssigned(nodeId)) continue;

        const TimeSlot& start = timetable.slotAt(nodeId);
        const int dayIndex = static_cast<int>(start.getDay());
        if (dayIndex < 0 || dayIndex >= numberOfDays) continue;
        ++sessionsPerDay[static_cast<std::size_t>(dayIndex)];
        ++assignedSessions;

        for (int offset = 0; offset < nodePtr->duration(); ++offset) {
            const int period = start.getPeriod() + offset;
            if (period < 1 || period > periodsPerDay) continue;
            const int poolIndex = pool.indexOf(TimeSlot(start.getDay(), period));
            if (poolIndex >= 0 && poolIndex < totalGridPeriods) {
                occupiedPeriods[static_cast<std::size_t>(poolIndex)] = true;
            }
        }
    }

    int occupiedCount = 0;
    for (bool occupied : occupiedPeriods) {
        if (occupied) ++occupiedCount;
    }
    const int freePeriods = totalGridPeriods - occupiedCount;

    out << "\n==========================================\n";
    out << "       DAILY SCHEDULE ANALYTICS\n";
    out << "==========================================\n";
    out << "Class sessions per day:\n";
    for (int day = 0; day < numberOfDays; ++day) {
        out << dayName(day) << ": "
            << sessionsPerDay[static_cast<std::size_t>(day)] << " sessions\n";
    }

    if (assignedSessions == 0) {
        out << "Busiest day: N/A (no scheduled sessions)\n";
        out << "Least busy day: N/A (no scheduled sessions)\n";
    } else {
        int busiestCount = sessionsPerDay[0];
        int leastBusyCount = sessionsPerDay[0];
        for (int day = 1; day < numberOfDays; ++day) {
            const int count = sessionsPerDay[static_cast<std::size_t>(day)];
            if (count > busiestCount) busiestCount = count;
            if (count < leastBusyCount) leastBusyCount = count;
        }

        out << "Busiest day:";
        for (int day = 0; day < numberOfDays; ++day) {
            if (sessionsPerDay[static_cast<std::size_t>(day)] == busiestCount) {
                out << " " << dayName(day);
            }
        }
        out << " (" << busiestCount << " sessions)\n";

        out << "Least busy day:";
        for (int day = 0; day < numberOfDays; ++day) {
            if (sessionsPerDay[static_cast<std::size_t>(day)] == leastBusyCount) {
                out << " " << dayName(day);
            }
        }
        out << " (" << leastBusyCount << " sessions)\n";
    }

    out << "Scheduled class sessions: " << assignedSessions << " / "
        << graph.size() << "\n";
    out << "Occupied timetable periods: " << occupiedCount << " / "
        << totalGridPeriods << "\n";
    out << "Free timetable periods: " << freePeriods << "\n";
    out << "==========================================\n";
}

} // namespace daily_schedule_analytics

#endif // DAILY_SCHEDULE_ANALYTICS_HPP
