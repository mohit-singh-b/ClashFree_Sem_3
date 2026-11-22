#ifndef ROOM_CAPACITY_REPORT_HPP
#define ROOM_CAPACITY_REPORT_HPP

#include <iostream>
#include <iomanip>
#include "Dataset.hpp"

namespace room_capacity_report {

inline void printRoomCapacityReport(
    const Dataset& dataset,
    std::ostream& out = std::cout
) {
    const auto& rooms = dataset.getRooms();

    int theoryRooms = 0;
    int labRooms = 0;
    int totalCapacity = 0;

    out << "\n========================================\n";
    out << "        ROOM CAPACITY REPORT\n";
    out << "========================================\n";

    if (rooms.empty()) {
        out << "No rooms are available in the dataset.\n";
        return;
    }

    for (const auto& room : rooms) {
        totalCapacity += room.getCapacity();

        if (room.isLab()) {
            ++labRooms;
        } else {
            ++theoryRooms;
        }
    }

    const double averageCapacity =
        static_cast<double>(totalCapacity) / rooms.size();

    out << "Total Rooms: " << rooms.size() << '\n';
    out << "Theory Rooms: " << theoryRooms << '\n';
    out << "Lab Rooms: " << labRooms << '\n';
    out << "Total Seating Capacity: " << totalCapacity << '\n';
    out << "Average Room Capacity: "
        << std::fixed << std::setprecision(2)
        << averageCapacity << " students\n";

    out << "\nRoom Details\n";
    out << "----------------------------------------\n";

    for (const auto& room : rooms) {
        out << "Room ID: " << room.getId() << '\n';
        out << "Room Name: " << room.getName() << '\n';
        out << "Type: " << (room.isLab() ? "LAB" : "THEORY") << '\n';
        out << "Capacity: " << room.getCapacity() << " students\n";
        out << "----------------------------------------\n";
    }
}

} // namespace room_capacity_report

#endif // ROOM_CAPACITY_REPORT_HPP
