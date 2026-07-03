# GoTo Engine
### Design notes — full architecture & build plan

---

## 0. Why this design

A GoTo mount's actual hard problem isn't spinning motors — it's the math.
Every object in the sky has a fixed position in equatorial coordinates
(RA/Dec), but where that object appears from a given point on Earth changes
continuously as the planet rotates. The engine's job: take a target, my
location, and the current time, continuously output where the mount should
physically point, then translate that into motor commands.

Building the coordinate/tracking core first, fully testable against known
reference values, with zero hardware required — same sim-first shape as
ACE. Motors, mount frame, and camera come later; the math has to be right
first, and it's checkable independently of any hardware ever existing.

**Design constraint carried through the whole project:** the mount layer
must be swappable (virtual ↔ real stepper motors) without touching the
coordinate math or tracking loop. Same actuator-abstraction pattern as ACE
— if swapping `VirtualMount` for `StepperMount` isn't a one-line change in
the factory, the abstraction is wrong.

---

## 1. What this is, end to end

A C++ system that:
- Converts catalog objects (fixed RA/Dec) into real-time Alt/Az coordinates
  for a specific location and moment
- Continuously re-computes that position as time advances (tracking)
- Converts Alt/Az into motor step counts for a two-axis mount
- Runs entirely in simulation first — a virtual mount holding "current
  position" in memory, no hardware needed to validate the math
- Later: drives real stepper motors over serial, and eventually
  self-calibrates using a camera and star-pattern matching (plate solving)

---

## 2. Full system architecture

```
        ┌─────────────────┐
        │  Object catalog    │   (RA/Dec, static data — Messier + bright stars)
        └─────────┬─────────┘
                   │
                   ▼
        ┌─────────────────────┐
        │  Coordinate transform │   RA/Dec + lat/long + time → Alt/Az
        │  engine                │   (sidereal time + spherical trig)
        └─────────┬─────────────┘
                   │
                   ▼
        ┌─────────────────────┐
        │  Tracking loop         │   recomputes Alt/Az on a timer as
        │                        │   Earth rotates, feeds mount driver
        └─────────┬─────────────┘
                   │
                   ▼
        ┌─────────────────────┐
        │  Mount driver           │   Alt/Az → motor step counts
        │  (abstract interface)   │   (gear ratios, step resolution,
        └───┬─────────────┬──────┘    backlash compensation)
            │             │
     ┌──────▼───┐   ┌─────▼──────────┐
     │ Virtual   │   │ Stepper mount    │
     │ mount     │   │ (real HW, over    │
     │ (sim)     │   │  serial to MCU)   │
     └───────────┘   └─────────┬────────┘
                                │
                     ┌──────────▼──────────┐
                     │ Plate solver (later)  │
                     │ camera → star match →  │
                     │ position correction    │
                     └────────────────────────┘
```

---

## 3. Repo layout (full)

```
goto-engine/
├── CMakeLists.txt
├── include/gte/
│   ├── catalog.hpp             # object database (RA/Dec + metadata)
│   ├── coord_transform.hpp     # RA/Dec + observer + time → Alt/Az
│   ├── time_utils.hpp          # sidereal time, UTC handling
│   ├── tracking_loop.hpp       # continuous position recompute
│   ├── mount_driver.hpp        # abstract interface
│   ├── mount_virtual.hpp
│   ├── mount_stepper.hpp       # real HW, stub until hardware exists
│   ├── plate_solver.hpp        # camera-based self-calibration (later)
│   └── error_correction.hpp    # periodic error correction (extension)
├── src/
│   └── (mirrors include/)
├── data/
│   └── catalog_messier.json
├── firmware/
│   └── goto_mount_mcu.ino      # microcontroller-side stepper driver (later)
├── tests/
│   └── (gtest: transform accuracy against known reference values,
│           boundary cases, tracking regression)
├── cli/
│   └── main.cpp                # target selection + live Alt/Az printout
└── docs/
    ├── COORD_MATH.md
    └── SERIAL_PROTOCOL.md      # mount command protocol (later)
```

---

## 4. Coordinate transform — the actual math

### 4a. Sidereal time (Phase 1)

