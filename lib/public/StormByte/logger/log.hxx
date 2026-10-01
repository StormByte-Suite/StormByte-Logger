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

#include <StormByte/binary_data.hxx>
#include <StormByte/byte_size.hxx>
#include <StormByte/safe/clonable.hxx>
#include <StormByte/safe/cstring.hxx>
#include <StormByte/logger/manipulators.hxx>
#include <StormByte/logger/typedefs.hxx>
#include <StormByte/size.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/safe/wcstring.hxx>
#include <StormByte/safe/wstring.hxx>
#include <StormByte/type_traits.hxx>

#include <cstddef>
#include <memory>
#include <ostream>
#include <span>
#include <string>
#include <string_view>

/**
 * @namespace StormByte::Logger
 * @brief Logger module of the StormByte suite.
 */
namespace StormByte::Logger {
	class Engine;

	/**
	 * @brief Write raw bytes into an `std::ostream` owned by the caller.
	 *
	 * Instantiated in the caller's module. The DLL only stores the function pointer.
	 * @param context Address of the caller's `std::ostream`.
	 * @param data Bytes to write.
	 * @param size Number of bytes.
	 */
	inline void OStreamWrite(void* context, const char* data, std::size_t size) {
		if (context == nullptr || data == nullptr || size == 0)
			return;
		static_cast<std::ostream*>(context)->write(data, static_cast<std::streamsize>(size));
	}

	/**
	 * @brief Apply an `std::ostream` manipulator in the caller's module.
	 * @param context Address of the caller's `std::ostream`.
	 * @param manip Manipulator, for example `std::endl`.
	 */
	inline void OStreamManip(void* context, std::ostream& (*manip)(std::ostream&)) {
		if (context == nullptr || manip == nullptr)
			return;
		*static_cast<std::ostream*>(context) << manip;
	}

	/**
	 * @class Log
	 * @brief Public streaming facade for the StormByte logger.
	 *
	 * Owns a shared_ptr to the internal Engine and exposes operator<<
	 * overloads similar to std::ostream. Filtered levels early-out without I/O.
	 * The configured print level does not suppress Warning, Error or Fatal.
	 *
	 * Numeric and narrow-text payloads share WriteValue after WillWrite.
	 * ThreadedLog only overrides BeginPayload for those payloads.
	 * Owned suite text has its own overloads so WillWrite runs before conversion.
	 * StormByte::Size and StormByte::ByteSize are sugar over operator std::string.
	 *
	 * Binary payloads are std::span<const std::byte>, std::vector<std::byte>
	 * and StormByte::BinaryData. Default formatting is Base64. With hex(N)
	 * they use BinaryData::HexDump(N). Text still uses the 0xAA dump.
	 */
	class STORMBYTE_LOGGER_PUBLIC Log : protected StormByte::Safe::Clonable<Log> {
		friend STORMBYTE_LOGGER_PUBLIC Log& humanreadable_number(Log& log) noexcept;
		friend STORMBYTE_LOGGER_PUBLIC Log& humanreadable_bytes(Log& log) noexcept;
		friend STORMBYTE_LOGGER_PUBLIC Log& nohumanreadable(Log& log) noexcept;
		friend STORMBYTE_LOGGER_PUBLIC Log& noredact(Log& log) noexcept;

		public:
			/**
			 * @brief Owner returned by Clone, Move and Scope. Base's heap.
			 */
			using PointerType = StormByte::Safe::Clonable<Log>::PointerType;

			/**
			 * @brief Construct a Log writing to out.
			 * @param out Output stream (e.g. std::cout). Must outlive this logger.
			 * @param level Minimum Level that will be emitted.
			 * @param format Header format: %L level, %T timestamp, %i thread id, %c component, %g group, %% literal %.
			 *
			 * The stream is not touched from inside the DLL. Writes jump back to
			 * @ref OStreamWrite and @ref OStreamManip in the module that constructs this logger.
			 */
			Log(std::ostream& out, const Level& level = Level::Info, std::string_view format = "[%L] %T")
				: Log(&OStreamWrite, &OStreamManip, &out, level, format) {}

			/**
			 * @brief Copy constructor.
			 * @note Shares the Engine. Copies the sticky component path.
			 */
			Log(const Log&) = default;

