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
 * thirdparty/, in particular the bundled StormByte Base tree), which remains
 * under its own license.
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

#include <StormByte/logger/visibility.h>
#include <StormByte/safe/function.hxx>
#include <StormByte/safe/string.hxx>

#include <ostream>

/**
 * @namespace StormByte::Logger
 * @brief Logger module of the StormByte suite.
 */
namespace StormByte::Logger {
	/**
	 * @enum Color
	 * @brief ANSI foreground colors supported by the logger.
	 *
	 * @c Default emits no ANSI sequence and leaves the terminal color unchanged.
	 */
	enum class STORMBYTE_LOGGER_PUBLIC Color : unsigned char {
		Default,        ///< No ANSI sequence; preserve the terminal's current color.
		Black,          ///< Standard black foreground.
		Red,            ///< Standard red foreground.
		Green,          ///< Standard green foreground.
		Yellow,         ///< Standard yellow foreground.
		Blue,           ///< Standard blue foreground.
		Magenta,        ///< Standard magenta foreground.
		Cyan,           ///< Standard cyan foreground.
		Gray,           ///< Bright black/gray foreground.
		White,          ///< Standard white foreground.
		BrightBlack,    ///< Bright black foreground.
		BrightRed,      ///< Bright red foreground.
		BrightGreen,    ///< Bright green foreground.
		BrightYellow,   ///< Bright yellow foreground.
		BrightBlue,     ///< Bright blue foreground.
		BrightMagenta,  ///< Bright magenta foreground.
		BrightCyan,     ///< Bright cyan foreground.
		BrightWhite     ///< Bright white foreground.
	};

	/**
	 * @enum Level
	 * @brief Severity levels used by the logger.
	 *
	 * Ordered from least to most severe. Used both as the print floor
	 * and as the level of the current message. Warning, Error and Fatal
	 * are always emitted regardless of the configured print floor.
	 */
	enum class STORMBYTE_LOGGER_PUBLIC Level : unsigned short {
		LowLevel = 0,   ///< Verbose diagnostics
		Debug,          ///< Debug information
		Warning,        ///< Recoverable problems
		Notice,         ///< Significant normal events
		Info,           ///< Informational messages
		Error,          ///< Error conditions
		Fatal           ///< Unrecoverable errors
	};

	/**
	 * @brief Convert a Level to a short name.
	 * @param l Level to convert.
	 * @return Name such as "Info" or "Error". A string literal, not an owning string.
	 */
	constexpr static const char* LevelToString(const Level& l) noexcept {
		switch (l) {
			case Level::LowLevel:	return "LowLevel";
			case Level::Debug:		return "Debug";
			case Level::Warning:	return "Warning";
			case Level::Notice:		return "Notice";
			case Level::Info:		return "Info";
			case Level::Error:		return "Error";
			case Level::Fatal:		return "Fatal";
			default:				return "Error";
		}
	}

	/**
	 * @brief Caller-side write of raw bytes into the sink.
	 *
	 * The function runs in the module that created the logger, not in this DLL.
	 */
	using SinkWrite = void (*)(void* context, const char* data, std::size_t size);

	/**
	 * @brief Caller-side application of an `std::ostream` manipulator.
	 *
	 * Used for `std::endl` and any other stream manipulator. The call happens
	 * in the module that owns the stream.
	 */
	using SinkManip = void (*)(void* context, std::ostream& (*manip)(std::ostream&));

	/**
	 * @brief Typed text callback whose context is released in its creator module.
	 */
	using SinkFunction = StormByte::Safe::Function<void(const StormByte::Safe::String&)>;
}
