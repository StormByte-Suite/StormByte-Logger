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

#include <StormByte/logger/typedefs.hxx>
#include <StormByte/logger/visibility.h>
#include <StormByte/safe/string.hxx>

#include <cstddef>
#include <optional>
#include <string_view>

/**
 * @namespace StormByte::Logger
 * @brief Logger module of the StormByte suite.
 */
namespace StormByte::Logger {
	class Log;

	/**
	 * @enum ThrottlePolicy
	 * @brief Count policy applied before the optional rate bucket.
	 */
	enum class STORMBYTE_LOGGER_PUBLIC ThrottlePolicy : unsigned char {
		Drop,   ///< Admit while the rate bucket has credit.
		Sample, ///< Admit the first and then one line of every sample period.
		Window  ///< Admit the first WindowKeep lines of every WindowPeriod.
	};

	/**
	 * @struct ThrottleSpec
	 * @brief Immutable rule description used by Log::Throttle.
	 *
	 * An absent Component, Level or Group is a wildcard. An engaged empty
	 * Component or Group selects the root/empty key.
	 */
	struct STORMBYTE_LOGGER_PUBLIC ThrottleSpec {
		std::optional<StormByte::Safe::String> Component; ///< Optional component selector.
		std::optional<Level> Level;							///< Optional level selector.
		std::optional<StormByte::Safe::String> Group;		///< Optional group selector.
		double Rate = 0.0;									///< Lines per second; zero disables refill.
		std::size_t Burst = 0;								///< Initial and maximum token capacity.
		ThrottlePolicy Policy = ThrottlePolicy::Drop;		///< Count policy.
		std::size_t SampleN = 0;							///< Sample period when Policy is Sample.
		std::size_t WindowKeep = 0;							///< Kept lines when Policy is Window.
		std::size_t WindowPeriod = 0;						///< Window size when Policy is Window.
	};

	/**
	 * @struct GroupManip
	 * @brief Labels the current logging line with a producer group.
	 */
	struct STORMBYTE_LOGGER_PUBLIC GroupManip {
		StormByte::Safe::String name; ///< Group name; an empty name clears the current group.
	};

	/**
	 * @brief Set the producer group for the current line.
	 * @param name Group name, or empty text to clear the group.
	 * @return Group manipulator carrying the requested name.
	 */
	STORMBYTE_LOGGER_PUBLIC GroupManip group(StormByte::Safe::String name);

	/**
	 * @brief Set the producer group from caller-owned text.
	 * @param name Group name viewed in the caller; copied into an owned String.
	 * @return Group manipulator carrying the requested name.
	 */
	inline GroupManip group(std::string_view name) {
		return group(StormByte::Safe::String{name});
	}

	/**
	 * @struct ComponentManip
	 * @brief Selects the sticky component for the current thread.
	 *
	 * The component is thread-local to the calling thread, not tied to a
	 * particular Log instance. Two Log instances used by one thread therefore
	 * observe the same component; this is intentional for shared logger use.
	 */
	struct STORMBYTE_LOGGER_PUBLIC ComponentManip {
		StormByte::Safe::String name; ///< Component name; empty selects the root component.
	};

	/**
	 * @brief Select the component associated with subsequent log lines on this thread.
	 * @param name Component name; empty selects the root component.
	 * @return Component manipulator carrying the requested name.
	 * @note An empty component is allowed for compatibility, but @ref reset_component
	 *       is preferred when returning to the root component explicitly.
	 */
	STORMBYTE_LOGGER_PUBLIC ComponentManip component(StormByte::Safe::String name);

	/**
	 * @brief Select the component from caller-owned text.
	 * @param name Component name viewed in the caller; copied into an owned String.
	 * @return Component manipulator carrying the requested name.
	 */
	inline ComponentManip component(std::string_view name) {
		return component(StormByte::Safe::String{name});
	}

