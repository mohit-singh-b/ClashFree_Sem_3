"""Independent check of a timetable (does not trust the C++ result).

It re-tests the result against the same rules the original C++ code enforces:

    rule                               enforced in the C++ by
    ---------------------------------  ------------------------------------------------
    1. no section in two places        Graph: same section -> conflict edge
    2. no faculty in two places        Graph: same faculty -> conflict edge
    3. faculty only when available     GreedyScheduler/DSATUR: faculty->isAvailable(slot)
    4. no room used twice in a slot    RoomAllocator: occupied[slot][room]
    5. room big enough for the section RoomAllocator: room.fits(studentCount)
    6. lab course in lab room          RoomAllocator: room.isLab() == node->needsLab()
       (and theory in theory room)

    results = verify(report, data)     # [(rule text, [problem, ...]), ...]
"""

from collections import defaultdict


def verify(report, data):
    """Returns one (rule, problems) pair per rule; an empty problem list means the rule holds."""
    sections = {s["name"]: s for s in data["sections"]}
    courses = {str(c["id"]): c for c in data["courses"]}
    rooms = {str(r["id"]): r for r in data.get("rooms", [])}
    available = {str(f["id"]): {(s["day"], s["period"]) for s in f["availableSlots"]}
                 for f in data["faculty"]}

    by_section, by_faculty, by_room = defaultdict(list), defaultdict(list), defaultdict(list)
    unavailable, too_small, wrong_type = [], [], []

    for a in report["assignments"]:
        slot = (a["day"], a["period"])
        where = f"{a['day']} P{a['period']}"
        course = courses.get(str(a["courseId"]), {})
        label = course.get("code") or course.get("name") or str(a["courseId"])

        by_section[(a["section"],) + slot].append(label)
        by_faculty[(a["faculty"],) + slot].append(f"{label} ({a['section']})")
        if slot not in available.get(str(a["facultyId"]), set()):
            unavailable.append(f"{a['faculty']} teaches {label} at {where} but is not available")

        room = rooms.get(str(a.get("roomId")))
        if room:
            by_room[(room["name"],) + slot].append(f"{label} ({a['section']})")
            students = sections.get(a["section"], {}).get("studentCount", 0)
            if students > room["capacity"]:
                too_small.append(f"{room['name']} ({room['capacity']} seats) for {a['section']} "
                                 f"({students} students) at {where}")
            if (course.get("type") == "LAB") != (room["type"] == "LAB"):
                wrong_type.append(f"{label} ({course.get('type')}) in {room['name']} ({room['type']}) at {where}")

    def doubles(groups):
        return [f"{key[0]} at {key[1]} P{key[2]}: " + ", ".join(items)
                for key, items in groups.items() if len(items) > 1]

    results = [
        ("No section has two classes at the same time", doubles(by_section)),
        ("No faculty member teaches two classes at the same time", doubles(by_faculty)),
        ("Faculty only teach when they are available", unavailable),
    ]
    if rooms:
        results += [
            ("No room is used twice at the same time", doubles(by_room)),
            ("Every room is big enough for its section", too_small),
            ("Lab courses are in labs, theory courses in classrooms", wrong_type),
        ]
    return results


def not_placed(report, data):
    """Sessions with no time slot, and placed sessions that got no room."""
    courses = {str(c["id"]): c for c in data["courses"]}

    def describe(a, problem):
        course = courses.get(str(a["courseId"]), {})
        return {"section": a["section"],
                "course": course.get("code") or course.get("name") or str(a["courseId"]),
                "session": f"{a.get('session', 1)} of {a.get('sessions', 1)}",
                "faculty": a["faculty"],
                "problem": problem}

    rows = [describe(a, "no free time slot") for a in report["unassigned"]]
    if data.get("rooms"):
        rows += [describe(a, f"no suitable room on {a['day']} P{a['period']}")
                 for a in report["assignments"] if not a.get("room")]
    return rows
