# TinyRdr — UI/UX Overhaul Plan

Scope of this plan: Home/Library navigation, Settings screens, theme/visual design, a set of new on-device
apps/utilities, and a companion iPhone app. In-book reader chrome (EpubReaderActivity, footnotes, chapter/percent
selection) is intentionally out of scope for this pass, except where a new feature (dictionary lookup) explicitly
requires touching it.

**Scope note (project direction change):** Sections 4 and 5 below (apps/utilities, companion app) intentionally
go beyond upstream `SCOPE.md`, which currently states *"This is a reader, not a PDA"* and explicitly lists
Interactive Apps and Games as out-of-scope. This is a deliberate decision for this fork to become a
general-purpose "Swiss Army knife" device rather than a single-purpose reader. Before merging any of Section 4/5
work, `SCOPE.md`'s In-Scope/Out-of-Scope lists and the "Philosophy" line in `CLAUDE.md`
("We are building a dedicated e-reader, not a Swiss Army knife...") need to be rewritten to match — otherwise
these PRs will contradict the project's own contributor-facing docs. That rewrite is tracked as a follow-up, not
done in this document. All other constraints from CLAUDE.md remain fully in force: 380KB RAM ceiling, single
48KB framebuffer, no PSRAM, button-only input (the device has no touchscreen, confirmed by the absence of any
touch input handling outside SDK/driver files), no true partial/regional e-ink refresh (only `FULL_REFRESH` /
`HALF_REFRESH` / `FAST_REFRESH` whole-screen modes exist — `lib/hal/HalDisplay.h:14-17` — which is why every new
app below is turn-based/static rather than real-time).

## 1. Current-state findings

### 1.1 Home screen

- `HomeActivity` treats "Continue Reading" and the primary menu as one interleaved selection list: recent-book
  tiles occupy index `0..N-1` and menu entries follow (`src/activities/home/HomeActivity.cpp:23-32`,
  `169-207`). The Classic theme shows exactly one recent book (`homeRecentBooksCount = 1`,
  `src/components/themes/BaseTheme.h:144`) in a tile up to 400px tall on an ~480px-tall display
  (`homeCoverTileHeight = 400`, `BaseTheme.h:143`), leaving little room for anything else.
- A richer pattern already exists and ships today: `Lyra3CoversTheme` shows a 3-book shelf by overriding
  `homeRecentBooksCount = 3` and shrinking `homeCoverTileHeight` to 300
  (`src/components/themes/lyra/Lyra3CoversTheme.h:9-16`), proving multi-cover shelves are cheap (metrics-only
  change plus one overridden draw method) and already validated in production as "Lyra Extended."
- The primary menu is a flat, undifferentiated list of up to 6 items — Browse Files, Recents, OPDS Browser (if
  configured), File Transfer, Settings, plus an optional "Continue Reading" row
  (`HomeActivity.cpp:234-247`). Frequency of use is not reflected in ordering or grouping: a one-time-setup action
  (File Transfer, OPDS) sits at the same navigational depth as the every-session action (Browse Files).
- `getCoverThumbPath` / thumbnail generation infrastructure already exists for recent books
  (`UITheme::getCoverThumbPath`, used in `HomeActivity.cpp:62`), but is not reused anywhere else (e.g., file
  browser, recents list) — those screens fall back to generic per-type icons only.

### 1.2 Library navigation (File Browser / Recents)

- `FileBrowserActivity` is a flat, single-directory list with alphabetical sort only
  (`FsHelpers::sortFileList`, `src/activities/home/FileBrowserActivity.cpp:63`). There is no sort-by-recent,
  sort-by-title/author, or filter/search. With physical up/down buttons as the only input, long folders require
  many button presses; `ButtonNavigator::nextPageIndex` provides page-jump but no letter-jump or "jump to top."
- The file browser's `drawList` call passes no subtitle callback (`FileBrowserActivity.cpp:369-374`), so entries
  show filename + generic type icon only — no author, no per-book cover. `RecentBooksActivity`, by contrast,
  already renders a title + author subtitle (`src/activities/home/RecentBooksActivity.cpp:140-143`), showing the
  underlying `drawList` component already supports this; the file browser simply doesn't populate it (metadata
  isn't read for unopened books, since it would require touching the EPUB/XTC cache for every visible row).