			/**
			 * @brief Move constructor.
			 */
			Log(Log&&) noexcept = default;

			/**
			 * @brief Destructor. Defined out of line so the backend is released inside the DLL.
			 */
			~Log() noexcept override;

			/**
			 * @brief Copy assignment. Defined out of line so the previous backend is released inside the DLL.
			 * @return Reference to this logger.
			 * @note Shares the Engine. Copies the sticky component path.
			 */
			Log& operator=(const Log&);

			/**
			 * @brief Move assignment. Defined out of line so the previous backend is released inside the DLL.
			 * @return Reference to this logger.
			 */
			Log& operator=(Log&&) noexcept;

			/**
			 * @brief Another facade on the same backend, with a sticky component path.
			 * @param path Segment relative to this facade, or a /-separated path.
				 * @return @ref StormByte::Safe::Shared of a Log (ThreadedLog if *this is one). Never null.
			 * @note Does not register the component and does not preconfigure Format, Color or Throttle.
			 *       An empty path returns a clone of this facade.
			 */
			PointerType Scope(std::string_view path);

			/**
			 * @brief Whether level would be emitted given the print floor.
			 * @param level Level to test.
			 * @return true if that level is at or above the floor, or is Warning/Error/Fatal.
			 * @note Does not open a line and does not consult throttle admission.
			 */
			bool Enabled(const Level& level) const noexcept;

			/**
			 * @brief Set the configured color for a logging level.
			 * @param level Level whose color is changed.
			 * @param color Color to use for that level.
			 * @return Reference to this logger.
			 */
			virtual Log& Color(const Level& level, const StormByte::Logger::Color& color);

			/**
			 * @brief Get the configured color for a logging level.
			 * @param level Level whose color is requested.
			 * @return Configured color.
			 */
			virtual StormByte::Logger::Color Color(const Level& level) const;

			/**
			 * @brief Set a color override for a component path and level.
			 * @param component Component path.
			 * @param level Level whose color is changed.
			 * @param color Color to use for that component and level.
			 * @return Reference to this logger.
			 */
			virtual Log& Color(std::string_view component, const Level& level, const StormByte::Logger::Color& color);

			/**
			 * @brief Get a component color, falling back along the path then to the general color.
			 * @param component Component path.
			 * @param level Level whose color is requested.
			 * @return Component override or general color.
			 */
			virtual StormByte::Logger::Color Color(std::string_view component, const Level& level) const;

			/**
			 * @brief Set the header format.
			 * @param format Format used by default, or for the sticky path on a scoped facade.
			 * @return Reference to this logger.
			 */
			virtual Log& Format(std::string_view format);

			/**
			 * @brief Get the effective current header format.
			 * @return Owned copy of the temporary, component-specific or general format.
			 */
			virtual StormByte::Safe::String Format() const;

			/**
			 * @brief Set or remove a component-specific header format.
			 * @param component Component path; empty selects the general format.
			 * @param format Format, or empty to remove the override.
			 * @return Reference to this logger.
			 */
			virtual Log& Format(std::string_view component, std::string_view format);

			/**
			 * @brief Get a component-specific format, falling back along the path then to general.
			 * @param component Component path.
			 * @return Owned copy of the component format or general format.
			 */
			virtual StormByte::Safe::String Format(std::string_view component) const;

			/**
			 * @brief Install a throttle rule.
			 * @param spec Rule to install. An absent Component uses the sticky path when this facade is scoped.
			 * @return Reference to this logger.
			 */
			virtual Log& Throttle(const ThrottleSpec& spec);

			/**
			 * @brief Remove a throttle rule with the same selectors.
			 * @param spec Selectors of the rule to remove.
			 * @return Reference to this logger.
			 */
			virtual Log& NoThrottle(const ThrottleSpec& spec);

			/**
			 * @brief Install a Drop rule.
			 * @param rate Lines per second; zero disables refill.
			 * @param burst Initial and maximum token capacity.
			 * @return Reference to this logger.
			 */
			virtual Log& Throttle(double rate, std::size_t burst);

			/**
			 * @brief Install a Sample or Window rule.
			 * @param rate Lines per second; zero disables refill.
			 * @param burst Initial and maximum token capacity.
			 * @param policy Sample or Window.
			 * @param value SampleN or WindowKeep.
			 * @param period WindowPeriod when policy is Window.
			 * @return Reference to this logger.
			 */
			virtual Log& Throttle(double rate, std::size_t burst, ThrottlePolicy policy, std::size_t value, std::size_t period = 0);

