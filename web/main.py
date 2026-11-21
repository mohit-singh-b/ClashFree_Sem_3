"""ClashFree - Python running in the browser (PyScript).

1. Starts the C++ scheduler, compiled to WebAssembly (clashfree.js + clashfree.wasm).
2. Data: the sample CSVs, or CSV files the user uploads (read in the browser, nothing is sent anywhere).
3. "Generate": converts the CSVs to the scheduler's JSON (data_convert.py), runs all three
   C++ algorithms with runSchedule(), and keeps the best result (ranking.py).
4. Shows the weekly timetable for a chosen section / faculty member / room (timetable_view.py),
   with downloads (CSV / JSON) and printing.
5. Re-checks the result independently against the scheduler's rules (checks.py).
6. If the timetable is incomplete, explains why in plain words and how to fix the data (advice.py).
"""

import asyncio
import json

import js                                  # things the web page loaded (our clashfree.js)
from pyodide.ffi import to_js              # turns Python values into browser (JavaScript) values
from pyscript import document, when        # document = the web page, when = "run this on an event"

from data_convert import FILES, REQUIRED, build_input
from ranking import CRITERIA, explain, rank
from advice import build_notice, explain_error
from checks import not_placed, verify
from timetable_view import course_colours, grid_csv, grid_html, names_for, session_rows, sessions_csv


def page(selector):
    """Find an element on the page, e.g. page("#output")."""
    return document.querySelector(selector)


def show(text, problem=False):
    """Write a short status message under the Generate button (in red if it is a problem)."""
    box = page("#output")
    box.innerText = text
    box.className = "status problem" if problem else "status"


# The latest result, kept so the timetable view can be changed without re-running.
state = {"data": None, "report": None, "rows": []}
uploaded = {}                              # CSV files the user uploaded: {file name: text}


# --- 1. Start the C++ scheduler -------------------------------------------------------------
page("#slow-hint").hidden = True           # Python is running, so the "still loading?" hint is not needed
show("Loading the scheduler…")
engine = None
if not hasattr(js, "createClashFree"):
    show("The scheduler files (clashfree.js and clashfree.wasm) were not found in the web folder. "
         "Build them with web\\build_wasm.ps1, then reload this page.", problem=True)
else:
    try:
        engine = await js.createClashFree()  # createClashFree comes from clashfree.js
        page("#run").disabled = False
        show("Ready.")
    except Exception as error:
        show(f"The scheduler could not start ({error}). Rebuild it with web\\build_wasm.ps1 "
             "and reload the page.", problem=True)

# Backtracking last: it is slow on impossible data, so it can be skipped (see impossible()).
ALGORITHMS = {"Greedy": "greedy", "DSATUR": "dsatur", "Backtracking": "backtracking"}

# Warnings from the C++ Dataset::validate() that prove a complete timetable cannot exist.
IMPOSSIBLE = ("but is available in only", "but the week has only")


def impossible(results):
    """True if the data checks already show that not every session can be placed."""
    return any(marker in warning
               for report in results.values()
               for warning in report["warnings"]
               for marker in IMPOSSIBLE)


def sample_texts():
    """Read the sample CSV files (copied in by pyscript.toml) as {file name: text}."""
    texts = {}
    for name in FILES:
        try:
            with open(f"data/{name}", encoding="utf-8") as f:
                texts[name] = f.read()
        except FileNotFoundError:
            pass                           # optional files may be missing
    return texts


# --- 2. Data: sample or uploaded CSV files ------------------------------------------------------
def columns_help():
    """The 'Expected columns' list, built from FILES so it always matches data_convert.py."""
    items = []
    for name, (required, columns) in FILES.items():
        need = "required" if required else "optional"
        items.append(f"<li><b>{name}</b> ({need}): <code>{', '.join(columns)}</code></li>")
    return "<ul>" + "".join(items) + "</ul>"


