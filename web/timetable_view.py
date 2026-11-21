"""Turn the scheduler's report into weekly timetable grids.

Pure Python (no browser code), so it can be tested on its own:

    rows  = session_rows(report, data)              # one dict per placed session
    names = names_for(rows, "section")              # e.g. ["CSE-A", "CSE-B", ...]
    html  = grid_html(rows, "section", "CSE-A", days=6, periods=9)
    text  = grid_csv(rows, "section", "CSE-A", days=6, periods=9)   # same grid, for Excel
    text  = sessions_csv(rows)                                      # every session, one per line
"""

import csv
import io
from html import escape

DAYS = ["MON", "TUE", "WED", "THU", "FRI", "SAT"]           # as the scheduler writes them
DAY_LABELS = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat"]     # as the page shows them
TIMES = ["8:00-8:55", "9:00-9:55", "9:55-10:50", "11:10-12:05", "12:05-1:00",
         "1:55-2:50", "3:10-4:05", "4:05-5:00", "5:00-5:55"]
LUNCH_AFTER = 5          # lunch break is shown after period 5 (same as app.py)

# Course code colours: bright enough to read on the dark background, clearly different from each other.
# Each course keeps its colour everywhere (timetable grid and the Checks table).
COURSE_COLOURS = ["#7aa2ff", "#ffb86b", "#7ee2a8", "#ff7eb6", "#c3a6ff",
                  "#f4e26b", "#62d9e8", "#ff9776", "#b5e86a", "#e4a6ff"]


def course_colours(data):
    """{course label: colour}, in the order the courses appear in courses.csv."""
    colours = {}
    for i, c in enumerate(data["courses"]):
        label = c.get("code") or c.get("name") or str(c["id"])
        colours[label] = COURSE_COLOURS[i % len(COURSE_COLOURS)]
    return colours

# What each cell shows, depending on whose timetable we are looking at.
# view -> (field that selects the timetable, fields shown inside each cell)
VIEWS = {
    "section": ("section", ("faculty", "room")),
    "faculty": ("faculty", ("section", "room")),
    "room": ("room", ("section", "faculty")),
}



def session_rows(report, data):
    """One dict per placed session: section, day, period, course label and colour, faculty, room."""
    courses = {str(c["id"]): c for c in data["courses"]}
    colours = course_colours(data)

    rows = []
    for a in report["assignments"]:
        course = courses.get(str(a["courseId"]), {})
        rows.append({
            "section": a["section"],
            "day": a["day"],
            "period": a["period"],
            "course": course.get("code") or course.get("name") or str(a["courseId"]),
            "courseName": course.get("name", ""),
            "lab": course.get("type") == "LAB",
            "colour": colours.get(course.get("code") or course.get("name") or str(a["courseId"]),
                                  COURSE_COLOURS[0]),
            "faculty": a["faculty"],
            "room": a.get("room") or "",
        })
    rows.sort(key=lambda r: (r["section"], DAYS.index(r["day"]), r["period"]))
    return rows


def names_for(rows, view):
    """All sections / faculty / rooms that appear in the timetable, sorted."""
    field = VIEWS[view][0]
    return sorted({r[field] for r in rows if r[field]})


def grid_html(rows, view, who, days, periods):
    """HTML <table> for one section / faculty member / room: days across, periods down."""
    field, shown = VIEWS[view]

    # (day index, period) -> list of sessions in that cell
    cells = {}
    for r in rows:
        if r[field] == who:
            cells.setdefault((DAYS.index(r["day"]), r["period"]), []).append(r)

    out = ['<table class="grid"><thead><tr><th>Period</th>']
    out += [f"<th>{d}</th>" for d in DAY_LABELS[:days]]
    out.append("</tr></thead><tbody>")

    for p in range(1, periods + 1):
        time = TIMES[p - 1] if p <= len(TIMES) else ""
        out.append(f'<tr><th class="period">P{p}<br><small>{time}</small></th>')
        for d in range(days):
            sessions = cells.get((d, p), [])
            if not sessions:
                out.append("<td></td>")
                continue
            # More than one session in a cell would be a clash; show it clearly.
            css = "clash" if len(sessions) > 1 else ""
            parts = []
            for s in sessions:
                # The room (venue) gets its own class so it can be shown in bold white.
                lines = "".join(f'<span class="line{" room" if f == "room" else ""}">{escape(s[f])}</span>'
                                for f in shown if s[f])
                css_class = "session lab" if s["lab"] else "session"
                parts.append(f'<div class="{css_class}" title="{escape(s["courseName"])}">'
                             f'<span class="code" style="color:{s["colour"]}">{escape(s["course"])}</span>{lines}</div>')
            out.append(f'<td class="{css}">{"".join(parts)}</td>')
        out.append("</tr>")
        if periods == len(TIMES) and p == LUNCH_AFTER:
            out.append(f'<tr class="lunch"><td colspan="{days + 1}">Lunch break</td></tr>')

    out.append("</tbody></table>")
    return "".join(out)


def _csv_text(table):
    out = io.StringIO()
    csv.writer(out).writerows(table)
    return out.getvalue()


def grid_csv(rows, view, who, days, periods):
    """The same weekly grid as grid_html, as CSV text (opens in Excel)."""
    field, shown = VIEWS[view]
    cells = {}
    for r in rows:
        if r[field] == who:
            text = r["course"] + " (" + ", ".join(r[f] for f in shown if r[f]) + ")"
            key = (DAYS.index(r["day"]), r["period"])
            cells[key] = cells[key] + " / " + text if key in cells else text

    table = [["Period", "Time"] + DAYS[:days]]
    for p in range(1, periods + 1):
        time = TIMES[p - 1] if p <= len(TIMES) else ""
        table.append([f"P{p}", time] + [cells.get((d, p), "") for d in range(days)])
        if periods == len(TIMES) and p == LUNCH_AFTER:
            table.append(["", "LUNCH"] + [""] * days)
    return _csv_text(table)


def sessions_csv(rows):
    """Every placed session on its own line (section, day, period, time, course, faculty, room)."""
    table = [["section", "day", "period", "time", "course", "course name", "faculty", "room"]]
    for r in rows:
        time = TIMES[r["period"] - 1] if r["period"] <= len(TIMES) else ""
        table.append([r["section"], r["day"], r["period"], time, r["course"], r["courseName"],
                      r["faculty"], r["room"]])
    return _csv_text(table)
