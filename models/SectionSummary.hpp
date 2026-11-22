#pragma once

#include "models/Dataset.hpp"
#include <iostream>
#include <ostream>

// Prints a quick overview of every section in the loaded dataset.
// The output stream can be changed when needed, for example to write to a file.
inline void printSectionSummary(const Dataset& dataset,
                                std::ostream& out = std::cout) {
    const auto& sections = dataset.getSections();

    out << "\n========== SECTION SUMMARY ==========\n";

    if (sections.empty()) {
        out << "No sections found in the dataset.\n";
        return;
    }

    for (const auto& section : sections) {
        out << "\nSection ID: " << section.getId() << '\n'
            << "Section Name: " << section.getName() << '\n'
            << "Students: " << section.getStudentCount() << '\n'
            << "Total Courses: " << section.getEnrolledCourses().size() << '\n'
            << "Weekly Hours: " << section.totalWeeklyHours() << '\n'
            << "-------------------------------------\n";
    }

    out << "Total Sections: " << sections.size() << '\n';
}
