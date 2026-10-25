#pragma once

#include "Scheduler.hpp"
#include "graph/Graph.hpp"
#include "SlotChoice.hpp"
#include "models/TimeSlotPool.hpp"

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

class GreedyScheduler : public Scheduler {
public:
    ScheduleResult schedule(const Graph& graph, const TimeSlotPool& pool) override {
        const auto start = std::chrono::steady_clock::now();

        // Start with every node unassigned.
        Timetable timetable(graph.size(), pool);

        // Collect nodes, then process the most constrained (highest degree) first.
        std::vector<const Node*> orderedNodes;
        orderedNodes.reserve(graph.getNodes().size());

        for (const auto& nodePtr : graph.getNodes()) {
            orderedNodes.push_back(nodePtr.get());
        }

        // Break degree ties by node ID so results are reproducible.
        std::sort(orderedNodes.begin(), orderedNodes.end(),
                  [](const Node* left, const Node* right) {
                      if (left->degree() != right->degree()) {
                          return left->degree() > right->degree();
                      }
                      return left->getId() < right->getId();
                  });

        // Give each node the least crowded legal time slot.
        for (const Node* node : orderedNodes) {
            const int slotIndex = pickSlot(*node, timetable, pool, [&](int slot) {
                return canUseSlot(*node, slot, timetable, pool);
            });
            if (slotIndex >= 0) {
                timetable.assign(node->getId(), slotIndex);
            }
            // If no slot is legal, this node stays unassigned and success is false.
        }

        const bool success = timetable.isComplete();
        const int slotsUsed = timetable.slotsUsed();
        const auto end = std::chrono::steady_clock::now();
        const double runtimeMs =
            std::chrono::duration<double, std::milli>(end - start).count();

        ScheduleResult result(timetable);
        result.success = success;
        result.slotsUsed = slotsUsed;
        result.runtimeMs = runtimeMs;
        return result;
    }

    std::string name() const override {
        return "Greedy";
    }

private:
    static bool canUseSlot(const Node& node,
                           int slotIndex,
                           const Timetable& timetable,
                           const TimeSlotPool& pool) {
        const auto& slot = pool.at(slotIndex);

        // Constraint 1: faculty must be available in this slot.
        if (!node.getFaculty()->isAvailable(slot)) {
            return false;
        }

        // Constraint 2: no already-assigned neighbor may use this slot.
        for (const Node* neighbor : node.getNeighbors()) {
            if (timetable.slotOf(neighbor->getId()) == slotIndex) {
                return false;
            }
        }

        return true;
    }
};
