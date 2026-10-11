#ifndef TIMETABLE_H
#define TIMETABLE_H

#include <vector>
#include <stdexcept>
#include "models/TimeSlot.hpp"
#include "models/TimeSlotPool.hpp"


class Timetable {
private:
    const TimeSlotPool* pool;
    std::vector<int> slotOfNode;  

    void checkNode(int nodeId) const {
        if (nodeId < 0 || nodeId >= static_cast<int>(slotOfNode.size())) {
            throw std::out_of_range("Timetable: node id out of range");
        }
    }

public:
    Timetable(int nodeCount, const TimeSlotPool& pool)
        : pool(&pool), slotOfNode(nodeCount, -1) {}

    int nodeCount() const { return static_cast<int>(slotOfNode.size()); }
    const TimeSlotPool& getPool() const { return *pool; }

    void assign(int nodeId, int slotIndex) {
        checkNode(nodeId);
        if (slotIndex < 0 || slotIndex >= pool->size()) {
            throw std::out_of_range("Timetable: slot index not in pool");
        }
        slotOfNode[nodeId] = slotIndex;
    }

    void unassign(int nodeId) {
        checkNode(nodeId);
        slotOfNode[nodeId] = -1;
    }

    bool isAssigned(int nodeId) const {
        checkNode(nodeId);
        return slotOfNode[nodeId] != -1;
    }

    int slotOf(int nodeId) const {
        checkNode(nodeId);
        return slotOfNode[nodeId];
    }

    const TimeSlot& slotAt(int nodeId) const {
        int idx = slotOf(nodeId);
        if (idx == -1) {
            throw std::logic_error("Timetable: node is not assigned");
        }
        return pool->at(idx);
    }

    int assignedCount() const {
        int count = 0;
        for (int s : slotOfNode) {
            if (s != -1) count++;
        }
        return count;
    }

    bool isComplete() const { return assignedCount() == nodeCount(); }

    int slotsUsed() const {
        std::vector<bool> used(pool->size(), false);
        int count = 0;
        for (int s : slotOfNode) {
            if (s != -1 && !used[s]) {
                used[s] = true;
                count++;
            }
        }
        return count;
    }

    std::vector<int> nodesAt(int slotIndex) const {
        std::vector<int> result;
        for (int id = 0; id < nodeCount(); id++) {
            if (slotOfNode[id] == slotIndex) result.push_back(id);
        }
        return result;
    }

    std::vector<int> unassignedNodes() const {
        std::vector<int> result;
        for (int id = 0; id < nodeCount(); id++) {
            if (slotOfNode[id] == -1) result.push_back(id);
        }
        return result;
    }
};

#endif 
