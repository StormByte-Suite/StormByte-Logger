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
#include <StormByte/logger/threaded_log.hxx>
#include <StormByte/test_handlers.h>

#include <iostream>
#include <sstream>
#include <string>
#include <thread>

using namespace StormByte::Logger;

// -------------------
// Format
// -------------------

int test_format_mask_literals() {
	std::ostringstream output;
	Log log(output, Level::Info, "[%L] [T%i] %T: ");
	log << Level::Info << "hello" << std::endl;
	const std::string out = output.str();
	ASSERT_NOT_CONTAINS(out, "%L");
	ASSERT_NOT_CONTAINS(out, "%i");
	ASSERT_NOT_CONTAINS(out, "%T");
	ASSERT_CONTAINS(out, "[T");
	ASSERT_CONTAINS(out, "hello");
	RETURN_TEST(0);
}

int test_thread_id_mask_is_reused_per_thread_without_changing_text() {
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%i/%i:");
	auto write = [&]() {
		std::ostringstream id;
		id << std::this_thread::get_id();
		for (int i = 0; i < 3; ++i)
			log << Level::Info << id.str() << std::endl;
		return id.str();
	};
	const std::string main_id = write();
	std::string worker_id;
	std::thread worker([&]() { worker_id = write(); });
	worker.join();
	ASSERT_TRUE(main_id != worker_id);
	const auto line = [](const std::string& id) { return id + "/" + id + ": " + id + "\n"; };
	ASSERT_EQUAL(line(main_id) + line(main_id) + line(main_id)
		+ line(worker_id) + line(worker_id) + line(worker_id), output.str());
	RETURN_TEST(0);
}

int test_component_overrides_after_default_scoped_emission() {
	std::ostringstream output;
	Log log(output, Level::Info, "%c:");
	auto scoped = log.Scope("parent/child/long-component");
	*scoped << Level::Info << "default" << std::endl;
	log.Format("parent", "[%c]");
	log.Color("parent", Level::Info, Color::Red);
	*scoped << Level::Info << "override" << std::endl;
	log.Format("parent", "");
	log.Color("parent", Level::Info, Color::Default);
	*scoped << Level::Info << "restored" << std::endl;
	ASSERT_EQUAL(std::string{"parent/child/long-component: default\n"
		"\033[31m[parent/child/long-component] override\033[0m\n"
		"parent/child/long-component: restored\n"}, output.str());
	RETURN_TEST(0);
}

int main() {
	int result = 0;

	// -------------------
	// Format
	// -------------------
	result += test_format_mask_literals();
	result += test_thread_id_mask_is_reused_per_thread_without_changing_text();
	result += test_component_overrides_after_default_scoped_emission();

	if (result == 0)
		std::cout << "All tests passed!" << std::endl;
	else
		std::cout << result << " tests failed." << std::endl;
	return result;
}