Inputs: UTC timestamp, observer longitude.
Output: Local Sidereal Time (degrees, [0, 360)).

1. Convert UTC to Julian Date (JD)
2. T = (JD − 2451545.0) / 36525.0 — Julian centuries since J2000.0
3. GMST (degrees) = 280.46061837 + 360.98564736629 × (JD − 2451545.0)
   + 0.000387933 × T² − T³ / 38710000.0
4. Normalize into [0, 360)
5. LST = GMST + longitude (east-positive convention, documented and
   consistent everywhere in the codebase — sign errors here are the #1
   source of "points at the wrong side of the sky" bugs)

### 4b. RA/Dec → Alt/Az (Phase 2)

Inputs: target RA/Dec, observer latitude, local sidereal time.
Output: Alt (elevation), Az (compass direction).

1. Hour angle H = LST − RA
2. sin(alt) = sin(dec)·sin(lat) + cos(dec)·cos(lat)·cos(H) → alt = asin(...)
3. cos(az) = (sin(dec) − sin(alt)·sin(lat)) / (cos(alt)·cos(lat)) →
   az = acos(...), sign resolved by sin(H)
4. Normalize az to [0, 360)

**Edge cases handled explicitly, with tests:** object at zenith (az
undefined/unstable — defined convention needed), object at the celestial
pole, object below horizon (valid output, tracking loop must check this
before issuing a slew).

**Correctness check for both 4a and 4b:** known (location, time, object)
triples checked against a reference planetarium tool's published values,
within a fraction of a degree tolerance. This is the real test suite, not
just "does it compile."

---

## 5. Catalog (Phase 3)

Messier catalog (110 deep-sky objects) + a few dozen bright named stars —
enough for a genuinely useful "what can I point at tonight" experience
without needing a full astronomical database. Static JSON: RA/Dec, common
name, object type. Loader is a thin lookup layer — resist over-engineering
this part, it's just data access.

---

## 6. Mount driver abstraction (Phase 4, extended in Phase 8)

```cpp
class IMountDriver {
public:
    virtual void slewTo(double alt_deg, double az_deg) = 0;
    virtual HorizontalCoord currentPosition() const = 0;
    virtual ~IMountDriver() = default;
};

class VirtualMount : public IMountDriver {
    // in-memory position, instant "slew" — no simulated slew speed in v1
};

class StepperMount : public IMountDriver {
    // real HW: alt/az → step counts, over serial to a microcontroller
    // that actually drives the stepper motors
};
```

Gear ratios, step resolution, and (later) backlash compensation values live
in a config file — placeholder values now, real ones once motors are
picked, same pattern as ACE's `actuators.json`.

---

## 7. Tracking loop (Phase 5)

- Recompute target Alt/Az on a fixed interval (e.g. every 1s)
- Earth's rotation means the "correct" pointing position is always
  slightly ahead of real time by the time a motor command executes — loop
  needs to run ahead slightly, not just react
- Loop timing uses `steady_clock` (monotonic, same discipline as ACE);
  the sidereal math itself still needs wall-clock UTC, just not for loop
  scheduling
- Before issuing a slew, checks target Alt ≥ 0 (above horizon) — silently
  refusing to point at the ground is a real requirement, not an edge case
  to skip

---

## 8. CLI (Phase 6)

Pick a catalog object, print live Alt/Az every second as the tracking loop
ticks:

```
$ goto-engine track M42
[12:00:01] M42 (Orion Nebula) — Alt: 34.21°  Az: 142.09°
[12:00:02] M42 (Orion Nebula) — Alt: 34.22°  Az: 142.11°
```

This is the first fully demoable artifact — correct real-time sky tracking,
zero hardware required.

---

## 9. Test suite (Phase 7)

- Per-function tests for sidereal time and coordinate transform against
  known reference values
- End-to-end tests: known (object, location, time) → known Alt/Az through
  the full pipeline
- Boundary tests: pole, zenith, below-horizon
- Regression test: run the tracking loop for N simulated ticks, assert
  Alt/Az moves in the expected direction for a rising/setting object

---

## 10. Real mount hardware (Phase 8 — blocked on hardware)

Once motors and a mount frame exist:

