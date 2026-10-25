#pragma once

#include <tuple>
#include <vector>

#include "graph/Node.hpp"
#include "models/TimeSlotPool.hpp"
#include "Timetable.hpp"

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
