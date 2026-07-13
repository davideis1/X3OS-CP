# TinyRdr — Design Brief

This is a condensed, design-focused companion to `docs/ui-ux-overhaul-plan.md`. That document has the full
engineering rationale and code citations; this one is meant to be handed to a designer (or Claude Design)
without requiring familiarity with the codebase. Read `SCOPE.md` alongside this for the project's current
in-scope/out-of-scope boundaries.

## 1. What this device is

A dual-hardware e-reader firmware (Xteink X3 and X4, both ESP32-C3-based) that's expanding from a pure reading
device into a curated set of on-device tools, games, and reference utilities, plus a companion iPhone app for
managing device data remotely. Reading remains the primary use case and gets first claim on screen time and
resources; everything else is secondary and must not compromise it.

## 2. Hardware constraints that shape every screen (non-negotiable)

- **800×480 E-Ink display**, monochrome (1-bit) framebuffer. Limited grayscale is achievable for images via a
  tiled multi-pass banding technique, but UI chrome (text, icons, layout) should be designed as pure black/white
  — no assumption of a grayscale or color palette for interface elements.
- **No touchscreen, no stylus.** All navigation is physical buttons only: Up/Down (fixed side buttons),
  Confirm/Back/Left/Right (front buttons, user-remappable), plus short-press vs. long-press as a real,
  already-used gesture vocabulary (e.g. long-press-Back = jump to root folder, long-press-Confirm = delete/remove
  item). Any new screen needs to work entirely through this vocabulary — no gestures, no drag, no multi-touch.
- **No partial/regional e-ink refresh** — only whole-screen refresh at three speed/quality tiers (fast, half,
  full). This is why every proposed game is turn-based rather than real-time: continuous animation is not
  physically achievable well on this panel.
- **All 4 orientations are supported** (Portrait, Landscape CW, Inverted, Landscape CCW/native) via logical
  rotation — any new screen's layout needs to work rotated, not just in one fixed orientation.
- **Single 48KB framebuffer, 380KB total RAM.** This mostly constrains engineering, not visual design directly,
  but it does mean: no large full-screen imagery beyond what's already used for covers/sleep screens, and no
  assumption that multiple screens' worth of content can be pre-rendered/cached simultaneously.
- **4 existing themes** (Classic, Lyra, Lyra Extended, RoundedRaff) are all driven by one shared `ThemeMetrics`
  struct (row heights, spacing, corner radii, icon usage, etc.) — new screens should be designed to work with
  this metrics system (i.e., describe layout in terms of reusable spacing/row-height tokens) rather than as
  one-off pixel-perfect compositions, so they can be re-skinned across themes for free.

## 3. Existing visual language to build from

