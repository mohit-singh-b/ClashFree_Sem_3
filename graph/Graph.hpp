#ifndef GRAPH_H
#define GRAPH_H

#include <vector>
#include <memory>
#include <fstream>
#include <string>
#include "Node.hpp"
#include "models/Course.hpp"
#include "models/Section.hpp"
#include "models/Faculty.hpp"
#include "models/Dataset.hpp"

class Graph {
private:
    std::vector<std::unique_ptr<Node>> nodes;
    int nextId = 0;

public:
    Node* addNode(const Course* course, const Section* section, const Faculty* faculty) {
        auto node = std::make_unique<Node>(nextId++, course, section, faculty);
        Node* ptr = node.get();
        nodes.push_back(std::move(node));
        return ptr;
    }

    void buildFromDataset(const Dataset& dataset) {
        for (const auto& [sec, course, fac] : dataset.getNodeSpecs()) {
            addNode(course, sec, fac);
        }
        buildEdges();
    }

    const std::vector<std::unique_ptr<Node>>& getNodes() const { return nodes; }

    int size() const { return nodes.size(); }

    void buildEdges() {
        for (size_t i = 0; i < nodes.size(); i++) {
            for (size_t j = i + 1; j < nodes.size(); j++) {
                Node* a = nodes[i].get();
                Node* b = nodes[j].get();
                if (a == b) continue;

                if (conflicts(a, b)) {
                    a->addNeighbor(b);
                    b->addNeighbor(a);
                }
            }
        }
    }

    void exportEdgeList(const std::string& filename) const {
        std::ofstream out(filename);
        for (auto& nodePtr : nodes) {
            Node* n = nodePtr.get();
            for (Node* neighbor : n->getNeighbors()) {
                if (n->getId() < neighbor->getId()) {
                    out << n->getId() << " " << neighbor->getId() << "\n";
                }
            }
        }
        out.close();
    }

private:
    bool conflicts(Node* a, Node* b) const {
        bool sameSection = (a->getSection()->getId() == b->getSection()->getId());
        bool sameFaculty = (a->getFaculty()->getId() == b->getFaculty()->getId());
        return sameSection || sameFaculty;
    }
};

#endif
