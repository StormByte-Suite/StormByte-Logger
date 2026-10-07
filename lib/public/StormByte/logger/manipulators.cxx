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

#include <StormByte/logger/log.hxx>
#include <StormByte/logger/manipulators.hxx>
#include <StormByte/logger/engine.hxx>
#include <utility>

namespace StormByte::Logger {
	STORMBYTE_LOGGER_PUBLIC ComponentManip component(StormByte::Safe::String name) {
		return ComponentManip{std::move(name)};
	}

	STORMBYTE_LOGGER_PUBLIC GroupManip group(StormByte::Safe::String name) {
		return GroupManip{std::move(name)};
	}

	STORMBYTE_LOGGER_PUBLIC FormatManip push_format(StormByte::Safe::String format) {
		return FormatManip{std::move(format)};
	}

	STORMBYTE_LOGGER_PUBLIC Log& humanreadable_number(Log& log) noexcept {
		humanreadable_number(*log.m_engine);
		return log;
	}

	STORMBYTE_LOGGER_PUBLIC Log& humanreadable_bytes(Log& log) noexcept {
		humanreadable_bytes(*log.m_engine);
		return log;
	}

	STORMBYTE_LOGGER_PUBLIC Log& nohumanreadable(Log& log) noexcept {
		nohumanreadable(*log.m_engine);
		return log;
	}

	STORMBYTE_LOGGER_PUBLIC Log& noredact(Log& log) noexcept {
		log.m_engine->SetRedact(false, 0, false);
		return log;
	}
}
