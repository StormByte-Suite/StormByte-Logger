# StormByte

![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20macOS-lightgrey)
![C++26](https://img.shields.io/badge/C%2B%2B-26-00599C?logo=c%2B%2B&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.28+-064F8C?logo=cmake&logoColor=white)
![License: LGPL v3 or commercial](https://img.shields.io/badge/License-LGPL_v3_or_commercial-blue.svg)
[![CI](https://github.com/StormByte-Suite/StormByte-Logger/actions/workflows/ci.yml/badge.svg)](https://github.com/StormByte-Suite/StormByte-Logger/actions/workflows/ci.yml)
[![Sponsor](https://img.shields.io/badge/Sponsor-StormBytePP-ea4aaa?logo=githubsponsors)](https://github.com/sponsors/StormBytePP)

This repository is **StormByte Logger**: stream logging for the StormByte C++ suite.

It depends directly on [StormByte Base 2.0.0](https://github.com/StormByte-Suite/StormByte/releases/tag/2.0.0) or newer. Public headers live under `StormByte/logger/` and cover `Log`, `ThreadedLog`, header formats, hierarchical components, `Scope` facades, groups, colors, temporary formats, human-readable numbers, redaction, hex dumps, binary payloads, and owned text types that can cross a DLL / `.so` boundary (`StormByte::Safe::String` / `WString`, `StormByte::Safe::CString` / `WCString`, `StormByte::Size`).

The suite is split on purpose. Base, Buffer, Config, Crypto, Database, Multimedia, Network and System are **other repositories**. This one does not implement them.

## What this module does

- **Log** — `operator<<` facade with a minimum print `Level`. Copies and `Scope` facades share the same backend.
- **Levels** — ordered from least to most severe: `LowLevel`, `Debug`, `Warning`, `Notice`, `Info`, `Error`, `Fatal`. `Warning`, `Error` and `Fatal` are always emitted.
- **Headers** — `%L` level, `%T` timestamp, `%i` thread id, `%c` component path, `%g` group, `%%` literal `%`.
- **Components** — thread-local stack. `component("A")` pushes a segment; nested calls join with `/` (`Multimedia/Decoder`). `pop_component` pops one segment. `reset_component` clears the stack. `component("")` does not push.
- **Scope** — `log.Scope("Multimedia/Decoder")` returns a `StormByte::Safe::Shared<Log>` facade with a sticky path. Nested `Scope("Encoder")` joins relative to the parent. Config methods without a component argument bind to that sticky path (root facade = global). The facade shares the backend and, on `ThreadedLog`, the line lock. `Safe::Shared` converts to `std::shared_ptr<Log>` and keeps Base's deleter.
- **Groups** — line-scoped `group("name")` labels, cleared by a newline.
- **Colors** — ANSI colors configured by level or component path (longest prefix wins). `color`, `color(Color::X)` and `nocolor` content manipulators. Disabled by default.
- **Formats** — persistent general / component-path formats (longest prefix wins) plus nested temporary `push_format("...")` / `pop_format`.
- **Human-readable** — `humanreadable_number`, `humanreadable_bytes`, `nohumanreadable`. Formatting lives in Logger (`Detail`).
- **Redaction** — `redact` / `redact(N)` keep last N, `redact_first(N)` keep first N, `noredact`.
- **Hex** — `hex` / `hex(N)` dumps text payloads as `0xAA` with N bytes per row (default 16). `nohex` restores the default. Applies to every subsequent text payload, including numbers (text bytes, not numeric hex). Binary payloads use `HexDump(N)` instead.
- **Binary** — `std::span<const std::byte>`, `std::vector<std::byte>` and `StormByte::BinaryData` print as Base64 by default (`StormByte::Base64Encode` returns `CString`). With `hex(N)` the dump is `BinaryData::HexDump(N)`.
- **Owned text** — `String`, `WString`, `CString`, `WCString`, `Size` and `ByteSize` have explicit `operator<<`. Conversion runs only when the line will be written (`WillWrite()`). Ill-formed wide text is emitted as U+FFFD.
- **ThreadedLog** — one lock per logical line. Binary encoding (Base64 / hex) and wide-to-UTF-8 run before the lock. Filtered writes do not take the lock.
- **Not thread-safe** — plain `Log` is single-threaded. Share a logger across threads only via `ThreadedLog`.

## The rest of the suite

| Module | Role | API |
| --- | --- | --- |
| [Base](https://github.com/StormByte-Suite/StormByte) | Exceptions, Expected, serialization, UUID, concepts, safe text and pointers | [/StormByte](https://suite.stormbyte.org/StormByte) |
| [Buffer](https://github.com/StormByte-Suite/StormByte-Buffer) | FIFO, SharedFIFO, Ring, Producer/Consumer and multi-stage pipelines | [/StormByte-Buffer](https://suite.stormbyte.org/StormByte-Buffer) |
| [Config](https://github.com/StormByte-Suite/StormByte-Config) | Human-readable text and versioned binary documents (groups, lists, raw bytes) | [/StormByte-Config](https://suite.stormbyte.org/StormByte-Config) |
| [Crypto](https://github.com/StormByte-Suite/StormByte-Crypto) | Hash, compress, encrypt, sign and key agreement — Crypto++ never leaves the private tree | [/StormByte-Crypto](https://suite.stormbyte.org/StormByte-Crypto) |
| [Database](https://github.com/StormByte-Suite/StormByte-Database) | One API over SQLite, PostgreSQL and MariaDB | [/StormByte-Database](https://suite.stormbyte.org/StormByte-Database) |
| [Multimedia](https://github.com/StormByte-Suite/StormByte-Multimedia) | Decode, encode and containers without raw FFmpeg types; codecs enabled only if present | [/StormByte-Multimedia](https://suite.stormbyte.org/StormByte-Multimedia) |
| **Logger** | This repository | [/StormByte-Logger](https://suite.stormbyte.org/StormByte-Logger) |
| [Network](https://github.com/StormByte-Suite/StormByte-Network) | Framed packets, Client/Server, IPv4/IPv6 TCP and Buffer pipelines (compress/encrypt) | [/StormByte-Network](https://suite.stormbyte.org/StormByte-Network) |
| [System](https://github.com/StormByte-Suite/StormByte-System) | Processes, pipes and environment variables across Linux, Windows and macOS | [/StormByte-System](https://suite.stormbyte.org/StormByte-System) |

## Table of Contents

- [What this module does](#what-this-module-does)
- [The rest of the suite](#the-rest-of-the-suite)
- [Documentation](#documentation)
- [Levels](#levels)
- [Headers](#headers)
- [Installation](#installation)
- [Usage](#usage)
  - [Log and ThreadedLog](#log-and-threadedlog)
  - [A line](#a-line)
  - [Sharing a logger](#sharing-a-logger)
  - [Owned text and Size](#owned-text-and-size)
  - [Human-readable numbers](#human-readable-numbers)
  - [Redaction](#redaction)
  - [Hex and binary payloads](#hex-and-binary-payloads)
  - [Colors](#colors)
  - [Temporary formats](#temporary-formats)
  - [Groups and components](#groups-and-components)
  - [Scope](#scope)
  - [Throttle](#throttle)
  - [Use from other suite modules](#use-from-other-suite-modules)
- [ThreadedLog contract](#threadedlog-contract)
- [Contributing](#contributing)
- [License](#license)

## Documentation

- This README: how to build, levels, headers, streaming contract, examples.
- Doxygen class reference (headers under `StormByte/logger/`): [https://suite.stormbyte.org/StormByte-Logger/](https://suite.stormbyte.org/StormByte-Logger/).

Other modules that take a `std::shared_ptr<StormByte::Logger::Log>` still compile: `Scope` returns `StormByte::Safe::Shared<Log>`, which converts to that `shared_ptr` and still frees on Base's heap. The print floor is chosen by the **application**, not by the library that logs.

## Levels

`StormByte::Logger::Level` is the only severity type. It is used twice:

1. **Print floor** — constructor argument. Ordinary levels below that value are filtered. `Warning`, `Error` and `Fatal` are always written.
2. **Message level** — first `operator<<(Level)` on a line. That sets the level of everything until the next `Level` or the end of the line.

Order is **least severe → most severe** (this is not syslog):

| Level | Value | When to use |
| --- | --- | --- |
| `LowLevel` | 0 | High-volume diagnostics: per-unit PTS/DTS, wait/wake, hopper chatter. Expect slowness if the floor is this low. |
| `Debug` | 1 | Useful but quieter: binds, reserves, work summaries, codec open. |
| `Warning` | 2 | Recoverable problems that did not fail the job. |
| `Notice` | 3 | Significant *normal* events: created, opened path, eof, closed. Keep this quiet. |
| `Info` | 4 | Job-level information, such as a completed operation. |
| `Error` | 5 | Error conditions. |
| `Fatal` | 6 | Unrecoverable errors. |

A message is emitted when `message_level >= print_floor`, or when its level is `Warning`, `Error` or `Fatal`.

Examples:

- Floor `Info` prints `Info`, `Warning`, `Error` and `Fatal`. It does **not** print `Notice`, `Debug` or `LowLevel`.
- Floor `Debug` prints `Debug`, `Warning`, `Error` and `Fatal`. It does **not** print `Notice` or `LowLevel`.
- Floor `LowLevel` prints everything.

`LevelToString(Level)` returns the short name used in `%L` (`"LowLevel"`, `"Debug"`, `"Notice"`, …). The header pads the name to 8 characters.

Payload `operator<<` for ordinary filtered levels returns immediately below the floor. `Warning`, `Error` and `Fatal` remain enabled. Setting a `Level`, applying a manipulator, or writing `std::endl` is still forwarded so logger state stays consistent.

`Enabled(Level)` asks whether that level would pass the print floor (including the Warning/Error/Fatal exception). It does not open a line and does not consult throttle. Use it to skip building a payload. Throttle still runs later on `PrepareLine` if you do write.

```cpp
if (log.Enabled(Level::Debug)) {
	log << Level::Debug << std::string_view{detail} << std::endl;
}
```

## Headers

Third constructor argument. Specifiers:

| Token | Meaning |
| --- | --- |
| `%L` | Current message level, padded to 8 characters |
| `%T` | Local timestamp `dd/mm/YYYY HH:MM:SS` |
| `%i` | `std::this_thread::get_id()` |
| `%c` | Component path for the current line (stack join or Scope sticky path) |
| `%g` | Group for the current line, if any |
| `%%` | A literal `%` |

Default format is `"[%L] %T"`. A typical multi-thread format is `"[%L] %T"` or `"[%L %i] %T"`. Components and groups are opt-in: use `"[%L] %T %c %g"` when you want them.

The logger writes the header once per line, then the payload, then the newline manipulator.

A component format override can introduce `%c` / `%g` even when the general format does not contain them. Longest matching path wins; then the general format.

## Installation

Needs a C++26 compiler, CMake 3.28 or newer, and [StormByte Base 2.0.0](https://github.com/StormByte-Suite/StormByte/releases/tag/2.0.0) or newer.

```sh
git clone --recursive https://github.com/StormByte-Suite/StormByte-Logger.git
cd StormByte-Logger
cmake -S . -B build
cmake --build build
```

Shared vs static follows CMake `BUILD_SHARED_LIBS` (declared in `lib/`, default ON). A plain configure builds the shared library. `-DBUILD_SHARED_LIBS=OFF` builds a static archive; on Windows the headers then do not use `dllimport`. Vendored StormByte Base follows the same mode.

A shared build keeps this library as its own `.so` / `.dll`. Under the LGPL that is usually the simpler way to ship: the user can replace that file. A static archive is folded into your binary. The LGPL still applies to this code; you must give the recipient a way to relink your product with a different build of this library. If that does not fit how you distribute the final product, a commercial license is available from the copyright holder (see [License](#license)).

Link `StormByte-Logger` (and Base). Include path: the public install prefix, headers as `#include <StormByte/logger/….hxx>`.

## Usage

Headers are `#include <StormByte/logger/….hxx>`. Namespace root is `StormByte::Logger`.

### Log and ThreadedLog

```cpp
#include <StormByte/logger/log.hxx>
#include <StormByte/logger/threaded_log.hxx>
#include <StormByte/logger/manipulators.hxx>
#include <iostream>
#include <memory>

using namespace StormByte::Logger;

Log log(std::cout, Level::Info, "[%L] %T");
log << Level::Info << "hello" << std::endl;

auto tlog = std::make_shared<ThreadedLog>(std::cout, Level::Debug, "[%L] %T");
tlog << Level::Notice << "opened source /tmp/in.mkv" << std::endl;
tlog << Level::Debug  << "mapped Video 0 -> order 0" << std::endl;
```

`operator<<` unpacks `std::shared_ptr` / `std::unique_ptr` and `StormByte::Safe::Shared` / `StormByte::Safe::Unique` whose element type derives from `Log` (`Log` and `ThreadedLog`). `*tlog <<` still works, and so does `tlog <<` on those owners:

```cpp
StormByte::Safe::Shared<ThreadedLog> log = StormByte::Safe::Shared<ThreadedLog>::MakePointer<ThreadedLog>(std::cout);
log << Level::Info << "Hola" << std::endl;
```

`Log` and `ThreadedLog` accept any `std::ostream` (`std::cout`, a file stream, a string stream). The stream must outlive the logger. The DLL never calls into that stream: each write and each manipulator (`std::endl`, `std::flush`, …) jumps back to `OStreamWrite` / `OStreamManip`, which are compiled into the module that constructed the logger.

Streamed payload types: `bool`, the standard integer and floating types, `char` / `unsigned char` / `wchar_t`, `const char*`, `const wchar_t*`, `std::string`, `std::string_view`, `std::wstring_view`, `std::span<const std::byte>`, `StormByte::BinaryData`, `StormByte::Safe::String`, `StormByte::Safe::WString`, `StormByte::Safe::CString`, `StormByte::Safe::WCString`, `StormByte::Size`, `StormByte::ByteSize`. The inline `std::string` overload passes a view during the call; Logger copies its bytes into the line buffer synchronously. `std::wstring` converts to the wide view. `std::vector<std::byte>` converts to the span. There is no `std::format` overload on the logger itself; format first, then stream the view or an owned Base text type.

### A line

A line is:

```cpp
log << Level::Notice << "opened source " << path << std::endl;
```

1. `<< Level` selects the message level and starts (or restarts) the line.
2. Payload writes append to that line.
3. `std::endl` (or any stream manipulator that writes a newline) ends the line, prints the header if needed, and is the point at which `ThreadedLog` drops the line lock.

Do not start a line without a `Level` if you care about the filter. Do not omit the newline: `ThreadedLog` holds the line lock until one is seen.

### Sharing a logger

Copy and copy-assignment of `Log` / `ThreadedLog` share the same `Engine` (`shared_ptr`). That is the intended way to hand one logger to several objects on **one** thread.

Across threads, construct a `ThreadedLog` (or `std::make_shared<ThreadedLog>`) and pass that pointer. `Log` has no line lock; concurrent `operator<<` will interleave characters.

```cpp
auto log = std::make_shared<ThreadedLog>(std::cout, Level::Notice, "[%L] %T");
Demuxer demux(log);
Muxer   mux(log, container);
```

The objects store `StormByte::Safe::Shared<Log>` (or a `std::shared_ptr<Log>` converted from it). `ThreadedLog` *is-a* `Log`, so the same pointer type works. `Scope` facades also share that backend.

### Owned text and Size

Text that is owned by another module, or that must remain valid after returning across a DLL / `.so`, is `StormByte::Safe::String` / `WString` / `CString` / `WCString`. Do not put `std::string` in objects that cross that boundary.

`Safe::String` has an implicit inline `string_view` in the **caller**. That view points at the other module's buffer. Logger provides an explicit `operator<<(const Safe::String&)` so the copy into the line happens on this side after `WillWrite()`.

```cpp
#include <StormByte/safe/cstring.hxx>
#include <StormByte/size.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/safe/wcstring.hxx>
#include <StormByte/safe/wstring.hxx>

using StormByte::Safe::CString;
using StormByte::Size;
using StormByte::Safe::WCString;
using StormByte::Safe::String;
using StormByte::Safe::WString;

log << Level::Info << String{"owned utf-8"} << std::endl;
log << Level::Info << CString{"owned cstring"} << std::endl;
log << Level::Info << WString{L"wide"} << std::endl;
log << Level::Info << Size{1024} << std::endl;

if (!log.Enabled(Level::Debug)) {
  // Safe::String / WString are not converted here: the overload returns before ToStd.
	log << Level::Debug << WString{L"dropped"} << std::endl;
}
```

`component("Media")`, `group("work")` and `push_format("[%L]")` take `std::string_view` in the caller (literals work). The manipulator stores an owned `String`.

Ill-formed wide input is written as U+FFFD (`EF BF BD`). Logger does not throw `UTF8Error` on that path. A filtered wide write does not convert at all.

### Human-readable numbers

State stays until another of these manipulators is applied.

```cpp
log << Level::Info << humanreadable_number << 1000 << std::endl;   // e.g. 1,000
log << Level::Info << humanreadable_bytes << 10240 << std::endl;  // e.g. 10 KiB
log << Level::Info << nohumanreadable << 1000 << std::endl;
```

### Redaction

Applies to strings **and** numbers (numbers are converted first). Stays on until `noredact`.

| Manipulator | Effect |
| --- | --- |
| `redact` / `redact(0)` | Every character becomes `*` |
| `redact(N)` | Keep the **last** N characters |
| `redact_first(N)` | Keep the **first** N characters |
| `noredact` | Disable |

```cpp
log << Level::Info << redact << "super-secret" << std::endl;
// ************

log << Level::Info << redact(4) << "super-secret" << std::endl;
// ********cret

log << Level::Info << redact_first(4) << "super-secret" << std::endl;
// supe********

log << Level::Info << noredact << "visible again" << std::endl;
```

Same contract on `ThreadedLog`. Hex encoding runs **before** redaction.

### Hex and binary payloads

`hex` dumps subsequent text payloads as space-separated `0xHH` bytes. `hex(N)` wraps every N bytes with a raw newline (no new header, line stays open). Default N is 16. `hex(0)` is the same as `nohex`.

```cpp
log << Level::Info << hex << "AB" << std::endl;
// 0x41 0x42

log << Level::Info << hex(2) << "ABCD" << std::endl;
// 0x41 0x42
// 0x43 0x44

log << Level::Info << nohex << "plain" << std::endl;
```

Numbers are converted to text first, then those text bytes are dumped. It is not a numeric hex printer.

`std::span<const std::byte>`, `std::vector<std::byte>` and `StormByte::BinaryData` are Base64 by default. With `hex(N)` they use `BinaryData::HexDump(N)` (offset, hex columns, ASCII), not the `0xHH` text dump:

```cpp
const std::vector<std::byte> raw{std::byte{'H'}, std::byte{'i'}};
log << Level::Info << raw << std::endl;            // Base64
log << Level::Info << hex(8) << raw << std::endl;  // HexDump, 8 columns
```

On `ThreadedLog` the Base64 / hex string is built **before** the line lock, then written as a prepared payload.

### Colors

ANSI color output is disabled by default. Configure a general color per level
with `Color(level, color)`. A component-path rule has priority while that
path (or a child of it) is active. Longest matching prefix wins; then the
general level color.

```cpp
log.Color(Level::Warning, Color::Yellow);
log.Color("Multimedia", Level::Warning, Color::BrightYellow);

log << component("Multimedia") << Level::Warning << "recoverable" << std::endl;
log << reset_component << Level::Warning << "general warning" << std::endl;
```

`color` re-enables the configured color for the current level. `color(Color::X)`
temporarily selects an explicit content color, and `nocolor` suppresses color
for subsequent content. The header remains configured and every line ends with
an ANSI reset when a color was active.

### Temporary formats

`push_format` saves the current format and activates a temporary one. Calls
nest, and `pop_format` restores the most recent saved format. An empty pop is
a no-op, and the stack persists across lines until explicitly popped.

Changing the format while a line is active closes that line before the new
format is used.

Formats can also be configured persistently per component path:

```cpp
log.Format("[%L] %T");
log.Format("Multimedia", "[%L] %T %c");

log << component("Multimedia") << component("Decoder")
	<< Level::Info << "inherits Multimedia format" << std::endl;
log.Format("Multimedia/Decoder", "[DEC] %c %L:");
```

Effective precedence: `push_format` first, then the longest matching component
path, then the general format. `Format("Path", "")` removes that override.

On a `Scope` facade, `Format("mask")` with no path argument binds to the
facade's sticky path. On the root logger it changes the global format.

### Groups and components

`group("name")` labels one line and is rendered by `%g`. A newline clears the
group automatically; `group("")` also selects no group. Without `%g`, the group
is intentionally not added to the payload.

`component("name")` **pushes** a segment onto a thread-local stack. Nested
pushes join with `/` for `%c` and for config lookup:

```cpp
log << component("Multimedia") << component("Decoder")
	<< Level::Notice << "open" << std::endl;
// %c is Multimedia/Decoder

log << pop_component << Level::Info << "parent" << std::endl;
// %c is Multimedia

log << reset_component << Level::Info << "root" << std::endl;
```

`component("")` does **not** push. Use `reset_component` to return to root, or
`pop_component` to drop one segment. The stack is not cleared by `endl`.

The stack is thread-local, not tied to a `Log` instance. Two `Log` objects used
by the same thread share it. Start tests and job boundaries with
`reset_component` if a previous caller may have left segments.

To switch to a sibling path, reset (or pop) first. Otherwise
`component("Media")` then `component("Other")` becomes `Media/Other`.

### Scope

`Scope` is the API intended for libraries that should not touch the TLS stack.

```cpp
auto log = std::make_shared<ThreadedLog>(std::cout, Level::Info, "[%L] %c");
auto mm  = log->Scope("Multimedia");
auto dec = mm->Scope("Decoder");          // Multimedia/Decoder
auto enc = log->Scope("Multimedia/Encoder");

dec << Level::Notice << "open" << std::endl;
log << Level::Info << "root still has an empty %c" << std::endl;
```

Rules:

- Never returns `nullptr`.
- Nested `Scope("Child")` joins onto the parent's sticky path. A path that
  already contains `/` is joined as given.
- A Scope line uses the sticky path, not the TLS stack. Pushing `component`
  on the original logger does not change a Scope facade, and a Scope write
  does not push onto the TLS stack.
- Copies share the backend (`std::out`, file, throttle table, formats, colors).
- `ThreadedLog` facades share the same line lock.
- `Format`, `Color` and `Throttle` **without** a component argument bind to
  the facade path. On the root logger (empty path) they remain global.
- `Format("Multimedia/Decoder", mask)` on any logger still sets that path
  explicitly.

```cpp
auto dec = log->Scope("Multimedia/Decoder");
dec->Format("[%L] %c:");          // format for Multimedia/Decoder
dec->Throttle(0.0, 1);            // throttle only that leaf
dec->Color(Level::Info, Color::Cyan);
```

Child rules win over parent rules. If the leaf has no format/color, the
longest matching ancestor is used, then the general setting.

### Throttle

Throttle is disabled by default. It limits complete logical lines, not payload
fragments.

Rules are selected by the most specific matching key. Component matching uses
path prefixes (`Multimedia` matches `Multimedia/Decoder`). When two rules
match, the longer component string wins.

```text
(component, level, group) > (component, group) > (component, level)
> (component) > (level, group) > (group) > (level) > global
```

`Error` and `Fatal` are never throttled. `Warning` can be throttled even though
it is always visible with respect to the print floor.

`ThrottleSpec::Component` and `ThrottleSpec::Group` are
`std::optional<StormByte::Safe::String>`. Assigning a literal still works
where `Safe::String` can be constructed from it.

```cpp
ThrottleSpec spec;
spec.Component = StormByte::Safe::String{"Multimedia/Decoder"};
spec.Level = Level::LowLevel;
spec.Policy = ThrottlePolicy::Window;
spec.WindowKeep = 20;
spec.WindowPeriod = 500;
log.Throttle(spec);

log.Throttle(100.0, 20);
log.NoThrottle(spec);

auto dec = log.Scope("Multimedia/Decoder");
dec->Throttle(0.0, 20);   // same leaf binding, no Component field needed
```

Policies are `Drop`, `Sample` and `Window`. `Sample(n)` admits the first line
and then one of every `n` attempts. `Window(keep, period)` admits the first
`keep` lines of each count window. Rate/burst can additionally limit the
admitted lines by time. `rate == 0 && burst == 0` disables the time ceiling;
`rate > 0` requires `burst >= 1`, while `rate == 0 && burst > 0` allows exactly
that initial burst with no refill.

When lines are dropped, the next admitted line is preceded by `dropped N
messages` using the same level, component, group, effective format and color.
Call `FlushThrottle()` at a job boundary to emit pending summaries.
`FlushThrottle(spec)` limits the flush to matching selectors.

Install rules before concurrent writers start.

### Use from other suite modules

Other suite modules log through this module. A useful convention is:

- Identify the module with `Scope("Multimedia")` (or a nested `Scope("Decoder")`), not by repeating `component(...)` on every line.
- Pass owned `String` / `CString` when the text is produced in that module and must survive the return.
- `LowLevel` — per-packet / per-frame / wait-wake. Sparse-sample if the volume would drown the log.
- `Debug` — binds, reserves, work `n/min/max`.
- `Notice` — created, open path, eof, closed. Must stay low-noise.
- `Info` — job close or other application-level completion events.

```cpp
auto decoderLog = appLog->Scope("Multimedia/Decoder");
decoderLog << Level::Notice << "open" << std::endl;
```

The application chooses the floor. A user who sets `LowLevel` is asking for noise and the cost that comes with it.

## ThreadedLog contract

`ThreadedLog` serializes **logical lines**, not individual `<<` tokens from one thread.

- The line lock is taken when a write that will be printed starts (or when `<< Level` starts a line).
- The lock is dropped when a stream manipulator that writes a newline is applied (`std::endl`).
- Filtered payload writes do not take the lock and do not convert owned / wide text.
- Binary payloads (Base64 / hex) and wide-to-UTF-8 are formatted before the lock is taken.
- `<< Level` always updates the current message level. If that level is an ordinary filtered level below the floor, the lock is released immediately after the update; Warning, Error and Fatal remain enabled.
- `Scope` facades of a `ThreadedLog` share that same lock.

`endl` must drop the lock even if another thread just changed the current level. That is required so a filtered `LowLevel` line cannot leave the lock held and stall every other writer.

`Engine` current-level / enabled flags are still process-wide, not `thread_local`. Do not interleave two unfinished lines on the same logger from two threads without finishing each line with a newline. The supported pattern is: one thread writes a complete line (`Level` … `endl`) at a time; `ThreadedLog` only prevents those complete lines from mixing characters.

The component **stack** is thread-local. `Scope` paths are per-facade and do not use that stack. `group` remains line-scoped.

## Contributing

Issues only on this repository. Fork and open a pull request against `master`.

## License

From 2.0.0, original StormByte-Logger source is dual-licensed:

1. GNU Lesser General Public License version 3 or later. See [LICENSE](LICENSE) and <https://www.gnu.org/licenses/lgpl-3.0.html>.
2. A commercial license from the copyright holder (David C. Manuelda, StormBytePP).

Neither license covers other StormByte modules or third-party material shipped under `thirdparty/` (including bundled StormByte-String and the Base tree it vendors). Those keep their own licenses. Neither license grants patent rights.

Static linking under the LGPL is described under [Installation](#installation).

## Support

StormByte is developed in spare time. Sponsorship is optional and does not buy features, priority or support.

- [GitHub Sponsors](https://github.com/sponsors/StormBytePP)
- [PayPal](https://paypal.me/StormBytePP)
