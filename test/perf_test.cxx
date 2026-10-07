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
#include <StormByte/safe/string.hxx>
#include <StormByte/test_handlers.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using StormByte::Safe::String;
using namespace StormByte::Logger;

// -------------------
// Filter
// -------------------

int test_log_filtered_high_volume() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Error, "%L:");
	constexpr int N = 100000;
	const auto t0 = std::chrono::steady_clock::now();
	for (int i = 0; i < N; ++i) {
		log << Level::Debug << "x=" << i << " b=" << true << " d=" << 1.5 << std::endl;
		log << Level::Info << "info " << i << std::endl;
		log << Level::Notice << "notice " << i << std::endl;
	}
	const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - t0).count();
	ASSERT_EQUAL("test_log_filtered_high_volume (output)", std::string(""), output.str());
	std::cout << "  [perf] Log filtered " << (N * 3) << " lines in " << ms << " ms\n";
	RETURN_TEST("test_log_filtered_high_volume", result);
}

int test_log_filtered_owned_text_high_volume() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Error, "%L:");
	constexpr int N = 50000;
	const String hidden{"hidden"};
	const auto t0 = std::chrono::steady_clock::now();
	for (int i = 0; i < N; ++i)
		log << Level::Debug << hidden << std::endl;
	const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - t0).count();
	ASSERT_EQUAL("test_log_filtered_owned_text_high_volume (output)", std::string(""), output.str());
	std::cout << "  [perf] Log filtered owned text " << N << " lines in " << ms << " ms\n";
	RETURN_TEST("test_log_filtered_owned_text_high_volume", result);
}

int test_threaded_filtered_high_volume() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Error, "%L:");
	constexpr int N = 50000;
	const auto t0 = std::chrono::steady_clock::now();
	for (int i = 0; i < N; ++i)
		tlog << Level::Debug << "hidden " << i << std::endl;
	tlog << Level::Error << "only" << std::endl;
	const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - t0).count();
	ASSERT_EQUAL("test_threaded_filtered_high_volume (output)",
		std::string("Error   : only\n"), output.str());
	std::cout << "  [perf] ThreadedLog filtered " << N << " lines in " << ms << " ms\n";
	RETURN_TEST("test_threaded_filtered_high_volume", result);
}

int test_threaded_filtered_multithreaded_volume() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Info, "%L:");
	constexpr int threads = 8;
	constexpr int per_thread = 8000;
	std::atomic<int> finished{0};
	auto worker = [&](int id) {
		for (int i = 0; i < per_thread; ++i)
			tlog << Level::Debug << "t" << id << ":" << i << std::endl;
		finished.fetch_add(1, std::memory_order_relaxed);
	};
	std::vector<std::thread> pool;
	pool.reserve(threads);
	const auto t0 = std::chrono::steady_clock::now();
	for (int t = 0; t < threads; ++t)
		pool.emplace_back(worker, t);
	for (auto& th : pool)
		th.join();
	const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - t0).count();
	ASSERT_EQUAL("test_threaded_filtered_multithreaded_volume (workers)",
		std::to_string(threads), std::to_string(finished.load()));
	tlog << Level::Info << "done" << std::endl;
	ASSERT_EQUAL("test_threaded_filtered_multithreaded_volume (output)",
		std::string("Info    : done\n"), output.str());
	std::cout << "  [perf] ThreadedLog filtered "
		<< (threads * per_thread) << " lines (" << threads << " threads) in "
		<< ms << " ms\n";
	RETURN_TEST("test_threaded_filtered_multithreaded_volume", result);
}

int main() {
	int result = 0;

	// -------------------
	// Filter
	// -------------------
	result += test_log_filtered_high_volume();
	result += test_log_filtered_owned_text_high_volume();
	result += test_threaded_filtered_high_volume();
	result += test_threaded_filtered_multithreaded_volume();

	if (result == 0)
		std::cout << "All tests passed!" << std::endl;
	else
		std::cout << result << " tests failed." << std::endl;
	return result;
}
