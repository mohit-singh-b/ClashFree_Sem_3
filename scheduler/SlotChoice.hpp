#pragma once

#include <algorithm>
#include <tuple>
#include <vector>

#include "graph/Node.hpp"
#include "models/TimeSlotPool.hpp"
#include "Timetable.hpp"

// A multi-period session may not run across lunch (after period 5 of the day).
constexpr int LUNCH_AFTER_PERIOD = 5;

// The ONE legality rule every algorithm uses. result[s] is true if the node could START in slot s:
// its whole block stays inside one day and does not cross lunch, its faculty is free for every
// period of the block (available(slot) says if a slot is free), and no assigned neighbor overlaps
// the block. For 1-period sessions this is the old rule: faculty available, no neighbor in the slot.
template <typename Available>
std::vector<bool> legalStarts(const Node& node, const Timetable& timetable, const TimeSlotPool& pool,
                              Available available) {
    const int periodsPerDay = pool.getPeriodsPerDay();
    const int length = node.duration();
    std::vector<bool> legal(pool.size(), true);

    // A neighbor in [s, s + its length) blocks every start whose block would overlap it.
    for (const Node* neighbor : node.getNeighbors()) {
        const int s = timetable.slotOf(neighbor->getId());
        if (s < 0) continue;
        const int from = std::max(0, s - length + 1);
        const int to = std::min(pool.size() - 1, s + neighbor->duration() - 1);
        for (int start = from; start <= to; ++start) legal[start] = false;
    }

    for (int start = 0; start < pool.size(); ++start) {
        if (!legal[start]) continue;
        const int period = start % periodsPerDay;   // 0-based
        bool ok = period + length <= periodsPerDay &&
                  !(period < LUNCH_AFTER_PERIOD && period + length > LUNCH_AFTER_PERIOD);
        for (int k = 0; ok && k < length; ++k) ok = available(start + k);
        legal[start] = ok;
    }
    return legal;
}

// Same, using the faculty's own availability list.
inline std::vector<bool> legalStarts(const Node& node, const Timetable& timetable, const TimeSlotPool& pool) {
    return legalStarts(node, timetable, pool, [&](int slot) {
        return node.getFaculty()->isAvailable(pool.at(slot));
    });
}

// Picks the legal slot with the fewest sessions of the same section on that day, then the same
// faculty on that day, then the fewest sessions in the slot itself. This spreads the week out
// instead of filling Monday first. Returns -1 if no slot is legal.
template <typename IsLegal>
int pickSlot(const Node& node, const Timetable& timetable, const TimeSlotPool& pool, IsLegal isLegal) {
    const int periodsPerDay = pool.getPeriodsPerDay();
    std::vector<int> sectionLoad(pool.getNumDays(), 0);
    std::vector<int> facultyLoad(pool.getNumDays(), 0);

    // Same-section and same-faculty nodes are always neighbors, so the neighbors are enough.
    for (const Node* neighbor : node.getNeighbors()) {
        const int slot = timetable.slotOf(neighbor->getId());
        if (slot < 0) continue;
        if (neighbor->getSection()->getId() == node.getSection()->getId()) ++sectionLoad[slot / periodsPerDay];
        if (neighbor->getFaculty()->getId() == node.getFaculty()->getId()) ++facultyLoad[slot / periodsPerDay];
    }

    int best = -1;
    std::tuple<int, int, int> bestCost;
    for (int slot = 0; slot < pool.size(); ++slot) {
        if (!isLegal(slot)) continue;
        const int day = slot / periodsPerDay;
        const std::tuple<int, int, int> cost(sectionLoad[day], facultyLoad[day],
                                             static_cast<int>(timetable.nodesAt(slot).size()));
        if (best < 0 || cost < bestCost) {
            best = slot;
            bestCost = cost;
        }
    }
    return best;
}
