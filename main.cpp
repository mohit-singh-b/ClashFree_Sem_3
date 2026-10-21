#include <iostream>
#include "loader/DataLoader.hpp"
#include "models/Dataset.hpp"
#include "graph/Graph.hpp"
#include "models/TimeSlotPool.hpp"
#include "scheduler/DSATURScheduler.hpp"

int main(int argc, char* argv[]) {
    std::string filepath = "data.json";
    if (argc > 1) {
        filepath = argv[1];
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

        DSATURScheduler scheduler;
        const ScheduleResult result = scheduler.schedule(graph, pool);

        std::cout << "\nAlgorithm: " << scheduler.name() << "\n"
                  << "Scheduling " << (result.success ? "succeeded" : "could not place every session") << ".\n"
                  << "Sessions assigned: " << result.timetable.assignedCount()
                  << " / " << graph.size() << "\n"
                  << "Time slots used: " << result.slotsUsed << "\n"
                  << "Runtime: " << result.runtimeMs << " ms\n\n";

        const char* dayNames[] = {"MON", "TUE", "WED", "THU", "FRI", "SAT"};
        for (const auto& nodePtr : graph.getNodes()) {
            const Node& node = *nodePtr;
            if (!result.timetable.isAssigned(node.getId())) {
                std::cout << "UNASSIGNED  " << node.getSection()->getName()
                          << "  course " << node.getCourse()->getId()
                          << "  faculty " << node.getFaculty()->getName() << "\n";
                continue;
            }

            const TimeSlot& slot = result.timetable.slotAt(node.getId());
            const int dayIndex = static_cast<int>(slot.getDay());
            std::cout << dayNames[dayIndex] << " period " << slot.getPeriod()
                      << "  " << node.getSection()->getName()
                      << "  course " << node.getCourse()->getId()
                      << "  faculty " << node.getFaculty()->getName() << "\n";
        }

        graph.exportEdgeList("edges.txt");

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
