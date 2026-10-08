#ifndef DATASET_H
#define DATASET_H

#include <deque>
#include <map>
#include <vector>
#include <tuple>
#include <string>
#include <stdexcept>
#include "Course.hpp"
#include "Faculty.hpp"
#include "Section.hpp"
#include "Room.hpp"
#include "TimeSlot.hpp"

class Dataset {
public:
    using NodeSpec = std::tuple<Section*, Course*, Faculty*>;

private:
    std::deque<Course>  courses;
    std::deque<Faculty> faculty;
    std::deque<Section> sections;
    std::deque<Room>    rooms;

    std::map<int, Course*>  courseById;
    std::map<int, Faculty*> facultyById;
    std::map<int, Section*> sectionById;
    std::map<int, Room*>    roomById;

    std::vector<NodeSpec> nodeSpecs;

    int days = 5;            
    int periodsPerDay = 6;

public:
    Dataset() = default;
    Dataset(const Dataset&) = delete;
    Dataset& operator=(const Dataset&) = delete;


    Course* addCourse(int id, const std::string& name, int loadHours, CourseType type) {
        if (courseById.count(id)) {
            throw std::runtime_error("Duplicate courseId " + std::to_string(id));
        }
        courses.emplace_back(id, name, loadHours, type);
        courseById[id] = &courses.back();
        return &courses.back();
    }

    Faculty* addFaculty(int id, const std::string& name, int maxLoad) {
        if (facultyById.count(id)) {
            throw std::runtime_error("Duplicate facultyId " + std::to_string(id));
        }
        faculty.emplace_back(id, name, maxLoad);
        facultyById[id] = &faculty.back();
        return &faculty.back();
    }

    Section* addSection(int id, const std::string& name, int studentCount) {
        if (sectionById.count(id)) {
            throw std::runtime_error("Duplicate sectionId " + std::to_string(id));
        }
        sections.emplace_back(id, name, studentCount);
        sectionById[id] = &sections.back();
        return &sections.back();
    }

    Room* addRoom(int id, const std::string& name, int capacity, RoomType type) {
        if (roomById.count(id)) {
            throw std::runtime_error("Duplicate roomId " + std::to_string(id));
        }
        rooms.emplace_back(id, name, capacity, type);
        roomById[id] = &rooms.back();
        return &rooms.back();
    }

    // Link a course (taught by a faculty member) to a section.
    // Enrolls the course in the section and records the node spec.
    void addSectionCourse(int sectionId, int courseId, int facultyId) {
        Section* sec = findSection(sectionId);
        if (!courseById.count(courseId)) {
            throw std::runtime_error("Section " + sec->getName() +
                                     " references unknown courseId " + std::to_string(courseId));
        }
        if (!facultyById.count(facultyId)) {
            throw std::runtime_error("Section " + sec->getName() +
                                     " references unknown facultyId " + std::to_string(facultyId));
        }
        if (sec->isEnrolledIn(courseId)) {
            throw std::runtime_error("Section " + sec->getName() +
                                     " already has courseId " + std::to_string(courseId));
        }

        Course* course = courseById[courseId];
        Faculty* fac = facultyById[facultyId];
        sec->enrollCourse(*course);
        nodeSpecs.push_back({sec, course, fac});
    }

    void setConfig(int newDays, int newPeriodsPerDay) {
        if (newDays < 1 || newDays > 6) {
            throw std::invalid_argument("days must be between 1 and 6");
        }
        if (newPeriodsPerDay < 1) {
            throw std::invalid_argument("periodsPerDay must be at least 1");
        }
        days = newDays;
        periodsPerDay = newPeriodsPerDay;
    }


    Course* getCourse(int id) const { return find(courseById, id, "courseId"); }
    Faculty* getFaculty(int id) const { return find(facultyById, id, "facultyId"); }
    Section* getSection(int id) const { return find(sectionById, id, "sectionId"); }
    Room* getRoom(int id) const { return find(roomById, id, "roomId"); }


    const std::deque<Course>& getCourses() const { return courses; }
    const std::deque<Faculty>& getFacultyList() const { return faculty; }
    const std::deque<Section>& getSections() const { return sections; }
    const std::deque<Room>& getRooms() const { return rooms; }
    const std::vector<NodeSpec>& getNodeSpecs() const { return nodeSpecs; }

    int getDays() const { return days; }
    int getPeriodsPerDay() const { return periodsPerDay; }

    std::vector<std::string> validate() const {
        std::vector<std::string> problems;

        if (sections.empty()) problems.push_back("No sections defined");
        if (courses.empty())  problems.push_back("No courses defined");
        if (faculty.empty())  problems.push_back("No faculty defined");

        for (const auto& sec : sections) {
            if (sec.getEnrolledCourses().empty()) {
                problems.push_back("Section " + sec.getName() + " has no courses");
            }
        }

        for (const auto& f : faculty) {
            for (const auto& slot : f.getAvailableSlots()) {
                int d = static_cast<int>(slot.getDay());
                int p = slot.getPeriod();
                if (d >= days || p < 1 || p > periodsPerDay) {
                    problems.push_back("Faculty " + f.getName() +
                                       " has an availability slot outside the " +
                                       std::to_string(days) + "x" +
                                       std::to_string(periodsPerDay) + " grid");
                    break;
                }
            }
        }

        std::map<int, int> hoursByFaculty;
        for (const auto& [sec, course, fac] : nodeSpecs) {
            hoursByFaculty[fac->getId()] += course->getLoadHours();
        }
        for (const auto& f : faculty) {
            auto it = hoursByFaculty.find(f.getId());
            if (it != hoursByFaculty.end() && it->second > f.getMaxLoad()) {
                problems.push_back("Faculty " + f.getName() + " is assigned " +
                                   std::to_string(it->second) + " hours, above maxLoad " +
                                   std::to_string(f.getMaxLoad()));
            }
        }

        return problems;
    }

private:
    template <typename T>
    static T* find(const std::map<int, T*>& m, int id, const char* label) {
        auto it = m.find(id);
        if (it == m.end()) {
            throw std::runtime_error(std::string("Unknown ") + label + " " + std::to_string(id));
        }
        return it->second;
    }

    Section* findSection(int id) const { return getSection(id); }
};

#endif 
