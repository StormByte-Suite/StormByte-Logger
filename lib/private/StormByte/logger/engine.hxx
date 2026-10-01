/*
 * Copyright (C) 2024-2026 David C. Manuelda (StormBytePP)
 *
 * This file is part of StormByte-Logger.
 *
 * StormByte-Logger original source is dual-licensed:
 *
 * 1. GNU Lesser General Public License v3.0 (or later)
 *    You may redistribute and/or modify this file under the terms of the
 *    GNU Lesser General Public License as published by the Free Software
 *    Foundation, either version 3 of the License, or (at your option)
 *    any later version.
 *
 * 2. Commercial license
 *    Alternatively, this file may be used under the terms of a commercial
 *    license agreement with the copyright holder
 *    (David C. Manuelda <StormByte@gmail.com>).
 *
 * Both licenses apply only to original StormByte-Logger source in this
 * repository. They do not cover other StormByte modules or any third-party
 * material shipped with this repository (including everything under
 * thirdparty/, and in particular the bundled StormByte-String tree and
 * the StormByte Base tree it vendors), which remains under its own license.
 *
 * Neither license grants any patent rights. Any patent licenses required
 * to use this software or third-party components must be obtained separately
 * from the patent holders.
 *
 * StormByte-Logger is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * version 3 along with StormByte-Logger. If not, see
 * <https://www.gnu.org/licenses/lgpl-3.0.html>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later OR LicenseRef-StormByte-Commercial
 */

#pragma once

#include <StormByte/base64.hxx>
#include <StormByte/binary_data.hxx>
#include <StormByte/logger/human_readable.hxx>
#include <StormByte/logger/manipulators.hxx>
#include <StormByte/logger/typedefs.hxx>
#include <StormByte/size.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/safe/wstring.hxx>
#include <StormByte/type_traits.hxx>

#include <charconv>
#include <system_error>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

/**
 * @namespace StormByte::Logger
 * @brief Logger module of the StormByte suite.
 */
namespace StormByte::Logger {
	/**
	 * @struct ThrottleRuleState
	 * @brief Mutable counters for one throttle rule or emitting path.
	 */
	struct ThrottleRuleState {
		std::atomic<std::uint64_t> dropped{0};		///< Dropped lines waiting for a summary
		std::atomic<std::uint64_t> sample_count{0};	///< Sample policy counter
		std::atomic<std::uint64_t> window_count{0};	///< Window policy counter
		std::atomic<std::uint64_t> finite_credits{0};	///< Remaining burst credits
		std::atomic<std::int64_t> next_token_ns{0};	///< Next token refill time
	};

	/**
	 * @struct ThrottleRule
	 * @brief One published throttle rule and its shared state.
	 */
	struct ThrottleRule {
		ThrottleSpec spec;								///< Selectors and rates
		std::shared_ptr<ThrottleRuleState> state;					///< Shared counters
		std::shared_ptr<std::mutex> leaf_mutex;						///< Guards leaf_states
		std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<ThrottleRuleState>>> leaf_states;	///< Per-path counters
	};

	/**
	 * @struct ThrottleTable
	 * @brief Immutable snapshot of installed rules.
	 */
	struct ThrottleTable {
		std::vector<ThrottleRule> rules;	///< Published rules
	};

	/**
	 * @class Engine
	 * @brief Internal logger implementation (private).
	 *
	 * Thread-safety note: `m_enabled` is atomic so `Enabled()` / filtered fast-paths may be
	 * observed concurrently with `operator<<(Level)` (as with `ThreadedLog`). Full multi-threaded
	 * emission still requires `ThreadedLog` (line lock around actual writes).
	 * Throttle rules use atomic shared-pointer publication; Logger does not promise
	 * that the standard library implementation is physically lock-free.
	 *
	 * Component emission path is the facade path (@ref SetFacadePath) when set,
	 * otherwise the thread-local stack joined with `/`.
	 * A prefix throttle rule supplies the spec; counters are per emitting path.
	 */
	class STORMBYTE_LOGGER_PRIVATE Engine final {
		friend STORMBYTE_LOGGER_PRIVATE Engine& humanreadable_number(Engine& logger) noexcept;
		friend STORMBYTE_LOGGER_PRIVATE Engine& humanreadable_bytes(Engine& logger) noexcept;
		friend STORMBYTE_LOGGER_PRIVATE Engine& nohumanreadable(Engine& logger) noexcept;

