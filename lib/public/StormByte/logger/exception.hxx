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

#include <StormByte/exception.hxx>
#include <StormByte/logger/visibility.h>
#include <StormByte/type_traits/safe.hxx>

#include <format>
#include <string>
#include <utility>

/**
 * @namespace StormByte::Logger
 * @brief Logger module of the StormByte suite.
 */
namespace StormByte::Logger {
	/**
	 * @class Exception
	 * @brief Root exception for Logger. `what()` is `StormByte.Logger: message`.
	 *
	 * Forwards the format and the arguments. Does not format. A child segment
	 * is prepended under `Logger`.
	 */
	class STORMBYTE_LOGGER_PUBLIC Exception: public StormByte::Exception {
		public:
			/**
			 * @brief Format under `StormByte.Logger`.
			 * @tparam Args Format argument types.
			 * @param fmt Format string.
			 * @param args Format arguments.
			 */
			template <typename... Args>
			explicit Exception(std::format_string<Args...> fmt, Args&&... args)
				: StormByte::Exception(StormByte::Exception::Path{"Logger"}, fmt, std::forward<Args>(args)...) {}

			/**
			 * @brief Destructor. Defined in this module so `catch` matches across a DLL.
			 */
			~Exception() noexcept override;

		protected:
			/**
			 * @brief Format under `StormByte.Logger.<child>`.
			 * @tparam Args Format argument types.
			 * @param child Segment under `Logger`.
			 * @param fmt Format string.
			 * @param args Format arguments.
			 */
			template <typename... Args>
			explicit Exception(StormByte::Exception::Path child, std::format_string<Args...> fmt, Args&&... args)
				: StormByte::Exception(
					StormByte::Exception::Path{std::string("Logger.") + std::string(child.text)},
					fmt,
					std::forward<Args>(args)...) {}
	};

	/**
	 * @class ThrottleError
	 * @brief Thrown when a throttle rule is invalid. Leaf: no extra segment.
	 */
	class STORMBYTE_LOGGER_PUBLIC ThrottleError: public Exception {
		public:
			using Exception::Exception;

			/**
			 * @brief Destructor. Defined in this module so `catch` matches across a DLL.
			 */
			~ThrottleError() noexcept override;
	};
}

STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Logger::Exception);
STORMBYTE_DECLARE_MAYBE_SAFE(StormByte::Logger::ThrottleError);
