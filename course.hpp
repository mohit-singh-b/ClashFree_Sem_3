#ifndef COURSE_H
#define COURSE_H

#include <string>

enum class CourseType {THEORY,LAB};

class Course {
private:
    int id;
    std::string name;
    int load_hours;
    CourseType type;

public:
    Course(int id, const std::string& name, int weeklyHours, CourseType type)
        : id(id), name(name), load_hours(weeklyHours), type(type) {}


    int getId() const { return id; }
    std::string getName() const { return name; }
    int getLoadHours() const { return load_hours; }
    CourseType getType() const { return type; }
    bool isLab() const { return type == CourseType::LAB; }

    std::string toString() const { //for debigging
        return "id " + std::to_string(id) + ": " + name +
               ", hrs : " + std::to_string(load_hours) +
               ", type : " + (isLab() ? "LAB" : "THEORY");
    }
};

#endif