			/**
			 * @brief Install a level-scoped Drop rule.
			 * @param level Level selector.
			 * @param rate Lines per second.
			 * @param burst Token capacity.
			 * @return Reference to this logger.
			 */
			virtual Log& Throttle(const Level& level, double rate, std::size_t burst);

			/**
			 * @brief Install a group-scoped Drop rule.
			 * @param group Group selector.
			 * @param rate Lines per second.
			 * @param burst Token capacity.
			 * @return Reference to this logger.
			 */
			virtual Log& Throttle(GroupManip group, double rate, std::size_t burst);

			/**
			 * @brief Install a component-scoped Drop rule.
			 * @param component Component path used as-is.
			 * @param rate Lines per second.
			 * @param burst Token capacity.
			 * @return Reference to this logger.
			 */
			virtual Log& Throttle(ComponentManip component, double rate, std::size_t burst);

			/**
			 * @brief Install an exact component/level/group rule.
			 * @param component Component path used as-is.
			 * @param level Level selector.
			 * @param group Group selector.
			 * @param rate Lines per second.
			 * @param burst Token capacity.
			 * @param policy Count policy.
			 * @param value SampleN or WindowKeep.
			 * @param period WindowPeriod when policy is Window.
			 * @return Reference to this logger.
			 */
			virtual Log& Throttle(ComponentManip component, const Level& level, GroupManip group, double rate, std::size_t burst, ThrottlePolicy policy = ThrottlePolicy::Drop, std::size_t value = 0, std::size_t period = 0);

			/**
			 * @brief Remove all throttle rules.
			 * @return Reference to this logger.
			 */
			virtual Log& NoThrottle();

			/**
			 * @brief Remove a level-scoped rule.
			 * @param level Level selector.
			 * @return Reference to this logger.
			 */
			virtual Log& NoThrottle(const Level& level);

			/**
			 * @brief Remove a group-scoped rule.
			 * @param group Group selector.
			 * @return Reference to this logger.
			 */
			virtual Log& NoThrottle(GroupManip group);

			/**
			 * @brief Remove a component-scoped rule.
			 * @param component Component path used as-is.
			 * @return Reference to this logger.
			 */
			virtual Log& NoThrottle(ComponentManip component);

			/**
			 * @brief Remove an exact component/level/group rule.
			 * @param component Component path used as-is.
			 * @param level Level selector.
			 * @param group Group selector.
			 * @return Reference to this logger.
			 */
			virtual Log& NoThrottle(ComponentManip component, const Level& level, GroupManip group);

			/**
			 * @brief Flush dropped summaries for all throttle rules.
			 * @return Reference to this logger.
			 */
			virtual Log& FlushThrottle();

			/**
			 * @brief Flush dropped summaries for matching throttle rules.
			 * @param spec Selectors of the rules to flush.
			 * @return Reference to this logger.
			 */
			virtual Log& FlushThrottle(const ThrottleSpec& spec);

			/**
			 * @name Streaming Operators
			 */
			//@{

			/**
			 * @brief Stream a numeric or narrow-text payload.
			 * @tparam T Arithmetic type other than wchar_t, string_view, or C string.
			 * @param v Value to write.
			 * @return Reference to this logger.
			 */
			template <typename T>
			inline Log& operator<<(const T& v)
			requires (
				(StormByte::Type::Arithmetic<std::decay_t<T>> && !StormByte::Type::SameAs<std::decay_t<T>, wchar_t>)
				|| StormByte::Type::SameAs<std::decay_t<T>, std::string_view>
				|| StormByte::Type::SameAs<std::decay_t<T>, const char*>
				|| StormByte::Type::SameAs<std::decay_t<T>, char*>
			) {
				if (!WillWrite()) [[likely]]
					return *this;
				if constexpr (StormByte::Type::SameAs<std::decay_t<T>, const char*> || StormByte::Type::SameAs<std::decay_t<T>, char*>)
					WriteValue(static_cast<const char*>(v));
				else if constexpr (StormByte::Type::SameAs<std::decay_t<T>, std::string_view>)
					WriteValue(v);
				else
					WriteValue(static_cast<std::decay_t<T>>(v));
				return *this;
			}