		public:
			/**
			 * @brief Construct the internal logger implementation.
			 * @param write Caller function that receives raw bytes.
			 * @param manip Caller function that applies an ostream manipulator.
			 * @param context Opaque pointer passed back to @p write and @p manip. Not owned.
			 * @param level Initial minimum Level that will be emitted.
			 * @param format Header format string (%L, %T, %i, %c, %g, %%).
			 */
			Engine(SinkWrite write, SinkManip manip, void* context, const Level& level = Level::Info, const std::string& format = "[%L] %T");

			Engine(const Engine&) = delete;

			Engine(Engine&&) noexcept = delete;

			Engine& operator=(const Engine&) = delete;

			Engine& operator=(Engine&&) noexcept = delete;

			/**
			 * @brief Destructor.
			 */
			~Engine() noexcept;

			/**
			 * @brief Get the minimum print level.
			 * @return Current minimum Level.
			 */
			const Level& PrintLevel() const noexcept {
				return m_print_level;
			}

			/**
			 * @brief Get the level of the current message.
			 * @return Current message Level (or print level if none set).
			 */
			const Level& CurrentLevel() const noexcept;

			/**
			 * @brief Whether the current message level will be emitted.
			 * @return true if the message will be written.
			 * @note Warning, Error and Fatal remain enabled even when below the
			 *       configured print level.
			 */
			bool Enabled() const noexcept;

			/**
			 * @brief Enable or disable redaction for subsequent values.
			 * @param active true to redact text and numbers.
			 * @param count 0 = mask all characters; N = keep N characters.
			 * @param keep_first true = keep first N characters, false = keep last N characters.
			 */
			void SetRedact(bool active, std::size_t count, bool keep_first) noexcept {
				m_redact_active = active;
				m_redact_count = count;
				m_redact_keep_first = keep_first;
			}

			/**
			 * @brief Enable or disable hex dumps for subsequent values.
			 * @param active true to dump payload bytes as hex.
			 * @param columns Bytes per row; 0 disables hex (same as nohex).
			 */
			void SetHex(bool active, std::size_t columns) noexcept {
				m_hex_active = active && columns != 0;
				m_hex_columns = columns;
			}

			/**
			 * @brief Set the emitting facade's sticky component path for the next line.
			 * @param path Sticky path from Log::Scope; empty uses the thread-local stack.
			 * Assigned into a reused thread-local buffer. No temporary string.
			 */
			void SetFacadePath(std::string_view path) noexcept;

			/**
			 * @brief Format raw bytes for a payload (HexDump or Base64). Does not write.
			 * @param v Contiguous bytes.
			 * @return Display string. HexDump uses @c m_hex_columns. Base64 is one line.
			 */
			std::string FormatBinary(std::span<const std::byte> v) const {
				const StormByte::BinaryData data(v);
				if (m_hex_active)
					return static_cast<std::string>(data.HexDump(StormByte::Size{m_hex_columns}));
				return static_cast<std::string>(StormByte::Base64Encode(data));
			}

			/**
			 * @brief Write an already-formatted payload (no second hex pass).
			 * @param text Prepared text (Base64 or hex dump).
			 */
			void WritePrepared(std::string_view text) noexcept {
				ensure_header();
				if (!LineAdmitted())
					return;
				sync_content_color();
				if (m_redact_active)
					sink_write(ApplyRedact(text, m_redact_count, m_redact_keep_first));
				else
					sink_write(text);
			}

			/**
			 * @brief Set the configured color for a logging level.
			 * @param level Level whose color is changed.
			 * @param color Color to use for that level.
			 */
			void Color(const Level& level, const StormByte::Logger::Color& color) noexcept;

			/**
			 * @brief Get the configured color for a logging level.
			 * @param level Level whose color is requested.
			 * @return Configured color.
			 */
			StormByte::Logger::Color Color(const Level& level) const noexcept;

			/**
			 * @brief Set a color override for a component path and level.
			 * @param component Component path.
			 * @param level Level whose color is changed.
			 * @param color Color to use for that component and level.
			 */
			void Color(const std::string& component, const Level& level, const StormByte::Logger::Color& color);

