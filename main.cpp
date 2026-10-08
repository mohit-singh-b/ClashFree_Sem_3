#include <iostream>
#include "loader/DataLoader.hpp"
#include "models/Dataset.hpp"
#include "graph/Graph.hpp"
#include "models/TimeSlotPool.hpp"

void printAdjacencyList(const Graph& graph) {
    for (auto& nodePtr : graph.getNodes()) {
        std::cout << nodePtr->section->getName() << " " << nodePtr->course->getId() << " :  [ ";
        for (auto& neibourNode : nodePtr->getNeighbors()) {
            std::cout << neibourNode->section->getName() << " " << neibourNode->course->getId() << " , ";
        }
        std::cout << " ] " << std::endl;
    }
}

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

        printAdjacencyList(graph);

        std::cout << "\nGraph loaded and validated successfully.\n";

        graph.exportEdgeList("edges.txt");

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