			/**
			 * @brief Stream caller-owned narrow text without passing its std::string object across the DLL boundary.
			 * @param v Text viewed during this call and copied into the logger's line buffer.
			 * @return Reference to this logger.
			 */
			inline Log& operator<<(const std::string& v) {
				if (!WillWrite()) [[likely]]
					return *this;
				WriteValue(std::string_view{v});
				return *this;
			}

			/**
			 * @brief Stream owned UTF-8 bytes.
			 * @param v Buffer owned by Base. Copied into the line buffer.
			 * @return Reference to this logger.
			 */
			inline Log& operator<<(const StormByte::Safe::CString& v) {
				if (!WillWrite()) [[likely]]
					return *this;
				WriteValue(static_cast<std::string_view>(v));
				return *this;
			}

			/**
			 * @brief Stream owned UTF-8 text.
			 * @param v Text owned by Base. Copied into the line buffer.
			 * @return Reference to this logger.
			 */
			inline Log& operator<<(const StormByte::Safe::String& v) {
				if (!WillWrite()) [[likely]]
					return *this;
				WriteValue(static_cast<std::string_view>(v));
				return *this;
			}

			/**
			 * @brief Stream wide text. std::wstring converts to this view.
			 * @param v Wide text to write.
			 * @return Reference to this logger.
			 */
			inline Log& operator<<(std::wstring_view v) {
				if (!WillWrite()) [[likely]]
					return *this;
				Write(v);
				return *this;
			}

			/**
			 * @brief Stream a wide C string.
			 * @param v Text to write; may be null.
			 * @return Reference to this logger.
			 */
			inline Log& operator<<(const wchar_t* v) {
				if (!WillWrite()) [[likely]]
					return *this;
				Write(v);
				return *this;
			}

			/**
			 * @brief Stream owned wide bytes.
			 * @param v Buffer owned by Base. Copied into the line buffer.
			 * @return Reference to this logger.
			 */
			inline Log& operator<<(const StormByte::Safe::WCString& v) {
				if (!WillWrite()) [[likely]]
					return *this;
				Write(static_cast<std::wstring_view>(v));
				return *this;
			}

			/**
			 * @brief Stream owned wide text.
			 * @param v Text owned by Base. Copied into the line buffer.
			 * @return Reference to this logger.
			 */
			inline Log& operator<<(const StormByte::Safe::WString& v) {
				if (!WillWrite()) [[likely]]
					return *this;
				Write(static_cast<std::wstring_view>(v));
				return *this;
			}

			/**
			 * @brief Stream raw bytes.
			 * @param v Contiguous bytes. std::vector<std::byte> converts to this span.
			 * @return Reference to this logger.
			 */
			inline Log& operator<<(std::span<const std::byte> v) {
				if (!WillWrite()) [[likely]]
					return *this;
				Write(v);
				return *this;
			}

			/**
			 * @brief Stream owned bytes.
			 * @param v BinaryData. Same contract as a byte span: Base64, or HexDump when hex is active.
			 * @return Reference to this logger.
			 */
			inline Log& operator<<(const StormByte::BinaryData& v) {
				if (!WillWrite()) [[likely]]
					return *this;
				Write(static_cast<std::span<const std::byte>>(v));
				return *this;
			}

			/**
			 * @brief Stream a byte count.
			 * @param v Size. Uses Size::operator std::string.
			 * @return Reference to this logger.
			 */
			inline Log& operator<<(const StormByte::Size& v) {
				if (!WillWrite()) [[likely]]
					return *this;
				const std::string text = static_cast<std::string>(v);
				WriteValue(std::string_view{text});
				return *this;
			}

			/**
			 * @brief Stream an octet count.
			 * @param v ByteSize. Uses ByteSize::operator std::string.
			 * @return Reference to this logger.
			 */
			inline Log& operator<<(const StormByte::ByteSize& v) {
				if (!WillWrite()) [[likely]]
					return *this;
				const std::string text = static_cast<std::string>(v);
				WriteValue(std::string_view{text});
				return *this;
			}

			/**
			 * @brief Set the level of the current line.
			 * @param level Level to emit.
			 * @return Reference to this logger.
			 */
			inline Log& operator<<(const Level& level) {
				Write(level);
				return *this;
			}