def show_file_status(ignored=()):
    """Table of all expected files: found / missing (required) / not given (optional)."""
    rows = []
    for name, (required, _) in FILES.items():
        if name in uploaded:
            status = '<span class="found">found</span>'
        elif required:
            status = '<span class="missing">MISSING</span>'
        else:
            status = '<span class="not-given">not given</span>'
        rows.append(f"<tr><td>{name}</td><td>{'required' if required else 'optional'}</td>"
                    f"<td>{status}</td></tr>")
    html = '<table class="files"><tr><th>File</th><th>Needed</th><th>Status</th></tr>' + "".join(rows) + "</table>"
    if ignored:
        html += "<p>Ignored (file name not recognised): " + ", ".join(ignored) + "</p>"
    page("#file-status").innerHTML = html


def current_texts():
    """The CSV texts to use: sample data or the uploaded files."""
    return sample_texts() if page("#source-sample").checked else dict(uploaded)


@when("change", "#source-sample")
def use_sample(event):
    page("#upload-box").hidden = True


@when("change", "#source-upload")
def use_upload(event):
    page("#upload-box").hidden = False
    show_file_status()


@when("change", "#upload")
async def files_chosen(event):
    """Read the chosen files in the browser (they never leave the computer)."""
    uploaded.clear()
    ignored = []
    files = event.target.files
    for i in range(files.length):
        file = files.item(i)
        name = file.name.strip().lower()   # "Faculty.CSV" counts as faculty.csv
        if name in FILES:
            uploaded[name] = await file.text()
        else:
            ignored.append(file.name)
    show_file_status(ignored)
    if uploaded:
        show(f"{len(uploaded)} file(s) read. Press Generate timetable.")


page("#columns-help").innerHTML = columns_help()


def summary(report):
    """Key figures of the chosen (best) result, shown under the Generate button."""
    figures = [("Sessions placed", f"{report['assigned']} / {report['total']}"),
               ("Time slots used", f"{report['slotsUsed']} / {report['slotsAvailable']}")]
    if "roomAllocation" in report:
        rooms = report["roomAllocation"]
        figures.append(("Rooms assigned", f"{rooms['assigned']} / {rooms['total']}"))
    figures.append(("Conflicts avoided", f"{report['edges']:,}"))
    html = '<dl class="stats">' + "".join(f"<div><dt>{label}</dt><dd>{value}</dd></div>"
                                          for label, value in figures) + "</dl>"
    return html


def comparison_html(ranked, skipped=None):
    """Compact table of the algorithms; the winner (first row) is marked, skipped ones listed last."""
    skipped = skipped or {}
    rows = []
    for name, m in ranked:
        css = ' class="best"' if name == ranked[0][0] else ""
        rows.append(f"<tr{css}><td>{name}</td><td class=\"num\">{m['placed']}</td>"
                    f"<td class=\"num\">{m['gaps']}</td><td class=\"num\">{m['runtime']:.0f} ms</td></tr>")
    for name in skipped:
        rows.append(f'<tr><td>{name}</td><td class="num" colspan="3">skipped</td></tr>')
    order = ", then ".join(label for _, label, _ in CRITERIA)
    notes = "".join(f'<p class="order">{name} was skipped: {why}</p>' for name, why in skipped.items())
    return ('<table class="compare"><tr><th>Algorithm</th><th class="num">Placed</th>'
            '<th class="num">Idle gaps</th><th class="num">Time</th></tr>' + "".join(rows) + "</table>"
            f'<p class="reason">{explain(ranked)}</p>' + notes +
            f'<p class="order">Ranked by: {order}.</p>')


# --- 3. Run the scheduler ---------------------------------------------------------------------
def clear_results():
    """Hide every result after a problem, so an old timetable is never mistaken for a new one."""
    page("#summary").innerHTML = ""
    for selector in ("#result-block", "#timetable", "#checks-panel", "#notice"):
        page(selector).hidden = True
    page("#empty").hidden = False


