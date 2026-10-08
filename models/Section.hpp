#ifndef SECTION_H
#define SECTION_H

#include <string>
#include <vector>
#include "Course.hpp"   

class Section {
private:
    int id;
    std::string name;             
    int studentCount;
    std::vector<Course> enrolledCourses;

public:
    Section(int id, const std::string& name, int studentCount)
        : id(id), name(name), studentCount(studentCount) {}

    int getId() const { return id; }
    std::string getName() const { return name; }
    int getStudentCount() const { return studentCount; }
    const std::vector<Course>& getEnrolledCourses() const { return enrolledCourses; }

    void enrollCourse(const Course& course) {
        enrolledCourses.push_back(course);
    }

    bool isEnrolledIn(int courseId) const {
        for (const auto& c : enrolledCourses) {
            if (c.getId() == courseId) return true;
        }
        return false;
    }

    int totalWeeklyHours() const {
        int total = 0;
        for (const auto& c : enrolledCourses) {
            total += c.getLoadHours();
        }
        return total;
    }

    std::string toString() const {
        return "id : " + std::to_string(id) + " " + name +
               " students : " + std::to_string(studentCount) +
               ", courses : " + std::to_string(enrolledCourses.size()) ;
    }
};

#endif // SECTION_H