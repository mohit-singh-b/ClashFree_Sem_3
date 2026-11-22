#ifndef UNASSIGNED_SESSION_REPORT_HPP
#define UNASSIGNED_SESSION_REPORT_HPP

#include <iostream>
#include <vector>
#include "Timetable.hpp"
#include "graph/Graph.hpp"

namespace unassigned_report {

inline void printUnassignedSessionReport(
    const Graph& graph,
    const Timetable& timetable,
    std::ostream& out = std::cout
) {
    // Store graph nodes by ID because Timetable uses node IDs as indices.
    std::vector<const Node*> nodesById(
        static_cast<std::size_t>(timetable.nodeCount()), nullptr
    );

    for (const auto& nodePtr : graph.getNodes()) {
        const int id = nodePtr->getId();
        if (id >= 0 && id < timetable.nodeCount()) {
            nodesById[static_cast<std::size_t>(id)] = nodePtr.get();
        }
    }

    const std::vector<int> unassigned = timetable.unassignedNodes();

    out << "\n========================================\n";
    out << "       UNASSIGNED SESSION REPORT\n";
    out << "========================================\n";
    out << "Scheduled sessions: " << timetable.assignedCount()
        << " / " << timetable.nodeCount() << "\n";
    out << "Unassigned sessions: " << unassigned.size() << "\n";

    if (unassigned.empty()) {
        out << "All sessions have been successfully scheduled.\n";
        out << "========================================\n";
        return;
    }

    out << "\nSessions without a time-slot assignment:\n";

    for (int nodeId : unassigned) {
        if (nodeId < 0 || nodeId >= static_cast<int>(nodesById.size()) ||
            nodesById[static_cast<std::size_t>(nodeId)] == nullptr) {
            out << "Node ID: " << nodeId << " (matching graph node not found)\n";
            continue;
        }

        const Node& node = *nodesById[static_cast<std::size_t>(nodeId)];

        out << "\nNode ID: " << node.getId() << "\n";
        out << "Section: " << node.getSection()->getName() << "\n";
        out << "Course: " << node.getCourse()->getName()
            << " (ID: " << node.getCourse()->getId() << ")\n";
        out << "Faculty: " << node.getFaculty()->getName() << "\n";
        out << "Session: " << node.getSessionIndex() + 1
            << " of " << node.getSessionCount() << "\n";
        out << "Duration: " << node.duration() << " period(s)\n";
        out << "Status: NOT SCHEDULED\n";
        out << "----------------------------------------\n";
    }

    out << "========================================\n";
}

} // namespace unassigned_report

#endif // UNASSIGNED_SESSION_REPORT_HPP
