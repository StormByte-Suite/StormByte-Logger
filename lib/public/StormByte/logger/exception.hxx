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

#include <string_view>
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
	 * This type applies the `Logger` segment. A further segment is an @ref Path. The body does not include the path. Literal `{}` are replaced by @ref StormByte::Safe::String::ReplaceToken. This does not call `std::format`. The destructor is defined in this module.
	 */
	class STORMBYTE_LOGGER_PUBLIC Exception: public StormByte::Exception {
		public:
			/**
			 * @struct Path
			 * @brief Additional path segments nested below `StormByte.Logger`.
			 */
			struct Path {
				std::string_view segments; ///< Additional dotted path segments.

				/**
				 * @brief Wraps additional path segments.
				 * @param segments Dotted segments appended below `Logger`.
				 */
				explicit constexpr Path(std::string_view segments) noexcept: segments(segments) {}
			};

			/**
			 * @brief Replace literal `{}` under `StormByte.Logger`.
			 * @tparam Args Views, numbers, or types that convert to `std::string`. A number uses `std::to_string`.
			 * @param pattern Body with literal `{}` tokens. Not a `std::format` string.
			 * @param args Replacements. One per token, in order.
			 */
			template <typename... Args>
				requires (sizeof...(Args) >= 1)
			Exception(std::string_view pattern, Args&&... args)
				: StormByte::Exception(StormByte::Exception::Path{"Logger"}, pattern, std::forward<Args>(args)...) {}

			/**
			 * @brief Copy a plain message under `StormByte.Logger`.
			 * @param message Exception text. Not a format string.
			 */
			explicit Exception(std::string_view message);

			/**
			 * @brief Plain message under an additional Logger path.
			 * @param path Additional segments nested below `StormByte.Logger`.
			 * @param message Exception text. Not a format string.
			 */
			Exception(Path path, std::string_view message);

			/**
			 * @brief Replace literal `{}` under an additional Logger path.
			 * @tparam Args Views, numbers, or types that convert to `std::string`. A number uses `std::to_string`.
			 * @param path Additional segments nested below `StormByte.Logger`.
			 * @param pattern Body with literal `{}` tokens. Not a `std::format` string.
			 * @param args Replacements. One per token, in order.
			 */
			template <typename... Args>
				requires (sizeof...(Args) >= 1)
			Exception(Path path, std::string_view pattern, Args&&... args)
				: Exception(path, static_cast<std::string_view>(StormByte::Safe::String::ReplaceToken(pattern, std::forward<Args>(args)...))) {}

			/**
			 * @brief Copy constructor.
			 * @param other Exception to copy.
			 */
			Exception(const Exception& other) = default;

			/**
			 * @brief Move constructor.
			 * @param other Exception to move.
			 */
			Exception(Exception&& other) noexcept = default;

			/**
			 * @brief Destructor. Defined in this module so `catch` matches across a DLL.
			 */
			~Exception() noexcept override;

			/**
			 * @brief Copy assignment operator.
			 * @param other Exception to copy.
			 * @return This exception.
			 */
			Exception& operator=(const Exception& other) = default;

			/**
			 * @brief Move assignment operator.
			 * @param other Exception to move.
			 * @return This exception.
			 */
			Exception& operator=(Exception&& other) noexcept = default;
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
