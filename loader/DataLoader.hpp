#ifndef DATA_LOADER_H
#define DATA_LOADER_H

#include <fstream>
#include <stdexcept>
#include <string>
#include "json.hpp"
#include "models/Course.hpp"
#include "models/Faculty.hpp"
#include "models/Section.hpp"
#include "models/Room.hpp"
#include "models/TimeSlot.hpp"
#include "models/Dataset.hpp"

using json = nlohmann::json;
class DataLoader {
private:
    Day parseDay(const std::string& d) {
        if (d == "MON") return Day::MON;
        if (d == "TUE") return Day::TUE;
        if (d == "WED") return Day::WED;
        if (d == "THU") return Day::THU;
        if (d == "FRI") return Day::FRI;
        if (d == "SAT") return Day::SAT;
        throw std::runtime_error("Unknown day: " + d);
    }

    CourseType parseCourseType(const std::string& t) {
        if (t == "THEORY") return CourseType::THEORY;
        if (t == "LAB") return CourseType::LAB;
        throw std::runtime_error("Unknown course type: " + t);
    }

    RoomType parseRoomType(const std::string& t) {
        if (t == "THEORY") return RoomType::THEORY;
        if (t == "LAB") return RoomType::LAB;
        throw std::runtime_error("Unknown room type: " + t);
    }

public:
    void loadFromFile(const std::string& path, Dataset& dataset) {
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Could not open file: " + path);
        }

        json data;
        file >> data;

        if (data.contains("config")) {
            const auto& cfg = data["config"];
            dataset.setConfig(cfg.value("days", dataset.getDays()),
                              cfg.value("periodsPerDay", dataset.getPeriodsPerDay()));
        }

        for (auto& f : data["faculty"]) {
            Faculty* fPtr = dataset.addFaculty(f["id"].get<int>(),
                                               f["name"].get<std::string>(),
                                               f["maxLoad"].get<int>());

            for (auto& slot : f["availableSlots"]) {
                Day day = parseDay(slot["day"].get<std::string>());
                int period = slot["period"].get<int>();
                fPtr->addAvailableSlot(TimeSlot(day, period));
            }
        }

        for (auto& c : data["courses"]) {
            dataset.addCourse(c["id"].get<int>(),
                              c["name"].get<std::string>(),
                              c["loadHours"].get<int>(),
                              parseCourseType(c["type"].get<std::string>()));
        }

        if (data.contains("rooms")) {
            for (auto& r : data["rooms"]) {
                dataset.addRoom(r["id"].get<int>(),
                                r["name"].get<std::string>(),
                                r["capacity"].get<int>(),
                                parseRoomType(r["type"].get<std::string>()));
            }
        }

        for (auto& s : data["sections"]) {
            int sectionId = s["id"].get<int>();
            dataset.addSection(sectionId,
                               s["name"].get<std::string>(),
                               s["studentCount"].get<int>());

            for (auto& c : s["courses"]) {
                dataset.addSectionCourse(sectionId,
                                         c["courseId"].get<int>(),
                                         c["facultyId"].get<int>());
            }
        }
    }
};

#endif
