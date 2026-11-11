#pragma once

#include "Timetable.hpp"
#include "graph/Graph.hpp"
#include "models/Room.hpp"
#include "models/TimeSlotPool.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <deque>
#include <vector>

struct RoomAssignmentResult {
    // Indexed by Node::getId(); -1 means that no room was assigned.
    std::vector<int> roomIndexByNode;
    bool success = false;
    int assignedCount = 0;
    int scheduledCount = 0;
};

class RoomAllocator {
public:
    RoomAssignmentResult assignRooms(
        const Graph& graph,
        const Timetable& timetable,
        const TimeSlotPool& pool,
        const std::deque<Room>& rooms
    ) const {
        RoomAssignmentResult result;
        const int nodeCount = static_cast<int>(graph.size());
        const int slotCount = static_cast<int>(pool.size());
        const int roomCount = static_cast<int>(rooms.size());

        result.roomIndexByNode.assign(static_cast<std::size_t>(nodeCount), -1);
        result.scheduledCount = timetable.assignedCount();

        if (roomCount == 0 || slotCount == 0) {
            return result;
        }

        // occupied[slot][room] is true when that room is already booked
        // for that time slot.
        std::vector<std::vector<bool>> occupied(
            static_cast<std::size_t>(slotCount),
            std::vector<bool>(static_cast<std::size_t>(roomCount), false)
        );

        // Schedule larger classes first so that small classes do not take
        // the only rooms capable of accommodating larger classes.
        std::vector<const Node*> orderedNodes;
        orderedNodes.reserve(graph.getNodes().size());

        for (const auto& nodePtr : graph.getNodes()) {
            orderedNodes.push_back(nodePtr.get());
        }

        std::sort(
            orderedNodes.begin(), orderedNodes.end(),
            [](const Node* a, const Node* b) {
                const int aId = a->getId();
                const int bId = b->getId();
                const int aCount = a->getSection()->getStudentCount();
                const int bCount = b->getSection()->getStudentCount();

                if (aCount != bCount) return aCount > bCount;
                if (a->degree() != b->degree()) return a->degree() > b->degree();
                return aId < bId;
            }
        );

        for (const Node* node : orderedNodes) {
            const int nodeId = node->getId();

            if (nodeId < 0 || nodeId >= nodeCount) {
                continue;
            }

            if (!timetable.isAssigned(nodeId)) {
                continue;
            }

            const int slotIndex = timetable.slotOf(nodeId);
            if (slotIndex < 0 || slotIndex >= slotCount) {
                continue; // This class has no valid time-slot assignment.
            }

            const int requiredCapacity = node->getSection()->getStudentCount();

            int bestRoomIndex = -1;
            int minimumWaste = std::numeric_limits<int>::max();

            // Best-fit: choose the smallest free room that can hold the class.
            for (int roomIndex = 0; roomIndex < roomCount; ++roomIndex) {
                if (occupied[static_cast<std::size_t>(slotIndex)]
                            [static_cast<std::size_t>(roomIndex)]) {
                    continue;
                }

                const Room& room = rooms[static_cast<std::size_t>(roomIndex)];
                if (!room.fits(requiredCapacity) ||
                    room.isLab() != node->needsLab()) {
                    continue;
                }

                const int unusedSeats = room.getCapacity() - requiredCapacity;
                if (unusedSeats < minimumWaste) {
                    minimumWaste = unusedSeats;
                    bestRoomIndex = roomIndex;
                }
            }

            if (bestRoomIndex == -1) {
                continue; // No suitable room is available for this class.
            }

            result.roomIndexByNode[static_cast<std::size_t>(nodeId)] = bestRoomIndex;
            occupied[static_cast<std::size_t>(slotIndex)]
                    [static_cast<std::size_t>(bestRoomIndex)] = true;
            ++result.assignedCount;
        }

        result.success = (result.assignedCount == result.scheduledCount);
        return result;
    }
};
