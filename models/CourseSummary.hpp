#ifndef COURSE_SUMMARY_HPP
#define COURSE_SUMMARY_HPP

#include <iostream>
#include <ostream>
#include "Dataset.hpp"

namespace course_summary {

inline const char* courseTypeName(CourseType type) {
    return type == CourseType::LAB ? "LAB" : "THEORY";
}

inline void printCourseSummary(const Dataset& dataset,
                               std::ostream& out = std::cout) {
    const auto& courses = dataset.getCourses();

    int theoryCount = 0;
    int labCount = 0;
    int totalWeeklyHours = 0;
    int totalSessions = 0;
    int invalidLoads = 0;

    out << "\n========================================\n";
    out << "             COURSE SUMMARY\n";
    out << "========================================\n";

    if (courses.empty()) {
        out << "No courses found.\n";
        return;
    }

    for (const auto& course : courses) {
        const int hours = course.getLoadHours();
        const int duration = course.getDuration();

        totalWeeklyHours += hours;
        if (course.isLab()) {
            ++labCount;
        } else {
            ++theoryCount;
        }

        out << "\nCourse: " << course.getName() << " (ID: "
            << course.getId() << ")\n";
        out << "Type: " << courseTypeName(course.getType()) << "\n";
        out << "Weekly Hours: " << hours << "\n";
        out << "Session Duration: " << duration << " period(s)\n";

        if (duration > 0 && hours > 0 && hours % duration == 0) {
            const int sessions = hours / duration;
            totalSessions += sessions;
            out << "Sessions Per Week: " << sessions << "\n";
        } else {
            ++invalidLoads;
            out << "Sessions Per Week: Cannot be calculated (check weekly hours)\n";
        }

        out << "----------------------------------------\n";
    }

    out << "\n--------------- TOTALS ----------------\n";
    out << "Total Courses: " << courses.size() << "\n";
    out << "Theory Courses: " << theoryCount << "\n";
    out << "Lab Courses: " << labCount << "\n";
    out << "Total Weekly Hours: " << totalWeeklyHours << "\n";
    out << "Total Sessions Per Week: " << totalSessions << "\n";

    if (invalidLoads > 0) {
        out << "Courses With Invalid Session Hours: " << invalidLoads << "\n";
    }

    out << "========================================\n";
}

} // namespace course_summary

#endif // COURSE_SUMMARY_HPP