@when("click", "#run")
async def run(event):
    page("#run").disabled = True           # no second click while the scheduler is running
    try:
        await generate()
    except Exception as error:             # anything unexpected: a short message instead of a traceback
        js.console.error(str(error))
        show(f"Something went wrong while generating the timetable ({error}). Check the data files; "
             "if they look correct, reload the page and try again.", problem=True)
    finally:
        page("#run").disabled = False


async def generate():
    texts = current_texts()
    if not texts:
        show("No files chosen yet. Select all your CSV files with 'Choose files', "
             "or switch to the sample data.", problem=True)
        return
    missing = [name for name in REQUIRED if name not in texts]
    if missing:
        show("Still needed: " + ", ".join(missing) + ". Select these files together with the others.",
             problem=True)
        return
    try:
        data = build_input(texts)
    except ValueError as problem:
        show(f"Problem in the data: {problem}", problem=True)
        clear_results()
        return

    # Run every algorithm on the same data (C++ code: JSON text in, JSON text out).
    text = json.dumps(data)
    results, skipped = {}, {}
    for name, key in ALGORITHMS.items():
        if name == "Backtracking" and impossible(results):
            skipped[name] = ("the data checks show a complete timetable is impossible, so it cannot do "
                             "better, and its search would take a very long time.")
            continue
        show(f"Running {name}…")
        await asyncio.sleep(0.05)          # give the page a moment to show the message
        report = json.loads(engine.runSchedule(text, key, True))   # True = also optimise soft constraints
        if "error" in report:              # bad data fails for every algorithm, so stop here
            show("The scheduler could not use this data: " + explain_error(report["error"]), problem=True)
            clear_results()
            return
        results[name] = report

    ranked = rank(results)
    best_name = ranked[0][0]
    report = results[best_name]
    page("#compare").innerHTML = comparison_html(ranked, skipped)
    page("#result-block").hidden = False

    state["data"], state["report"] = data, report
    state["rows"] = session_rows(report, data)
    notice = show_notice(report)
    show(f"Done. Best result: {best_name}." if notice is None
         else f"Done, but the timetable is incomplete. Best result: {best_name}.")
    page("#summary").innerHTML = summary(report)
    page("#empty").hidden = True
    page("#timetable").hidden = False
    fill_names()
    show_checks()


# --- 4. Timetable view --------------------------------------------------------------------------
def fill_names():
    """Fill the second drop-down with all sections / faculty / rooms, then draw the grid."""
    view = page("#view").value
    page("#who-label").innerText = view.capitalize()
    names = names_for(state["rows"], view)
    page("#who").innerHTML = "".join(f"<option>{name}</option>" for name in names)
    if names:
        draw_grid()
        return
    # Nothing to show for this view: say why instead of drawing an empty grid.
    if not state["rows"]:
        reason = "No sessions could be placed, so there is no timetable to show."
    elif view == "room":
        reason = "No rooms were assigned (no rooms.csv was given), so there is no room view."
    else:
        reason = f"There is nothing to show by {view}."
    page("#tt-title").innerText = "Nothing to show"
    page("#tt-meta").innerText = ""
    page("#grid").innerHTML = f'<p class="grid-empty">{reason}</p>'


def draw_grid():
    """Draw the weekly grid for the chosen name, with its title."""
    config = state["data"]["config"]
    view = page("#view").value
    page("#tt-title").innerText = page("#who").value
    page("#tt-meta").innerText = (f"{view.capitalize()} timetable, generated by "
                                  f"{state['report']['algorithm']}")
    page("#grid").innerHTML = grid_html(state["rows"], page("#view").value, page("#who").value,
                                        config["days"], config["periodsPerDay"])


@when("change", "#view")
def view_changed(event):
    fill_names()


@when("change", "#who")
def who_changed(event):
    draw_grid()