- Icons are 32×32 1-bit bitmaps, currently ~15 of them (folder, book, bookmark, file-type badges, settings,
  wifi, etc.), used sparingly — home-screen menu rows and file-type badges today, not yet used on settings rows
  or tabs (that's part of this redesign).
- Primary UI patterns already built and reusable: a scrollable single-column **list** (with optional icon +
  subtitle per row), a **button menu** (icon + label grid, used on Home), a **tab bar** (used for Settings
  categories), an **option popup** (modal single-select list, used for enum settings), and a **keyboard entry**
  screen (on-screen QWERTY-style grid navigated via buttons, used for Wi-Fi passwords etc. today). New screens
  should be designed as compositions of these existing patterns wherever the content allows, since each new
  pattern is real engineering cost.
- Typography: Noto Serif / Noto Sans in several sizes, plus custom SD-card font support. Reading screens use
  serif by default; UI chrome uses a dedicated UI sans font family.

## 4. Information architecture (proposed)

```
Home
├── Continue Reading (shelf of last 3 books)
├── Browse Files
├── Recents
├── Tools                      [NEW]
│   ├── Calculator
│   ├── Unit Converter
│   ├── To-Do List
│   ├── Notes (opens /Notes/ folder in the existing text viewer)
│   ├── Games
│   │   ├── Sudoku
│   │   ├── Klondike
│   │   ├── Nonograms
│   │   ├── Interactive Fiction
│   │   └── Five Crowns
│   ├── Dictionary/Encyclopedia lookup   (reader-integrated, reachable in-book too)
│   └── Weather
├── Connect                    [regrouped]
│   ├── File Transfer
│   └── OPDS Browser
└── Settings
    ├── Display
    ├── Typography              [split from Reader]
    ├── Layout & Behavior        [split from Reader]
    ├── Controls
    └── System
        └── Diagnostics (battery, heap, uptime, storage)   [NEW]
```

Companion iPhone app (separate IA, own navigation — see §6):

```
App
├── Status (battery, storage, connection state)
├── Library (browse/upload/download/rename/move/delete)
├── Settings (mirrors device categories above)
├── Fonts (SD-card font manager)
├── To-Do List (offline-capable, syncs on session)
├── Wi-Fi / OPDS server manager
└── Pairing (one-time device pairing flow)
```

## 5. Screen inventory, prioritized

Priorities mirror the phased rollout in `docs/ui-ux-overhaul-plan.md` §6. Start design work on Phase 1.

**Phase 1 (design first)**
- Home screen redesign: 3-book shelf + restructured primary menu (Browse/Recents/Tools/Connect/Settings)
- Settings: tab bar with icons, Reader split into Typography/Layout & Behavior, Sleep Screen settings grouped
- Tools menu (new top-level destination)
- Calculator
- Unit Converter
- To-Do List (device screen — list with checkboxes + add-item entry)
- Diagnostics panel (Settings > System)
- "Notes" entry point (near-zero new UI, mostly a Home/Tools shortcut)

**Phase 2**
- File browser: sort-by control, author subtitle + cover thumbnail per row, jump-to-letter affordance
- Sudoku (defines the shared board-game interaction pattern — grid, cell selection, digit cycling)
- Klondike (defines the shared card-rendering style — text/glyph-based cards, not pictorial)
- Nonograms
- Interactive Fiction reader (passage text + choice list)
- Weather widget (on-demand connect flow)
- In-reader dictionary lookup (word-selection interaction — new territory, needs its own exploration since
  there's no touch/cursor precedent yet)
- Companion app v1 (Status, Library, Settings, Fonts, Wi-Fi/OPDS — all against existing APIs) + pairing flow

**Phase 3**
- Five Crowns (multi-round card game, AI opponents, scrollable large hand)
- Compact theme variant (settings/library-focused density option)
- Extended icon set / icon style consolidation pass
- Companion app v2 (to-do sync, content-pack push, quick-capture notes)

**Explicitly not ready for design yet** (research spikes, feasibility unproven): full Z-machine interactive
fiction, Kiwix/ZIM offline encyclopedia, on-device unzip. Don't invest design time here until the engineering
spike confirms these are buildable at all.

## 6. Companion app notes for design

- **Sync model is "offline-first, sync-on-session."** The app has no persistent connection to the device — it
  works from local cache and syncs only when the device is reachable (in File Transfer mode, on the same
  network). This needs to be visible in the UI, not hidden: a clear "Last synced X ago" / "Not connected — open
  File Transfer on your reader" state is a first-class design element, not an error state.
- The device is the source of truth for settings and files; the to-do list is the one place with real two-way
  sync (edits possible on both sides while apart), so its UI should make "pending sync" state visible per item.
- No push notifications are possible (no persistent connection) — don't design around them.
- This is otherwise a fairly standard iOS settings/file-manager app in terms of component needs (lists, forms,
  file browser, toggle/enum editors) — the interesting design problem is entirely the connection-state model
  above, not novel interaction patterns.

## 7. Explicit non-negotiables (recap)

Don't propose: touch gestures or swipe interactions, color UI (grayscale/color chrome), continuous
animation/real-time games, background/always-on sync on the device side, audio/haptic feedback (no
speaker/vibration motor), or handwriting/stylus input. Every one of these is a hardware limitation, not a
style preference — see `SCOPE.md` §2 "Out-of-Scope" for the full reasoning.