	/**
	 * @struct ResetComponentManip
	 * @brief Clears the component associated with the current thread.
	 *
	 * This is the canonical way to return to the root component.
	 */
	struct STORMBYTE_LOGGER_PUBLIC ResetComponentManip {};

	/**
	 * @brief Clear the current thread's component.
	 * @note The reset is thread-local and does not affect other threads.
	 */
	inline constexpr ResetComponentManip reset_component{};

	/**
	 * @struct PopComponentManip
	 * @brief Pops one segment from the current thread's component stack.
	 *
	 * Does nothing when the stack is empty. Does not affect a Scope facade path.
	 */
	struct STORMBYTE_LOGGER_PUBLIC PopComponentManip {};

	/**
	 * @brief Pop one component segment from the current thread's stack.
	 * @note Thread-local; does not affect other threads or Scope facades.
	 */
	inline constexpr PopComponentManip pop_component{};

	/**
	 * @struct FormatManip
	 * @brief Temporarily replaces the logger format and saves the previous one.
	 */
	struct STORMBYTE_LOGGER_PUBLIC FormatManip {
		StormByte::Safe::String format; ///< Temporary format, including an empty format if requested.
	};

	/**
	 * @struct PopFormatManip
	 * @brief Restores the most recently saved logger format.
	 */
	struct STORMBYTE_LOGGER_PUBLIC PopFormatManip {};

	/**
	 * @brief Save the current format and activate a temporary format.
	 * @param format Format to activate until pop_format is streamed.
	 * @return Format manipulator containing the requested format.
	 */
	STORMBYTE_LOGGER_PUBLIC FormatManip push_format(StormByte::Safe::String format);

	/**
	 * @brief Save the current format and activate a temporary format from caller-owned text.
	 * @param format Format viewed in the caller; copied into an owned String.
	 * @return Format manipulator containing the requested format.
	 */
	inline FormatManip push_format(std::string_view format) {
		return push_format(StormByte::Safe::String{format});
	}

	/**
	 * @brief Restore the most recently saved format, or do nothing if empty.
	 */
	inline constexpr PopFormatManip pop_format{};

	/**
	 * @struct ColorManip
	 * @brief Temporarily selects a configured or explicit content color.
	 */
	struct STORMBYTE_LOGGER_PUBLIC ColorManip {
		std::optional<Color> value; ///< Explicit color, or empty for the configured level color.

		/**
		 * @brief Select an explicit color for subsequent content.
		 * @param selected Color to use until another color manipulator or endl.
		 * @return A color manipulator carrying the selected color.
		 */
		constexpr ColorManip operator()(Color selected) const noexcept {
			return ColorManip{selected};
		}
	};

	/**
	 * @brief Restore the configured color for the current level.
	 */
	inline constexpr ColorManip color{};

	/**
	 * @struct NoColorManip
	 * @brief Disables color for subsequent content until changed.
	 */
	struct STORMBYTE_LOGGER_PUBLIC NoColorManip {};

	/**
	 * @brief Disable color for subsequent content in the current line.
	 */
	inline constexpr NoColorManip nocolor{};

	/**
	 * @brief Stateful redaction manipulator.
	 *
	 * - count == 0: every character becomes '*'.
	 * - keep_first == false (default): last `count` characters stay readable.
	 * - keep_first == true: first `count` characters stay readable.
	 *
	 * Applies to both text and numbers (numbers are converted to string first).
	 * Remains active until @ref noredact.
	 *
	 * Usage:
	 * @code
	 * log << redact << secret << std::endl;              // full mask
	 * log << redact(4) << token << std::endl;            // keep last 4
	 * log << redact_first(4) << token << std::endl;      // keep first 4
	 * log << noredact << plain << std::endl;
	 * @endcode
	 */
	struct STORMBYTE_LOGGER_PUBLIC RedactManip {
		std::size_t count = 0;     ///< 0 = mask all; N = keep N characters
		bool keep_first = false;   ///< true = keep first N, false = keep last N characters

