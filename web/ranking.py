"""Pick the best timetable out of the three algorithms.

Every result is compared on these criteria, in this order (the first difference decides):

    1. most sessions placed            (a complete, clash-free timetable matters most)
    2. most sessions with a room
    3. fewest idle gaps for students   (free periods between a section's first and last class of a day)
    4. fewest repeats of the same course on one day
    5. fastest runtime                 (only as a final tie-break)

    results = {"Greedy": report, "DSATUR": report, "Backtracking": report}
    ranked  = rank(results)            # list of (name, metrics), best first
    reason  = explain(ranked)          # one sentence saying why the winner won
"""

from collections import defaultdict

# (metric key, how a win on it is described, True if a higher value is better)
CRITERIA = [
    ("placed", "more sessions placed", True),
    ("rooms", "more sessions with a room", True),
    ("gaps", "fewer student idle gaps", False),
    ("repeats", "fewer same-day course repeats", False),
    ("runtime", "faster", False),
]


def metrics(report):
    """Quality numbers for one scheduler report."""
    periods_by_day = defaultdict(list)       # (section, day) -> periods with a class
    per_course_day = defaultdict(int)        # (section, day, course) -> sessions that day
    for a in report["assignments"]:
        periods_by_day[(a["section"], a["day"])].append(a["period"])
        per_course_day[(a["section"], a["day"], a["courseId"])] += 1

    # Idle gaps: periods between the first and last class of a day that have no class.
    gaps = sum(max(p) - min(p) + 1 - len(p) for p in periods_by_day.values())
    repeats = sum(n - 1 for n in per_course_day.values() if n > 1)
    rooms = report.get("roomAllocation", {}).get("assigned", report["assigned"])

    return {"placed": report["assigned"], "total": report["total"], "rooms": rooms,
            "gaps": gaps, "repeats": repeats, "runtime": report["runtimeMs"]}


def sort_key(m):
    """Smaller key = better result (higher-is-better values are negated)."""
    return tuple(-m[key] if higher_better else m[key] for key, _, higher_better in CRITERIA)


def rank(results):
    """results: {algorithm name: report}. Returns [(name, metrics), ...], best first."""
    scored = [(name, metrics(report)) for name, report in results.items()]
    return sorted(scored, key=lambda item: sort_key(item[1]))


def explain(ranked):
    """One sentence: why the winner beat the runner-up."""
    if len(ranked) < 2:
        return f"{ranked[0][0]} was the only result."
    (best, b), (second, s) = ranked[0], ranked[1]
    for key, label, _ in CRITERIA:
        if b[key] != s[key]:
            if key == "runtime":
                return (f"{best} and {second} produced timetables of equal quality; "
                        f"{best} was faster ({b[key]:.0f} ms vs {s[key]:.0f} ms).")
            return f"{best} wins: {label} ({b[key]} vs {s[key]} for {second})."
    return f"{best} and {second} are identical on every criterion."
