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

#include <StormByte/logger/human_readable.hxx>

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

using namespace StormByte::Logger::Detail;

namespace StormByte::Logger::Detail {
	template<typename T>
	requires StormByte::Type::Arithmetic<T> && (!StormByte::Type::SameAs<T, wchar_t>)
	std::string FormatNumber(const T& number, const std::string& locale) noexcept {
		try {
			std::ostringstream oss;
			try {
				oss.imbue(std::locale(locale));
			} catch (...) {
				oss.imbue(std::locale("C"));
			}

			if constexpr (StormByte::Type::Integral<T>) {
				oss << number;
			} else if constexpr (StormByte::Type::FloatingPoint<T>) {
				if (std::fmod(number, 1.0) == 0.0)
					oss << static_cast<int64_t>(number);
				else
					oss << std::fixed << std::setprecision(2) << number;
			}

			return oss.str();
		} catch (...) {
			try {
				return std::to_string(number);
			} catch (...) {
				return {};
			}
		}
	}

	template<typename T>
	requires StormByte::Type::Arithmetic<T>
	std::string FormatBytes(const T& bytes, const std::string& locale) noexcept {
		try {
			constexpr uint64_t KB = 1024;
			constexpr uint64_t MB = KB * 1024;
			constexpr uint64_t GB = MB * 1024;
			constexpr uint64_t TB = GB * 1024;
			constexpr uint64_t PB = TB * 1024;
			const long double signed_bytes = static_cast<long double>(bytes);
			const bool negative = signed_bytes < 0;
			const long double magnitude = negative ? -signed_bytes : signed_bytes;
			double value = static_cast<double>(magnitude);
			std::string suffix = "Bytes";
			if (magnitude >= PB) {
				value /= PB;
				suffix = "PiB";
			} else if (magnitude >= TB) {
				value /= TB;
				suffix = "TiB";
			} else if (magnitude >= GB) {
				value /= GB;
				suffix = "GiB";
			} else if (magnitude >= MB) {
				value /= MB;
				suffix = "MiB";
			} else if (magnitude >= KB) {
				value /= KB;
				suffix = "KiB";
			}

			std::ostringstream oss;
			try {
				oss.imbue(std::locale(locale));
			} catch (...) {
				oss.imbue(std::locale("C"));
			}

			if (std::fabs(value - std::round(value)) < 0.01)
				oss << static_cast<int64_t>(std::round(value));
			else if (value < 0.01)
				oss << "0";
			else
				oss << std::fixed << std::setprecision(2) << value;

			return (negative ? "-" : "") + oss.str() + " " + suffix;
		} catch (...) {
			try {
				return std::to_string(bytes) + " Bytes";
			} catch (...) {
				return {};
			}
		}
	}
}

template std::string StormByte::Logger::Detail::FormatNumber<bool>(const bool&, const std::string&) noexcept;
template std::string StormByte::Logger::Detail::FormatNumber<char>(const char&, const std::string&) noexcept;
template std::string StormByte::Logger::Detail::FormatNumber<signed char>(const signed char&, const std::string&) noexcept;
template std::string StormByte::Logger::Detail::FormatNumber<unsigned char>(const unsigned char&, const std::string&) noexcept;
template std::string StormByte::Logger::Detail::FormatNumber<short>(const short&, const std::string&) noexcept;
template std::string StormByte::Logger::Detail::FormatNumber<unsigned short>(const unsigned short&, const std::string&) noexcept;
template std::string StormByte::Logger::Detail::FormatNumber<int>(const int&, const std::string&) noexcept;
template std::string StormByte::Logger::Detail::FormatNumber<unsigned int>(const unsigned int&, const std::string&) noexcept;
template std::string StormByte::Logger::Detail::FormatNumber<long>(const long&, const std::string&) noexcept;
template std::string StormByte::Logger::Detail::FormatNumber<unsigned long>(const unsigned long&, const std::string&) noexcept;
template std::string StormByte::Logger::Detail::FormatNumber<long long>(const long long&, const std::string&) noexcept;
template std::string StormByte::Logger::Detail::FormatNumber<unsigned long long>(const unsigned long long&, const std::string&) noexcept;
template std::string StormByte::Logger::Detail::FormatNumber<float>(const float&, const std::string&) noexcept;
template std::string StormByte::Logger::Detail::FormatNumber<double>(const double&, const std::string&) noexcept;
template std::string StormByte::Logger::Detail::FormatNumber<long double>(const long double&, const std::string&) noexcept;

template std::string StormByte::Logger::Detail::FormatBytes<bool>(const bool&, const std::string&) noexcept;
template std::string StormByte::Logger::Detail::FormatBytes<char>(const char&, const std::string&) noexcept;
template std::string StormByte::Logger::Detail::FormatBytes<signed char>(const signed char&, const std::string&) noexcept;
template std::string StormByte::Logger::Detail::FormatBytes<unsigned char>(const unsigned char&, const std::string&) noexcept;
template std::string StormByte::Logger::Detail::FormatBytes<short>(const short&, const std::string&) noexcept;
template std::string StormByte::Logger::Detail::FormatBytes<unsigned short>(const unsigned short&, const std::string&) noexcept;
template std::string StormByte::Logger::Detail::FormatBytes<int>(const int&, const std::string&) noexcept;
template std::string StormByte::Logger::Detail::FormatBytes<unsigned int>(const unsigned int&, const std::string&) noexcept;
template std::string StormByte::Logger::Detail::FormatBytes<long>(const long&, const std::string&) noexcept;
template std::string StormByte::Logger::Detail::FormatBytes<unsigned long>(const unsigned long&, const std::string&) noexcept;
template std::string StormByte::Logger::Detail::FormatBytes<long long>(const long long&, const std::string&) noexcept;
template std::string StormByte::Logger::Detail::FormatBytes<unsigned long long>(const unsigned long long&, const std::string&) noexcept;
template std::string StormByte::Logger::Detail::FormatBytes<float>(const float&, const std::string&) noexcept;
template std::string StormByte::Logger::Detail::FormatBytes<double>(const double&, const std::string&) noexcept;
template std::string StormByte::Logger::Detail::FormatBytes<long double>(const long double&, const std::string&) noexcept;
