#pragma once

// Simulated annealing that improves the SOFT quality of a timetable that is already valid.
//
// Input : a Timetable from Greedy / DSATUR / Backtracking (hard constraints already satisfied).
// Output: a Timetable with the same sessions, moved to better slots.
//
// Hard constraints can never break: a session is only ever moved to a slot where its faculty is
// available and no neighbour in the conflict graph (same section or same faculty) already sits.
// This is the same legality rule the schedulers use, so the result stays clash-free.
//
// Soft rules (per section, per day), each with a weight in SoftSettings:
//   1. repeats : more than 2 sessions of the same course on one day
//   2. runs    : more than 3 classes in a row
//   3. gaps    : free periods between the first and last class of the day
//   4. chunks  : the day should stay in one part of the day (morning / noon / evening)
//   5. splits  : morning + evening with nothing at noon (extra penalty on top of 3 and 4)

#include <algorithm>
#include <cmath>
#include <map>
#include <random>
#include <vector>

#include "graph/Graph.hpp"
#include "models/TimeSlotPool.hpp"
#include "SlotChoice.hpp"
#include "Timetable.hpp"

struct SoftSettings {
    // How bad each kind of problem is (higher = avoided more strongly).
    int repeatWeight = 4;
    int runWeight    = 3;
    int gapWeight    = 2;
    int chunkWeight  = 2;   // per extra chunk used in a day: 2 chunks cost 1x, 3 chunks cost 2x
    int splitWeight  = 4;

    int maxSameCourse = 2;  // per section per day
    int maxRun        = 3;  // consecutive classes

    // Chunks, as 1-based period numbers. With the 9-period day used in app.py:
    //   morning = P1-P4 (starts before 12:00), noon = P5-P7 (starts before 4:00), evening = P8-P9.
    int morningLastPeriod = 4;
    int noonLastPeriod    = 7;

    // Search. Fixed seed + fixed iteration count = the same answer every run (also in WebAssembly).
    int      iterations = 200000;
    double   startTemp  = 3.0;
    double   endTemp    = 0.05;
    unsigned seed       = 12345;
};

// Raw counts of each problem, so the result can be reported per rule (before / after tables).
struct SoftPenalties {
    int repeats = 0, runs = 0, gaps = 0, chunks = 0, splits = 0;

    SoftPenalties& operator+=(const SoftPenalties& o) {
        repeats += o.repeats; runs += o.runs; gaps += o.gaps; chunks += o.chunks; splits += o.splits;
        return *this;
    }

    int total(const SoftSettings& s) const {
        return repeats * s.repeatWeight + runs * s.runWeight + gaps * s.gapWeight +
               chunks * s.chunkWeight + splits * s.splitWeight;
    }
};

class AnnealingOptimizer {
public:
    explicit AnnealingOptimizer(SoftSettings settings = SoftSettings()) : cfg(settings) {}

    // Problems found in a timetable (use it before and after optimize()).
    SoftPenalties measure(const Graph& graph, const Timetable& timetable, const TimeSlotPool& pool) const {
        const Model m = buildModel(graph, pool);
        SoftPenalties sum;
        for (size_t sec = 0; sec < m.bySection.size(); ++sec) {
            for (int day = 0; day < m.days; ++day) sum += cell(m, sec, day, timetable);
        }
        return sum;
    }

    Timetable optimize(const Graph& graph, const Timetable& start, const TimeSlotPool& pool) const {
        const Model m = buildModel(graph, pool);

        std::vector<int> movable;                       // only sessions that were placed
        for (const Node* n : m.nodes) {
            if (start.isAssigned(n->getId())) movable.push_back(n->getId());
        }
        if (movable.empty() || pool.size() < 2) return start;

        Timetable current = start;
        Timetable best = start;
        int currentCost = 0;
        for (size_t sec = 0; sec < m.bySection.size(); ++sec) {
            for (int day = 0; day < m.days; ++day) currentCost += cell(m, sec, day, current).total(cfg);
        }
        int bestCost = currentCost;

        std::mt19937 rng(cfg.seed);                     // mt19937 output is identical on every platform
        const double cooling = std::pow(cfg.endTemp / cfg.startTemp, 1.0 / cfg.iterations);
        double temp = cfg.startTemp;

        for (int i = 0; i < cfg.iterations; ++i, temp *= cooling) {
            const int id = movable[rng() % movable.size()];
            const int newSlot = static_cast<int>(rng() % pool.size());
            const int oldSlot = current.slotOf(id);
            if (newSlot == oldSlot || !legalStarts(*m.nodes[id], current, pool)[newSlot]) continue;

            // Only this section's old day and new day change, so only those are re-scored.
            const size_t sec = m.sectionOf[id];
            const int oldDay = oldSlot / m.periods, newDay = newSlot / m.periods;
            int before = cell(m, sec, oldDay, current).total(cfg);
            if (newDay != oldDay) before += cell(m, sec, newDay, current).total(cfg);

            current.assign(id, newSlot);

            int after = cell(m, sec, oldDay, current).total(cfg);
            if (newDay != oldDay) after += cell(m, sec, newDay, current).total(cfg);

            const int delta = after - before;
            const double roll = rng() / 4294967296.0;   // [0, 1)
            if (delta <= 0 || roll < std::exp(-delta / temp)) {
                currentCost += delta;
                if (currentCost < bestCost) {
                    bestCost = currentCost;
                    best = current;
                }
            } else {
                current.assign(id, oldSlot);            // rejected: undo
            }
        }
        return best;
    }

private:
    SoftSettings cfg;