			/**
			 * @brief Apply a stream manipulator (e.g. std::endl).
			 * @param manip Stream manipulator.
			 * @return Reference to this logger.
			 */
			inline Log& operator<<(std::ostream& (*manip)(std::ostream&)) {
				Write(manip);
				return *this;
			}

			/**
			 * @brief Apply a Log manipulator.
			 * @param manip Logger manipulator.
			 * @return Reference to this logger.
			 */
			inline Log& operator<<(Log& (*manip)(Log&) noexcept) {
				Write(manip);
				return *this;
			}

			/**
			 * @brief Apply redaction policy.
			 * @param m Redaction manipulator.
			 * @return Reference to this logger.
			 */
			inline Log& operator<<(RedactManip m) {
				Write(m);
				return *this;
			}

			/**
			 * @brief Dump subsequent payloads as hex bytes until nohex.
			 * @param m Hex manipulator.
			 * @return Reference to this logger.
			 */
			inline Log& operator<<(HexManip m) {
				Write(m);
				return *this;
			}

			/**
			 * @brief Disable hex dumps.
			 * @param m No-hex manipulator.
			 * @return Reference to this logger.
			 */
			inline Log& operator<<(NoHexManip m) {
				Write(m);
				return *this;
			}

			/**
			 * @brief Apply a configured or explicit content color.
			 * @param manip Color manipulator.
			 * @return Reference to this logger.
			 */
			inline Log& operator<<(ColorManip manip) {
				Write(manip);
				return *this;
			}

			/**
			 * @brief Disable color for subsequent content.
			 * @param manip No-color manipulator.
			 * @return Reference to this logger.
			 */
			inline Log& operator<<(NoColorManip manip) {
				Write(manip);
				return *this;
			}

			/**
			 * @brief Save the current format and activate a temporary format.
			 * @param manip Format manipulator.
			 * @return Reference to this logger.
			 */
			inline Log& operator<<(FormatManip manip) {
				Write(manip);
				return *this;
			}

			/**
			 * @brief Restore the most recently saved format.
			 * @param manip Pop-format manipulator.
			 * @return Reference to this logger.
			 */
			inline Log& operator<<(PopFormatManip manip) {
				Write(manip);
				return *this;
			}

			/**
			 * @brief Set the producer group for the current line.
			 * @param manip Group manipulator.
			 * @return Reference to this logger.
			 */
			inline Log& operator<<(GroupManip manip) {
				Write(manip);
				return *this;
			}

			/**
			 * @brief Push a component segment onto the current thread's stack.
			 * @param manip Component manipulator.
			 * @return Reference to this logger.
			 */
			inline Log& operator<<(ComponentManip manip) {
				Write(manip);
				return *this;
			}

			/**
			 * @brief Pop one component segment from the current thread's stack.
			 * @param manip Pop-component manipulator.
			 * @return Reference to this logger.
			 */
			inline Log& operator<<(PopComponentManip manip) {
				Write(manip);
				return *this;
			}

			/**
			 * @brief Clear the current thread's component stack.
			 * @param manip Reset-component manipulator.
			 * @return Reference to this logger.
			 */
			inline Log& operator<<(ResetComponentManip manip) {
				Write(manip);
				return *this;
			}

			//@}

		protected:
			/**
			 * @brief Construct a logger that emits through caller callbacks.
			 * @param write Receives raw bytes. May be null.
			 * @param manip Applies an ostream manipulator in the caller. May be null.
			 * @param context Passed back to the callbacks. Not owned. Must outlive this logger.
			 * @param level Minimum Level that will be emitted.
			 * @param format Header format string.
			 */
			Log(SinkWrite write, SinkManip manip, void* context, const Level& level, std::string_view format);

			std::shared_ptr<Engine> m_engine;			///< Shared backend
			StormByte::Safe::String m_scope_path;			///< Sticky component path; empty = root facade

			/**
			 * @brief Whether the current line level will be written.
			 * @return true if the current level is at or above the print floor.
			 */
			bool WillWrite() const noexcept;

			/**
			 * @brief Decide throttle admission before a payload claims a line lock.
			 * @return true when the current line may emit.
			 */
			bool PrepareLine();

