import io
import json
import re
import subprocess
import tempfile
import zipfile
from pathlib import Path

import pandas as pd
import streamlit as st

APP_DIR = Path(__file__).resolve().parent
DAYS = ["MON", "TUE", "WED", "THU", "FRI", "SAT"]
TIMES = ["8:00-8:55", "9:00-9:55", "9:55-10:50", "11:10-12:05", "12:05-1:00",
         "1:55-2:50", "3:10-4:05", "4:05-5:00", "5:00-5:55"]
LUNCH_AFTER = 5

FILES = {
    "faculty.csv": (True, ["id", "name", "maxLoad"]),
    "courses.csv": (True, ["id", "name", "loadHours", "type"]),
    "sections.csv": (True, ["id", "name", "studentCount"]),
    "section_courses.csv": (True, ["sectionId", "courseId", "facultyId"]),
    "rooms.csv": (False, ["id", "name", "capacity", "type"]),
    "config.csv": (False, ["key", "value"]),
    "unavailable.csv": (False, ["facultyId", "day", "period"]),
}
REQUIRED = [n for n, (req, _) in FILES.items() if req]


def read_csv_bytes(name, raw):
    df = pd.read_csv(io.BytesIO(raw), dtype=str, encoding="utf-8-sig", keep_default_na=False)
    df.columns = [c.strip() for c in df.columns]
    missing = [c for c in FILES[name][1] if c not in df.columns]
    if missing:
        raise ValueError(f"{name}: missing column(s) {', '.join(missing)}")
    return df


def table(frames, name, ints=()):
    if name not in frames:
        return []
    rows = [{k: v.strip() for k, v in r.items()} for r in frames[name].to_dict("records")]
    for r in rows:
        for c in ints:
            try:
                r[c] = int(r[c])
            except ValueError:
                raise ValueError(f"{name}: '{c}' must be a whole number (found '{r[c]}')")
    return rows


def build_json(frames):
    cfg = {"days": 6, "periodsPerDay": 9}
    for r in table(frames, "config.csv"):
        if r["key"] in cfg:
            cfg[r["key"]] = int(r["value"])
    if not 1 <= cfg["days"] <= len(DAYS):
        raise ValueError(f"config.csv: days must be between 1 and {len(DAYS)}")
    days = DAYS[:cfg["days"]]

    blocked = {(r["facultyId"], r["day"].upper(), r["period"])
               for r in table(frames, "unavailable.csv", ("facultyId", "period"))}
    faculty = [{"id": r["id"], "name": r["name"], "maxLoad": r["maxLoad"],
                "availableSlots": [{"day": d, "period": p} for d in days
                                   for p in range(1, cfg["periodsPerDay"] + 1)
                                   if (r["id"], d, p) not in blocked]}
               for r in table(frames, "faculty.csv", ("id", "maxLoad"))]
    courses = [{"id": r["id"], "name": r["name"], "loadHours": r["loadHours"], "type": r["type"].upper(),
                **({"code": r["code"]} if r.get("code") else {})}
               for r in table(frames, "courses.csv", ("id", "loadHours"))]
    rooms = [{"id": r["id"], "name": r["name"], "capacity": r["capacity"], "type": r["type"].upper()}
             for r in table(frames, "rooms.csv", ("id", "capacity"))]

    links = {}
    for r in table(frames, "section_courses.csv", ("sectionId", "courseId", "facultyId")):
        links.setdefault(r["sectionId"], []).append({"courseId": r["courseId"], "facultyId": r["facultyId"]})
    sections = [{"id": r["id"], "name": r["name"], "studentCount": r["studentCount"],
                 "courses": links.get(r["id"], [])}
                for r in table(frames, "sections.csv", ("id", "studentCount"))]

    data = {"config": cfg, "faculty": faculty, "courses": courses, "sections": sections}
    if rooms:
        data["rooms"] = rooms
    return data


def find_executable():
    candidates = [base / n for base in (APP_DIR, APP_DIR.parent, Path.cwd())
                  for n in ("clashfree.exe", "clashfree")]
    return next((c for c in candidates if c.is_file()), None)


def run_cpp(exe, data):
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        (tmp / "data.json").write_text(json.dumps(data), encoding="utf-8")
        proc = subprocess.run([str(exe), "data.json"], cwd=tmp, capture_output=True, text=True, timeout=60)
        edges_file, report_file = tmp / "edges.txt", tmp / "timetable.json"
        edges = len(edges_file.read_text().splitlines()) if edges_file.exists() else 0
        schedule = json.loads(report_file.read_text()) if report_file.exists() else None
    slots = re.search(r"Time slots available:\s*(\d+)", proc.stdout)
    messages = [l.strip() for l in proc.stderr.splitlines() if l.strip()]
    if proc.returncode == 0 and schedule is None:
        messages.append("This clashfree build did not write timetable.json. Recompile it from the latest main.cpp.")
    return {
        "ok": proc.returncode == 0 and schedule is not None,
        "edges": edges,
        "slots": int(slots.group(1)) if slots else None,
        "schedule": schedule,
        "messages": messages,
    }