# --- 5. Downloads and printing --------------------------------------------------------------------
def download(filename, text, kind="text/csv"):
    """Let the browser save `text` as a file (nothing is sent to a server)."""
    blob = js.Blob.new(to_js([text]), to_js({"type": kind}, dict_converter=js.Object.fromEntries))
    url = js.URL.createObjectURL(blob)
    link = document.createElement("a")
    link.href = url
    link.download = filename
    link.click()
    js.URL.revokeObjectURL(url)


def safe_name(text):
    """File-name friendly version of a section / faculty / room name."""
    return "".join(c if c.isalnum() or c in "-_." else "_" for c in text).strip("_")


@when("click", "#save-grid")
def save_grid(event):
    config = state["data"]["config"]
    view, who = page("#view").value, page("#who").value
    text = grid_csv(state["rows"], view, who, config["days"], config["periodsPerDay"])
    download(f"timetable_{view}_{safe_name(who)}.csv", text)


@when("click", "#save-all")
def save_all(event):
    download("timetable_all_sessions.csv", sessions_csv(state["rows"]))


@when("click", "#save-json")
def save_json(event):
    download("timetable.json", json.dumps(state["report"], indent=2), "application/json")


@when("click", "#print")
def print_page(event):
    js.window.print()                      # style.css hides everything except the timetable


# --- 6. Independent checks ------------------------------------------------------------------------
def show_checks():
    """Run checks.py on the chosen result and list each rule as passed / failed."""
    items = []
    for rule, problems in verify(state["report"], state["data"]):
        if not problems:
            items.append(f'<li><span class="mark">&#10003;</span><span>{rule}</span></li>')
        else:
            details = "".join(f"<li>{p}</li>" for p in problems[:10])
            more = f"<li>… and {len(problems) - 10} more</li>" if len(problems) > 10 else ""
            items.append(f'<li class="bad"><span class="mark">&#10007;</span><span>{rule}: '
                         f'{len(problems)} problems<ul>{details}{more}</ul></span></li>')
    page("#checks").innerHTML = '<ul class="checks">' + "".join(items) + "</ul>"

    missing = not_placed(state["report"], state["data"])
    if not missing:
        page("#not-placed").innerHTML = '<p class="all-placed">Every session has a time slot and a room.</p>'
    else:
        head = "<tr><th>Section</th><th>Course</th><th>Session</th><th>Faculty</th><th>Problem</th></tr>"
        colours = course_colours(state["data"])
        rows = "".join(f"<tr><td>{r['section']}</td>"
                       f"<td class=\"course\" style=\"color:{colours.get(r['course'], '#ffffff')}\">{r['course']}</td>"
                       f"<td>{r['session']}</td><td>{r['faculty']}</td><td>{r['problem']}</td></tr>"
                       for r in missing)
        page("#not-placed").innerHTML = (f'<p class="bad">{len(missing)} sessions could not be fully placed:</p>'
                                         f'<table class="files placed-table">{head}{rows}</table>')
    page("#checks-panel").hidden = False


# --- 7. Notice for an incomplete timetable --------------------------------------------------------
def show_notice(report):
    """Show the plain-language notice above the timetable (or hide it). Returns the notice or None."""
    failed = [rule for rule, problems in verify(report, state["data"]) if problems]
    notice = build_notice(report, not_placed(report, state["data"]), failed,
                          has_rooms=bool(state["data"].get("rooms")))
    box = page("#notice")
    if notice is None:
        box.hidden = True
        return None
    html = f"<h2>{notice['title']}</h2><p>{notice['summary']}</p>"
    if notice["causes"]:
        html += "<h3>Why:</h3><ul>" + "".join(f"<li>{c}</li>" for c in notice["causes"]) + "</ul>"
    if notice["fixes"]:
        html += "<h3>How to fix the data:</h3><ul>" + "".join(f"<li>{f}</li>" for f in notice["fixes"]) + "</ul>"
    box.innerHTML = html
    box.hidden = False
    return notice
