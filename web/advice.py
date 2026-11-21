"""Explain an incomplete timetable in plain words, with ways to fix the data.

The C++ Dataset::validate() writes warnings such as
    "Faculty Dr. X is assigned 25 hours but is available in only 20 slots"
    "Faculty Dr. X is assigned 25 hours, above maxLoad 20"
    "Section CSE-A needs 60 hours but the week has only 54 slots"
This module turns those (plus the list of sessions that could not be placed) into a notice:

    notice = build_notice(report, missing)   # None if the timetable is complete
    notice = {"title": "...", "summary": "...", "causes": [...], "fixes": [...]}
"""

import re

PATTERNS = [
    (re.compile(r"Faculty (?P<who>.+) is assigned (?P<need>\d+) hours but is available in only (?P<have>\d+) slots"),
     "{who} has to teach {need} hours a week but is available for only {have} periods.",
     "Give some of {who}'s classes to another teacher (section_courses.csv), or make {who} "
     "available for more periods (unavailable.csv)."),
    (re.compile(r"Section (?P<who>.+) needs (?P<need>\d+) hours but the week has only (?P<have>\d+) slots"),
     "Section {who} needs {need} hours of classes, but the week has only {have} periods.",
     "Add days or periods per day (config.csv), or reduce {who}'s weekly hours (courses.csv)."),
    (re.compile(r"Faculty (?P<who>.+) is assigned (?P<need>\d+) hours, above maxLoad (?P<have>\d+)"),
     "{who} is given {need} hours, more than their limit of {have} hours (maxLoad).",
     "Raise {who}'s maxLoad (faculty.csv) or give some of their classes to another teacher."),
    (re.compile(r"Course (?P<who>.+) has loadHours below 1"),
     "The course {who} has 0 weekly hours, so it never appears in the timetable.",
     "Set loadHours for {who} to 1 or more (courses.csv), or remove the course."),
    (re.compile(r"Section (?P<who>.+) has no courses"),
     "Section {who} has no courses, so it has an empty timetable.",
     "Add lines for {who} in section_courses.csv, or remove the section from sections.csv."),
]

# Errors the C++ loader can stop with, rewritten to name the CSV file to change.
ERRORS = [
    (re.compile(r"Duplicate (?P<what>course|faculty|section|room)Id (?P<id>\S+)"),
     "{file}: the id {id} is used more than once. Every line needs its own id."),
    (re.compile(r"Section (?P<sec>.+) references unknown (?P<what>course|faculty)Id (?P<id>\S+)"),
     "section_courses.csv: section {sec} uses {what}Id {id}, which is not in {file}."),
    (re.compile(r"Unknown (?P<what>course|room) type: (?P<id>.*)"),
     "{file}: type must be THEORY or LAB (found '{id}')."),
    (re.compile(r"no sessions to schedule"),
     "There is nothing to schedule: no section has any course with at least 1 weekly hour."),
]
FILE_FOR = {"course": "courses.csv", "faculty": "faculty.csv", "section": "sections.csv", "room": "rooms.csv"}


def explain_error(message):
    """Plain-language version of a C++ loader error (or the message itself if it is not recognised)."""
    for pattern, text in ERRORS:
        match = pattern.search(message)
        if match:
            values = match.groupdict()
            return text.format(file=FILE_FOR.get(values.get("what", ""), "the data"), **values)
    return message


def causes_and_fixes(warnings):
    """Plain-language causes and fixes for the warnings we recognise; other warnings are kept as they are."""
    causes, fixes = [], []
    for warning in warnings:
        for pattern, cause, fix in PATTERNS:
            match = pattern.search(warning)
            if match:
                causes.append(cause.format(**match.groupdict()))
                fix_text = fix.format(**match.groupdict())
                if fix_text not in fixes:
                    fixes.append(fix_text)
                break
        else:
            causes.append(warning)
    return causes, fixes


def build_notice(report, missing, failed_checks=(), has_rooms=True):
    """missing: rows from checks.not_placed(); failed_checks: rules that did not hold.
    Returns a notice dict, or None if there is nothing to tell the user."""
    no_slot = [m for m in missing if m["problem"] == "no free time slot"]
    no_room = [m for m in missing if m["problem"] != "no free time slot"]
    causes, fixes = causes_and_fixes(report["warnings"])
    placed, total = report["assigned"], report["total"]

    if failed_checks:
        title = "The checks found problems in this timetable"
        summary = ("The independent checks below the timetable found clashes or broken rules. "
                   "Do not use this timetable; the details are listed under Checks.")
        causes = [f"Failed: {rule}" for rule in failed_checks] + causes
    elif placed == 0:
        title = "No sessions could be placed"
        summary = f"None of the {total} sessions fit into the week, so there is no timetable to show."
    elif no_slot:
        title = "A complete timetable is not possible with this data"
        summary = (f"The best result places {placed} of {total} sessions without any clashes. "
                   f"{len(no_slot)} sessions have no free time slot; they are listed under Checks.")
    elif no_room:
        title = "Every class has a time, but some have no room"
        summary = (f"{len(no_room)} of {total} sessions could not get a suitable room "
                   f"(big enough, and a lab for lab courses). They are listed under Checks.")
        fixes.append("Add rooms, or larger labs, in rooms.csv; or reduce the number of lab hours.")
    elif report["warnings"]:
        title = "The timetable is complete, but the data has warnings"
        summary = "All sessions were placed. Check the points below before using the timetable."
    elif not has_rooms:
        title = "Rooms were not assigned"
        summary = "All sessions were placed, but no rooms.csv was given, so the timetable shows no rooms."
        fixes.append("Add rooms.csv (id, name, capacity, type) to assign rooms as well.")
    else:
        return None

    if (no_slot or placed == 0) and not causes:
        causes.append("The data is very tight: some teachers or sections have almost no free periods left.")
        fixes.append("Free up periods (unavailable.csv), add periods per day (config.csv), "
                     "or spread classes over more teachers.")
    return {"title": title, "summary": summary, "causes": causes, "fixes": fixes}