			/**
			 * @brief Get a component color, falling back along the path then to the general color.
			 * @param component Component path.
			 * @param level Level whose color is requested.
			 * @return Component override or general color.
			 */
			StormByte::Logger::Color Color(const std::string& component, const Level& level) const noexcept;

			/**
			 * @brief Set the general header format.
			 * @param format Format used when no component or temporary override applies.
			 */
			void Format(const std::string& format);

			/**
			 * @brief Get the effective current header format.
			 * @return Temporary, component-specific or general format.
			 */
			const std::string& Format() const noexcept;

			/**
			 * @brief Set or remove a component-specific header format.
			 * @param component Component path; empty selects the general format.
			 * @param format Format, or empty to remove the component override.
			 */
			void Format(const std::string& component, const std::string& format);

			/**
			 * @brief Get a component-specific format, falling back along the path then to general.
			 * @param component Component path.
			 * @return Component format or general format.
			 */
			const std::string& Format(const std::string& component) const noexcept;

			/**
			 * @brief Install a throttle rule.
			 * @param spec Rule to install.
			 */
			void Throttle(const ThrottleSpec& spec);

			/**
			 * @brief Remove a throttle rule with the same selectors.
			 * @param spec Selectors of the rule to remove.
			 */
			void NoThrottle(const ThrottleSpec& spec);

			/**
			 * @brief Remove all throttle rules.
			 */
			void NoThrottleAll() noexcept;

			/**
			 * @brief Flush all dropped summaries without changing throttle rules.
			 */
			void FlushThrottle();

			/**
			 * @brief Flush matching dropped summaries without changing throttle rules.
			 * @param spec Selectors of the rules to flush.
			 */
			void FlushThrottle(const ThrottleSpec& spec);

			/**
			 * @brief Decide whether the current line may emit.
			 * @return true when the line is admitted.
			 */
			bool PrepareLine();

			/**
			 * @brief Whether the current thread's line was admitted.
			 * @return true when payload output is allowed.
			 */
			bool LineAdmitted() const noexcept;

			/**
			 * @brief Whether throttle has already decided the current line.
			 * @return true when the line decision exists.
			 */
			bool LineDecided() const noexcept;

			/**
			 * @brief Whether a header/output line is currently open.
			 * @return true when output has started for the line.
			 */
			bool HasOpenOutputLine() const noexcept;

			/**
			 * @brief Mark the current line as having started output.
			 */
			void BeginOutputLine() noexcept;

			/**
			 * @brief Set the current logging level.
			 * @param level New Level for subsequent messages.
			 * @return Reference to this Engine.
			 */
			Engine& operator<<(const Level& level) noexcept;

			/**
			 * @brief Forward a standard stream manipulator.
			 * @param manip Stream manipulator (e.g. std::endl).
			 * @return Reference to this Engine.
			 */
			Engine& operator<<(std::ostream& (*manip)(std::ostream&)) noexcept;

			/**
			 * @brief Apply a temporary content color manipulator.
			 * @param manip Color selection to apply.
			 * @return Reference to this Engine.
			 */
			Engine& operator<<(ColorManip manip) noexcept;

			/**
			 * @brief Disable content color until another color manipulator or endl.
			 * @param manip No-color manipulator.
			 * @return Reference to this Engine.
			 */
			Engine& operator<<(NoColorManip manip) noexcept;

			/**
			 * @brief Push and activate a temporary format.
			 * @param manip Format manipulator.
			 * @return Reference to this Engine.
			 */
			Engine& operator<<(FormatManip manip);

			/**
			 * @brief Restore the last saved format when one exists.
			 * @param manip Pop-format manipulator.
			 * @return Reference to this Engine.
			 */
			Engine& operator<<(PopFormatManip manip) noexcept;

			/**
			 * @brief Set the producer group for the current line.
			 * @param manip Group manipulator.
			 * @return Reference to this Engine.
			 */
			Engine& operator<<(GroupManip manip);

			/**
			 * @brief Push a component segment onto the current thread's stack.
			 * @param manip Component manipulator.
			 * @return Reference to this Engine.
			 */
			Engine& operator<<(ComponentManip manip);

