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
#include <StormByte/test_handlers.h>

#include <iostream>
#include <sstream>
#include <string>

using namespace StormByte::Logger;

// -------------------
// Format
// -------------------

int test_format_mask_literals() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "[%L] [T%i] %T: ");
	log << Level::Info << "hello" << std::endl;
	std::string out = output.str();
	if (out.find("%L") != std::string::npos) {
		ASSERT_EQUAL("test_format_mask_literals (leftover %L)", std::string("none"), std::string("%L"));
		RETURN_TEST("test_format_mask_literals", 1);
	}
	if (out.find("%i") != std::string::npos) {
		ASSERT_EQUAL("test_format_mask_literals (leftover %i)", std::string("none"), std::string("%i"));
		RETURN_TEST("test_format_mask_literals", 1);
	}
	if (out.find("%T") != std::string::npos) {
		ASSERT_EQUAL("test_format_mask_literals (leftover %T)", std::string("none"), std::string("%T"));
		RETURN_TEST("test_format_mask_literals", 1);
	}
	if (out.find("[T") == std::string::npos) {
		ASSERT_EQUAL("test_format_mask_literals (missing [T)", std::string("found"), out);
		RETURN_TEST("test_format_mask_literals", 1);
	}
	if (out.find("hello") == std::string::npos) {
		ASSERT_EQUAL("test_format_mask_literals (missing message)", std::string("hello"), out);
		RETURN_TEST("test_format_mask_literals", 1);
	}
	RETURN_TEST("test_format_mask_literals", result);
}

int main() {
	int result = 0;

	// -------------------
	// Format
	// -------------------
	result += test_format_mask_literals();

	if (result == 0)
		std::cout << "All tests passed!" << std::endl;
	else
		std::cout << result << " tests failed." << std::endl;
	return result;
}
