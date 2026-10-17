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

#include <StormByte/byte_size.hxx>
#include <StormByte/logger/log.hxx>
#include <StormByte/logger/threaded_log.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/size.hxx>
#include <StormByte/test_handlers.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using StormByte::ByteSize;
using StormByte::Safe::String;
using StormByte::Size;
using namespace StormByte::Logger;

namespace {

std::size_t count_text(const std::string& text, const std::string_view needle) {
	std::size_t count = 0;
	for (std::size_t pos = 0; (pos = text.find(needle, pos)) != std::string::npos; pos += needle.size())
		++count;
	return count;
}

std::size_t line_count(const std::string& text) {
	return static_cast<std::size_t>(std::count(text.begin(), text.end(), '\n'));
}

long elapsed_ms(const std::chrono::steady_clock::time_point start) {
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - start).count();
}

}

// -------------------
// Filter
// -------------------

int test_log_filtered_high_volume() {
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
	ASSERT_EMPTY(output.str());
	std::cout << "  [perf] Log filtered " << (N * 3) << " lines in " << ms << " ms\n";
	RETURN_TEST(0);
}

int test_log_filtered_owned_text_high_volume() {
	std::ostringstream output;
	Log log(output, Level::Error, "%L:");
	constexpr int N = 50000;
	const String hidden{"hidden"};
	const auto t0 = std::chrono::steady_clock::now();
	for (int i = 0; i < N; ++i)
		log << Level::Debug << hidden << std::endl;
	const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - t0).count();
	ASSERT_EMPTY(output.str());
	std::cout << "  [perf] Log filtered owned text " << N << " lines in " << ms << " ms\n";
	RETURN_TEST(0);
}

int test_threaded_filtered_high_volume() {
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Error, "%L:");
	constexpr int N = 50000;
	const auto t0 = std::chrono::steady_clock::now();
	for (int i = 0; i < N; ++i)
		tlog << Level::Debug << "hidden " << i << std::endl;
	tlog << Level::Error << "only" << std::endl;
	const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - t0).count();
	ASSERT_EQUAL(std::string("Error   : only\n"), output.str());
	std::cout << "  [perf] ThreadedLog filtered " << N << " lines in " << ms << " ms\n";
	RETURN_TEST(0);
}

int test_threaded_filtered_multithreaded_volume() {
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
	ASSERT_EQUAL(threads, finished.load());
	tlog << Level::Info << "done" << std::endl;
	ASSERT_EQUAL(std::string("Info    : done\n"), output.str());
	std::cout << "  [perf] ThreadedLog filtered "
		<< (threads * per_thread) << " lines (" << threads << " threads) in "
		<< ms << " ms\n";
	RETURN_TEST(0);
}

// A hot caller does not split "will this print?" from "build the line".
// Each iteration here is that shape: one level under the floor (no conversion),
// one level that passes the floor and is then refused by the throttle, and a
// sparse line that is actually written. Rate stays at zero so the counts do
// not depend on the clock.

int test_log_throttle_drop_and_filter() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	log.Throttle(0.0, 1);
	constexpr int N = 16384;
	const String owned{"owned"};
	const std::string_view view = "view";
	ASSERT_TRUE(!log.Enabled(Level::Debug));
	ASSERT_TRUE(log.Enabled(Level::Info));
	const auto t0 = std::chrono::steady_clock::now();
	for (int i = 0; i < N; ++i) {
		(void)log.Enabled(Level::Debug);
		(void)log.Enabled(Level::Info);
		log << Level::Debug << "HIDDEN " << i << ' ' << owned << ' ' << Size{i}
			<< ' ' << ByteSize{static_cast<std::uint64_t>(i)} << std::endl;
		log << Level::Info << "keep " << i << ' ' << owned << ' ' << view << ' '
			<< (i % 2 == 0) << ' ' << 1.5 << ' ' << Size{i} << std::endl;
		if (i % 128 == 0)
			log << Level::Error << "ERR " << i << std::endl;
	}
	log.FlushThrottle();
	const auto ms = elapsed_ms(t0);
	const std::string text = output.str();
	ASSERT_EQUAL(std::string::npos, text.find("HIDDEN"));
	ASSERT_EQUAL(std::size_t{1}, count_text(text, "keep "));
	ASSERT_EQUAL(static_cast<std::size_t>(N / 128), count_text(text, "ERR "));
	ASSERT_CONTAINS(text, "dropped");
	std::cout << "  [perf] Log drop+filter " << (N * 2) << " lines in " << ms << " ms\n";
	RETURN_TEST(0);
}

