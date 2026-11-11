#pragma once

#include "Scheduler.hpp"
#include "graph/Graph.hpp"
#include "graph/Node.hpp"
#include "models/timeslotpool.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <string>
#include <tuple>
#include <vector>

// Finds a complete timetable by trying legal time slots recursively.
// If a later class cannot be scheduled, it removes the previous assignment
// and tries another slot (backtracking).
class BacktrackingScheduler : public Scheduler {
public:
    ScheduleResult schedule(const Graph& graph, const TimeSlotPool& pool) override {
        const auto start = std::chrono::steady_clock::now();

        Timetable timetable(graph.size(), pool);
        Timetable bestTimetable(graph.size(), pool);

        // Graph node IDs are used as indices by Timetable.
        std::vector<const Node*> nodesById(static_cast<std::size_t>(graph.size()), nullptr);
        for (const auto& nodePtr : graph.getNodes()) {
            const int id = nodePtr->getId();
            if (id >= 0 && id < graph.size()) {
                nodesById[static_cast<std::size_t>(id)] = nodePtr.get();
            }
        }

        // Cache faculty availability once. Faculty::isAvailable() scans its
        // availability list, so caching avoids repeating that work at every
        // recursive search state.
        std::vector<std::vector<bool>> facultyAvailability(
            nodesById.size(),
            std::vector<bool>(static_cast<std::size_t>(pool.size()), false)
        );
        for (const Node* node : nodesById) {
            if (node == nullptr) continue;
            const int id = node->getId();
            for (int slot = 0; slot < pool.size(); ++slot) {
                facultyAvailability[static_cast<std::size_t>(id)]
                                   [static_cast<std::size_t>(slot)] =
                    node->getFaculty()->isAvailable(pool.at(slot));
            }
        }

        SearchContext context;
        const bool success = backtrack(nodesById, facultyAvailability, pool,
                                       timetable, bestTimetable, context);

        // Keep the most complete valid partial timetable if no full solution was found.
        if (!success) {
            timetable = bestTimetable;
        }

        const auto end = std::chrono::steady_clock::now();
        ScheduleResult result(timetable);
        result.success = success && timetable.isComplete();
        result.slotsUsed = timetable.slotsUsed();
        result.runtimeMs = std::chrono::duration<double, std::milli>(end - start).count();

        lastExploredStates_ = context.exploredStates;
        lastSearchLimitReached_ = context.limitReached;
        return result;
    }

    std::string name() const override {
        return "Backtracking";
    }

    // Useful for reporting/debugging when the search reaches its safety limit.
    std::size_t exploredStates() const {
        return lastExploredStates_;
    }

    bool searchLimitReached() const {
        return lastSearchLimitReached_;
    }

private:
    // A safety limit prevents a very difficult dataset from searching forever.
    // Increase this value if you want the algorithm to explore more states.
    static constexpr std::size_t MAX_SEARCH_STATES = 1000000;

    struct SearchContext {
        std::size_t exploredStates = 0;
        bool limitReached = false;
    };

    std::size_t lastExploredStates_ = 0;
    bool lastSearchLimitReached_ = false;

    static std::vector<int> getLegalSlots(
        const Node& node,
        const Timetable& timetable,
        const std::vector<bool>& facultyAvailability,
        const TimeSlotPool& pool
    ) {
        std::vector<bool> blockedSlots(static_cast<std::size_t>(pool.size()), false);

        // Mark slots already used by assigned neighbors as blocked.
        for (const Node* neighbor : node.getNeighbors()) {
            const int assignedSlot = timetable.slotOf(neighbor->getId());
            if (assignedSlot >= 0 && assignedSlot < pool.size()) {
                blockedSlots[static_cast<std::size_t>(assignedSlot)] = true;
            }
        }

        std::vector<int> legalSlots;
        for (int slot = 0; slot < pool.size(); ++slot) {
            const std::size_t index = static_cast<std::size_t>(slot);
            if (facultyAvailability[index] && !blockedSlots[index]) {
                legalSlots.push_back(slot);
            }
        }
        return legalSlots;
    }

    // DSATUR-style tie breaker: count distinct slots already used by neighbors.
    static int saturationDegree(const Node& node,
                                const Timetable& timetable,
                                const TimeSlotPool& pool) {
        std::vector<bool> usedSlots(static_cast<std::size_t>(pool.size()), false);
        int saturation = 0;

        for (const Node* neighbor : node.getNeighbors()) {
            const int slot = timetable.slotOf(neighbor->getId());
            if (slot >= 0 && slot < pool.size() &&
                !usedSlots[static_cast<std::size_t>(slot)]) {
                usedSlots[static_cast<std::size_t>(slot)] = true;
                ++saturation;
            }
        }
        return saturation;
    }

