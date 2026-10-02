# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Summary]

StormByte Logger is the stream-logging module of the StormByte C++ suite.

It depends directly on [StormByte Base 2.0.0](https://github.com/StormByte-Suite/StormByte/releases/tag/2.0.0) or newer. This repository is not Base, Buffer, Config, Crypto, Database, Multimedia, Network or System.

Public headers under `StormByte/logger/` cover `Log`, `ThreadedLog`, header formats (`%L` `%T` `%i` `%c` `%g`), hierarchical components and `Scope` facades, groups, ANSI colors, temporary formats, human-readable numbers and bytes, redaction of text and numbers, hex dumps (`hex` / `nohex`), and binary payloads (`std::span<const std::byte>`, default Base64). Owned text that crosses the logger DLL boundary uses `StormByte::Safe::String` / `WString` and `StormByte::Safe::CString` / `WCString`; `StormByte::Size`, `StormByte::ByteSize` and `StormByte::BinaryData` are accepted as payloads. Views and caller-owned `std::string` are consumed synchronously.

From 2.0.0, original Logger sources are dual-licensed: GNU Lesser General Public License v3.0 or later, or a commercial license from the copyright holder. That change does not cover other StormByte modules or third-party material under `thirdparty/` (including bundled StormByte Base).

If you landed here from a release link and have not read the tree:

- What this module is, how to build it, and short examples: [README.md](https://github.com/StormBytePP/StormByte-Logger/blob/master/README.md)
- License: dual license LGPL-3.0-or-later or commercial, [LICENSE](https://github.com/StormBytePP/StormByte-Logger/blob/master/LICENSE)

## [Unreleased]

### Fixed

- Qualify the `ThrottleSpec::Level` member type so public headers compile with GCC 14+ without `-Wchanges-meaning`; the `Level` field and designated-initializer API remain unchanged.

[Unreleased]: https://github.com/StormBytePP/StormByte-Logger/compare/2.0.0...HEAD

## [2.0.0] - 2026-10-02

### Added

- `operator<<` on `Log` and `ThreadedLog` for `StormByte::Safe::String`, `StormByte::Safe::WString`, `StormByte::Safe::CString`, `StormByte::Safe::WCString` and `StormByte::Size`. Conversion and copy run only when `WillWrite()` is true.
- `component`, `group` and `push_format` accept `std::string_view` (literals) in the caller and `StormByte::Safe::String` by value at the DLL boundary. Manipulator payloads use Base-owned text.
- Private `StormByte::Logger::Detail` human-readable number and IEC byte formatting (the manipulator API is unchanged; this logic no longer lives in String).
- Tests for owned-text payloads, filtered drop of owned text, `Size`, and ill-formed wide input substituted as U+FFFD.
- `operator<<` for `StormByte::BinaryData` and `StormByte::ByteSize` on `Log` and `ThreadedLog`. Conversion runs only when `WillWrite()` is true.
- `operator<<` on `StormByte::Safe::Shared` and `StormByte::Safe::Unique` of `Log` or `ThreadedLog`, same sugar as `std::shared_ptr`: `log << "Hola"` without a dereference.
- `~Log` and `~ThreadedLog` are defined in the library, so the backend and the line lock are released inside the DLL. Copy and move assignment are too.

### Changed

- Logger's text API and build metadata now use StormByte Base 2.0.0 directly; owned text and clone owners use Base's `StormByte::Safe` types.
- Shared vs static follows CMake `BUILD_SHARED_LIBS` (declared in the project root, default ON). There is no `STORMBYTE_LOGGER_SHARED` CMake option. When the library is shared, the compile definition `STORMBYTE_LOGGER_SHARED` is still set so `visibility.h` can distinguish `dllexport` / `dllimport` / static. CI passes `-DBUILD_SHARED_LIBS=ON`. Vendored StormByte Base follows the same `BUILD_SHARED_LIBS` mode.
- **Breaking:** Logger vendors [StormByte Base 2.0.0](https://github.com/StormByte-Suite/StormByte/releases/tag/2.0.0) directly and exposes Base's `StormByte::Safe` owned-text and ownership types in its public API.
- **Breaking:** public streaming no longer treats `std::string` as an owned cross-module type. Use `StormByte::Safe::String` / `StormByte::Safe::CString` when the buffer is owned by another module; `string_view` remains valid for caller-owned data. A caller-owned `std::string` is streamed inline as a view and copied synchronously.
- `LevelToString` returns `const char*` (a string literal) instead of `std::string`. Call sites that store it in a `std::string` are unchanged.
- The private backend is `Engine` (`m_engine`), in `engine.hxx` / `engine.cxx`. It was `Implementation`.
- Filtered payloads (`WillWrite()` false) return before any conversion or copy. A filtered or throttled `std::endl` does not touch the stream. An emitted line still forwards real `std::endl`, so the text is flushed at that instant and the next payload prints a new header. `std::endl` / `std::flush` / `std::ends` are recognized without probing a string stream.
- **Breaking (boundary):** `Log` and `ThreadedLog` no longer write an `std::ostream` from inside the DLL. Construction from an `std::ostream` still works and still requires the stream to outlive the logger. Bytes and manipulators (`std::endl`) are applied by `OStreamWrite` / `OStreamManip` in the module that constructed the logger. A protected constructor takes those callbacks directly.
- **Breaking:** `ThrottleSpec::Component` and `ThrottleSpec::Group` are `std::optional<StormByte::Safe::String>`.
- **Breaking:** ill-formed wide text is written as U+FFFD (`EF BF BD`). Logger does not throw `StormByte::UTF8Error` on that path.
- `Log::m_scope_path` is `StormByte::Safe::String` so a copied or derived `Log` does not carry `std::string` across a DLL boundary.
- Numeric and narrow-text payloads share `Log::WriteValue`; `ThreadedLog` only overrides `BeginPayload` for those payloads.
- `StormByte::Base64Encode` returns `StormByte::Safe::CString` (Base 2.0.0). Binary-span default output is unchanged for the reader.
- **License:** original Logger sources are dual-licensed LGPL-3.0-or-later or commercial. Third-party trees under `thirdparty/` keep their own licenses. Neither license grants patent rights.
- **Breaking:** with `hex(N)` active, `std::span<const std::byte>`, `std::vector<std::byte>` and `StormByte::BinaryData` are formatted with `BinaryData::HexDump(N)`. Text, wide text and numbers still use the `0xAA` dump. Without `hex`, binary payloads stay Base64.
- **Breaking:** `Logger::Exception` takes `Exception::Path{"Logger"}`. `what()` is `StormByte.Logger: message`. `Component` is gone. `ThrottleError` is a leaf and adds no segment. Destructors are defined in this module.
- **Breaking:** `Log` is `StormByte::Safe::Clonable<Log>`, so `Clone`, `Move` and `Scope` return `StormByte::Safe::Shared<Log>` allocated on Base's heap. `std::shared_ptr<Log>` is no longer a `PointerType`. `Safe::Shared` still converts to `std::shared_ptr<Log>` and keeps Base's deleter. The same applies to `ThreadedLog`.

[2.0.0]: https://github.com/StormBytePP/StormByte-Logger/compare/1.2.0...2.0.0

## [1.2.0] - 2026-09-17

### Added

- `Log::Enabled(Level)`: print-floor query (Warning/Error/Fatal always true). Does not open a line and does not consult throttle.
- `operator<<(std::string_view)` and `operator<<(std::wstring_view)` on `Log` and `ThreadedLog`, with the same filtered early-out as other payloads. `std::string` / `std::wstring` convert to the views.
- `hex` / `hex(N)` / `nohex`: dump subsequent payloads as spaced `0xAA` bytes. `N` is bytes per row (default 16); wrap uses a raw newline without a new header and without ending the logical line. `hex(0)` is `nohex`. Applies to text, wide text (after UTF-8), numbers (`42` → bytes of `"42"`) and binary spans. Hex runs before redaction.
- `operator<<(std::span<const std::byte>)` on `Log` and `ThreadedLog`. Default output is Base64 (`StormByte::Base64Encode`). `std::vector<std::byte>` converts to the span. With `hex` the dump is the raw bytes, not the Base64 text. Empty spans emit an empty payload. `ThreadedLog` formats the payload before taking the line lock (`FormatBinary` + `WritePrepared`).
- Hierarchical components: `component("A")` pushes a thread-local segment; nested pushes join with `/` for `%c` and config lookup. `pop_component` pops one segment. `reset_component` still clears the stack. `component("")` does not push.
- `Log::Scope(path)` returns a `std::shared_ptr<Log>` facade with a sticky component path. Nested `Scope` joins relative to the parent. Never returns `nullptr`. Facades share the backend and, on `ThreadedLog`, the line lock. A Scope line uses the sticky path, not the TLS stack.
- Protected `Clonable<Log, std::shared_ptr<Log>>` on `Log` (`Clone` / `Move`) so `Scope` can copy the facade without exposing cloning in the public API.
- Pointer `operator<<` accepts `shared_ptr` / `unique_ptr` whose element type `Type::DerivedFrom` `Log` (`Log` and `ThreadedLog`).

### Changed

- Dropped the dedicated `operator<<(const std::string&)` / `operator<<(const std::wstring&)` overloads. Call sites that pass `std::string` still compile.
- `ThreadedLog` wide payloads encode with `String::UTF8Encode(std::wstring_view)` before taking the line lock.
- Bundled StormByte Base is [1.2.0](https://github.com/StormBytePP/StormByte/releases/tag/1.2.0). Using `UTF8Encode(std::wstring_view)` and `Base64Encode(std::span<const std::byte>)` requires Base 1.2.0 or newer.
- **Breaking:** `no_redact` is now `noredact`, same shape as `nocolor`, `nohex` and `nohumanreadable`. There is no compatibility alias.
- **Breaking:** `component("name")` pushes onto the thread-local stack instead of replacing the current name. Sibling switches must `reset_component` or `pop_component` first, otherwise `component("Media")` then `component("Other")` becomes `Media/Other`.
- Format and color lookup use the longest matching component-path prefix, then the general setting. A child format may introduce `%c` / `%g` even when the parent format does not.
- Throttle still picks the most specific rule; component matching is by path prefix and longer paths win. `Format` / `Color` / `Throttle` without a component argument bind to the current facade path (`Scope` leaf, or global on the root logger).
- README documents the component stack, `Scope`, hex dumps and binary payloads.

### Fixed

- `~Implementation` no longer first-touches thread-local line state (Valgrind still-reachable TLS at exit).
- Inherited throttle rules keep one spec per prefix but one counter set per emitting path. A parent `Scope("Multimedia").Throttle(Debug, …)` no longer lets `Encoder` consume `watermark` tokens or print `watermark dropped N messages` when that leaf had not emitted anything at that level. Drop summaries stay on the leaf that dropped. Tests cover `Log` and `ThreadedLog`.

[1.2.0]: https://github.com/StormBytePP/StormByte-Logger/compare/1.1.1...1.2.0

## [1.1.1] - 2026-09-15

### Changed

- Bundled StormByte Base is [1.1.1](https://github.com/StormBytePP/StormByte/releases/tag/1.1.1). The declared requirement stays [1.1.0](https://github.com/StormBytePP/StormByte/releases/tag/1.1.0) or newer.

[1.1.1]: https://github.com/StormBytePP/StormByte-Logger/compare/1.1.0...1.1.1

## [1.1.0] - 2026-09-13

### Added

- `group("name")` and `%g`; the group is cleared by newline and is not rendered in the body when `%g` is absent.
- Sticky per-thread `component("name")`, `reset_component` and `%c`. `component("")` selects the root without throwing, while `reset_component` is the canonical reset. The state belongs to the thread, not the `Log`; two `Log` instances on one thread observe the same component.
- ANSI colors by `Level` and component override, with `color`, `color(Color::X)` and `nocolor`. Color output is disabled by default.
- Persistent general and component formats, plus nested `push_format` / `pop_format`; an empty `pop_format` is idempotent.
- Expanded tests for colors, temporary formats, groups, components, threads, Unicode and lock recovery.
- Added configurable line throttle policies (`Drop`, `Sample`, `Window`) with component/level/group rule precedence and drop summaries.
- Added `FlushThrottle()` and selective `FlushThrottle(spec)` for pending drop summaries at job boundaries.

### Changed

- Requires [StormByte Base 1.1.0](https://github.com/StormBytePP/StormByte/releases/tag/1.1.0) or newer (`Exception` with `Component`). Logger did not use the two-string `Exception` constructor.
- `Warning`, `Error` and `Fatal` are always emitted, even when the configured floor is higher. This is intentional.
- Expanded `ThreadedLog` coverage for the filtered hot path, wide conversion before locking, and recovery when Unicode conversion fails.

### Fixed

- `ThreadedLog::FlushThrottle` preserves ownership of an already-held line lock.
- Format changes now reset the active throttle line snapshot after closing an open line.
- `endl` releases the `ThreadedLog` line lock even if `WillWrite()` changes midway through a line.
- Wide-string logging converts before acquiring the lock; a Unicode conversion error no longer terminates the logger from an internal `noexcept` path.
- **ThreadedLog::Write(Level)** — two threads sharing a `ThreadedLog` raced on `Implementation`’s `optional<Color>` (`m_content_color.reset()` at the start of a line). The level token now takes the line lock like other `Write` overloads; payload and `endl` already did.

[1.1.0]: https://github.com/StormBytePP/StormByte-Logger/releases/tag/1.1.0

## [1.0.0] - 2026-09-05

Initial public release of StormByte Logger.

### Added

- `Log` streaming facade with `operator<<`
- Level filter: `LowLevel`, `Debug`, `Warning`, `Notice`, `Info`, `Error`, `Fatal`
- Header format: `%L`, `%T`, `%i`, `%%`
- Manipulators: `humanreadable_number`, `humanreadable_bytes`, `nohumanreadable`
- Redaction: `redact` / `redact(N)` keep last N; `redact_first(N)` keep first N; `no_redact`; applies to text and numbers
- `ThreadedLog`: one lock per logical line; filtered messages do not take the lock
- Uses [StormByte Base 1.0.0](https://github.com/StormBytePP/StormByte/releases/tag/1.0.0) (`String`, `ThreadLock`, platform)
- Unit tests (filter, threads, redaction)
- Project version read from the `VERSION` file
- CMake 3.28 floor

### Notes

- `Log` is not thread-safe. Use `ThreadedLog` when several threads share one logger.
- Messages below the print level return early.
- Needs a C++26 compiler and [StormByte Base 1.0.0](https://github.com/StormBytePP/StormByte/releases/tag/1.0.0).

[1.0.0]: https://github.com/StormBytePP/StormByte-Logger/releases/tag/1.0.0
