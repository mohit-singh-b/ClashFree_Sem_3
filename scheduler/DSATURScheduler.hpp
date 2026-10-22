#pragma once

#include "Scheduler.hpp"
#include "graph/Graph.hpp"
#include "models/timeslotpool.hpp"

#include <chrono>
#include <string>
#include <vector>

class DSATURScheduler : public Scheduler {
public:
    ScheduleResult schedule(
        const Graph& graph,
        const TimeSlotPool& pool
    ) override {
        const auto start = std::chrono::steady_clock::now();
        Timetable timetable(graph.size(), pool);

        // Repeatedly select the unassigned node with the highest saturation.
        while (!timetable.isComplete()) {
            const Node* selectedNode = nullptr;
            int bestSaturation = -1;
            int bestDegree = -1;
            int bestId = 0;

            for (const auto& nodePtr : graph.getNodes()) {
                const Node* node = nodePtr.get();

                if (timetable.isAssigned(node->getId())) {
                    continue;
                }

                const int saturation = saturationDegree(*node, timetable, pool);
                const int degree = node->degree();

                if (selectedNode == nullptr ||
                    saturation > bestSaturation ||
                    (saturation == bestSaturation && degree > bestDegree) ||
                    (saturation == bestSaturation && degree == bestDegree &&
                     node->getId() < bestId)) {
                    selectedNode = node;
                    bestSaturation = saturation;
                    bestDegree = degree;
                    bestId = node->getId();
                }
            }

            // No unassigned node remains.
            if (selectedNode == nullptr) {
                break;
            }

            bool assigned = false;

            // Assign the first slot satisfying all current constraints.
            for (int slotIndex = 0; slotIndex < pool.size(); ++slotIndex) {
                if (canUseSlot(*selectedNode, slotIndex, timetable, pool)) {
                    timetable.assign(selectedNode->getId(), slotIndex);
                    assigned = true;
                    break;
                }
            }

            // DSATUR has no backtracking: stop if this node cannot be colored.
            if (!assigned) {
                break;
            }
        }

        const auto end = std::chrono::steady_clock::now();
        ScheduleResult result(timetable);
        result.success = timetable.isComplete();
        result.slotsUsed = timetable.slotsUsed();
        result.runtimeMs = std::chrono::duration<double, std::milli>(
            end - start
        ).count();

        return result;
    }

    std::string name() const override {
        return "DSATUR";
    }

private:
    // Number of distinct time slots already used by this node's neighbors.
    static int saturationDegree(
        const Node& node,
        const Timetable& timetable,
        const TimeSlotPool& pool
    ) {
        std::vector<bool> usedSlots(pool.size(), false);
        int saturation = 0;

        for (const Node* neighbor : node.getNeighbors()) {
            const int slotIndex = timetable.slotOf(neighbor->getId());

            if (slotIndex >= 0 && slotIndex < pool.size() &&
                !usedSlots[slotIndex]) {
                usedSlots[slotIndex] = true;
                ++saturation;
            }
        }

        return saturation;
    }

    static bool canUseSlot(
        const Node& node,
        int slotIndex,
        const Timetable& timetable,
        const TimeSlotPool& pool
    ) {
        const auto& slot = pool.at(slotIndex);

        // Faculty must be available in this time slot.
        if (!node.getFaculty()->isAvailable(slot)) {
            return false;
        }

        // No already-assigned neighbor may use the same time slot.
        for (const Node* neighbor : node.getNeighbors()) {
            if (timetable.slotOf(neighbor->getId()) == slotIndex) {
                return false;
            }
        }

        return true;
    }
};
