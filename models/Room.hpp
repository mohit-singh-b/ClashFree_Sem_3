#ifndef ROOM_H
#define ROOM_H

#include <string>

enum class RoomType {THEORY, LAB};

class Room {
private:
    int id;
    std::string name;
    int capacity;
    RoomType type;

public:
    Room(int id, const std::string& name, int capacity, RoomType type)
        : id(id), name(name), capacity(capacity), type(type) {}

    int getId() const { return id; }
    std::string getName() const { return name; }
    int getCapacity() const { return capacity; }
    RoomType getType() const { return type; }
    bool isLab() const { return type == RoomType::LAB; }

    // can this room hold a section of the given size?
    bool fits(int studentCount) const { return studentCount <= capacity; }

    std::string toString() const {
        return "id " + std::to_string(id) + ": " + name +
               ", capacity : " + std::to_string(capacity) +
               ", type : " + (isLab() ? "LAB" : "THEORY");
    }
};

#endif // ROOM_H
