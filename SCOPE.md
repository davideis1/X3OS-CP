# Project Vision & Scope: TinyRdr

The goal of TinyRdr is to create an efficient, open-source e-reader/Swiss Army knife for the Xteink X4/X3. We
believe a great dedicated device can do more than one thing well — reading first, but also the small everyday utilities
and games that make sense on a low-power, always-with-you e-ink device — as long as every addition is held to the same
bar of stability, legibility, and respect for the hardware's limits.

## 1. Core Mission

To provide a lightweight, high-performance firmware that maximizes the potential of the X3/X4, prioritizing legibility,
usability, and reliability first, while embracing a broader set of tools, games, and utilities that fit naturally on a
button-only, single-core, no-touch e-ink device. "Swiss Army knife" here means *curated and disciplined*, not
*unlimited*: every feature — reading-related or not — is still judged against the hardware constraints in `CLAUDE.md`
(380KB RAM ceiling, single 48KB framebuffer, no PSRAM, no true partial e-ink refresh) before it's accepted.

## 2. Scope

### In-Scope

*These are features that directly improve the primary purpose of the device, or add genuine utility without
compromising its stability, battery life, or the reading experience.*

* **User Experience:** E.g. User-friendly interfaces, and interactions, both inside the reader and navigating the
  firmware. This includes things like button mapping, book loading, and book navigation like bookmarks.
* **Document Rendering:** E.g. Support for rendering documents (primarily EPUB) and improvements to the rendering
  engine.
* **Format Optimization:** E.g. Efficiently parsing EPUB (CSS/Images) and other documents within the device's
  capabilities.
* **Typography & Legibility:** E.g. Custom font support, hyphenation engines, and adjustable line spacing.
* **E-Ink Driver Refinement:** E.g. Reducing full-screen flashes (ghosting management) and improving general rendering.
* **Library Management:** E.g. Simple, intuitive ways to organize and navigate a collection of books.
* **Local Transfer:** E.g. Simple, "pull" based book loading via a basic web-server or public and widely-used standards.
* **Language Support:** E.g. Support for multiple languages both in the reader and in the interfaces.
* **Reference Tools:** E.g. Local dictionary lookup, offline encyclopedia lookup. Providing quick, offline information
  to enhance comprehension without breaking focus.
* **Turn-Based Games & Puzzles:** E.g. Sudoku, Klondike, Nonograms, chess/checkers-style games, choice-based
  interactive fiction. Must be turn-based or otherwise tolerant of whole-screen e-ink refresh — this hardware has no
  partial/regional refresh, so anything needing fast, continuous redraws (action games, real-time anything) does not
  belong here regardless of RAM budget.
* **Everyday Utilities:** E.g. Calculator, unit converter, to-do list/task manager, plain-text notepad viewing, a
  battery/system diagnostics panel. These should be lightweight, turn/input-driven (no background processing), and
  built from existing UI components (lists, popups, keyboard entry) wherever possible rather than new interaction
  paradigms.
* **On-Demand Connectivity:** E.g. weather snapshot, OPDS browsing, OTA updates, dictionary/encyclopedia content
  delivery. Wi-Fi is joined deliberately for a bounded task and dropped afterward — see **Background Connectivity**
  below for the line this doesn't cross.
* **Companion Apps:** E.g. a mobile app for managing settings, files, and data (such as to-do lists) on the device.
  Given the device's own no-background-Wi-Fi posture, companion apps should be **sync-on-session**, not dependent on
  a persistent connection to the device — see `docs/ui-ux-overhaul-plan.md` for the current design.
* **Clock Display (device dependent):**

| Device | Scope |
| -- | -- |
| X3 | The X3 uses a dedicated DS3231 RTC, which maintains accurate time across sleep cycles and can be treated as a reliable wall clock. |
| X4 | The X4 relies on the ESP32-C3's internal RTC, which drifts significantly during deep sleep. NTP sync could correct this, with an appropriate user experience around connecting to the internet on wake or on demand. This causes some tension with the **Background Connectivity** section below, so please open a discussion about this UX if it's a feature you would find useful. |

### Out-of-Scope

*These items are rejected because they compromise the device's stability, battery life, or hardware reality — not
because they're "too fun" or "not reading." A feature that fails one of these tests is out of scope regardless of how
useful it sounds.*

* **Background Connectivity:** No always-on background Wi-Fi tasks — no polling RSS feeds, push notifications, or
  anything that keeps the radio alive outside a bounded, user-initiated session. This is a battery and single-core-CPU
  constraint, not a philosophical one: on-demand connectivity (join Wi-Fi, do one task, drop it) is explicitly
  in-scope above.
* **General Web Browsing:** Full web browsers are out — the rendering engine, memory footprint, and interaction model
  required are fundamentally incompatible with this hardware, not just "distracting."
* **Real-Time / Fast-Redraw Games:** Anything that assumes smooth continuous animation (action games, physics,
  real-time multiplayer) — the display hardware has no partial/regional refresh, only whole-screen `FULL_REFRESH` /
  `HALF_REFRESH` / `FAST_REFRESH` modes (`lib/hal/HalDisplay.h`), so this is a hardware wall, not a taste call.
* **Media Playback:** No audio players or audiobook playback. The X3/X4 have no DAC or speaker — this is a hardware
  limitation, not a scope decision; see the Technically Unsupported section below.
* **Freeform/Handwritten Annotation:** No stylus-based handwriting or drawing input. There's no touch or stylus
  hardware on these devices — structured, button/keyboard-driven note-taking (to-do items, plain-text notes) is
  in-scope; open-ended handwritten annotation is not physically possible on this hardware.

### In-scope — Technically Unsupported

*These features align with TinyRdr's goals but are impractical on the current hardware, or need real feasibility
validation before they can be committed to as shipped features.*

* **PDF Rendering:** PDFs are fixed-layout documents, so rendering them requires displaying pages as images rather
  than reflowable text — resulting in constant panning and zooming that makes for a poor reading experience on e-ink.
* **Audio/Media Playback:** No DAC or speaker exists on X3/X4 hardware — this would require a hardware revision, not
  a firmware change.
* **Full Z-Machine Interactive Fiction:** A real Z-machine interpreter is a nontrivial VM with per-title memory
  requirements that need validation against the 380KB ceiling before committing — see
  `docs/ui-ux-overhaul-plan.md` §4.4 for the current research-spike plan. A simpler choice-based-narrative reader
  (Twee-style) is in-scope and does not have this risk.
* **Offline Encyclopedia (Kiwix/ZIM):** ZIM archives are typically zstd/lzma-compressed, and this codebase currently
  only has deflate/zlib decompression (`lib/miniz`, `lib/uzlib`). This needs a decoder proof-of-concept before it's a
  committed feature — see `docs/ui-ux-overhaul-plan.md` §4.14.

## 3. Idea Evaluation

TinyRdr is designed to be a lightweight, reliable, and performant device first — reading is still the
primary use case, and every new feature (reading-related or not) needs to earn its place against the hardware
constraints in `CLAUDE.md`. As guiding questions: does this respect the RAM/flash/battery budget, does it use
button-only/no-touch interaction patterns consistently with the rest of the firmware, and does it avoid dragging in
background connectivity or hardware the device doesn't have? A feature doesn't need to *be* reading to be in scope,
but it does need to fit the device TinyRdr actually is.

> **Note to Contributors:** If you are unsure if your idea fits the scope, please open a **Discussion** before you start
> coding!
