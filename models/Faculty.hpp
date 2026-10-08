#ifndef FACULTY_H
#define FACULTY_H

#include <string>
#include <vector>
#include "TimeSlot.hpp"   

class Faculty {
private:
    int id;
    std::string name;
    std::vector<TimeSlot> availableSlots;
    int maxLoad;   

public:
    Faculty(int id, const std::string& name, int maxLoad)
        : id(id), name(name), maxLoad(maxLoad) {}

    int getId() const { return id; }
    std::string getName() const { return name; }
    int getMaxLoad() const { return maxLoad; }
    const std::vector<TimeSlot>& getAvailableSlots() const { return availableSlots; }

  
    void addAvailableSlot(const TimeSlot& slot) { availableSlots.push_back(slot); }

    bool isAvailable(const TimeSlot& slot) const {
        for (const auto& s : availableSlots) {
            if (s == slot) return true;
        }
        return false;
    }
};

#endif 