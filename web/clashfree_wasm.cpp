// ClashFree - browser (WebAssembly) entry point.
//
// The browser cannot run the clashfree.exe program, so this file wraps the SAME scheduler code
// (Dataset, DataLoader, Graph, Greedy/DSATUR/Backtracking, RoomAllocator) in one function:
//
//     runSchedule(inputJson, algorithm, optimize)  ->  reportJson
//
// inputJson   : the same JSON that main.cpp reads from sem3.json / data.json
// algorithm   : "greedy", "dsatur" or "backtracking"
// optimize    : true = improve the soft quality with simulated annealing after scheduling
// reportJson  : the same report main.cpp writes to timetable.json, plus
//               "warnings" (Dataset::validate), "nodes", "edges", "slotsAvailable",
//               or {"error": "..."} if the data could not be loaded.
//
// Build for the browser (Emscripten):   see web/build_wasm.sh
// Build natively for testing:           g++ -std=c++17 -I. web/clashfree_wasm.cpp -o wasm_test
//                                       ./wasm_test sem3.json dsatur

#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include "json.hpp"
#include "loader/DataLoader.hpp"
#include "models/Dataset.hpp"
#include "graph/Graph.hpp"
#include "models/TimeSlotPool.hpp"
#include "scheduler/GreedyScheduler.hpp"
#include "scheduler/DSATURScheduler.hpp"
#include "scheduler/BacktrackingScheduler.hpp"
#include "scheduler/RoomAllocator.hpp"
#include "scheduler/AnnealingOptimizer.hpp"

namespace {

std::unique_ptr<Scheduler> makeScheduler(const std::string& name) {
    if (name == "greedy") return std::make_unique<GreedyScheduler>();
    if (name == "dsatur") return std::make_unique<DSATURScheduler>();
    if (name == "backtracking") return std::make_unique<BacktrackingScheduler>();
    throw std::runtime_error("Unknown algorithm '" + name + "' (use greedy, dsatur, or backtracking)");
}

// Number of conflict edges (each pair counted once), same rule as Graph::exportEdgeList.
std::size_t countEdges(const Graph& graph) {
    std::size_t edges = 0;
    for (const auto& nodePtr : graph.getNodes()) {
        for (const Node* neighbor : nodePtr->getNeighbors()) {
            if (nodePtr->getId() < neighbor->getId()) ++edges;
        }
    }
    return edges;
}

}  // namespace

std::string runSchedule(const std::string& inputJson, const std::string& algorithm, bool optimize) {
    try {
        // The loader reads from a file. In the browser this "file" lives in memory (Emscripten MEMFS).
        const std::string path = "clashfree_input.json";
        {
            std::ofstream out(path);
            out << inputJson;
        }

        Dataset dataset;
        DataLoader loader;
        loader.loadFromFile(path, dataset);

        nlohmann::json warnings = nlohmann::json::array();
        for (const auto& problem : dataset.validate()) warnings.push_back(problem);

        TimeSlotPool pool(dataset.getDays(), dataset.getPeriodsPerDay());
        Graph graph;
        graph.buildFromDataset(dataset);
        if (graph.size() == 0) {
            return nlohmann::json{{"error", "The data has no sessions to schedule (graph has 0 nodes)."},
                                  {"warnings", warnings}}.dump();
        }

        std::unique_ptr<Scheduler> scheduler = makeScheduler(algorithm);
        const ScheduleResult scheduled = scheduler->schedule(graph, pool);
        ScheduleResult result = scheduled;
        if (optimize) {
            result.timetable = AnnealingOptimizer().optimize(graph, scheduled.timetable, pool);
            result.slotsUsed = result.timetable.slotsUsed();
        }

        const bool roomAllocationRequired = !dataset.getRooms().empty();
        RoomAssignmentResult roomResult;
        if (roomAllocationRequired) {
            RoomAllocator roomAllocator;
            roomResult = roomAllocator.assignRooms(graph, result.timetable, pool, dataset.getRooms());
        }

        // From here on this mirrors main.cpp, so the browser report has the same shape as timetable.json.
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
                if (roomIndex >= 0 && static_cast<std::size_t>(roomIndex) < dataset.getRooms().size()) {
                    const Room& room = dataset.getRooms()[static_cast<std::size_t>(roomIndex)];
                    entry["roomId"] = room.getId();
                    entry["room"] = room.getName();
                } else {
                    entry["roomId"] = nullptr;
                    entry["room"] = nullptr;
                }
            }
            if (!result.timetable.isAssigned(node.getId())) {
                unassigned.push_back(entry);
                continue;
            }
            const TimeSlot& slot = result.timetable.slotAt(node.getId());
            entry["day"] = dayNames[static_cast<int>(slot.getDay())];
            for (int k = 0; k < node.duration(); ++k) {   // one row per period the session covers
                entry["period"] = slot.getPeriod() + k;
                assignments.push_back(entry);
            }
        }

        const bool overallSuccess = result.success && (!roomAllocationRequired || roomResult.success);
        nlohmann::json report = {{"algorithm", scheduler->name()},
                                 {"success", overallSuccess},
                                 {"scheduleSuccess", result.success},
                                 {"assigned", result.timetable.assignedCount()},
                                 {"total", graph.size()},
                                 {"slotsUsed", result.slotsUsed},
                                 {"slotsAvailable", pool.size()},
                                 {"runtimeMs", result.runtimeMs},
                                 {"nodes", graph.size()},
                                 {"edges", countEdges(graph)},
                                 {"warnings", warnings},
                                 {"assignments", assignments},
                                 {"unassigned", unassigned}};
        if (roomAllocationRequired) {
            report["roomAllocation"] = {{"success", roomResult.success},
                                        {"assigned", roomResult.assignedCount},
                                        {"total", roomResult.scheduledCount}};
        }
        return report.dump();
    } catch (const std::exception& e) {
        return nlohmann::json{{"error", e.what()}}.dump();
    }
}

#ifdef __EMSCRIPTEN__
// Make runSchedule callable from the web page (Python in the browser calls it through PyScript).
#include <emscripten/bind.h>
EMSCRIPTEN_BINDINGS(clashfree) {
    emscripten::function("runSchedule", &runSchedule);
}
#else
// Native test build: same function, run from the command line, prints the report.
#include <iostream>
int main(int argc, char* argv[]) {
    const std::string path = argc > 1 ? argv[1] : "sem3.json";
    const std::string algorithm = argc > 2 ? argv[2] : "greedy";
    std::ifstream in(path);
    std::stringstream text;
    text << in.rdbuf();
    const bool optimize = argc > 3 && std::string(argv[3]) == "optimize";
    std::cout << runSchedule(text.str(), algorithm, optimize) << "\n";
    return 0;
}
#endif
