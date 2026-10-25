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

    int sessionIndex;           // which weekly session of the course this is (0-based)

    std::set<Node*> neighbors;

    TimeSlot* assignedSlot;     // timeslot it got (color)

public:
    Node(int id, const Course* course, const Section* section, const Faculty* faculty,
         int sessionIndex = 0)
        : id(id), course(course), section(section), faculty(faculty),
          sessionIndex(sessionIndex), assignedSlot(nullptr) {}


    int getId() const { return id; }
    const Course* getCourse() const { return course; }
    const Section* getSection() const { return section; }
    const Faculty* getFaculty() const { return faculty; }


    bool needsLab() const { return course->isLab(); }
    int getLoadHours() const { return course->getLoadHours(); }

    // A node is ONE weekly session (one hour). A course with loadHours = 4 gives 4 nodes.
    int getSessionIndex() const { return sessionIndex; }
    int getSessionCount() const { return course->getLoadHours(); }


    void addNeighbor(Node* n) { neighbors.insert(n); }
    const std::set<Node*>& getNeighbors() const { return neighbors; }
    int degree() const { return neighbors.size(); }


    void assign(TimeSlot* slot) { assignedSlot = slot; }
    void unassign() { assignedSlot = nullptr; }
    bool isAssigned() const { return assignedSlot != nullptr; }
    TimeSlot* getAssignedSlot() const { return assignedSlot; }

  
};

#endif