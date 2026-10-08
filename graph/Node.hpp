#ifndef NODE_H
#define NODE_H

#include <string>
#include <set>
#include "models/Course.hpp"
#include "models/Faculty.hpp"
#include "models/Section.hpp"
#include "models/TimeSlot.hpp"

class Node {
public:
    int id;                    

    const Course* course;       
    const Section* section;     
    const Faculty* faculty;     

    std::set<Node*> neighbors;  

    TimeSlot* assignedSlot;     // timeslot it got (color)

public:
    Node(int id, const Course* course, const Section* section, const Faculty* faculty)
        : id(id), course(course), section(section), faculty(faculty),
          assignedSlot(nullptr) {}


    int getId() const { return id; }
    const Course* getCourse() const { return course; }
    const Section* getSection() const { return section; }
    const Faculty* getFaculty() const { return faculty; }


    bool needsLab() const { return course->isLab(); }
    int getLoadHours() const { return course->getLoadHours(); }


    void addNeighbor(Node* n) { neighbors.insert(n); }
    const std::set<Node*>& getNeighbors() const { return neighbors; }
    int degree() const { return neighbors.size(); }


    void assign(TimeSlot* slot) { assignedSlot = slot; }
    void unassign() { assignedSlot = nullptr; }
    bool isAssigned() const { return assignedSlot != nullptr; }
    TimeSlot* getAssignedSlot() const { return assignedSlot; }

  
};

#endif