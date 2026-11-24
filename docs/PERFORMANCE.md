# Performance and controller verification

The macOS frame timers now sleep through their remaining deadline rather than
busy-spinning. The interpolated frontend is capped at 60 FPS (15 while inactive).
Dungeon drawing is limited to 30 FPS while inactive and 10 while minimized;
network input, packet exchange and simulation updates continue independently.
Headless dedicated hosts bypass this presentation policy.

An identical 1,000-wait workload against the actual old/new timer objects used
about 1.99 CPU seconds before and 0.005 after. That isolates the waiting cost;
it is not a claim of an equivalent whole-game speedup. Saved-dungeon captures
used roughly 276–280 MB peak RSS. Before/after whole-process CPU measurements
were noisy (18.8 vs 20.3 CPU seconds over about 27 seconds) and did not establish
a whole-game improvement. Rendering p95 was about 33.4 ms in these mixed-focus
runs. Larger dungeons, long sessions and foreground/background comparisons on
multiple Macs remain useful performance work.

Capture a real saved dungeon in an isolated profile:

```sh
python3 scripts/profile-game.py --app '/Applications/Keeper-Enhanced.app' \
  --engine build-mac-universal/keeperfx --game-dir /path/to/local/runtime \
  --save /path/to/copy-of-save.sav --seconds 30 --output /tmp/keeperfx-profile.json
```

The tool imports into a temporary profile, copies the selected save, and never
writes to the original save or installed app. Reports include startup/load time;
frame intervals are a rolling window of at most 4,096 presentations, flushed
every 120 frames. CPU and memory are process measurements, not GPU counters.
For manual capture, set `KFX_FRAME_PROFILE` to an output JSON path. Disable it
for ordinary play to avoid measurement/file-write overhead. Reports stay local.

Under Controller → Select Controller → Test and calibrate controller, raw axes
and buttons can be tested without the normal back button closing the page.
Leave both sticks untouched for two seconds to measure resting drift; movement
or a controller change cancels calibration. Dead zones remain manually adjustable
and can be restored. Start returns; Y/Triangle calibrates; X/Square restores.
Camera pan sensitivity remains adjustable under Accessibility Presets. Virtual
Xbox/DualSense tests cover input logic; physical transport behavior still needs
testing with the actual controller and macOS version.