		/**
		 * @brief Build a manipulator that keeps the last @p n characters visible.
		 * @param n Number of trailing characters to keep unmasked.
		 * @return A new RedactManip configured for keep-last.
		 */
		constexpr RedactManip operator()(std::size_t n) const noexcept {
			return RedactManip{ n, false };
		}
	};

	/**
	 * @brief Full redaction manipulator (mask everything).
	 * @see RedactManip
	 */
	inline constexpr RedactManip redact{};

	/**
	 * @brief Build a manipulator that keeps the first @p n characters visible.
	 * @param n Number of leading characters to keep unmasked.
	 * @return A RedactManip configured for keep-first.
	 */
	constexpr RedactManip redact_first(std::size_t n) noexcept {
		return RedactManip{ n, true };
	}

	/**
	 * @struct HexManip
	 * @brief Dump subsequent payloads as hex bytes until @ref nohex.
	 *
	 * The payload is converted as usual (numbers / bool to text, wide text to
	 * UTF-8) and then each byte is printed as @c 0xAA separated by spaces.
	 * Byte spans and @ref StormByte::BinaryData do not use that text dump:
	 * with this manipulator active they use @ref StormByte::BinaryData::HexDump
	 * and @c columns is the row width. Without it they stay Base64.
	 * @c columns is the number of bytes per continuation row; @c 0 disables
	 * wrapping. Continuations write a raw newline without a new header and
	 * without ending the logical line.
	 *
	 * Encoding runs before redaction.
	 *
	 * Usage:
	 * @code
	 * log << hex << "AB" << std::endl;       // 0x41 0x42  (16-byte rows)
	 * log << hex(2) << "ABCD" << std::endl;  // 0x41 0x42\\n0x43 0x44
	 * log << nohex << "plain" << std::endl;
	 * @endcode
	 */
	struct STORMBYTE_LOGGER_PUBLIC HexManip {
		std::size_t columns = 16; ///< Bytes per row; 0 = single line.

		/**
		 * @brief Build a manipulator with @p n bytes per row.
		 * @param n Bytes per continuation row; 0 disables wrapping.
		 * @return A HexManip with the requested width.
		 */
		constexpr HexManip operator()(std::size_t n) const noexcept {
			return HexManip{ n };
		}
	};

	/**
	 * @brief Enable hex dumps with 16 bytes per row.
	 */
	inline constexpr HexManip hex{};

	/**
	 * @struct NoHexManip
	 * @brief Disable hex dumps and restore default payload formatting.
	 */
	struct STORMBYTE_LOGGER_PUBLIC NoHexManip {};

	/**
	 * @brief Disable hex dumps for subsequent payloads.
	 */
	inline constexpr NoHexManip nohex{};

	/**
	 * @brief Enable human-readable formatting for numeric values.
	 * @param log The Log instance to modify.
	 * @return Reference to the same Log.
	 */
	STORMBYTE_LOGGER_PUBLIC Log& humanreadable_number(Log& log) noexcept;

	/**
	 * @brief Enable human-readable formatting for byte counts.
	 * @param log The Log instance to modify.
	 * @return Reference to the same Log.
	 */
	STORMBYTE_LOGGER_PUBLIC Log& humanreadable_bytes(Log& log) noexcept;

	/**
	 * @brief Disable human-readable formatting (raw numbers).
	 * @param log The Log instance to modify.
	 * @return Reference to the same Log.
	 */
	STORMBYTE_LOGGER_PUBLIC Log& nohumanreadable(Log& log) noexcept;

	/**
	 * @brief Disable redaction until the next redact / redact(n) / redact_first(n).
	 * @param log The Log instance to modify.
	 * @return Reference to the same Log.
	 */
	STORMBYTE_LOGGER_PUBLIC Log& noredact(Log& log) noexcept;
}
