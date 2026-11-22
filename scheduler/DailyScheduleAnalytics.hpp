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
    const int numberOfDays = timetable.getPool().getNumDays();
    std::vector<int> sessionsPerDay(static_cast<std::size_t>(numberOfDays), 0);
    int assignedSessions = 0;

    for (const auto& nodePtr : graph.getNodes()) {
        if (!nodePtr) continue;
        const int nodeId = nodePtr->getId();
        if (nodeId < 0 || nodeId >= timetable.nodeCount()) continue;
        if (!timetable.isAssigned(nodeId)) continue;

        const int dayIndex = static_cast<int>(timetable.slotAt(nodeId).getDay());
        if (dayIndex < 0 || dayIndex >= numberOfDays) continue;
        ++sessionsPerDay[static_cast<std::size_t>(dayIndex)];
        ++assignedSessions;
    }

    out << "\n==========================================\n";
    out << "       DAILY SCHEDULE ANALYTICS\n";
    out << "==========================================\n";
    out << "Class sessions per day:\n";
    for (int day = 0; day < numberOfDays; ++day) {
        out << dayName(day) << ": "
            << sessionsPerDay[static_cast<std::size_t>(day)] << " sessions\n";
    }
    out << "Scheduled class sessions: " << assignedSessions << " / "
        << graph.size() << "\n";
    out << "==========================================\n";
}

} // namespace daily_schedule_analytics

#endif // DAILY_SCHEDULE_ANALYTICS_HPP