			/**
			 * @brief Whether output has already started for the current line.
			 * @return true when the line header has been emitted.
			 */
			bool HasOpenOutputLine() const noexcept;

			/**
			 * @brief Whether throttle has already decided the current line.
			 * @return true when the line decision exists.
			 */
			bool LineDecided() const noexcept;

			/**
			 * @brief Whether the decided current line is admitted.
			 * @return true when payload output is allowed.
			 */
			bool LineAdmitted() const noexcept;

			/**
			 * @brief Admit a numeric or narrow-text payload. Log always admits.
			 * @return false when ThreadedLog drops the payload.
			 */
			virtual bool BeginPayload();

			/**
			 * @brief Forward a payload after BeginPayload.
			 * @tparam T Type accepted by Engine::operator<<.
			 * @param v Value to write.
			 */
			template <typename T>
			void WriteValue(const T& v);

			/**
				 * @brief Deep-copy this facade into a @ref StormByte::Safe::Shared.
			 * @return Pointer to the clone.
			 */
			PointerType Clone() const override;

			/**
				 * @brief Move this facade into a @ref StormByte::Safe::Shared.
			 * @return Pointer to the new facade.
			 */
			PointerType Move() override;

			/**
			 * @brief Forward wide text.
			 * @param v Text to write.
			 */
			virtual void Write(std::wstring_view v);

			/**
			 * @brief Forward a wide C string.
			 * @param v Text to write; may be null.
			 */
			virtual void Write(const wchar_t* v);

			/**
			 * @brief Forward raw bytes.
			 * @param v Contiguous bytes to format as Base64 or hex.
			 */
			virtual void Write(std::span<const std::byte> v);

			/**
			 * @brief Forward a level change.
			 * @param level Level of the current line.
			 */
			virtual void Write(const Level& level);

			/**
			 * @brief Forward a stream manipulator.
			 * @param manip Stream manipulator.
			 */
			virtual void Write(std::ostream& (*manip)(std::ostream&));

			/**
			 * @brief Forward a Log manipulator.
			 * @param manip Logger manipulator.
			 */
			virtual void Write(Log& (*manip)(Log&) noexcept);

			/**
			 * @brief Forward redaction state.
			 * @param m Redaction manipulator.
			 */
			virtual void Write(RedactManip m);

			/**
			 * @brief Forward hex-dump state.
			 * @param m Hex manipulator.
			 */
			virtual void Write(HexManip m);

			/**
			 * @brief Forward hex-dump disable.
			 * @param m No-hex manipulator.
			 */
			virtual void Write(NoHexManip m);

			/**
			 * @brief Forward a color manipulator.
			 * @param manip Color manipulator.
			 */
			virtual void Write(ColorManip manip);

			/**
			 * @brief Forward a no-color manipulator.
			 * @param manip No-color manipulator.
			 */
			virtual void Write(NoColorManip manip);

			/**
			 * @brief Forward a push-format manipulator.
			 * @param manip Format manipulator.
			 */
			virtual void Write(FormatManip manip);

			/**
			 * @brief Forward a pop-format manipulator.
			 * @param manip Pop-format manipulator.
			 */
			virtual void Write(PopFormatManip manip);

			/**
			 * @brief Forward a group manipulator.
			 * @param manip Group manipulator.
			 */
			virtual void Write(GroupManip manip);

			/**
			 * @brief Forward a component push manipulator.
			 * @param manip Component manipulator.
			 */
			virtual void Write(ComponentManip manip);

			/**
			 * @brief Forward a component pop manipulator.
			 * @param manip Pop-component manipulator.
			 */
			virtual void Write(PopComponentManip manip);

			/**
			 * @brief Forward a reset-component manipulator.
			 * @param manip Reset-component manipulator.
			 */
			virtual void Write(ResetComponentManip manip);
	};