def grid(days, periods, cells):
    index, rows = [], []
    for p in range(1, periods + 1):
        index.append(f"P{p}  {TIMES[p - 1]}" if p <= len(TIMES) else f"P{p}")
        rows.append([cells.get((d, p), "") for d in range(days)])
        if periods == len(TIMES) and p == LUNCH_AFTER:
            index.append("")
            rows.append(["LUNCH"] * days)
    return pd.DataFrame(rows, index=index, columns=DAYS[:days])


def session_rows(schedule, course_label):
    rows = [{"section": a["section"], "day": a["day"], "period": a["period"],
             "time": TIMES[a["period"] - 1] if a["period"] <= len(TIMES) else "",
             "course": course_label.get(a["courseId"], a["courseId"]), "faculty": a["faculty"]}
            for a in schedule["assignments"]]
    return sorted(rows, key=lambda r: (r["section"], DAYS.index(r["day"]), r["period"]))


def timetable_csv(rows):
    columns = ["section", "day", "period", "time", "course", "faculty"]
    return pd.DataFrame(rows, columns=columns).to_csv(index=False).encode("utf-8")


def grid_for(rows, view, who, days, periods):
    key, other = ("section", "faculty") if view == "Section" else ("faculty", "section")
    cells = {}
    for r in rows:
        if r[key] == who:
            slot, text = (DAYS.index(r["day"]), r["period"]), f"{r['course']} · {r[other]}"
            cells[slot] = f"{cells[slot]} / {text}" if slot in cells else text
    return grid(days, periods, cells)


def grid_csv(df):
    return df.to_csv(index_label="period").encode("utf-8")


def zip_grids(rows, view, names, days, periods):
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w", zipfile.ZIP_DEFLATED) as archive:
        for name in names:
            filename = re.sub(r"[^\w.-]+", "_", name).strip("_") + ".csv"
            archive.writestr(filename, grid_csv(grid_for(rows, view, name, days, periods)))
    return buffer.getvalue()


def read_inputs():
    with st.sidebar:
        st.markdown("## ClashFree")
        st.caption("Clash-free timetable scheduler")
        st.divider()
        sample = st.toggle("Use sample data")
        uploads = st.file_uploader("Upload CSV files", type="csv", accept_multiple_files=True,
                                   disabled=sample)
        st.caption("Required: " + ", ".join(REQUIRED))
        with st.expander("Expected columns"):
            for name, (req, cols) in FILES.items():
                st.markdown(f"**{name}**{'' if req else ' (optional)'}  \n`{', '.join(cols)}`")

    if sample:
        folder = next((f for f in (APP_DIR / "csv", APP_DIR.parent / "csv") if f.is_dir()), None)
        if folder is None:
            st.sidebar.error("No sample 'csv' folder found next to the app.")
            return {}
        return {f.name: f.read_bytes() for f in folder.glob("*.csv") if f.name in FILES}

    ignored = [u.name for u in uploads if u.name not in FILES]
    if ignored:
        st.sidebar.warning("Ignored (unknown name): " + ", ".join(ignored))
    return {u.name: u.getvalue() for u in uploads if u.name in FILES}


def show_files(raw):
    status = [{"file": n, "needed": "required" if req else "optional",
               "status": "found" if n in raw else ("MISSING" if req else "-")}
              for n, (req, _) in FILES.items()]
    st.dataframe(pd.DataFrame(status), hide_index=True)

    frames = {}
    try:
        for name, content in raw.items():
            frames[name] = read_csv_bytes(name, content)
    except ValueError as e:
        st.error(str(e))
        return frames, False
    if frames:
        with st.expander("Preview files"):
            for tab, (name, df) in zip(st.tabs(list(frames)), frames.items()):
                tab.dataframe(df, hide_index=True)

    missing = [n for n in REQUIRED if n not in raw]
    if missing and raw:
        st.info("Still needed: " + ", ".join(missing))
    return frames, not missing


def load(frames):
    st.session_state.pop("loaded", None)
    try:
        data = build_json(frames)
    except ValueError as e:
        st.error(f"Could not read the data: {e}")
        return False
    exe, result = find_executable(), None
    if exe is None:
        st.warning("The C++ program was not found. Compile it first: "
                   "g++ -std=c++17 -I. main.cpp -o clashfree")
    else:
        try:
            result = run_cpp(exe, data)
        except (OSError, subprocess.SubprocessError) as e:
            st.error(f"Could not run the C++ program: {e}")
            return False
    st.session_state["loaded"] = {"data": data, "result": result}
    return result is None or result["ok"]