int test_log_sample_and_window_hot_path() {
	constexpr int N = 8192;
	const String owned{"owned"};
	std::ostringstream sample_output;
	Log sample(sample_output, Level::Info, "%L:");
	ThrottleSpec sample_spec;
	sample_spec.Policy = ThrottlePolicy::Sample;
	sample_spec.SampleN = 16;
	sample.Throttle(sample_spec);
	const auto sample_t0 = std::chrono::steady_clock::now();
	for (int i = 0; i < N; ++i) {
		sample << Level::Debug << "HIDDEN " << owned << std::endl;
		sample << Level::Info << redact(4) << "secret-token" << noredact
			<< " s " << i << ' ' << owned << ' ' << humanreadable_number << 1000
			<< nohumanreadable << std::endl;
	}
	const auto sample_ms = elapsed_ms(sample_t0);
	const std::string sample_text = sample_output.str();
	ASSERT_EQUAL(std::string::npos, sample_text.find("HIDDEN"));
	ASSERT_CONTAINS(sample_text, "dropped");
	const std::size_t sample_admitted = static_cast<std::size_t>(N / 16);
	const std::size_t sample_lines = line_count(sample_text);
	ASSERT_TRUE(sample_lines >= sample_admitted);
	ASSERT_TRUE(sample_lines <= sample_admitted * 2);
	std::cout << "  [perf] Log sample " << N << " lines in " << sample_ms << " ms\n";

	std::ostringstream window_output;
	Log window(window_output, Level::Info, "%L:");
	ThrottleSpec window_spec;
	window_spec.Policy = ThrottlePolicy::Window;
	window_spec.WindowKeep = 2;
	window_spec.WindowPeriod = 8;
	window.Throttle(window_spec);
	const auto window_t0 = std::chrono::steady_clock::now();
	for (int i = 0; i < N; ++i) {
		window << Level::LowLevel << "HIDDEN " << i << std::endl;
		window << Level::Info << "w " << i << ' ' << 2.5f << std::endl;
	}
	const auto window_ms = elapsed_ms(window_t0);
	const std::string window_text = window_output.str();
	ASSERT_EQUAL(std::string::npos, window_text.find("HIDDEN"));
	ASSERT_CONTAINS(window_text, "dropped");
	const std::size_t window_admitted = static_cast<std::size_t>(N / 8 * 2);
	const std::size_t window_lines = line_count(window_text);
	ASSERT_TRUE(window_lines >= window_admitted);
	ASSERT_TRUE(window_lines <= window_admitted * 2);
	std::cout << "  [perf] Log window " << N << " lines in " << window_ms << " ms\n";
	RETURN_TEST(0);
}