	/// @cond
	extern template STORMBYTE_LOGGER_PUBLIC void Log::WriteValue<bool>(const bool& v);
	extern template STORMBYTE_LOGGER_PUBLIC void Log::WriteValue<char>(const char& v);
	extern template STORMBYTE_LOGGER_PUBLIC void Log::WriteValue<signed char>(const signed char& v);
	extern template STORMBYTE_LOGGER_PUBLIC void Log::WriteValue<unsigned char>(const unsigned char& v);
	extern template STORMBYTE_LOGGER_PUBLIC void Log::WriteValue<short>(const short& v);
	extern template STORMBYTE_LOGGER_PUBLIC void Log::WriteValue<unsigned short>(const unsigned short& v);
	extern template STORMBYTE_LOGGER_PUBLIC void Log::WriteValue<int>(const int& v);
	extern template STORMBYTE_LOGGER_PUBLIC void Log::WriteValue<unsigned int>(const unsigned int& v);
	extern template STORMBYTE_LOGGER_PUBLIC void Log::WriteValue<long>(const long& v);
	extern template STORMBYTE_LOGGER_PUBLIC void Log::WriteValue<unsigned long>(const unsigned long& v);
	extern template STORMBYTE_LOGGER_PUBLIC void Log::WriteValue<long long>(const long long& v);
	extern template STORMBYTE_LOGGER_PUBLIC void Log::WriteValue<unsigned long long>(const unsigned long long& v);
	extern template STORMBYTE_LOGGER_PUBLIC void Log::WriteValue<float>(const float& v);
	extern template STORMBYTE_LOGGER_PUBLIC void Log::WriteValue<double>(const double& v);
	extern template STORMBYTE_LOGGER_PUBLIC void Log::WriteValue<long double>(const long double& v);
	extern template STORMBYTE_LOGGER_PUBLIC void Log::WriteValue<std::string_view>(const std::string_view& v);
	extern template STORMBYTE_LOGGER_PUBLIC void Log::WriteValue<const char*>(const char* const& v);
	/// @endcond

	/**
	 * @brief Pointer-like owner of `Log` or a derived logger.
	 *
		 * Matches `std::shared_ptr`, `std::unique_ptr`, @ref StormByte::Safe::Shared and
		 * @ref StormByte::Safe::Unique, including a const owner captured by a lambda.
	 * `Ptr` is deduced from `Ptr&`, so a const argument deduces a const pointer type.
	 *
	 * @tparam Ptr Pointer type.
	 */
	template<typename Ptr>
	concept LogPointer =
		StormByte::Type::NullablePointer<Ptr>
		&& StormByte::Type::DerivedFrom<typename std::remove_cvref_t<Ptr>::element_type, Log>;

	/**
	 * @brief Stream a value into a smart pointer to Log or a derived logger.
	 * @tparam Ptr `std::shared_ptr`, `std::unique_ptr`, @ref StormByte::Safe::Shared or @ref StormByte::Safe::Unique whose element type derives from Log.
	 * @tparam T Value type.
	 * @param logger Smart pointer to the logger. An empty owner is a no-op.
	 * @param value Value to stream.
	 * @return Reference to the smart pointer.
	 */
	template <typename Ptr, typename T>
	Ptr& operator<<(Ptr& logger, const T& value) noexcept
		requires LogPointer<Ptr> {
		if (logger)
			*logger << value;
		return logger;
	}

	/**
	 * @brief Stream a Level into a smart pointer to Log or a derived logger.
	 * @tparam Ptr `std::shared_ptr`, `std::unique_ptr`, @ref StormByte::Safe::Shared or @ref StormByte::Safe::Unique whose element type derives from Log.
	 * @param logger Smart pointer to the logger. An empty owner is a no-op.
	 * @param level Level to set.
	 * @return Reference to the smart pointer.
	 */
	template <typename Ptr>
	Ptr& operator<<(Ptr& logger, const Level& level) noexcept
		requires LogPointer<Ptr> {
		if (logger)
			*logger << level;
		return logger;
	}

	/**
	 * @brief Stream a stream manipulator into a smart pointer to Log or a derived logger.
	 *
	 * A dedicated overload so overloaded manipulators such as `std::endl` can be resolved.
	 *
	 * @tparam Ptr `std::shared_ptr`, `std::unique_ptr`, @ref StormByte::Safe::Shared or @ref StormByte::Safe::Unique whose element type derives from Log.
	 * @param logger Smart pointer to the logger. An empty owner is a no-op.
	 * @param manip Stream manipulator.
	 * @return Reference to the smart pointer.
	 */
	template <typename Ptr>
	Ptr& operator<<(Ptr& logger, std::ostream& (*manip)(std::ostream&)) noexcept
		requires LogPointer<Ptr> {
		if (logger)
			*logger << manip;
		return logger;
	}
}