- `StepperMount` implementation: Alt/Az → step counts, using real gear
  ratios and step resolution
- Microcontroller firmware (`firmware/goto_mount_mcu.ino`): listens for
  slew commands over serial, drives the actual stepper motors — same
  serial-command pattern as ACE's Arduino slave sketch
- Command protocol documented in `docs/SERIAL_PROTOCOL.md`, same approach
  as ACE's protocol doc
- Backlash compensation: real gears have mechanical slop; naive step
  counting drifts without correcting for direction changes
- Slew speed limits and acceleration curves — real motors can't teleport
  the way `VirtualMount` does; sudden full-speed slews risk skipping steps
  or straining the mount
- Homing/calibration routine: the mount needs to know its own starting
  position on power-up, since steppers have no inherent absolute position
  sense
- Emergency stop / manual override, same reasoning as ACE's kill switch —
  build this before trusting the system to run unattended

---

## 11. Plate solving (Phase 9 — blocked on hardware, needs a camera)

Point the scope anywhere, take a photo, identify exactly what it's looking
at by matching detected star patterns against the catalog's geometry, then
use that to auto-correct the mount's known position. This is a genuine
computer vision pipeline:

1. Star detection in the captured image (blob detection on bright points)
2. Compute relative angular distances/patterns between detected stars
3. Match that pattern against the catalog's known star geometry
4. Solve for the actual pointing position implied by the match
5. Feed the correction back into the mount's known Alt/Az

This is the standout feature — real GoTo systems sell it as a premium
add-on, and it's the most technically differentiated piece of this project
relative to ACE (adds a CV component nothing else here has). Depends on
Phase 8 being solid and a camera being mounted and calibrated first.

---

## 12. Optional extensions (Phase 10 — future, not yet scheduled)

- **Periodic error correction** — log tracking error over time, fit a
  correction curve for repeating mechanical imperfections in the gears,
  feed it back into the tracking loop. Same statistical-rigor pattern as
  DriftGuard; produces a real before/after arcsecond accuracy number.
- **Sky tour optimizer** — given location, time, and observing duration,
  rank and sequence tonight's best objects (altitude, moon proximity, time
  above horizon before setting). A scheduling/optimization problem, not
  astronomy per se.
- **Satellite/ISS pass tracking** — fast-moving objects need continuously
  recomputed trajectories rather than the mostly-static catalog case, a
  harder stress test for the tracking loop's update rate.

---

## 13. Build phases (full)

| Phase | Scope | Est. time | Hardware needed? |
|---|---|---|---|
| 1 | Sidereal time: UTC handling, GMST/LST | 2 days | No |
| 2 | Coordinate transform: RA/Dec → Alt/Az, tested against reference values | 3-4 days | No |
| 3 | Catalog: load + query Messier/bright-star data | 1-2 days | No |
| 4 | Virtual mount + mount driver interface | 1-2 days | No |
| 5 | Tracking loop: continuous recompute, feed virtual mount | 2-3 days | No |
| 6 | CLI: pick a target, watch simulated Alt/Az track over time | 1-2 days | No |
| 7 | Test suite: accuracy validation, edge cases, tracking regression | 2-3 days | No |
| 8 | StepperMount + firmware + backlash/homing/e-stop | — | Yes — motors, mount frame, MCU |
| 9 | Plate solving | — | Yes — camera |
| 10 | Optional extensions (error correction, sky tour, satellite tracking) | — | Depends on extension |

Fully demoable, testable v1 (Phases 1-7) with zero hardware: roughly 2
weeks at a steady pace.

---

## 14. Tech stack

- C++20, CMake, GoogleTest
- No external ephemeris library for v1 — implementing the core transform
  from scratch; revisit only if precision requirements outgrow basic
  spherical trig
- nlohmann/json for catalog + config
- OpenCV for the plate-solving extension (Phase 9 only)
- CI: GitHub Actions — build + test on every commit, same pattern as ACE

---

## 15. First step

Sidereal time, tested against known reference values. Everything else in
the transform is downstream of this being right — get it wrong here and
every later Alt/Az output is subtly, confusingly off in a way that's hard
to trace back once the codebase has grown past this one function.