- No "sort by" or "view as grid" setting exists anywhere in `SettingsList.h`. Library Management is explicitly
  called out as in-scope in `SCOPE.md`, but no setting currently lets a user choose how their shelf is organized.
- Long-press-to-delete and long-press-back-to-home (`FileBrowserActivity.cpp:192-199, 230-261`) are undiscoverable
  — there's no on-screen hint that these gestures exist beyond the standard button-hint bar, which only shows
  short-press labels.

### 1.3 Settings

- Settings are organized into exactly 4 fixed tabs — Display, Reader, Controls, System
  (`src/activities/settings/SettingsActivity.cpp:29-30`) — each rendered as one long flat scrolling list
  (`GUI.drawList`, `SettingsActivity.cpp:373-412`). Counting entries in `SettingsList.h`: Display has 7 items
  (`SettingsList.h:106-130`), Reader has 12 base items plus 2 injected actions ("Manage Fonts," "Customise Status
  Bar" — `SettingsActivity.cpp:70-72`) for 14 total, Controls has 6-7 (7 with tilt sensor present), System has 4
  settings plus 6 injected action items for 10 total. A 14-item single-column list with no subgrouping is a long
  scroll on a button-only device — each item requires a discrete up/down press.
- Settings rows have no icons (`SettingsActivity.cpp:373-379` builds label/value text only), unlike the home menu
  which does use icons (`UIIcon` enum, `HomeActivity.cpp:236`). This makes the settings list visually
  monotonous and harder to scan than the home screen.
- "Customise Status Bar" (9 sub-settings) and KOReader Sync (4 sub-settings) are reachable only as a single
  `ACTION` row that launches a whole sub-activity (`SettingsActivity.cpp:267-271`) — a reasonable pattern, but it's
  applied inconsistently: some multi-setting groups (e.g. sleep-screen's 3 related settings, `SettingsList.h:106-114`)
  stay flat in the Display tab instead of being grouped the same way.
- Category tabs carry no icon or item-count indicator (`GUI.drawTabBar`, `SettingsActivity.cpp:364-370`), so a user
  can't tell from the tab bar alone that Reader is the "big" category.

### 1.4 Theming and visual system

- Theming is already a strong foundation: `ThemeMetrics` is a single `constexpr` struct
  (`src/components/themes/BaseTheme.h:120-254`), so every theme's layout constants live in flash, not RAM, and a
  new theme costs zero heap. `BaseTheme` is ~1100 lines of shared drawing code
  (`src/components/themes/BaseTheme.cpp`), with `LyraTheme` and `RoundedRaffTheme` overriding only specific draw
  methods and metrics.
- Icons are minimal and cheap: 15 icon headers averaging ~12 lines / ~130 bytes each as flash-resident
  `static const uint8_t[]` arrays (`src/components/icons/*.h`, e.g. `folder.h:1-17`, 32×32 1-bit bitmap). The
  `UIIcon` enum currently has 13 entries (`BaseTheme.h:120`) and is used only for home-menu items and file-type
  badges — settings categories/rows, and OPDS/library-specific concepts, have no icons defined at all.
- Four themes ship today: Classic, Lyra, Lyra Extended (3-cover shelf), RoundedRaff
  (`SettingsList.h:125-128`). There is no light/dark distinction beyond the sleep-screen setting, and no
  "compact / high information density" vs. "spacious / large-type" theme choice aimed specifically at settings and
  library screens (the existing themes primarily differentiate the home screen and corners/borders).

## 2. Proposed changes

### 2.1 Home screen

1. **Make the 3-cover shelf the default layout**, not just a Lyra Extended opt-in. Promote
   `Lyra3CoversTheme`'s approach (`homeRecentBooksCount = 3`) into `BaseMetrics` so Classic also shows a shelf of
   the last 3 books instead of 1. This is a metrics-only change (no new heap allocation — `HomeActivity`'s
   `loadRecentBooks(maxBooks)` already takes a count parameter, `HomeActivity.cpp:34-52`) and directly improves
   "continue reading" discoverability, which is the highest-frequency action on the device.
2. **Split the primary menu into "primary" and "connect" tiers.** Keep Browse Files, Recents, Settings (and
   Continue Reading, if not shown as a shelf) as the flat top-level menu; move File Transfer and OPDS Browser
   under a single "Connect" entry that opens a small sub-menu (reusing the existing `drawButtonMenu` component).
   This shortens the common path (open a book) by up to 2 button presses and only adds 1 press for the
   less-frequent networking actions.
3. **Reuse the existing cover-thumbnail pipeline** (`UITheme::getCoverThumbPath`) for the Recents list, not just
   the home shelf, so recently-read books show real covers instead of a generic Book icon — same underlying
   thumbnail cache, no new caching logic needed.

### 2.2 Library navigation

1. **Add a "Sort by" setting** (Title, Author, Recently Modified, File Type) for the file browser, stored as one
   new `uint8_t` in `TinyRdrSettings` (negligible RAM/flash cost) and applied in `FsHelpers::sortFileList` or a
   new sibling sort function. This directly serves SCOPE.md's "Library Management" in-scope category.
2. **Populate the file browser's subtitle slot with cached metadata only** (author, when a `book.bin` cache
   already exists for that path) — never trigger a fresh EPUB parse just to render a list row, to avoid I/O
   stalls scrolling a large folder. Fall back silently to no subtitle when the cache is cold, exactly as the
   thumbnail loader already does asynchronously on the home screen (`loadRecentCovers`, `HomeActivity.cpp:54-109`).
3. **Add a lightweight "jump to letter" shortcut** using the existing long-press convention (long-press Up/Down
   jumps to the next/previous initial letter) instead of a new input mode — keeps the button vocabulary
   consistent with the long-press-delete / long-press-home patterns already in `FileBrowserActivity`.
4. **Surface long-press gestures in the UI**, e.g. a one-line hint under the button-hint bar the first time a
   folder view is opened ("Hold OK to delete"), rather than leaving them fully undiscoverable.

### 2.3 Settings reorganization

1. **Split the Reader tab (14 items) into two logical groups** — "Typography" (font family/size/line
   spacing/margin/alignment/hyphenation/anti-aliasing) and "Layout & Behavior" (orientation, embedded style, focus
   reading, images, extra spacing, manage fonts, status bar) — using the same tab-bar mechanism already driving
   the 4 top-level categories (`SettingsActivity::categoryCount`, `categoryNames`). This is additive to an
   existing, well-tested pattern rather than a new UI paradigm.
2. **Group related Display settings** (Sleep Screen + Sleep Cover Mode + Sleep Cover Filter, currently 3 flat
   rows, `SettingsList.h:106-114`) behind a single "Sleep Screen" action row the same way Status Bar and KOReader
   Sync already work (`SettingsActivity.cpp:267-271`), for consistency and a shorter Display tab.
3. **Add icons to settings category tabs** (reusing the cheap ~130-byte icon format) so the tab bar is scannable
   at a glance, matching the visual language the home menu already uses.
4. **Show a small item-count badge** or denser row height option in list rendering when a category exceeds ~8
   items, so users get a sense of list length before scrolling — a metrics-level tweak (`ThemeMetrics.listRowHeight`
   already exists per-theme) rather than new logic.

### 2.4 Theme/visual polish

1. **Add row icons to the settings list**, not just tabs — reuse `UIIcon` and extend the enum with a handful of
   new small glyphs (typography, layout, controls, sync, wifi already exists, cache/trash). At ~130 bytes each in
   flash, even doubling the icon set costs under 2KB of flash and 0 bytes of RAM.
2. **Introduce a "Compact" theme variant** targeted at settings/library density — smaller `listRowHeight` and
   `menuRowHeight`, tighter `verticalSpacing` — for users who prioritize seeing more per screen over larger touch
   targets (irrelevant here since there's no touch input, so information density has no ergonomic downside).
3. **Consolidate the icon set's visual style.** Icons were added incrementally per-feature; a design pass to
   ensure consistent stroke weight/corner radius across `folder.h`, `book.h`, `bookmark.h`, etc. would improve
   polish without any code/RAM impact — this is pure asset rework.

## 3. Feature additions/removals (SCOPE.md-checked)

| Proposal | Scope check | Notes |
|---|---|---|
| Library sort options (title/author/date/type) | In-scope: "Library Management" | New 1-byte setting; sort logic is pure computation on an already-loaded file list, no new heap. |
| Cover thumbnails in file browser + recents list | In-scope: reuses existing thumbnail cache | No new caching mechanism; must stay lazy/async like the existing home-screen loader to avoid blocking navigation. |
| "Connect" sub-menu grouping (Transfer + OPDS) | In-scope: UX/navigation | Pure menu restructuring, no new functionality. |
| Split Settings tabs for Reader category | In-scope: UX | Additive use of the existing tab/category mechanism. |
| Jump-to-letter long-press in file browser | In-scope: book navigation | Reuses existing long-press input pattern; no new input modes. |
| Compact/high-density theme variant | In-scope: customization | Metrics-only theme, same pattern as existing 4 themes. |
| Any kind of in-device search/text-entry filtering across the whole library | Borderline | `KeyboardEntryActivity` already exists for text input (used elsewhere), so a filename filter is feasible, but should be scoped carefully — it must not become general-purpose text search/annotation, which risks drifting toward "Complex Annotation" (out-of-scope). Recommend a letter-jump instead of full free-text search for v1. |
| Folder-based "collections/shelves" beyond what the filesystem already provides | Out of scope for this pass | Adds persistent metadata and UI complexity disproportionate to benefit; the existing folder browser already lets users organize books via SD-card folders. Not recommended unless there's clear user demand. |
| Grid/cover-wall library view (vs. list+thumbnail) | Recommend against for now | Full-grid layouts redraw far more of the 48KB single framebuffer per navigation step than a list, increasing e-ink flash/ghosting frequency; a list with an inline thumbnail (as proposed above) gets most of the visual benefit at a fraction of the refresh cost. |

Nothing proposed here touches SCOPE.md's out-of-scope list (interactive apps, active/background connectivity,
media playback, complex annotation, PDF rendering).

## 4. Apps & utilities — new feature specs

General notes that apply to all items below: every new "app" is a new `Activity` subclass launched from Home
(most likely a new **Tools** menu entry, grouping Calculator/Unit Converter/Games/Notepad the same way "Connect"
groups Transfer/OPDS in Section 2.1) or from Settings > System (Diagnostics). Games that need a grid/board
renderer (Sudoku, Nonograms, Klondike, Five Crowns) should share one new `lib/BoardRenderer`-style component
built for the first game and reused by the rest, the same way `BaseTheme`/`ThemeMetrics` is shared today — this
keeps flash cost from multiplying per game. Every game needs a small persisted save-slot (in-progress board
state) using the exact `PersistableStore<T>` CRTP pattern already used by `RecentBooksStore`/`OpdsServerStore`
(`lib/Serialization/PersistableStore.h:53-82`), so resuming after sleep is free architecturally.

### 4.1 Sudoku (generator + solver assist)

Turn-based and grid-based — a good first candidate to build the shared board-input pattern (cycle 1–9 per cell
with Up/Down + Confirm, no keyboard needed) that Klondike/Nonograms/Five Crowns can then reuse. Generator and
solver are both pure backtracking over a fixed 9×9 array — negligible heap, all stack/static. Recommend
**Phase 1**: cheapest way to prove out the board-game UX pattern before investing in the others.

### 4.2 Klondike (1-card draw, endless deck cycling)

Recommend **text/glyph-styled cards** (rank + suit character, styled like the existing list rows) rather than
52 individual card-face bitmaps — keeps flash cost near zero and stays legible on a monochrome e-ink panel at
small sizes. The 1-card-draw / infinite-recycle rule set is deliberately the simplest Klondike variant (no
waste-pile-of-3 bookkeeping), which keeps the state machine small. **Phase 2**, after Sudoku validates the
board-input pattern; establishes a shared "Cards" rendering module reused by Five Crowns.

### 4.3 Nonograms / Picross

Needs a puzzle-pack format (row/column clues). Recommend SD-card-based packs (small JSON or compact binary),
following the same "user-installable content on SD, not baked into flash" pattern already used for fonts
(`SdCardFontRegistry`) — this also means new puzzle packs can ship via the existing file-upload API with zero
firmware changes. **Phase 2.**

### 4.4 Interactive fiction (Z-machine / Twine format)

This one needs an important scope correction: **Twine games export to HTML+JavaScript and cannot run on this
firmware** — there's no JS engine, and building one is far outside the RAM/flash budget. What's actually
buildable is a minimal choice-based-narrative reader for **Twee** (Twine's plain-text source format) or a
similarly simple branching-text format: parse a small structured text file into passages + choice links, render
passages as reflowed text (reusing the existing text-rendering path), and render choices as a `drawList`
selection menu. This is realistically cheap.

Z-machine (the actual Infocom/Inform format) is a real interpreter — a well-specified but nontrivial VM
(opcode dispatch, dynamic memory, save/restore) with no existing implementation in this codebase, and per-title
RAM use varies with the story file's dynamic memory size, which needs per-title validation against the 380KB
ceiling. Recommend treating full Z-machine support as a **Phase 3+ research spike** (validate one interpreter
against one real story file's memory footprint before committing), and shipping the Twee-style choice-narrative
reader as the actual **Phase 2** deliverable under this feature's name.

### 4.5 Five Crowns (card game, 2–4 computer players)

The most complex of the requested games: 11 rounds with escalating hand sizes (3 to 13 cards), a rotating wild
card per round, and rule-based (not ML) AI opponents. Hand sizes up to 13 cards need a scrollable/paginated hand
display on a single small screen. Builds directly on the "Cards" rendering module from Klondike (4.2), so should
land after it. **Phase 3.**

### 4.6 Calculator

Standard four-function/scientific calculator. Reuses `KeyboardEntryActivity`'s existing key-grid input pattern
(`src/activities/util/KeyboardEntryActivity.cpp`) instead of building a new input widget — this is largely an
exercise in wiring an existing component to numeric-entry mode plus an expression evaluator. No persistent
storage needed. **Phase 1** — cheapest way to validate how a non-book "Tool" fits into Home navigation.

### 4.7 Unit converter

Static, flash-resident `constexpr` conversion-factor tables; from-unit/to-unit selection reuses the existing
`OptionPopup` component (already used throughout Settings) instead of a new picker widget. Shares the numeric
keypad from 4.6. **Phase 1**, alongside Calculator.

### 4.8 To-do list / task manager (with iPhone sync)

New `TodoStore` following the exact `PersistableStore<T>` CRTP pattern used today by `RecentBooksStore` /
`OpdsServerStore` — a JSON array of `{id, text, done, createdAt}` in `/.tinyrdr/todos.json`
(`lib/Serialization/PersistableStore.h`). Device UI reuses `drawList` for a checkbox-style toggle per row (same
rendering as a `TOGGLE` setting) plus `KeyboardEntryActivity` to add new items. Since you specifically want to
manage this list from the iPhone app, it needs new `GET/POST /api/todos` endpoints mirroring the existing
`/api/opds` shape (list, add/update, delete-by-id) — see Section 5 for what that implies about the sync model.
**Phase 1** for the on-device version; the endpoint work should land in the same phase as the app's MVP.

### 4.9 Plain-text notepad viewer

This one is **effectively already shipped and just undiscoverable**: any `.txt` file already opens in the
existing `TxtReaderActivity` with full pagination and caching (`src/activities/reader/TxtReaderActivity.cpp`).
Recommended work is almost entirely discoverability: add a "Notes" shortcut (e.g. under the new Tools menu, or
Home) pointing at a `/Notes/` folder, and treat this as **view-only on-device** — editing happens via the
iPhone app or existing web file manager, both of which already support uploading/overwriting `.txt` files.
**Phase 1**, minimal new firmware code.

### 4.10 Battery & system diagnostics panel

Most of the underlying data already exists: `HalPowerManager::getBatteryPercentage()`
(`lib/hal/HalPowerManager.h:45`), free heap, and uptime — the latter two are already surfaced today via
`GET /api/status` (`docs/webserver-endpoints.md`). The one real gap is **SD card total/free space**: `HalStorage`
currently exposes only per-file `size()` / `fileSize()` (`lib/hal/HalStorage.h:77-79`), not card capacity, so
this needs one new call into SdFat's volume/sector-count API. Device screen is a new read-only Activity under
Settings > System reusing `drawList` for label/value rows — no new rendering primitives needed. This also
directly strengthens the iPhone app's status screen once `/api/status` is extended with battery % and storage.
**Phase 1.**

### 4.11 File manager extras: rename, zip/unzip, move, copy

Rename and move are **already implemented today**, just web-only (`POST /rename`, `POST /move` —
`docs/webserver-endpoints.md`) with no equivalent in the on-device `FileBrowserActivity` UI, which currently only
supports long-press delete. Copy doesn't exist anywhere yet (web or device). Unzip is the interesting one: the
web file manager already ships `jszip.min.js` for **client-side** zip handling in the browser
(`GET /js/jszip.min.js`), but there's no on-device unzip-to-SD capability — however, `lib/ZipFile` and
`lib/miniz` already exist in this codebase (used for EPUB parsing, since EPUB is a zip container internally),
which meaningfully lowers the lift versus a from-scratch implementation, pending confirmation those aren't
hard-coded to EPUB's specific access pattern. **Phase 2** for on-device rename/move/copy UI; **Phase 3** for
unzip, gated on a short research spike into `lib/ZipFile`'s reusability.

### 4.12 Weather snapshot widget

Requires a live data fetch (no local sensor). To stay consistent with the existing "no background Wi-Fi" battery
posture, this should be strictly **fetch-on-demand**: opening the widget briefly joins Wi-Fi (reusing the
existing `WifiSelectionActivity` connect flow), fetches one small JSON payload, displays it, and does not keep
the radio on afterward — the same posture OPDS browsing already has today. Needs an API-key story (most weather
APIs require one); recommend the key live in the iPhone app and be proxied or handed to the device per-session,
rather than stored in device settings, so it isn't sitting in plaintext on the SD card. This is an open design
question for you to weigh in on. **Phase 2/3**, gated on the API-key decision.

### 4.13 Dictionary/thesaurus lookup

Already on the public roadmap ("Coming soon" in `README.md`), so this is prioritization, not new scope. Likely
needs an SD-card dictionary data file (e.g. StarDict-style word→definition index) and, critically, a way to
select a word **without touch input** — some new in-reader interaction (a cursor/selection mode) that this
document's Section-1 scope boundary otherwise excludes. Recommend scoping the actual word-selection interaction
together with a future in-book-reader UX pass rather than bolting it on here. **Phase 2**, flagged as needing
its own small design pass for the selection interaction specifically.

### 4.14 Offline encyclopedia reader (Kiwix ZIM support)

**Highest technical risk item in this entire plan — flagging clearly rather than downplaying it.** ZIM is a
compressed, indexed archive format; article clusters are compressed with **zstd** (older files: xz/lzma). This
codebase currently has `lib/miniz` and `lib/uzlib`, both **deflate/zlib only** — there is no zstd or lzma
decoder anywhere in the tree today. Supporting ZIM means either porting a small embedded-friendly zstd
decompress-only implementation (a real new dependency, not a config flag) or restricting support to an
old/re-encoded ZIM variant most users won't have on hand. Separately, even a trimmed "Simple English Wikipedia,
no images" ZIM file is multiple gigabytes, so random-access read latency into a multi-GB file via SdFat needs
on-device validation, not just an assumption it'll be fine. Recommend this ship as a **research spike** — get a
zstd-decode proof of concept working against one real ZIM file before committing to it as a real feature — not
as a committed Phase item yet.

## 5. Companion iPhone app

### 5.1 What already exists to build on

This is a stronger starting point than a typical "build a companion app" project: TinyRdr's web server
already exposes a fairly complete REST/WebSocket/WebDAV API (`docs/webserver-endpoints.md`) — file
browse/upload/download/rename/move/delete, a full settings get/set API shared with the on-device settings list
(`SettingsList.h` — the same source of truth both UIs read from), SD-card font install/remove, OPDS server and
Wi-Fi credential management, live device status (`GET /api/status`: version, IP, mode, RSSI, free heap, uptime,
device model), a fast WebSocket binary-upload protocol, and a UDP discovery beacon on port 8134. A native iPhone
app is largely "a proper client for an API that already exists," not a from-scratch integration.

### 5.2 The architectural constraint that shapes everything else

The web server **only runs while the device is in File Transfer or Calibre Wireless mode**
(`docs/webserver.md`) — this is deliberate, to avoid the background Wi-Fi battery drain SCOPE.md's "Active
Connectivity" concern was originally about, and there's no reason to abandon that posture just because the
project is expanding into more features. Practically, this means the iPhone app **cannot** hold a persistent
connection or receive push updates from the device. **Confirmed decision: sync-on-session.** The app is
offline-first — it works entirely from its own local store (SwiftData/CoreData) when the device isn't reachable,
and syncs whenever the device happens to be in Transfer mode and reachable on the same network (found via the
existing UDP discovery beacon, 5.3.4). This is a first-class UI concept ("Not connected — open File Transfer on
your reader to sync"), not an edge case.

#### 5.2.1 What "a session" means, concretely

A session starts when the app's discovery probe gets a reply from the device and ends when either side becomes
unreachable (device leaves Transfer mode / sleeps, or the app backgrounds/loses the connection). Everything below
happens once per session, not continuously — there's no long-poll or keep-alive, since the whole point is to let
the device drop Wi-Fi again as soon as the user is done.

#### 5.2.2 Per-data-type sync direction

Not everything should sync the same way — the device and the app disagree about which one is the "source of
truth" depending on the data type, and getting this wrong is the main way this kind of app produces silent data
loss:

- **Settings**: the device is the sole source of truth (it's a single physical device, not a multi-device
  concept). On session start, the app **pulls** the full settings list and overwrites its local cache. Any edit
  made in the app while offline should be treated as a *pending write*, applied via `POST /api/settings` at the
  start of the next session, then immediately re-pulled to confirm — never blind-trusted as already-applied
  until the device confirms it.
- **Files (books, fonts, OPDS/Wi-Fi credentials)**: device is source of truth for what's on the SD card; the app
  is a browser/uploader, not a second copy of the filesystem. No merge logic needed — every file operation is
  just an immediate API call during a session, nothing to reconcile.
- **To-do list**: this is the one genuinely bidirectional case — items can be added/completed on the device
  *and* in the app while apart, so this needs real conflict resolution, detailed in 5.2.3.

#### 5.2.3 To-do list conflict resolution

Recommend keeping this simple given it's a single-user, single-device system (no multi-device fan-out to worry
about): each `TodoStore` item gets a server-assigned `id`, a `createdAt`, and an `updatedAt` timestamp (the
`TodoStore` JSON shape from 4.8 already anticipated `id`/`createdAt`; add `updatedAt`). Sync algorithm per
session:

1. App pulls the full device list via `GET /api/todos`.
2. For items that exist on both sides: last-`updatedAt`-wins per item (not per-field — a to-do item is small
   enough that whole-item overwrite is fine and avoids partial-merge edge cases).
3. For items that exist only locally (created offline in the app): push via `POST /api/todos`.
4. For items that exist only on the device (created on-device since the last sync): pull them into the local
   store.
5. Deletions need a tombstone, not a bare removal — otherwise a delete on one side can't be distinguished from
   "never synced yet" and will just get recreated by the other side on the next sync. Recommend soft-delete
   (`deleted: true` + `updatedAt`) with tombstones pruned after both sides have confirmed the delete, rather than
   hard-removing rows from the JSON store immediately.

This is a firmware-side detail, not just an app one: `/api/todos` needs to accept/return `updatedAt` and
tombstoned items (not silently omit deleted items from the list response), or the app has no way to detect a
device-side deletion happened while it was offline.

### 5.3 Gaps to close before shipping a real app

1. **No authentication exists today** (`docs/webserver.md`: "does not require authentication... use only on
   trusted networks"). An app that reads/writes device settings and files needs at least a lightweight pairing
   scheme — e.g., a one-time pairing code shown on the device's existing Transfer-mode QR screen, exchanged for a
   token the app stores in Keychain and sends as a header on every request. This is a firmware change (new
   `/api/pair` endpoint plus token-check middleware in front of the existing routes), not just an app-side one.
2. **No `/api/todos` endpoint** — new work, described in 4.8, symmetric in shape to the existing `/api/opds`.
3. **No SD capacity in `/api/status`** — new work, described in 4.10.
4. **Discovery already works and just needs an app-side client**: the existing UDP discovery beacon (port 8134)
   is exactly what the app should use to auto-find the device on the local network or hotspot, instead of asking
   the user to type an IP address — zero firmware change required here.

### 5.4 MVP feature set (v1) — ships almost entirely against today's API

- Device status dashboard (battery/heap/Wi-Fi mode now; storage once 4.10 ships)
- File browser: browse, upload, download, rename, move, delete — 100% covered by the existing API today
- Settings editor mirroring the on-device categories, backed by `GET/POST /api/settings` — 100% covered today
- SD-card font manager (install/remove) — 100% covered by `/api/fonts*` today
- OPDS server and Wi-Fi credential management — 100% covered today

### 5.5 v1.1 — needs the new firmware work from this plan

- To-do list manager, offline-capable with sync-on-connect (needs `/api/todos`, Section 4.8)
- Pairing/auth flow (needs `/api/pair`, Section 5.3.1)

### 5.6 v2+ — depends on which apps/utilities ship device-side

- Pushing new content packs (Nonogram puzzle packs, Twee interactive-fiction files) to the SD card via the
  **existing** upload API — no new endpoint needed, since these just land as files in a known folder.
- A companion "quick note" capture in the app that drops a `.txt` file into `/Notes/` for the device's Notepad
  viewer (4.9) to pick up — again, no new endpoint needed, just an app-side convenience wrapper around the
  existing upload API.

### 5.7 Tech notes (not prescriptive)

SwiftUI with a local persistence layer (SwiftData or CoreData) for offline-capable settings/todo caches;
Bonjour or raw UDP for discovery against the existing beacon; `URLSession` against the existing REST API; a
WebSocket client implementing the already-documented fast-upload protocol. Nothing here requires unusual
technology choices — the constraint is the sync model (5.2), not the client stack.

## 6. Phased rollout

**Phase 1 — Low-risk UI/UX polish + cheapest new apps + firmware API groundwork**
From Section 2: promote the 3-cover shelf to default, add settings tab icons, group Sleep Screen settings, split
the Reader settings tab. From Section 4: Sudoku (establishes the board-input pattern), Calculator, Unit
Converter, To-Do list (device-side), Notepad-viewer discoverability, Diagnostics panel (including the new SD
capacity API). From Section 5: `/api/todos` and SD-capacity additions land alongside their device-side features
so the app's MVP (5.4) and v1.1 (5.5) to-do sync can start immediately after. None of this phase requires new
compression libraries, new interpreters, or new input paradigms — everything reuses existing components.

**Phase 2 — Library navigation + card/puzzle games + app v1**
From Section 2: sort-by setting, cached-metadata subtitles, jump-to-letter long-press. From Section 4: Klondike
and the shared Cards module, Nonograms with SD-card puzzle packs, the Twee-style interactive-fiction reader,
on-device rename/move/copy, dictionary lookup (with its own word-selection design pass), and — gated on an
API-key decision — the weather widget. From Section 5: ship the companion app v1 MVP (5.4) against the API
surface that exists by end of Phase 1, then v1.1 (5.5) once `/api/pair` lands.

**Phase 3 — Visual system + complex games + research spikes**
From Section 2: extended icon set, Compact theme variant, thumbnail reuse in Recents. From Section 4: Five
Crowns (depends on the Cards module from Phase 2), on-device unzip (research spike into `lib/ZipFile` reuse),
and the Z-machine interpreter and Kiwix/ZIM support — both explicitly gated on a research spike proving
technical feasibility (RAM footprint for Z-machine, zstd decode for ZIM) before being treated as committed
work. From Section 5: app v2+ content-pack pushing and quick-capture notes.

## 7. Verification checklist

- `pio run -t clean && pio run` — 0 errors/warnings after each phase.
- `pio check` + clang-format pass per CLAUDE.md.
- Manual device pass in all 4 orientations for any changed screen (home, settings, file browser), per the
  project's human-tester checklist in CLAUDE.md.
- Heap check (`ESP.getFreeHeap()`) before/after opening the file browser with a large (200+ file) folder once
  sort/subtitle changes land, to confirm no regression from the new sort pass or cache lookups.
- Confirm new/changed settings persist correctly (`SETTINGS.saveToFile()`) and appear in the web settings JSON
  API (`SettingsList.h` is shared between device UI and web API — any new setting needs a `key` for the API to
  pick it up).
- Re-check `docs/comparison.md` screenshots after Phase 1 to see whether new comparison shots against XTOS are
  worth capturing for the README.
- Before any Section 4 game ships, confirm its save-slot file and in-memory board state together stay well
  within the 380KB ceiling alongside the reader engine's own footprint — measure with `ESP.getFreeHeap()` at
  worst-case board state (e.g. a fully-dealt Five Crowns hand), not just at rest.
- Before shipping `/api/pair` (Section 5.3.1), get an explicit security read on the token scheme (storage,
  expiry, revocation) — this is the first time the device will hold any form of credential-gated remote-control
  surface, which is a meaningfully different risk profile than today's open-by-design LAN-only server.
- Before committing to Z-machine (4.4) or Kiwix/ZIM (4.14) as real features, require a working proof-of-concept
  (one real story file / one real ZIM file, decoded on-device) as the exit criteria for their research spikes —
  don't let either slide into "committed" status without that evidence.
- Update `SCOPE.md` and `CLAUDE.md`'s Philosophy line (per the Scope note at the top of this document) before
  merging any Section 4/5 work, so the project's own contributor docs stop contradicting its direction.