    // Lower cost means a better slot. This follows the spreading preference
    // already used by SlotChoice.hpp: balance a section/faculty across days,
    // then prefer a less occupied slot.
    static std::tuple<int, int, int, int> slotCost(const Node& node,
                                                  int slotIndex,
                                                  const Timetable& timetable,
                                                  const TimeSlotPool& pool) {
        const int periodsPerDay = pool.getPeriodsPerDay();
        const int dayIndex = slotIndex / periodsPerDay;

        int sectionLoad = 0;
        int facultyLoad = 0;

        for (const Node* neighbor : node.getNeighbors()) {
            const int neighborSlot = timetable.slotOf(neighbor->getId());
            if (neighborSlot < 0) {
                continue;
            }

            const int neighborDay = neighborSlot / periodsPerDay;
            if (neighborDay != dayIndex) {
                continue;
            }

            if (neighbor->getSection()->getId() == node.getSection()->getId()) {
                ++sectionLoad;
            }
            if (neighbor->getFaculty()->getId() == node.getFaculty()->getId()) {
                ++facultyLoad;
            }
        }

        const int slotLoad = static_cast<int>(timetable.nodesAt(slotIndex).size());
        return std::make_tuple(sectionLoad, facultyLoad, slotLoad, slotIndex);
    }

    static bool backtrack(
        const std::vector<const Node*>& nodesById,
        const std::vector<std::vector<bool>>& facultyAvailability,
        const TimeSlotPool& pool,
        Timetable& timetable,
        Timetable& bestTimetable,
        SearchContext& context
    ) {
        if (timetable.isComplete()) {
            return true;
        }

        if (context.exploredStates >= MAX_SEARCH_STATES) {
            context.limitReached = true;
            return false;
        }
        ++context.exploredStates;

        // Minimum Remaining Values (MRV): choose the unassigned class with
        // the fewest legal slots. Break ties by saturation, degree, then ID.
        const Node* selectedNode = nullptr;
        std::vector<int> selectedSlots;
        int bestSaturation = -1;
        int bestDegree = -1;

        for (const Node* node : nodesById) {
            if (node == nullptr || timetable.isAssigned(node->getId())) {
                continue;
            }

            const auto& available = facultyAvailability[static_cast<std::size_t>(node->getId())];
            std::vector<int> legalSlots = getLegalSlots(*node, timetable, available, pool);

            // Forward checking: if any remaining class has no legal slot,
            // this branch cannot lead to a complete timetable.
            if (legalSlots.empty()) {
                return false;
            }

            const int saturation = saturationDegree(*node, timetable, pool);
            const int degree = node->degree();
            const bool betterChoice =
                selectedNode == nullptr ||
                legalSlots.size() < selectedSlots.size() ||
                (legalSlots.size() == selectedSlots.size() && saturation > bestSaturation) ||
                (legalSlots.size() == selectedSlots.size() && saturation == bestSaturation &&
                 degree > bestDegree) ||
                (legalSlots.size() == selectedSlots.size() && saturation == bestSaturation &&
                 degree == bestDegree && node->getId() < selectedNode->getId());

            if (betterChoice) {
                selectedNode = node;
                selectedSlots = std::move(legalSlots);
                bestSaturation = saturation;
                bestDegree = degree;
            }
        }

        // No unassigned node remains.
        if (selectedNode == nullptr) {
            return timetable.isComplete();
        }

        // Try the most balanced legal slots first; all legal slots are still
        // explored if an earlier choice leads to a dead end.
        std::sort(selectedSlots.begin(), selectedSlots.end(),
                  [&](int left, int right) {
                      return slotCost(*selectedNode, left, timetable, pool) <
                             slotCost(*selectedNode, right, timetable, pool);
                  });

        for (int slot : selectedSlots) {
            const int nodeId = selectedNode->getId();
            timetable.assign(nodeId, slot);

            if (timetable.assignedCount() > bestTimetable.assignedCount()) {
                bestTimetable = timetable;
            }

            if (backtrack(nodesById, facultyAvailability, pool,
                          timetable, bestTimetable, context)) {
                return true;
            }

            // Undo this choice and try the next available time slot.
            timetable.unassign(nodeId);

            if (context.limitReached) {
                return false;
            }
        }

        return false;
    }
};
