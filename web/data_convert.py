"""Turn the ClashFree CSV files into the JSON the C++ scheduler reads.

Same rules as build_json() in app.py, but using only the standard library (csv module),
because pandas is a large download in the browser.

    texts = {"faculty.csv": "...file text...", "courses.csv": "...", ...}
    data = build_input(texts)      # dict, same shape as sem3.json

Every problem found in the files raises ValueError with a message that names the file, the line
and what to change, so the page can show it to the user as it is.
"""

import csv
import io

DAYS = ["MON", "TUE", "WED", "THU", "FRI", "SAT"]

# file name -> (required?, columns that must be present)
FILES = {
    "faculty.csv": (True, ["id", "name", "maxLoad"]),
    "courses.csv": (True, ["id", "name", "loadHours", "type"]),
    "sections.csv": (True, ["id", "name", "studentCount"]),
    "section_courses.csv": (True, ["sectionId", "courseId", "facultyId"]),
    "rooms.csv": (False, ["id", "name", "capacity", "type"]),
    "config.csv": (False, ["key", "value"]),
    "unavailable.csv": (False, ["facultyId", "day", "period"]),
}
REQUIRED = [name for name, (required, _) in FILES.items() if required]


def read_rows(name, text, numbers=()):
    """Read one CSV file into a list of (line number, row dict), checking columns and whole numbers."""
    text = text.lstrip("\ufeff")
    first_line = text.splitlines()[0] if text.strip() else ""
    if not first_line.strip():
        raise ValueError(f"{name} is empty. It needs a header line with: {', '.join(FILES[name][1])}.")
    if ";" in first_line and "," not in first_line:
        raise ValueError(f"{name} uses ';' between values instead of ','. In Excel, save it with "
                         f"'Save as' -> 'CSV UTF-8 (Comma delimited)'.")

    reader = csv.DictReader(io.StringIO(text))
    columns = [c.strip() for c in (reader.fieldnames or [])]
    missing = [c for c in FILES[name][1] if c not in columns]
    if missing:
        raise ValueError(f"{name}: missing column(s) {', '.join(missing)}. "
                         f"The first line must contain: {', '.join(FILES[name][1])}.")

    rows = []
    for line_number, raw in enumerate(reader, start=2):
        row = {(k or "").strip(): (v or "").strip() for k, v in raw.items()}
        if not any(row.values()):
            continue  # skip empty lines
        for column in numbers:
            if row.get(column, "") == "":
                raise ValueError(f"{name}, line {line_number}: '{column}' is empty.")
            try:
                row[column] = int(row[column])
            except ValueError:
                raise ValueError(f"{name}, line {line_number}: '{column}' must be a whole number "
                                 f"(found '{row[column]}').")
        rows.append((line_number, row))
    if not rows and FILES[name][0]:
        raise ValueError(f"{name} has a header but no data lines.")
    return rows


def check_unique(name, rows, column="id"):
    """Every id may appear only once in a file."""
    seen = {}
    for line, r in rows:
        if r[column] in seen:
            raise ValueError(f"{name}, line {line}: the {column} {r[column]} is already used on line "
                             f"{seen[r[column]]}. Every line needs its own {column}.")
        seen[r[column]] = line


def check_choice(name, line, column, value, allowed):
    if value not in allowed:
        raise ValueError(f"{name}, line {line}: '{column}' must be {' or '.join(allowed)} (found '{value}').")


