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

#include <StormByte/logger/log.hxx>
#include <StormByte/thread_lock.hxx>

#include <memory>
#include <ostream>
#include <span>
#include <string_view>

/**
 * @namespace StormByte::Logger
 * @brief Logger module of the StormByte suite.
 */
namespace StormByte::Logger {
	/**
	 * @class ThreadedLog
	 * @brief Thread-safe logging facade.
	 *
	 * Serializes logical lines (until a newline manipulator) so concurrent
	 * writers do not interleave. Filtered messages do not hold the line lock.
	 *
	 * Payload operator<< is inherited from Log. Numeric and narrow-text
	 * payloads call Log::WriteValue, which uses BeginPayload. This class only
	 * overrides BeginPayload plus the Writes that must run work before the lock
	 * or that drop the lock on newline.
	 *
	 * Scope clones this type and shares both the Engine and the line lock.
	 */
	class STORMBYTE_LOGGER_PUBLIC ThreadedLog : public Log {
		public:
			/**
			 * @brief Construct a ThreadedLog writing to out.
			 * @param out Output stream. Must outlive this logger.
			 * @param level Minimum Level that will be emitted.
			 * @param format Header format string (%L, %T, %i, %c, %g).
			 */
			ThreadedLog(std::ostream& out, const Level& level = Level::Info, std::string_view format = "[%L] %T")
				: ThreadedLog(&OStreamWrite, &OStreamManip, &out, level, format) {}

			/**
			 * @brief Copy constructor.
			 * @note Shares the Engine and the line lock. Copies the sticky path.
			 */
			ThreadedLog(const ThreadedLog&) = default;

			/**
			 * @brief Move constructor.
			 */
			ThreadedLog(ThreadedLog&&) noexcept = default;

			/**
			 * @brief Destructor. Defined out of line so the line lock is released inside the DLL.
			 */
			~ThreadedLog() noexcept override;

			/**
			 * @brief Copy assignment. Defined out of line so the previous line lock is released inside the DLL.
			 * @return Reference to this logger.
			 * @note Shares the Engine and the line lock.
			 */
			ThreadedLog& operator=(const ThreadedLog&);

			/**
			 * @brief Move assignment. Defined out of line so the previous line lock is released inside the DLL.
			 * @return Reference to this logger.
			 */
			ThreadedLog& operator=(ThreadedLog&&) noexcept;

			/**
			 * @brief Set a level color while holding the line lock.
			 * @param level Level whose color is changed.
			 * @param color Color to use for that level.
			 * @return Reference to this logger.
			 */
			Log& Color(const Level& level, const StormByte::Logger::Color& color) override;

			/**
			 * @brief Get the configured color for a logging level.
			 * @param level Level whose color is requested.
			 * @return Configured color.
			 */
			StormByte::Logger::Color Color(const Level& level) const override;

			/**
			 * @brief Set a component color override under the line lock.
			 * @param component Component path.
			 * @param level Level whose color is changed.
			 * @param color Color to use for that component and level.
			 * @return Reference to this logger.
			 */
			Log& Color(std::string_view component, const Level& level, const StormByte::Logger::Color& color) override;

			/**
			 * @brief Get a component color, falling back along the path then to the general color.
			 * @param component Component path.
			 * @param level Level whose color is requested.
			 * @return Component override or general color.
			 */
			StormByte::Logger::Color Color(std::string_view component, const Level& level) const override;

			/**
			 * @brief Set the header format under the line lock.
			 * @param format Format used by default, or for the sticky path on a scoped facade.
			 * @return Reference to this logger.
			 */
			Log& Format(std::string_view format) override;

			/**
			 * @brief Get the effective current header format.
			 * @return Owned copy of the temporary, component-specific or general format.
			 */
					StormByte::Safe::String Format() const override;

			/**
			 * @brief Set or remove a component-specific header format under the line lock.
			 * @param component Component path.
			 * @param format Format, or empty to remove the override.
			 * @return Reference to this logger.
			 */
			Log& Format(std::string_view component, std::string_view format) override;

			/**
			 * @brief Get a component-specific format, falling back along the path then to general.
			 * @param component Component path.
			 * @return Owned copy of the component format or general format.
			 */
					StormByte::Safe::String Format(std::string_view component) const override;

			/**
			 * @brief Install a throttle rule under the line lock.
			 * @param spec Rule to install.
			 * @return Reference to this logger.
			 */
			Log& Throttle(const ThrottleSpec& spec) override;

			/**
			 * @brief Remove a throttle rule under the line lock.
			 * @param spec Selectors of the rule to remove.
			 * @return Reference to this logger.
			 */
			Log& NoThrottle(const ThrottleSpec& spec) override;

			/**
			 * @brief Install a Drop rule.
			 * @param rate Lines per second.
			 * @param burst Token capacity.
			 * @return Reference to this logger.
			 */
			Log& Throttle(double rate, std::size_t burst) override;

			/**
			 * @brief Install a Sample or Window rule.
			 * @param rate Lines per second.
			 * @param burst Token capacity.
			 * @param policy Sample or Window.
			 * @param value SampleN or WindowKeep.
			 * @param period WindowPeriod when policy is Window.
			 * @return Reference to this logger.
			 */
			Log& Throttle(double rate, std::size_t burst, ThrottlePolicy policy, std::size_t value, std::size_t period = 0) override;

			/**
			 * @brief Install a level-scoped Drop rule.
			 * @param level Level selector.
			 * @param rate Lines per second.
			 * @param burst Token capacity.
			 * @return Reference to this logger.
			 */
			Log& Throttle(const Level& level, double rate, std::size_t burst) override;

