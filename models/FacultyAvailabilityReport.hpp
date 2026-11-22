#ifndef FACULTY_AVAILABILITY_REPORT_HPP
#define FACULTY_AVAILABILITY_REPORT_HPP

#include <iostream>
#include <map>
#include <tuple>
#include "Dataset.hpp"

namespace faculty_report {

inline const char* dayName(Day day) {
    switch (day) {
        case Day::MON: return "MON";
        case Day::TUE: return "TUE";
        case Day::WED: return "WED";
        case Day::THU: return "THU";
        case Day::FRI: return "FRI";
        case Day::SAT: return "SAT";
        default: return "UNKNOWN";
    }
}

inline void printFacultyAvailabilityReport(
    const Dataset& dataset,
    std::ostream& out = std::cout
) {
    std::map<int, int> plannedLoad;

    // Calculate planned teaching workload for each faculty member.
    for (const auto& spec : dataset.getNodeSpecs()) {
        const Course* course = std::get<1>(spec);
        const Faculty* faculty = std::get<2>(spec);

        if (course != nullptr && faculty != nullptr) {
            plannedLoad[faculty->getId()] += course->getLoadHours();
        }
    }

    out << "\n========================================\n";
    out << "     FACULTY AVAILABILITY REPORT\n";
    out << "========================================\n";

    if (dataset.getFacultyList().empty()) {
        out << "No faculty members found.\n";
        return;
    }

    for (const auto& faculty : dataset.getFacultyList()) {
        const int load = plannedLoad[faculty.getId()];
        const int maxLoad = faculty.getMaxLoad();
        const auto& slots = faculty.getAvailableSlots();

        out << "\nFaculty: " << faculty.getName()
            << " (ID: " << faculty.getId() << ")\n";
        out << "Maximum Workload: " << maxLoad << " hours\n";
        out << "Planned Workload: " << load << " hours\n";

        if (load > maxLoad) {
            out << "Workload Status: EXCEEDED by "
                << load - maxLoad << " hours\n";
        } else {
            out << "Remaining Workload: "
                << maxLoad - load << " hours\n";
        }

        out << "Available Slots: " << slots.size() << "\n";

        if (slots.empty()) {
            out << "No availability slots configured.\n";
        } else {
            out << "Slot Details: ";
            for (const auto& slot : slots) {
                out << dayName(slot.getDay())
                    << "-P" << slot.getPeriod() << "  ";
            }
            out << "\n";
        }

        if (load > static_cast<int>(slots.size())) {
            out << "Availability Status: INSUFFICIENT SLOTS\n";
        } else {
            out << "Availability Status: SLOT COUNT OK\n";
        }

        out << "----------------------------------------\n";
    }
}

} // namespace faculty_report

#endif // FACULTY_AVAILABILITY_REPORT_HPP