def summary_step():
    loaded = st.session_state.get("loaded")
    if not loaded:
        return
    data, result = loaded["data"], loaded["result"]
    with st.container(border=True):
        st.subheader("Summary")
        stats = [("Sections", len(data["sections"])), ("Faculty", len(data["faculty"])),
                 ("Courses", len(data["courses"])), ("Rooms", len(data.get("rooms", []))),
                 ("Slots / week", data["config"]["days"] * data["config"]["periodsPerDay"])]
        for col, (label, value) in zip(st.columns(len(stats)), stats):
            col.metric(label, value)
        if result:
            if result["ok"]:
                sch = result["schedule"]
                st.success(f"C++ loaded it: {sch['total']} sessions, {result['edges']} conflict edges, "
                           f"{result['slots']} time slots.")
                message = (f"{sch['algorithm']} placed {sch['assigned']} of {sch['total']} sessions "
                           f"using {sch['slotsUsed']} slots in {sch['runtimeMs']:.2f} ms.")
                (st.success if sch["success"] else st.warning)(message)
            else:
                st.error("The C++ loader rejected the data:")
            for m in result["messages"]:
                st.write("- " + m)


def timetable_step():
    loaded = st.session_state.get("loaded")
    if not (st.session_state.get("generated") and loaded):
        return
    data, result = loaded["data"], loaded["result"]
    schedule = result["schedule"] if result else None
    with st.container(border=True):
        st.subheader("Timetable")
        if not schedule:
            st.warning("No timetable: the C++ program did not run.")
            return
        days, periods = data["config"]["days"], data["config"]["periodsPerDay"]
        names = [s["name"] for s in data["sections"]]
        course_label = {c["id"]: c.get("code", c["name"]) for c in data["courses"]}
        rows = session_rows(schedule, course_label)

        view = st.radio("View by", ["Section", "Faculty"], horizontal=True)
        options = names if view == "Section" else sorted({r["faculty"] for r in rows})
        chosen = st.selectbox(view, options)
        table_view = grid_for(rows, view, chosen, days, periods)
        st.dataframe(table_view)

        if schedule["unassigned"]:
            with st.expander(f"{len(schedule['unassigned'])} sessions could not be placed"):
                st.dataframe(pd.DataFrame([{"section": u["section"],
                                            "course": course_label.get(u["courseId"], u["courseId"]),
                                            "faculty": u["faculty"]} for u in schedule["unassigned"]]),
                             hide_index=True)
        st.caption(f"{schedule['algorithm']}: one slot per course. Weekly hours and rooms are not scheduled yet.")

        file_stem = re.sub(r"[^\w.-]+", "_", chosen).strip("_")
        left, right = st.columns(2)
        left.download_button(f"Download {chosen} (CSV)", grid_csv(table_view),
                             file_name=f"{file_stem}.csv", mime="text/csv")
        right.download_button(f"Download every {view.lower()} (ZIP)",
                              zip_grids(rows, view, options, days, periods),
                              file_name=f"timetables_by_{view.lower()}.zip", mime="application/zip")
        left, right = st.columns(2)
        left.download_button("Download all sessions (CSV)", timetable_csv(rows),
                             file_name="timetable.csv", mime="text/csv")
        right.download_button("Download result (JSON)", json.dumps(schedule, indent=2),
                              file_name="timetable.json", mime="application/json")

CSS = """
<style>
#MainMenu, footer {visibility: hidden;}
.block-container, [data-testid="stMainBlockContainer"] {max-width: 1100px; padding-top: 4.5rem;}
[data-testid="stSidebar"] {background: rgba(150,130,220,.12);}
.hero {background: linear-gradient(120deg, #7C6BC4, #B8A5E8); color: #fff;
       padding: 1.6rem 2rem; border-radius: 1rem; margin-bottom: 1rem;}
.hero h1 {color: #fff; font-size: 2.2rem; font-weight: 800; letter-spacing: -1px; margin: 0; padding: 0;}
.hero p {margin: .3rem 0 0; opacity: .92;}
[data-testid="stMetric"] {background: rgba(150,130,220,.14); border-left: 4px solid #7C6BC4;
                          padding: .7rem 1rem; border-radius: .6rem;}
[data-testid="stVerticalBlockBorderWrapper"] {border-radius: .8rem;}
button[kind="primary"], button[data-testid="stBaseButton-primary"] {background: #7C6BC4; border-color: #7C6BC4; color: #fff;}
button[kind="primary"]:hover, button[data-testid="stBaseButton-primary"]:hover {background: #6A59B3; border-color: #6A59B3;}
</style>
"""

HERO = """
<div class="hero"><h1>ClashFree</h1>
<p>Clash-free class timetables using graph coloring. Upload your data, check it, then submit.</p></div>
"""


def main():
    st.set_page_config(page_title="ClashFree", layout="wide")
    st.markdown(CSS + HERO, unsafe_allow_html=True)

    raw = read_inputs()
    signature = tuple(sorted((k, len(v)) for k, v in raw.items()))
    if st.session_state.get("signature") != signature:
        st.session_state.clear()
        st.session_state["signature"] = signature

    with st.container(border=True):
        st.subheader("Files")
        frames, can_load = show_files(raw)
        check_col, submit_col, _ = st.columns([1, 1, 6])
        check = check_col.button("Check", disabled=not can_load)
        submit = submit_col.button("Submit", type="primary", disabled=not can_load)

    if check or submit:
        with st.spinner("Running the scheduler..."):
            ok = load(frames)
        st.session_state["generated"] = bool(submit and ok)

    summary_step()
    timetable_step()


if __name__ == "__main__":
    main()