			/**
			 * @brief Install a group-scoped Drop rule.
			 * @param group Group selector.
			 * @param rate Lines per second.
			 * @param burst Token capacity.
			 * @return Reference to this logger.
			 */
			Log& Throttle(GroupManip group, double rate, std::size_t burst) override;

			/**
			 * @brief Install a component-scoped Drop rule.
			 * @param component Component path.
			 * @param rate Lines per second.
			 * @param burst Token capacity.
			 * @return Reference to this logger.
			 */
			Log& Throttle(ComponentManip component, double rate, std::size_t burst) override;

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
			Log& Throttle(ComponentManip component, const Level& level, GroupManip group, double rate, std::size_t burst, ThrottlePolicy policy = ThrottlePolicy::Drop, std::size_t value = 0, std::size_t period = 0) override;

			/**
			 * @brief Remove all throttle rules.
			 * @return Reference to this logger.
			 */
			Log& NoThrottle() override;

			/**
			 * @brief Remove a level-scoped rule.
			 * @param level Level selector.
			 * @return Reference to this logger.
			 */
			Log& NoThrottle(const Level& level) override;

			/**
			 * @brief Remove a group-scoped rule.
			 * @param group Group selector.
			 * @return Reference to this logger.
			 */
			Log& NoThrottle(GroupManip group) override;

			/**
			 * @brief Remove a component-scoped rule.
			 * @param component Component path.
			 * @return Reference to this logger.
			 */
			Log& NoThrottle(ComponentManip component) override;

			/**
			 * @brief Remove an exact component/level/group rule.
			 * @param component Component path used as-is.
			 * @param level Level selector.
			 * @param group Group selector.
			 * @return Reference to this logger.
			 */
			Log& NoThrottle(ComponentManip component, const Level& level, GroupManip group) override;

			/**
			 * @brief Flush all dropped summaries under the line lock.
			 * @return Reference to this logger.
			 */
			Log& FlushThrottle() override;

			/**
			 * @brief Flush matching dropped summaries under the line lock.
			 * @param spec Selectors of the rules to flush.
			 * @return Reference to this logger.
			 */
			Log& FlushThrottle(const ThrottleSpec& spec) override;

		protected:
			/**
					 * @brief Deep-copy this facade into a @ref StormByte::Safe::Shared.
			 * @return Pointer to a ThreadedLog that shares Engine and line lock.
			 */
			PointerType Clone() const override;

			/**
					 * @brief Move this facade into a @ref StormByte::Safe::Shared.
			 * @return Pointer to a ThreadedLog that shares Engine and line lock.
			 */
			PointerType Move() override;

			/**
			 * @brief Admit a payload and take the line lock.
			 * @return false when the payload is filtered or throttled.
			 */
			bool BeginPayload() override;

			/**
			 * @brief Encode wide text before taking the line lock.
			 * @param v Text to write.
			 */
			void Write(std::wstring_view v) override;

			/**
			 * @brief Encode a wide C string before taking the line lock.
			 * @param v Text to write; may be null.
			 */
			void Write(const wchar_t* v) override;

			/**
			 * @brief Format raw bytes before taking the line lock.
			 * @param v Contiguous bytes to format as Base64 or hex.
			 */
			void Write(std::span<const std::byte> v) override;

			/**
			 * @brief Forward a level change under the line lock.
			 * @param level Level of the current line.
			 */
			void Write(const Level& level) override;

			/**
			 * @brief Forward a stream manipulator; newline drops the line lock.
			 * @param manip Stream manipulator.
			 */
			void Write(std::ostream& (*manip)(std::ostream&)) override;

			/**
			 * @brief Forward a Log manipulator under the line lock.
			 * @param manip Logger manipulator.
			 */
			void Write(Log& (*manip)(Log&) noexcept) override;

			/**
			 * @brief Apply redaction state under the line lock.
			 * @param m Redaction manipulator.
			 */
			void Write(RedactManip m) override;

			/**
			 * @brief Apply hex-dump state under the line lock.
			 * @param m Hex manipulator.
			 */
			void Write(HexManip m) override;

			/**
			 * @brief Disable hex dumps under the line lock.
			 * @param m No-hex manipulator.
			 */
			void Write(NoHexManip m) override;

			/**
			 * @brief Apply a color manipulator under the line lock.
			 * @param manip Color manipulator.
			 */
			void Write(ColorManip manip) override;

			/**
			 * @brief Apply a no-color manipulator under the line lock.
			 * @param manip No-color manipulator.
			 */
			void Write(NoColorManip manip) override;

			/**
			 * @brief Apply a push-format manipulator under the line lock.
			 * @param manip Format manipulator.
			 */
			void Write(FormatManip manip) override;

			/**
			 * @brief Apply a pop-format manipulator under the line lock.
			 * @param manip Pop-format manipulator.
			 */
			void Write(PopFormatManip manip) override;

		protected:
			/**
			 * @brief Construct a threaded logger that emits through caller callbacks.
			 * @param write Receives raw bytes.
			 * @param manip Applies an ostream manipulator in the caller.
			 * @param context Passed back to the callbacks. Not owned.
			 * @param level Minimum Level that will be emitted.
			 * @param format Header format string.
			 */
			ThreadedLog(SinkWrite write, SinkManip manip, void* context, const Level& level, std::string_view format);

		private:
			std::shared_ptr<ThreadLock> m_lock;	///< Shared line lock (copy and Scope share it)
	};
}