			/**
			 * @brief Pop one component segment from the current thread's stack.
			 * @param manip Pop-component manipulator.
			 * @return Reference to this Engine.
			 */
			Engine& operator<<(PopComponentManip manip);

			/**
			 * @brief Clear the current thread's component stack.
			 * @param manip Reset-component manipulator.
			 * @return Reference to this Engine.
			 */
			Engine& operator<<(ResetComponentManip manip);

			/**
			 * @brief Apply an Engine-specific manipulator.
			 * @param manip Manipulator function.
			 * @return Reference to this Engine.
			 */
			inline Engine& operator<<(Engine& (*manip)(Engine&) noexcept) {
				return manip(*this);
			}

			/**
			 * @brief Stream a value into the log.
			 * @tparam T Type of the value.
			 * @param value Value to write.
			 * @return Reference to this Engine.
			 */
			template <typename T>
			Engine& operator<<(const T& value)
			requires (!StormByte::Type::SameAs<T, Engine& (*)(Engine&) noexcept>) {
				using DecayedT = std::decay_t<T>;

				if (!Enabled()) [[likely]] {
					return *this;
				}

				if constexpr (StormByte::Type::SameAs<DecayedT, bool>) {
					write_text(std::string_view{value ? "true" : "false"});
				} else if constexpr (StormByte::Type::SameAs<DecayedT, wchar_t>) {
					print_message(value);
				} else if constexpr (StormByte::Type::SameAs<DecayedT, std::span<const std::byte>>) {
					WritePrepared(FormatBinary(value));
				} else if constexpr (
					StormByte::Type::Integral<DecayedT>
					&& !StormByte::Type::SameAs<DecayedT, bool>
					&& !StormByte::Type::SameAs<DecayedT, char>
					&& !StormByte::Type::SameAs<DecayedT, signed char>
					&& !StormByte::Type::SameAs<DecayedT, unsigned char>
					&& !StormByte::Type::SameAs<DecayedT, wchar_t>
				) {
					if (m_human_readable_format == Detail::HumanReadable::Raw) {
						char buf[32];
						const auto result = std::to_chars(buf, buf + sizeof(buf), value);
						if (result.ec == std::errc{})
							write_text(std::string_view{buf, static_cast<std::size_t>(result.ptr - buf)});
						else
							write_text(Detail::FormatHuman(value, m_human_readable_format));
					} else {
						write_text(Detail::FormatHuman(value, m_human_readable_format));
					}
				} else if constexpr (StormByte::Type::Arithmetic<DecayedT>) {
					write_text(Detail::FormatHuman(value, m_human_readable_format));
				} else if constexpr (StormByte::Type::SameAs<DecayedT, std::string_view>) {
					write_text(value);
				} else if constexpr (StormByte::Type::SameAs<DecayedT, std::string>) {
					write_text(value);
				} else if constexpr (StormByte::Type::SameAs<DecayedT, const char*>) {
					write_text(value ? std::string_view{value} : std::string_view{});
				} else if constexpr (StormByte::Type::SameAs<DecayedT, std::wstring_view>) {
					const StormByte::Safe::String encoded{StormByte::Safe::WString{value}};
					write_text(static_cast<std::string_view>(encoded));
				} else if constexpr (StormByte::Type::SameAs<DecayedT, std::wstring>) {
					const StormByte::Safe::String encoded{StormByte::Safe::WString{std::wstring_view{value}}};
					write_text(static_cast<std::string_view>(encoded));
				} else if constexpr (StormByte::Type::SameAs<DecayedT, const wchar_t*>) {
					if (!value) {
						write_text(std::string_view{});
					} else {
						const StormByte::Safe::String encoded{StormByte::Safe::WString{value}};
						write_text(static_cast<std::string_view>(encoded));
					}
				} else {
					static_assert(!StormByte::Type::SameAs<T, T>, "Unsupported type for Engine::operator<<");
				}
				return *this;
			}

		private:
			/**
			 * @brief Hand bytes to the caller sink. Empty text is a no-op.
			 * @param text Bytes to write.
			 */
			void sink_write(std::string_view text) const noexcept {
				if (m_write == nullptr || text.empty())
					return;
				m_write(m_context, text.data(), text.size());
			}