int test_log_scope_group_and_rate() {
	std::ostringstream output;
	Log log(output, Level::Info, "%c %L:");
	auto decode = log.Scope("video/decode");
	auto idle = log.Scope("video/idle");
	decode->Throttle(0.0, 1);
	constexpr int N = 4096;
	const auto t0 = std::chrono::steady_clock::now();
	for (int i = 0; i < N; ++i) {
		decode << Level::Debug << "HIDDEN " << i << std::endl;
		decode << Level::Info << "frame " << i << std::endl;
		if (i % 32 == 0)
			idle << Level::Info << "tick " << i << std::endl;
		if (i % 64 == 0)
			decode << Level::Error << "bad " << i << std::endl;
	}
	decode->FlushThrottle();
	const std::string scoped = output.str();
	ASSERT_EQUAL(std::string::npos, scoped.find("HIDDEN"));
	ASSERT_EQUAL(std::size_t{1}, count_text(scoped, "frame "));
	ASSERT_EQUAL(static_cast<std::size_t>(N / 32), count_text(scoped, "tick "));
	ASSERT_EQUAL(static_cast<std::size_t>(N / 64), count_text(scoped, "bad "));
	ASSERT_CONTAINS(scoped, "dropped");
	std::cout << "  [perf] Log scope throttle " << N << " lines in " << elapsed_ms(t0) << " ms\n";

	std::ostringstream grouped_output;
	Log grouped(grouped_output, Level::Info, "%L:");
	grouped.Throttle(group("worker"), 0.0, 1);
	const auto group_t0 = std::chrono::steady_clock::now();
	for (int i = 0; i < N; ++i) {
		grouped << reset_component << component("decode") << group("worker") << Level::Info << "work " << i << std::endl;
		if (i % 16 == 0)
			grouped << reset_component << group("other") << Level::Info << "other " << i << std::endl;
	}
	grouped.FlushThrottle();
	const std::string groups = grouped_output.str();
	ASSERT_EQUAL(std::size_t{1}, count_text(groups, "work "));
	ASSERT_EQUAL(static_cast<std::size_t>(N / 16), count_text(groups, "other "));
	ASSERT_CONTAINS(groups, "dropped");
	std::cout << "  [perf] Log group throttle " << N << " lines in " << elapsed_ms(group_t0) << " ms\n";

	std::ostringstream rated_output;
	Log rated(rated_output, Level::Info, "%L:");
	rated.Throttle(5.0, 4);
	constexpr int rate_n = 2000;
	const auto rate_t0 = std::chrono::steady_clock::now();
	for (int i = 0; i < rate_n; ++i) {
		rated << Level::Debug << "HIDDEN " << i << std::endl;
		rated << Level::Warning << "rate " << i << std::endl;
	}
	const std::string rated_text = rated_output.str();
	ASSERT_EQUAL(std::string::npos, rated_text.find("HIDDEN"));
	const std::size_t rated_lines = line_count(rated_text);
	ASSERT_TRUE(rated_lines >= 4);
	ASSERT_TRUE(rated_lines < 500);
	std::cout << "  [perf] Log rate throttle " << rate_n << " lines in " << elapsed_ms(rate_t0) << " ms\n";
	RETURN_TEST(0);
}

int test_threaded_throttle_mixed() {
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Info, "%L:");
	ThrottleSpec spec;
	spec.Policy = ThrottlePolicy::Sample;
	spec.SampleN = 16;
	tlog.Throttle(spec);
	constexpr int threads = 4;
	constexpr int per_thread = 4096;
	const String owned{"owned"};
	std::atomic<int> finished{0};
	auto worker = [&](const int id) {
		for (int i = 0; i < per_thread; ++i) {
			tlog << Level::Debug << "HIDDEN " << id << ':' << i << ' ' << owned << std::endl;
			tlog << Level::Info << "row " << id << ':' << i << ' ' << owned << std::endl;
			if (i % 256 == 0)
				tlog << Level::Warning << "warn " << id << std::endl;
			if (i % 512 == 0)
				tlog << Level::Error << "ERR " << id << std::endl;
		}
		finished.fetch_add(1, std::memory_order_relaxed);
	};
	std::vector<std::thread> pool;
	pool.reserve(threads);
	const auto t0 = std::chrono::steady_clock::now();
	for (int t = 0; t < threads; ++t)
		pool.emplace_back(worker, t);
	for (auto& th : pool)
		th.join();
	tlog << Level::Error << "done" << std::endl;
	const auto ms = elapsed_ms(t0);
	const std::string text = output.str();
	ASSERT_EQUAL(threads, finished.load());
	ASSERT_EQUAL(std::string::npos, text.find("HIDDEN"));
	ASSERT_CONTAINS(text, "dropped");
	ASSERT_CONTAINS(text, "ERR ");
	ASSERT_CONTAINS(text, "done");
	std::cout << "  [perf] ThreadedLog throttle "
		<< (threads * per_thread) << " lines (" << threads << " threads) in "
		<< ms << " ms\n";
	RETURN_TEST(0);
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

	// -------------------
	// Throttle
	// -------------------
	result += test_log_throttle_drop_and_filter();
	result += test_log_sample_and_window_hot_path();
	result += test_log_scope_group_and_rate();
	result += test_threaded_throttle_mixed();

	if (result == 0)
		std::cout << "All tests passed!" << std::endl;
	else
		std::cout << result << " tests failed." << std::endl;
	return result;
}