def build_input(texts):
    """texts: {file name: file text}. Returns the scheduler input as a dict."""
    missing = [name for name in REQUIRED if name not in texts]
    if missing:
        raise ValueError("Missing required file(s): " + ", ".join(missing))

    def rows(name, numbers=()):
        return read_rows(name, texts[name], numbers) if name in texts else []

    # config.csv: days and periodsPerDay (optional)
    config = {"days": 6, "periodsPerDay": 9}
    for line, r in rows("config.csv"):
        key = r["key"]
        if key not in config:
            raise ValueError(f"config.csv, line {line}: unknown key '{key}' (use days or periodsPerDay).")
        try:
            config[key] = int(r["value"])
        except ValueError:
            raise ValueError(f"config.csv, line {line}: '{key}' must be a whole number (found '{r['value']}').")
    if not 1 <= config["days"] <= len(DAYS):
        raise ValueError(f"config.csv: days must be between 1 and {len(DAYS)} (found {config['days']}).")
    if not 1 <= config["periodsPerDay"] <= 12:
        raise ValueError(f"config.csv: periodsPerDay must be between 1 and 12 (found {config['periodsPerDay']}).")
    days = DAYS[:config["days"]]

    faculty_rows = rows("faculty.csv", ("id", "maxLoad"))
    course_rows = rows("courses.csv", ("id", "loadHours"))
    section_rows = rows("sections.csv", ("id", "studentCount"))
    room_rows = rows("rooms.csv", ("id", "capacity"))
    for name, file_rows in (("faculty.csv", faculty_rows), ("courses.csv", course_rows),
                            ("sections.csv", section_rows), ("rooms.csv", room_rows)):
        check_unique(name, file_rows)
    faculty_ids = {r["id"] for _, r in faculty_rows}
    course_ids = {r["id"] for _, r in course_rows}
    section_ids = {r["id"] for _, r in section_rows}

    # unavailable.csv: every line must name a real faculty member, day and period
    blocked = set()
    for line, r in rows("unavailable.csv", ("facultyId", "period")):
        day = r["day"].upper()
        if r["facultyId"] not in faculty_ids:
            raise ValueError(f"unavailable.csv, line {line}: facultyId {r['facultyId']} is not in faculty.csv.")
        check_choice("unavailable.csv", line, "day", day, days)
        if not 1 <= r["period"] <= config["periodsPerDay"]:
            raise ValueError(f"unavailable.csv, line {line}: period must be between 1 and "
                             f"{config['periodsPerDay']} (found {r['period']}).")
        blocked.add((r["facultyId"], day, r["period"]))

    faculty = [{"id": r["id"], "name": r["name"], "maxLoad": r["maxLoad"],
                "availableSlots": [{"day": d, "period": p}
                                   for d in days
                                   for p in range(1, config["periodsPerDay"] + 1)
                                   if (r["id"], d, p) not in blocked]}
               for _, r in faculty_rows]

    courses = []
    for line, r in course_rows:
        kind = r["type"].upper()
        check_choice("courses.csv", line, "type", kind, ["THEORY", "LAB"])
        course = {"id": r["id"], "name": r["name"], "loadHours": r["loadHours"], "type": kind}
        if r.get("code"):
            course["code"] = r["code"]
        courses.append(course)

    rooms = []
    for line, r in room_rows:
        kind = r["type"].upper()
        check_choice("rooms.csv", line, "type", kind, ["THEORY", "LAB"])
        rooms.append({"id": r["id"], "name": r["name"], "capacity": r["capacity"], "type": kind})

    # section_courses.csv: every line must point at a real section, course and faculty member
    links = {}
    for line, r in rows("section_courses.csv", ("sectionId", "courseId", "facultyId")):
        for column, known, file in (("sectionId", section_ids, "sections.csv"),
                                    ("courseId", course_ids, "courses.csv"),
                                    ("facultyId", faculty_ids, "faculty.csv")):
            if r[column] not in known:
                raise ValueError(f"section_courses.csv, line {line}: {column} {r[column]} is not in {file}.")
        links.setdefault(r["sectionId"], []).append({"courseId": r["courseId"], "facultyId": r["facultyId"]})

    sections = [{"id": r["id"], "name": r["name"], "studentCount": r["studentCount"],
                 "courses": links.get(r["id"], [])}
                for _, r in section_rows]

    data = {"config": config, "faculty": faculty, "courses": courses, "sections": sections}
    if rooms:
        data["rooms"] = rooms
    return data