			/**
			 * @brief Hand one byte to the caller sink.
			 * @param c Byte to write.
			 */
			void sink_char(char c) const noexcept {
				if (m_write == nullptr)
					return;
				m_write(m_context, &c, 1);
			}

			/**
			 * @brief Apply a stream manipulator in the caller.
			 * @param manip Manipulator, for example `std::endl`.
			 */
			void sink_manip(std::ostream& (*manip)(std::ostream&)) const noexcept {
				if (m_manip == nullptr || manip == nullptr)
					return;
				m_manip(m_context, manip);
			}

			SinkWrite m_write = nullptr;									///< Caller write callback
			SinkManip m_manip = nullptr;									///< Caller manipulator callback
			void* m_context = nullptr;									///< Opaque sink, not owned
			Level m_print_level;										///< Minimum level that will be printed
			std::optional<Level> m_current_level;								///< Level of the current message
			std::atomic<bool> m_enabled;									///< Whether the current level is enabled
			std::string m_format;										///< Header format string
			std::vector<std::string> m_format_stack;							///< Temporary formats for push/pop
			std::unordered_map<std::string, std::string> m_component_formats;				///< Persistent component formats
			Detail::HumanReadable m_human_readable_format;							///< Current human-readable format
			bool m_redact_active;										///< When true, text and numbers are redacted
			std::size_t m_redact_count;									///< 0 = all '*'; N = keep N chars
			bool m_redact_keep_first;									///< true = keep first N, false = keep last N
			bool m_hex_active;										///< When true, payloads are dumped as hex
			std::size_t m_hex_columns;									///< Bytes per hex row; 0 disables wrapping
			std::array<StormByte::Logger::Color, 7> m_level_colors{};					///< Configured color per level
			std::unordered_map<std::string, std::array<StormByte::Logger::Color, 7>> m_component_colors;	///< Component color overrides
			std::optional<StormByte::Logger::Color> m_content_color;						///< Temporary content color override
			bool m_content_nocolor = false;									///< Whether content color is suppressed
			std::optional<StormByte::Logger::Color> m_active_color;						///< Color currently emitted to the stream
#ifdef WINDOWS
			std::atomic<std::shared_ptr<const ThrottleTable>> m_throttle_table;				///< Immutable rules snapshot
#elifdef __GLIBCXX__
			std::atomic<std::shared_ptr<const ThrottleTable>> m_throttle_table;				///< Immutable rules snapshot
#else
			std::shared_ptr<const ThrottleTable> m_throttle_table;						///< Immutable rules snapshot, atomically accessed
#endif

			/**
			 * @brief Ensure the header has been printed for the current line.
			 */
			void ensure_header() noexcept {
				if (!PrepareLine())
					return;
				close_deferred_line();
				if (!HasOpenOutputLine()) {
					BeginOutputLine();
					write_drop_summary();
					print_header();
				}
			}

			/**
			 * @brief Apply redaction policy.
			 * @param in Input text.
			 * @param count 0 = all '*'; N = keep N characters.
			 * @param keep_first true = keep first N, false = keep last N.
			 * @return Redacted string of the same length.
			 */
			static std::string ApplyRedact(std::string_view in, std::size_t count, bool keep_first) {
				if (in.empty())
					return {};
				if (count >= in.size())
					return std::string{in};

				std::string out(in.size(), '*');
				if (keep_first) {
					for (std::size_t i = 0; i < count; ++i)
						out[i] = in[i];
				} else {
					const std::size_t start = in.size() - count;
					for (std::size_t i = 0; i < count; ++i)
						out[start + i] = in[start + i];
				}
				return out;
			}

			/**
			 * @brief Format payload bytes as spaced hex, wrapping every @p columns bytes.
			 * @param in Payload bytes.
			 * @param columns Bytes per row; 0 means a single line.
			 * @return Hex dump. Continuation rows use a raw newline (no new header).
			 */
			static std::string FormatHex(std::string_view in, std::size_t columns) {
				if (in.empty())
					return {};
				std::string out;
				out.reserve(in.size() * 5);
				static constexpr char kHex[] = "0123456789ABCDEF";
				for (std::size_t i = 0; i < in.size(); ++i) {
					if (i != 0) {
						if (columns != 0 && (i % columns) == 0)
							out.push_back('\n');
						else
							out.push_back(' ');
					}
					const auto b = static_cast<unsigned char>(in[i]);
					out += "0x";
					out.push_back(kHex[b >> 4]);
					out.push_back(kHex[b & 0x0F]);
				}
				return out;
			}

