#include <fstream>
#include <iostream>
#include <memory>
#include "json.hpp"
#include "loader/DataLoader.hpp"
#include "models/Dataset.hpp"
#include "graph/Graph.hpp"
#include "models/TimeSlotPool.hpp"
#include "scheduler/GreedyScheduler.hpp"
#include "scheduler/DSATURScheduler.hpp"
#include "scheduler/BacktrackingScheduler.hpp"
#include "scheduler/RoomAllocator.hpp"

std::unique_ptr<Scheduler> makeScheduler(const std::string& name) {
    if (name == "greedy") return std::make_unique<GreedyScheduler>();
    if (name == "dsatur") return std::make_unique<DSATURScheduler>();
    if (name == "backtracking") return std::make_unique<BacktrackingScheduler>();
    throw std::runtime_error("Unknown algorithm '" + name + "' (use greedy, dsatur, or backtracking)");
}

int main(int argc, char* argv[]) {
    std::string filepath = "data.json";
    if (argc > 1) {
        filepath = argv[1];
    }

    std::string algorithm = "greedy";
    if (argc > 2) {
        algorithm = argv[2];
    }

    std::cout << "Loading dataset from: " << filepath << "\n";

    try {
        Dataset dataset;
        DataLoader loader;
        loader.loadFromFile(filepath, dataset);

        for (const auto& problem : dataset.validate()) {
            std::cerr << "Warning: " << problem << "\n";
        }

        TimeSlotPool pool(dataset.getDays(), dataset.getPeriodsPerDay());
        std::cout << "Time slots available: " << pool.size() << "\n";

        Graph graph;
        graph.buildFromDataset(dataset);

        if (graph.size() == 0) {
            std::cerr << "Warning: graph has 0 nodes\n";
            return 1;
        }

        std::unique_ptr<Scheduler> scheduler = makeScheduler(algorithm);
        const ScheduleResult result = scheduler->schedule(graph, pool);
        const bool roomAllocationRequired = !dataset.getRooms().empty();
        RoomAssignmentResult roomResult;
        if (roomAllocationRequired) {
            RoomAllocator roomAllocator;
            roomResult = roomAllocator.assignRooms(
                graph, result.timetable, pool, dataset.getRooms()
            );
        }

        std::cout << "\nAlgorithm: " << scheduler->name() << "\n"
                  << "Scheduling " << (result.success ? "succeeded" : "could not place every session") << ".\n"
                  << "Sessions assigned: " << result.timetable.assignedCount()
                  << " / " << graph.size() << "\n"
                  << "Time slots used: " << result.slotsUsed << "\n"
                  << "Runtime: " << result.runtimeMs << " ms\n";
        if (roomAllocationRequired) {
            std::cout << "Room allocation "
                      << (roomResult.success ? "succeeded" : "could not place every scheduled session")
                      << ": " << roomResult.assignedCount << " / " << roomResult.scheduledCount
                      << " sessions\n";
        }
        std::cout << "\n";

        const char* dayNames[] = {"MON", "TUE", "WED", "THU", "FRI", "SAT"};
        nlohmann::json assignments = nlohmann::json::array();
        nlohmann::json unassigned = nlohmann::json::array();
        for (const auto& nodePtr : graph.getNodes()) {
            const Node& node = *nodePtr;
            nlohmann::json entry = {{"node", node.getId()},
                                    {"section", node.getSection()->getName()},
                                    {"courseId", node.getCourse()->getId()},
                                    {"facultyId", node.getFaculty()->getId()},
                                    {"faculty", node.getFaculty()->getName()},
                                    {"session", node.getSessionIndex() + 1},
                                    {"sessions", node.getSessionCount()}};
            if (roomAllocationRequired) {
                const int roomIndex = roomResult.roomIndexByNode[node.getId()];
                if (roomIndex >= 0 &&
                    static_cast<std::size_t>(roomIndex) < dataset.getRooms().size()) {
                    const Room& room = dataset.getRooms()[static_cast<std::size_t>(roomIndex)];
                    entry["roomId"] = room.getId();
                    entry["room"] = room.getName();
                } else {
                    entry["roomId"] = nullptr;
                    entry["room"] = nullptr;
                }
            }
            const std::string sessionLabel = "  (" + std::to_string(node.getSessionIndex() + 1) +
                                             "/" + std::to_string(node.getSessionCount()) + ")";
            if (!result.timetable.isAssigned(node.getId())) {
                unassigned.push_back(entry);
                std::cout << "UNASSIGNED  " << node.getSection()->getName()
                          << "  course " << node.getCourse()->getId() << sessionLabel
                          << "  faculty " << node.getFaculty()->getName() << "\n";
                continue;
            }

            const TimeSlot& slot = result.timetable.slotAt(node.getId());
            const int dayIndex = static_cast<int>(slot.getDay());
            entry["day"] = dayNames[dayIndex];
            entry["period"] = slot.getPeriod();
            assignments.push_back(entry);
            std::cout << dayNames[dayIndex] << " period " << slot.getPeriod()
                      << "  " << node.getSection()->getName()
                      << "  course " << node.getCourse()->getId() << sessionLabel
                      << "  faculty " << node.getFaculty()->getName();
            if (roomAllocationRequired && !entry["room"].is_null()) {
                std::cout << "  room " << entry["room"].get<std::string>();
            }
            std::cout << "\n";
        }

        const bool overallSuccess = result.success &&
            (!roomAllocationRequired || roomResult.success);
        nlohmann::json report = {{"algorithm", scheduler->name()},
                                 {"success", overallSuccess},
                                 {"scheduleSuccess", result.success},
                                 {"assigned", result.timetable.assignedCount()},
                                 {"total", graph.size()},
                                 {"slotsUsed", result.slotsUsed},
                                 {"runtimeMs", result.runtimeMs},
                                 {"assignments", assignments},
                                 {"unassigned", unassigned}};
        if (roomAllocationRequired) {
            report["roomAllocation"] = {
                {"success", roomResult.success},
                {"assigned", roomResult.assignedCount},
                {"total", roomResult.scheduledCount}
            };
        }
        std::ofstream("timetable.json") << report.dump(2);

        graph.exportEdgeList("edges.txt");

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