    // Lookup tables built once per call so the search loop stays simple.
    struct Model {
        std::vector<const Node*> nodes;               // indexed by node id
        std::vector<std::vector<int>> bySection;      // node ids of each section
        std::vector<size_t> sectionOf;                // node id -> index into bySection
        int days = 0, periods = 0;
    };

    static Model buildModel(const Graph& graph, const TimeSlotPool& pool) {
        Model m;
        m.days = pool.getNumDays();
        m.periods = pool.getPeriodsPerDay();
        m.nodes.assign(graph.size(), nullptr);
        m.sectionOf.assign(graph.size(), 0);

        std::map<int, size_t> indexOfSection;           // section id -> position in bySection
        for (const auto& nodePtr : graph.getNodes()) {
            const Node* n = nodePtr.get();
            m.nodes[n->getId()] = n;
            auto it = indexOfSection.find(n->getSection()->getId());
            if (it == indexOfSection.end()) {
                it = indexOfSection.emplace(n->getSection()->getId(), m.bySection.size()).first;
                m.bySection.emplace_back();
            }
            m.bySection[it->second].push_back(n->getId());
            m.sectionOf[n->getId()] = it->second;
        }
        return m;
    }

    int chunkOf(int periodIndex) const {                // periodIndex is 0-based
        const int period = periodIndex + 1;
        if (period <= cfg.morningLastPeriod) return 0;
        if (period <= cfg.noonLastPeriod) return 1;
        return 2;
    }

    // All soft problems of ONE section on ONE day.
    SoftPenalties cell(const Model& m, size_t sec, int day, const Timetable& t) const {
        std::vector<bool> used(m.periods, false);
        std::vector<int> courses;
        for (int id : m.bySection[sec]) {
            const int slot = t.slotOf(id);
            if (slot < 0 || slot / m.periods != day) continue;
            const int length = m.nodes[id]->duration();
            for (int k = 0; k < length; ++k) {
                used[slot % m.periods + k] = true;                   // a block fills all its periods
                courses.push_back(m.nodes[id]->getCourse()->getId());  // one entry per hour
            }
        }

        SoftPenalties p;
        if (courses.empty()) return p;

        // 1. Same course too often in a day.
        std::sort(courses.begin(), courses.end());
        for (size_t i = 0; i < courses.size();) {
            size_t j = i;
            while (j < courses.size() && courses[j] == courses[i]) ++j;
            p.repeats += std::max(0, static_cast<int>(j - i) - cfg.maxSameCourse);
            i = j;
        }

        // 2. Too many classes in a row, 3. free periods between classes, 4. chunks used.
        int run = 0, first = -1, last = -1, classes = 0;
        bool chunkUsed[3] = {false, false, false};
        for (int period = 0; period < m.periods; ++period) {
            if (used[period]) {
                ++run;
                ++classes;
                if (first < 0) first = period;
                last = period;
                chunkUsed[chunkOf(period)] = true;
            } else {
                p.runs += std::max(0, run - cfg.maxRun);
                run = 0;
            }
        }
        p.runs += std::max(0, run - cfg.maxRun);
        p.gaps += (last - first + 1) - classes;

        const int chunkCount = chunkUsed[0] + chunkUsed[1] + chunkUsed[2];
        p.chunks += chunkCount - 1;
        if (chunkUsed[0] && chunkUsed[2] && !chunkUsed[1]) p.splits += 1;   // morning + evening only
        return p;
    }
};