			/**
			 * @brief Write text, applying hex then redaction if active.
			 * @param text Text to write.
			 */
			void write_text(std::string_view text) noexcept {
				ensure_header();
				if (!LineAdmitted())
					return;
				sync_content_color();
				std::string formatted;
				std::string_view payload = text;
				if (m_hex_active) {
					formatted = FormatHex(text, m_hex_columns);
					payload = formatted;
				}
				if (m_redact_active)
					sink_write(ApplyRedact(payload, m_redact_count, m_redact_keep_first));
				else
					sink_write(payload);
			}

			/**
			 * @brief Write a std::string, applying hex then redaction if active.
			 * @param text Text to write.
			 */
			void write_text(const std::string& text) noexcept {
				write_text(std::string_view{text});
			}

			/**
			 * @brief Append the timestamp field.
			 * @param out Buffer that receives the text.
			 */
			void print_time(std::string& out) const noexcept;

			/**
			 * @brief Current timestamp text.
			 * @return Timestamp.
			 */
			std::string CurrentTime() const noexcept;

			/**
			 * @brief Append the level field, padded to 8 characters.
			 * @param out Buffer that receives the text.
			 */
			void print_level(std::string& out) const noexcept;

			/**
			 * @brief Append the thread-id field.
			 * @param out Buffer that receives the text.
			 */
			void print_thread_id(std::string& out) const noexcept;

			/**
			 * @brief Write the header for the current line.
			 */
			void print_header() noexcept;

			/**
			 * @brief Emit the content color if it changed.
			 */
			void sync_content_color() noexcept;

			/**
			 * @brief Append an ANSI color change and remember it.
			 * @param out Buffer that receives the sequence.
			 * @param color Color to select.
			 */
			void append_color(std::string& out, StormByte::Logger::Color color) noexcept;

			/**
			 * @brief Emit an ANSI color.
			 * @param color Color to emit.
			 */
			void emit_color(StormByte::Logger::Color color) noexcept;

			/**
			 * @brief Reset ANSI color.
			 */
			void reset_color() noexcept;

			/**
			 * @brief Close a deferred line if one is open.
			 */
			void close_deferred_line() noexcept;

			/**
			 * @brief Reset per-line state after a newline.
			 */
			void reset_line_state() noexcept;

			/**
			 * @brief Write a dropped-line summary when one is pending.
			 */
			void write_drop_summary() noexcept;

			/**
			 * @brief Load the published throttle table.
			 * @return Snapshot; may be null.
			 */
			std::shared_ptr<const ThrottleTable> LoadThrottleTable() const noexcept;

			/**
			 * @brief Publish a throttle table.
			 * @param table Snapshot to store.
			 */
			void StoreThrottleTable(std::shared_ptr<const ThrottleTable> table) noexcept;

			/**
			 * @brief Counters for this emitting path under an inherited spec.
			 * @param rule Selected rule (prefix or exact).
			 * @param path Current component path.
			 * @return Isolated state. Never null.
			 */
			std::shared_ptr<ThrottleRuleState> LeafThrottleState(const ThrottleRule& rule, const std::string& path);

			/**
			 * @brief Effective header format for the current line.
			 * @return Format string.
			 */
			const std::string& effective_format() const noexcept;

			/**
			 * @brief Format and write an arithmetic value.
			 * @tparam T Arithmetic type other than wchar_t.
			 * @param value Value to write.
			 */
			template <typename T>
			requires StormByte::Type::Arithmetic<T> && (!StormByte::Type::SameAs<T, wchar_t>)
			void print_message(const T& value) noexcept {
				print_message(Detail::FormatHuman(value, m_human_readable_format));
			}

			/**
			 * @brief Write already-formatted text through the message path.
			 * @param message Text to write.
			 */
			void print_message(const std::string& message) noexcept;

			/**
			 * @brief Write a wide character.
			 * @param value Character to write.
			 */
			void print_message(const wchar_t& value);
	};

	/**
	 * @brief Enable grouped-number formatting.
	 * @param logger Engine to update.
	 * @return @p logger.
	 */
	inline STORMBYTE_LOGGER_PRIVATE Engine& humanreadable_number(Engine& logger) noexcept {
		logger.m_human_readable_format = Detail::HumanReadable::Number;
		return logger;
	}

	/**
	 * @brief Enable IEC byte-size formatting.
	 * @param logger Engine to update.
	 * @return @p logger.
	 */
	inline STORMBYTE_LOGGER_PRIVATE Engine& humanreadable_bytes(Engine& logger) noexcept {
		logger.m_human_readable_format = Detail::HumanReadable::Bytes;
		return logger;
	}

	/**
	 * @brief Disable human-readable numeric formatting.
	 * @param logger Engine to update.
	 * @return @p logger.
	 */
	inline STORMBYTE_LOGGER_PRIVATE Engine& nohumanreadable(Engine& logger) noexcept {
		logger.m_human_readable_format = Detail::HumanReadable::Raw;
		return logger;
	}

	extern template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<bool>(const bool& value);
	extern template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<short>(const short& value);
	extern template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<unsigned short>(const unsigned short& value);
	extern template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<int>(const int& value);
	extern template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<unsigned int>(const unsigned int& value);
	extern template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<long>(const long& value);
	extern template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<unsigned long>(const unsigned long& value);
	extern template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<long long>(const long long& value);
	extern template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<unsigned long long>(const unsigned long long& value);
	extern template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<float>(const float& value);
	extern template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<double>(const double& value);
	extern template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<long double>(const long double& value);
	extern template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<char>(const char& value);
	extern template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<signed char>(const signed char& value);
	extern template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<unsigned char>(const unsigned char& value);
	extern template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<wchar_t>(const wchar_t& value);
	extern template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<std::string>(const std::string& value);
	extern template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<std::wstring>(const std::wstring& value);
	extern template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<const char*>(const char* const& value);
	extern template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<const wchar_t*>(const wchar_t* const& value);
	extern template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<std::string_view>(const std::string_view& value);
	extern template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<std::wstring_view>(const std::wstring_view& value);
	extern template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<std::span<const std::byte>>(const std::span<const std::byte>& value);

	/**
	 * @brief Stream a value into a smart pointer to Engine.
	 * @tparam Ptr shared_ptr or unique_ptr of Engine.
	 * @tparam T Value type.
	 * @param logger Smart pointer.
	 * @param value Value to stream.
	 * @return @p logger.
	 */
	template <typename Ptr, typename T>
	Ptr& operator<<(Ptr& logger, const T& value)
	requires StormByte::Type::SameAs<Ptr, std::shared_ptr<Engine>> || StormByte::Type::SameAs<Ptr, std::unique_ptr<Engine>> {
		if (logger)
			*logger << value;
		return logger;
	}

	/**
	 * @brief Stream a Level into a smart pointer to Engine.
	 * @tparam Ptr shared_ptr or unique_ptr of Engine.
	 * @param logger Smart pointer.
	 * @param level Level to set.
	 * @return @p logger.
	 */
	template <typename Ptr>
	Ptr& operator<<(Ptr& logger, const Level& level) noexcept
	requires StormByte::Type::SameAs<Ptr, std::shared_ptr<Engine>> || StormByte::Type::SameAs<Ptr, std::unique_ptr<Engine>> {
		if (logger)
			*logger << level;
		return logger;
	}

	/**
	 * @brief Stream a stream manipulator into a smart pointer to Engine.
	 * @tparam Ptr shared_ptr or unique_ptr of Engine.
	 * @param logger Smart pointer.
	 * @param manip Stream manipulator.
	 * @return @p logger.
	 */
	template <typename Ptr>
	Ptr& operator<<(Ptr& logger, std::ostream& (*manip)(std::ostream&)) noexcept
	requires StormByte::Type::SameAs<Ptr, std::shared_ptr<Engine>> || StormByte::Type::SameAs<Ptr, std::unique_ptr<Engine>> {
		if (logger)
			*logger << manip;
		return logger;
	}
}
