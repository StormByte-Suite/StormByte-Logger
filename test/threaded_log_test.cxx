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

#include <StormByte/base64.hxx>
#include <StormByte/binary_data.hxx>
#include <StormByte/byte_size.hxx>
#include <StormByte/safe/cstring.hxx>
#include <StormByte/size.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/logger/threaded_log.hxx>
#include <StormByte/test_handlers.h>

#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <memory>
#include <regex>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace StormByte::Logger;

namespace {
	void IsolateLine(Log& log) {
		if (!log.Enabled(Level::LowLevel))
			log << Level::LowLevel << std::endl;
	}
}

// -------------------
// Basic emit
// -------------------

int test_smart_pointer_usage() {
	int result = 0;
	std::ostringstream output;
	std::shared_ptr<StormByte::Logger::Log> log = std::make_shared<StormByte::Logger::ThreadedLog>(output, Level::Info, "%L:");
	log << Level::Info << "Smart pointer log message" << std::endl;
	ASSERT_EQUAL("test_smart_pointer_usage", std::string("Info    : Smart pointer log message\n"), output.str());
	RETURN_TEST("test_smart_pointer_usage", result);
}

int test_shared_unwraps_without_dereference() {
	int result = 0;
	std::ostringstream output;
	StormByte::Safe::Shared<ThreadedLog> log = StormByte::Safe::Shared<ThreadedLog>::MakePointer<ThreadedLog>(output, Level::Info, "%L:");
	log << Level::Info << "Hola" << std::endl;
	ASSERT_EQUAL("test_shared_unwraps_without_dereference", "Info    : Hola\n", output.str());
	RETURN_TEST("test_shared_unwraps_without_dereference", result);
}

int test_threadedlog_basic() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Info, "%L:");
	tlog << Level::Info << "Threaded basic message" << std::endl;
	ASSERT_EQUAL("test_threadedlog_basic", std::string("Info    : Threaded basic message\n"), output.str());
	RETURN_TEST("test_threadedlog_basic", result);
}

// -------------------
// Binary span
// -------------------

int test_threadedlog_span_default_is_base64() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%L:");
	const std::vector<std::byte> raw{
		std::byte{'H'}, std::byte{'e'}, std::byte{'l'}, std::byte{'l'}, std::byte{'o'}
	};
	log << Level::Info << std::span<const std::byte>{raw} << std::endl;
	ASSERT_EQUAL("test_threadedlog_span_default_is_base64",
		std::string("Info    : ") + static_cast<std::string>(StormByte::Base64Encode(raw)) + "\n", output.str());
	RETURN_TEST("test_threadedlog_span_default_is_base64", result);
}

int test_threadedlog_span_filtered() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%L:");
	const std::vector<std::byte> raw{std::byte{0xFF}};
	log << Level::Debug << raw << std::endl;
	ASSERT_EQUAL("test_threadedlog_span_filtered", std::string{}, output.str());
	log << Level::Info << "after" << std::endl;
	ASSERT_EQUAL("test_threadedlog_span_filtered (after)", "Info    : after\n", output.str());
	RETURN_TEST("test_threadedlog_span_filtered", result);
}

int test_threadedlog_span_hex() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%L:");
	const std::vector<std::byte> raw{std::byte{0x01}, std::byte{0xAB}};
	const StormByte::BinaryData dumped(raw);
	log << Level::Info << hex << raw << std::endl;
	ASSERT_EQUAL("test_threadedlog_span_hex",
		std::string("Info    : ") + static_cast<std::string>(dumped.HexDump(StormByte::Size{16})) + "\n", output.str());
	RETURN_TEST("test_threadedlog_span_hex", result);
}

int test_threadedlog_span_vector_converts() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%L:");
	const std::vector<std::byte> raw{std::byte{0x01}, std::byte{0x02}};
	log << Level::Info << raw << std::endl;
	ASSERT_EQUAL("test_threadedlog_span_vector_converts",
		std::string("Info    : ") + static_cast<std::string>(StormByte::Base64Encode(raw)) + "\n", output.str());
	RETURN_TEST("test_threadedlog_span_vector_converts", result);
}

int test_threadedlog_every_accepted_payload() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%L:");
	const StormByte::Safe::String owned{"owned"};
	const StormByte::Safe::CString bytes{"c"};
	const StormByte::Safe::WString wide_owned{L"wide-owned"};
	const StormByte::Safe::WCString wide_bytes{L"wide-c"};
	const std::string narrow_text{"string"};
	const char* literal_text = "literal";
	const StormByte::Size count{3};
	const StormByte::ByteSize octets{4};
	const StormByte::BinaryData raw{std::byte{'Z'}};
	const char* null_narrow = nullptr;
	log << Level::Info
		<< false << " "
		<< std::string_view{"std"} << " "
		<< narrow_text << " "
		<< literal_text << " "
		<< std::wstring_view{L"wide"} << " "
		<< owned << " "
		<< bytes << " "
		<< wide_owned << " "
		<< wide_bytes << " "
		<< count << " "
		<< octets << " "
		<< raw
		<< null_narrow
		<< std::endl;
	const std::string body =
		std::string("false std string literal wide owned c wide-owned wide-c ")
		+ static_cast<std::string>(count) + ' '
		+ static_cast<std::string>(octets) + ' '
		+ static_cast<std::string>(StormByte::Base64Encode(raw));
	ASSERT_EQUAL("test_threadedlog_every_accepted_payload", std::string("Info    : ") + body + "\n", output.str());
	RETURN_TEST("test_threadedlog_every_accepted_payload", result);
}

// -------------------
// Color
// -------------------

int test_threadedlog_colored_lines_do_not_mix() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Info, "%L:");
	tlog.Color(Level::Info, Color::Cyan);
	constexpr int threads = 8;
	constexpr int repeats = 250;
	std::vector<std::thread> pool;
	pool.reserve(threads);
	for (int id = 0; id < threads; ++id) {
		pool.emplace_back([&, id] {
			for (int i = 0; i < repeats; ++i)
				tlog << Level::Info << "T" << id << ':' << i << std::endl;
		});
	}
	for (auto& thread : pool)
		thread.join();
	std::istringstream input(output.str());
	std::string line;
	int count = 0;
	while (std::getline(input, line)) {
		ASSERT_TRUE("test_threadedlog_colored_lines_do_not_mix (prefix)", line.starts_with("\033[36mInfo    : T"));
		ASSERT_TRUE("test_threadedlog_colored_lines_do_not_mix (reset)", line.ends_with("\033[0m"));
		ASSERT_EQUAL("test_threadedlog_colored_lines_do_not_mix (single color)", std::size_t{1},
			static_cast<std::size_t>(line.find("\033[36m") != std::string::npos));
		++count;
	}
	ASSERT_EQUAL("test_threadedlog_colored_lines_do_not_mix (count)", threads * repeats, count);
	RETURN_TEST("test_threadedlog_colored_lines_do_not_mix", result);
}

// -------------------
// Components
// -------------------

int test_threadedlog_component_color_override_has_priority() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%c[%L]");
	log << reset_component;
	IsolateLine(log);
	log.Color(Level::Info, Color::Blue);
	log.Color("Multimedia", Level::Info, Color::Red);
	log << component("Multimedia") << Level::Info << "red" << std::endl;
	log << reset_component << Level::Info << "blue" << std::endl;
	ASSERT_EQUAL("test_threadedlog_component_color_override_has_priority",
		"\033[31mMultimedia[Info    ] red\033[0m\n\033[34m[Info    ] blue\033[0m\n", output.str());
	RETURN_TEST("test_threadedlog_component_color_override_has_priority", result);
}

int test_threadedlog_component_does_not_hold_line_lock() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Info, "%c[%L]");
	tlog << reset_component;
	tlog << component("Filtered") << Level::Debug << "hidden" << std::endl;
	tlog << Level::Info << "visible" << std::endl;
	ASSERT_EQUAL("test_threadedlog_component_does_not_hold_line_lock", "Filtered[Info    ] visible\n", output.str());
	RETURN_TEST("test_threadedlog_component_does_not_hold_line_lock", result);
}

int test_threadedlog_component_format_priority_and_fallback() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "GENERAL[%L]");
	log << reset_component;
	IsolateLine(log);
	log.Format("Media", "MEDIA[%L]");
	log << component("Media") << Level::Info << "media" << std::endl;
	log << reset_component << component("Other") << Level::Info << "fallback" << std::endl;
	log.Format("Media", "");
	log << reset_component << component("Media") << Level::Info << "general again" << std::endl;
	const std::string expected =
		"MEDIA[Info    ] media\n"
		"GENERAL[Info    ] fallback\n"
		"GENERAL[Info    ] general again\n";
	ASSERT_EQUAL("test_threadedlog_component_format_priority_and_fallback", expected, output.str());
	RETURN_TEST("test_threadedlog_component_format_priority_and_fallback", result);
}

int test_threadedlog_component_formats_do_not_mix() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Info, "GENERAL[%L]");
	tlog.Format("A", "A[%L]");
	tlog.Format("B", "B[%L]");
	constexpr int threads = 2;
	constexpr int repeats = 100;
	std::vector<std::thread> pool;
	pool.reserve(threads);
	for (int id = 0; id < threads; ++id) {
		pool.emplace_back([&, id] {
			const std::string name(1, static_cast<char>('A' + id));
			tlog << reset_component << component(name);
			for (int i = 0; i < repeats; ++i)
				tlog << Level::Info << "message" << std::endl;
		});
	}
	for (auto& thread : pool)
		thread.join();
	std::istringstream input(output.str());
	std::string line;
	int count = 0;
	while (std::getline(input, line)) {
		ASSERT_TRUE("test_threadedlog_component_formats_do_not_mix (line)",
			std::regex_match(line, std::regex("^[AB]\\[Info    \\] message$")));
		++count;
	}
	ASSERT_EQUAL("test_threadedlog_component_formats_do_not_mix (count)", threads * repeats, count);
	RETURN_TEST("test_threadedlog_component_formats_do_not_mix", result);
}

int test_threadedlog_component_header_is_sticky_and_resettable() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%c[%L]");
	log << reset_component;
	IsolateLine(log);
	log << component("Multimedia") << Level::Info << "first" << std::endl;
	log << Level::Info << "second" << std::endl;
	log << reset_component << Level::Info << "third" << std::endl;
	ASSERT_EQUAL("test_threadedlog_component_header_is_sticky_and_resettable",
		"Multimedia[Info    ] first\nMultimedia[Info    ] second\n[Info    ] third\n", output.str());
	RETURN_TEST("test_threadedlog_component_header_is_sticky_and_resettable", result);
}

int test_threadedlog_component_without_token_preserves_legacy_output() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%L:");
	log << reset_component;
	IsolateLine(log);
	log << component("Hidden") << Level::Info << "message" << std::endl;
	ASSERT_EQUAL("test_threadedlog_component_without_token_preserves_legacy_output", "Info    : message\n", output.str());
	RETURN_TEST("test_threadedlog_component_without_token_preserves_legacy_output", result);
}

int test_threadedlog_components_are_thread_local() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Info, "%c[%L]");
	constexpr int threads = 4;
	constexpr int repeats = 100;
	std::vector<std::thread> pool;
	pool.reserve(threads);
	for (int id = 0; id < threads; ++id) {
		pool.emplace_back([&, id] {
			tlog << reset_component << component("C" + std::to_string(id));
			for (int i = 0; i < repeats; ++i)
				tlog << Level::Info << "message" << std::endl;
		});
	}
	for (auto& thread : pool)
		thread.join();
	std::istringstream input(output.str());
	std::string line;
	int count = 0;
	while (std::getline(input, line)) {
		ASSERT_TRUE("test_threadedlog_components_are_thread_local (line)",
			std::regex_match(line, std::regex("^C[0-3]\\[Info    \\] message$")));
		++count;
	}
	ASSERT_EQUAL("test_threadedlog_components_are_thread_local (count)", threads * repeats, count);
	RETURN_TEST("test_threadedlog_components_are_thread_local", result);
}

int test_threadedlog_empty_component_does_not_push() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%c[%L]");
	log << reset_component;
	IsolateLine(log);
	log << component("Multimedia") << Level::Info << "named" << std::endl;
	log << component("") << Level::Info << "still named" << std::endl;
	log << reset_component << Level::Info << "root" << std::endl;
	ASSERT_EQUAL("test_threadedlog_empty_component_does_not_push",
		"Multimedia[Info    ] named\nMultimedia[Info    ] still named\n[Info    ] root\n", output.str());
	RETURN_TEST("test_threadedlog_empty_component_does_not_push", result);
}

int test_threadedlog_format_change_redecides_throttle_line() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "A[%L]");
	log << reset_component;
	IsolateLine(log);
	log.Throttle(0.0, 1);
	log << Level::Info << "first";
	log.Format("B[%L]");
	log << Level::Info << "second" << std::endl;
	ASSERT_EQUAL("test_threadedlog_format_change_redecides_throttle_line", "A[Info    ] first\n", output.str());
	RETURN_TEST("test_threadedlog_format_change_redecides_throttle_line", result);
}

int test_threadedlog_push_format_overrides_component_and_restores_resolution() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "GENERAL[%L]");
	log << reset_component;
	IsolateLine(log);
	log.Format("Media", "MEDIA[%L]");
	log << component("Media") << push_format("TEMP[%L]") << Level::Info << "temporary" << std::endl;
	log << pop_format << Level::Info << "component again" << std::endl;
	log << reset_component << component("Other") << push_format("TEMP[%L]") << Level::Info << "other temporary" << std::endl;
	log << pop_format;
	log << reset_component << component("Media") << Level::Info << "media after component switch" << std::endl;
	const std::string expected =
		"TEMP[Info    ] temporary\n"
		"MEDIA[Info    ] component again\n"
		"TEMP[Info    ] other temporary\n"
		"MEDIA[Info    ] media after component switch\n";
	ASSERT_EQUAL("test_threadedlog_push_format_overrides_component_and_restores_resolution", expected, output.str());
	RETURN_TEST("test_threadedlog_push_format_overrides_component_and_restores_resolution", result);
}

// -------------------
// Filter lock
// -------------------

int test_threadedlog_filtered_endl_no_deadlock() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Info, "%L:");
	for (int i = 0; i < 50; ++i)
		tlog << Level::Debug << "hidden " << i << std::endl;
	tlog << Level::Info << "after filtered" << std::endl;
	ASSERT_EQUAL("test_threadedlog_filtered_endl_no_deadlock", std::string("Info    : after filtered\n"), output.str());
	RETURN_TEST("test_threadedlog_filtered_endl_no_deadlock", result);
}

int test_threadedlog_filtered_hot_path() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Info, "%L:");
	constexpr int threads = 8;
	constexpr int repeats = 5000;
	std::atomic<int> completed{0};
	auto worker = [&](int id) {
		for (int i = 0; i < repeats; ++i)
			tlog << Level::Debug << "discarded-" << id << ':' << i << std::endl;
		completed.fetch_add(1, std::memory_order_release);
	};
	std::vector<std::thread> pool;
	pool.reserve(threads);
	for (int id = 0; id < threads; ++id)
		pool.emplace_back(worker, id);
	for (auto& thread : pool)
		thread.join();
	ASSERT_EQUAL("test_threadedlog_filtered_hot_path (workers)", threads, completed.load(std::memory_order_acquire));
	ASSERT_EQUAL("test_threadedlog_filtered_hot_path (no output)", std::string{}, output.str());
	tlog << Level::Info << "after filtered hot path" << std::endl;
	ASSERT_EQUAL("test_threadedlog_filtered_hot_path", "Info    : after filtered hot path\n", output.str());
	RETURN_TEST("test_threadedlog_filtered_hot_path", result);
}

int test_threadedlog_filtered_multithreaded_then_info() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Info, "%L:");
	const int threads = 4;
	const int repeats = 40;
	std::atomic<int> done{0};
	auto worker = [&](int id) {
		for (int i = 0; i < repeats; ++i)
			tlog << Level::Debug << "d" << id << ":" << i << std::endl;
		done.fetch_add(1);
	};
	std::vector<std::thread> pool;
	for (int t = 0; t < threads; ++t)
		pool.emplace_back(worker, t);
	for (auto& th : pool)
		th.join();
	ASSERT_EQUAL("test_threadedlog_filtered_multithreaded_then_info (workers)", threads, done.load());
	tlog << Level::Info << "ok" << std::endl;
	ASSERT_EQUAL("test_threadedlog_filtered_multithreaded_then_info", std::string("Info    : ok\n"), output.str());
	RETURN_TEST("test_threadedlog_filtered_multithreaded_then_info", result);
}

// -------------------
// Floor
// -------------------

int test_threadedlog_critical_levels_are_never_filtered() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Fatal, "%L:");
	tlog << Level::LowLevel << "hidden low level" << std::endl;
	tlog << Level::Debug << "hidden debug" << std::endl;
	tlog << Level::Warning << "visible warning" << std::endl;
	tlog << Level::Notice << "hidden notice" << std::endl;
	tlog << Level::Info << "hidden info" << std::endl;
	tlog << Level::Error << "visible error" << std::endl;
	tlog << Level::Fatal << "visible fatal" << std::endl;
	const std::string expected =
		"Warning : visible warning\n"
		"Error   : visible error\n"
		"Fatal   : visible fatal\n";
	ASSERT_EQUAL("test_threadedlog_critical_levels_are_never_filtered", expected, output.str());
	RETURN_TEST("test_threadedlog_critical_levels_are_never_filtered", result);
}

int test_threadedlog_enabled_and_views() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%L:");
	ASSERT_TRUE("test_threadedlog_enabled_and_views (Info)", log.Enabled(Level::Info));
	ASSERT_TRUE("test_threadedlog_enabled_and_views (Warning)", log.Enabled(Level::Warning));
	ASSERT_FALSE("test_threadedlog_enabled_and_views (Debug)", log.Enabled(Level::Debug));
	const std::string owned = "owned";
	const std::wstring wowned = L"wide";
	log << Level::Info << std::string_view{owned} << " " << std::wstring_view{wowned} << std::endl;
	log << Level::Debug << std::string_view{owned} << std::endl;
	ASSERT_EQUAL("test_threadedlog_enabled_and_views", "Info    : owned wide\n", output.str());
	RETURN_TEST("test_threadedlog_enabled_and_views", result);
}

// -------------------
// Format
// -------------------

int test_threadedlog_push_pop_format_is_line_safe() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Info, "BASE[%L]");
	constexpr int threads = 4;
	constexpr int repeats = 100;
	std::vector<std::thread> pool;
	pool.reserve(threads);
	for (int id = 0; id < threads; ++id) {
		pool.emplace_back([&, id] {
			for (int i = 0; i < repeats; ++i) {
				tlog << push_format("T" + std::to_string(id) + "[%L]")
					<< Level::Info << "message" << std::endl
					<< pop_format;
			}
		});
	}
	for (auto& thread : pool)
		thread.join();
	std::istringstream input(output.str());
	std::string line;
	int count = 0;
	while (std::getline(input, line)) {
		ASSERT_TRUE("test_threadedlog_push_pop_format_is_line_safe (format)",
			std::regex_match(line, std::regex("^T[0-3]\\[Info    \\] message$")));
		++count;
	}
	ASSERT_EQUAL("test_threadedlog_push_pop_format_is_line_safe (count)", threads * repeats, count);
	tlog << pop_format;
	tlog << Level::Info << "after empty pop" << std::endl;
	ASSERT_TRUE("test_threadedlog_push_pop_format_is_line_safe (restored base)",
		output.str().ends_with("BASE[Info    ] after empty pop\n"));
	RETURN_TEST("test_threadedlog_push_pop_format_is_line_safe", result);
}

// -------------------
// Groups
// -------------------

int test_threadedlog_filtered_group_releases_lock() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Info, "%g[%L]");
	tlog << group("Hidden") << Level::Debug << "hidden" << std::endl;
	tlog << Level::Info << "visible" << std::endl;
	ASSERT_EQUAL("test_threadedlog_filtered_group_releases_lock", "[Info    ] visible\n", output.str());
	RETURN_TEST("test_threadedlog_filtered_group_releases_lock", result);
}

int test_threadedlog_groups_do_not_mix() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Info, "%g[%L]");
	constexpr int threads = 4;
	constexpr int repeats = 200;
	std::vector<std::thread> pool;
	pool.reserve(threads);
	for (int id = 0; id < threads; ++id) {
		pool.emplace_back([&, id] {
			for (int i = 0; i < repeats; ++i)
				tlog << group("G" + std::to_string(id)) << Level::Info << "message" << std::endl;
		});
	}
	for (auto& thread : pool)
		thread.join();
	std::istringstream input(output.str());
	std::string line;
	int count = 0;
	while (std::getline(input, line)) {
		ASSERT_TRUE("test_threadedlog_groups_do_not_mix (line)",
			std::regex_match(line, std::regex("^G[0-3]\\[Info    \\] message$")));
		++count;
	}
	ASSERT_EQUAL("test_threadedlog_groups_do_not_mix (count)", threads * repeats, count);
	RETURN_TEST("test_threadedlog_groups_do_not_mix", result);
}

// -------------------
// Line lock
// -------------------

int test_threadedlog_deterministic_ordering() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Info, "%L:");
	const int threads = 6;
	std::vector<std::promise<void>> start_promises(threads);
	std::vector<std::future<void>> start_futures;
	start_futures.reserve(threads);
	for (int i = 0; i < threads; ++i)
		start_futures.push_back(start_promises[i].get_future());
	std::vector<std::promise<void>> done_promises(threads);
	std::vector<std::future<void>> done_futures;
	done_futures.reserve(threads);
	for (int i = 0; i < threads; ++i)
		done_futures.push_back(done_promises[i].get_future());
	std::vector<std::thread> pool;
	for (int i = 0; i < threads; ++i) {
		pool.emplace_back([i, &tlog, &start_futures, &done_promises]() {
			start_futures[i].get();
			tlog << Level::Info << "T" << i << std::endl;
			done_promises[i].set_value();
		});
	}
	for (int i = 0; i < threads; ++i) {
		start_promises[i].set_value();
		done_futures[i].get();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	for (auto& th : pool)
		th.join();
	std::istringstream in(output.str());
	std::string line;
	int idx = 0;
	while (std::getline(in, line)) {
		if (line.empty())
			continue;
		ASSERT_EQUAL("test_threadedlog_deterministic_ordering", "Info    : T" + std::to_string(idx), line);
		++idx;
	}
	ASSERT_EQUAL("test_threadedlog_deterministic_ordering (count)", threads, idx);
	RETURN_TEST("test_threadedlog_deterministic_ordering", result);
}

int test_threadedlog_level_switch_flush() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Debug, "%L:");
	tlog << Level::Info << "part1";
	tlog << Level::Debug << "part2" << std::endl;
	const std::string out = output.str();
	ASSERT_TRUE("test_threadedlog_level_switch_flush (part1)", out.find("part1") != std::string::npos);
	ASSERT_TRUE("test_threadedlog_level_switch_flush (part2)", out.find("part2") != std::string::npos);
	RETURN_TEST("test_threadedlog_level_switch_flush", result);
}

int test_threadedlog_multithreaded_ordering() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Info, "%L:");
	const int threads = 8;
	const int repeats = 50;
	auto worker = [&](int id) {
		for (int i = 0; i < repeats; ++i)
			tlog << Level::Info << "T" << id << ":" << i << std::endl;
	};
	std::vector<std::thread> pool;
	for (int t = 0; t < threads; ++t)
		pool.emplace_back(worker, t);
	for (auto& th : pool)
		th.join();
	std::istringstream in(output.str());
	std::string line;
	int count = 0;
	const std::regex r("^Info\\s+: T\\d+:\\d+$");
	while (std::getline(in, line)) {
		if (line.empty())
			continue;
		ASSERT_TRUE("test_threadedlog_multithreaded_ordering (line_format)", std::regex_match(line, r));
		++count;
	}
	ASSERT_EQUAL("test_threadedlog_multithreaded_ordering (count)", threads * repeats, count);
	RETURN_TEST("test_threadedlog_multithreaded_ordering", result);
}

int test_threadedlog_no_endl_sharing() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Info, "%L:");
	const int threads = 4;
	const int parts = 10;
	auto worker = [&](int id) {
		for (int i = 0; i < parts; ++i)
			tlog << Level::Info << "p" << id << ":" << i << " ";
		tlog << std::endl;
	};
	std::vector<std::thread> pool;
	for (int t = 0; t < threads; ++t)
		pool.emplace_back(worker, t);
	for (auto& th : pool)
		th.join();
	std::istringstream in(output.str());
	std::string line;
	int count = 0;
	while (std::getline(in, line)) {
		if (!line.empty())
			++count;
	}
	ASSERT_EQUAL("test_threadedlog_no_endl_sharing", threads, count);
	RETURN_TEST("test_threadedlog_no_endl_sharing", result);
}

// -------------------
// Scope
// -------------------

int test_threadedlog_component_stack_push_pop_and_join() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%c %L:");
	log << reset_component;
	IsolateLine(log);
	log << component("Multimedia") << component("Decoder") << Level::Info << "nested" << std::endl;
	log << pop_component << Level::Info << "parent" << std::endl;
	log << reset_component << Level::Info << "root" << std::endl;
	ASSERT_EQUAL("test_threadedlog_component_stack_push_pop_and_join",
		"Multimedia/Decoder Info    : nested\nMultimedia Info    : parent\n Info    : root\n",
		output.str());
	RETURN_TEST("test_threadedlog_component_stack_push_pop_and_join", result);
}

int test_threadedlog_scope_does_not_use_tls_stack() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%c %L:");
	log << reset_component;
	IsolateLine(log);
	log << component("TLS");
	auto scoped = log.Scope("Multimedia");
	scoped << Level::Info << "scoped" << std::endl;
	log << Level::Info << "stack" << std::endl;
	ASSERT_EQUAL("test_threadedlog_scope_does_not_use_tls_stack",
		"Multimedia Info    : scoped\nTLS Info    : stack\n", output.str());
	log << reset_component;
	RETURN_TEST("test_threadedlog_scope_does_not_use_tls_stack", result);
}

int test_threadedlog_scope_format_inherits_parent_and_leaf_wins() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "ROOT %L:");
	log << reset_component;
	IsolateLine(log);
	log.Format("Multimedia", "MM %c %L:");
	auto dec = log.Scope("Multimedia/Decoder");
	dec << Level::Info << "inherited" << std::endl;
	dec->Format("DEC %c %L:");
	dec << Level::Info << "leaf" << std::endl;
	log << Level::Info << "root" << std::endl;
	ASSERT_EQUAL("test_threadedlog_scope_format_inherits_parent_and_leaf_wins",
		"MM Multimedia/Decoder Info    : inherited\nDEC Multimedia/Decoder Info    : leaf\nROOT Info    : root\n",
		output.str());
	RETURN_TEST("test_threadedlog_scope_format_inherits_parent_and_leaf_wins", result);
}

int test_threadedlog_scope_is_threadedlog_and_concurrent() {
	int result = 0;
	std::ostringstream output;
	auto root = std::make_shared<ThreadedLog>(output, Level::Info, "%c %L:");
	auto scoped = root->Scope("Buffer/Pipeline");
	ASSERT_TRUE("Scope preserves ThreadedLog", dynamic_cast<ThreadedLog*>(scoped.get()) != nullptr);
	constexpr int kThreads = 12;
	constexpr int kLines = 64;
	std::vector<std::thread> workers;
	workers.reserve(static_cast<std::size_t>(kThreads));
	for (int t = 0; t < kThreads; ++t) {
		workers.emplace_back([scoped]() {
			for (int i = 0; i < kLines; ++i)
				scoped << Level::Info << "n" << std::endl;
		});
	}
	for (auto& worker : workers)
		worker.join();
	const auto text = output.str();
	std::size_t lines = 0;
	for (char c : text) {
		if (c == '\n')
			++lines;
	}
	ASSERT_EQUAL("test_threadedlog_scope_is_threadedlog_and_concurrent",
		static_cast<std::size_t>(kThreads * kLines), lines);
	RETURN_TEST("test_threadedlog_scope_is_threadedlog_and_concurrent", result);
}

int test_threadedlog_scope_path_and_nested_scope() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%c %L:");
	log << reset_component;
	IsolateLine(log);
	auto mm = log.Scope("Multimedia");
	auto dec = mm->Scope("Decoder");
	auto abs = log.Scope("Multimedia/Encoder");
	log << Level::Info << "root" << std::endl;
	mm << Level::Info << "mm" << std::endl;
	dec << Level::Info << "dec" << std::endl;
	abs << Level::Info << "enc" << std::endl;
	ASSERT_EQUAL("test_threadedlog_scope_path_and_nested_scope",
		" Info    : root\nMultimedia Info    : mm\nMultimedia/Decoder Info    : dec\nMultimedia/Encoder Info    : enc\n",
		output.str());
	RETURN_TEST("test_threadedlog_scope_path_and_nested_scope", result);
}

int test_threadedlog_scope_shares_lock_and_path() {
	int result = 0;
	std::ostringstream output;
	auto log = std::make_shared<ThreadedLog>(output, Level::Info, "%c %L:");
	*log << reset_component;
	IsolateLine(*log);
	auto dec = log->Scope("Multimedia/Decoder");
	dec << Level::Info << "dec" << std::endl;
	log << Level::Info << "root" << std::endl;
	ASSERT_EQUAL("test_threadedlog_scope_shares_lock_and_path",
		"Multimedia/Decoder Info    : dec\n Info    : root\n", output.str());
	RETURN_TEST("test_threadedlog_scope_shares_lock_and_path", result);
}

int test_threadedlog_scope_throttle_is_leaf() {
	int result = 0;
	std::ostringstream output;
	auto log = std::make_shared<ThreadedLog>(output, Level::Info, "%c %L:");
	*log << reset_component;
	IsolateLine(*log);
	auto dec = log->Scope("Multimedia/Decoder");
	dec->Throttle(0.0, 1);
	dec << Level::Info << "one" << std::endl;
	dec << Level::Info << "two" << std::endl;
	log << Level::Info << "root" << std::endl;
	ASSERT_EQUAL("test_threadedlog_scope_throttle_is_leaf",
		"Multimedia/Decoder Info    : one\n Info    : root\n", output.str());
	RETURN_TEST("test_threadedlog_scope_throttle_is_leaf", result);
}

// -------------------
// Throttle
// -------------------

int test_threadedlog_flush_mid_line_preserves_lock_owner() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%L:");
	ThrottleSpec spec;
	spec.Policy = ThrottlePolicy::Window;
	spec.WindowKeep = 1;
	spec.WindowPeriod = 2;
	log.Throttle(spec);
	log << Level::Info << "seed" << std::endl;
	log << Level::Info << "dropped" << std::endl;
	log << Level::Info << "first";
	log.FlushThrottle();
	log << Level::Fatal << "after" << std::endl;
	std::thread other([&] { log << Level::Fatal << "other" << std::endl; });
	other.join();
	ASSERT_TRUE("test_threadedlog_flush_mid_line_preserves_lock_owner",
		output.str().find("Info    : first\n") != std::string::npos &&
		output.str().find("Info    : dropped 1 messages\n") != std::string::npos &&
		output.str().find("Fatal   : after\n") != std::string::npos);
	ASSERT_TRUE("test_threadedlog_flush_mid_line_preserves_lock_owner (other)",
		output.str().find("Fatal   : other\n") != std::string::npos);
	RETURN_TEST("test_threadedlog_flush_mid_line_preserves_lock_owner", result);
}

int test_threadedlog_flush_throttle_releases_lock() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%L:");
	ThrottleSpec spec;
	spec.Policy = ThrottlePolicy::Window;
	spec.WindowKeep = 1;
	spec.WindowPeriod = 2;
	log.Throttle(spec);
	log << Level::Info << "first" << std::endl;
	log << Level::Info << "dropped" << std::endl;
	log.FlushThrottle();
	log << Level::Fatal << "fatal after flush" << std::endl;
	ASSERT_TRUE("test_threadedlog_flush_throttle_releases_lock",
		output.str().find("Fatal   : fatal after flush\n") != std::string::npos);
	RETURN_TEST("test_threadedlog_flush_throttle_releases_lock", result);
}

int test_threadedlog_inherited_drop_summary_stays_on_the_leaf() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog log(output, Level::Debug, "%c %L:");
	log << reset_component;
	IsolateLine(log);
	auto mm = log.Scope("Multimedia");
	mm->Throttle(Level::Debug, 0.0, 1);
	auto encoder = mm->Scope("Encoder");
	auto watermark = mm->Scope("Filters/Video/watermark");
	*encoder << Level::Debug << "e0" << std::endl;
	*encoder << Level::Debug << "e1" << std::endl;
	*watermark << Level::Debug << "w0" << std::endl;
	const std::string out = output.str();
	ASSERT_TRUE("test_threadedlog_inherited_drop_summary_stays_on_the_leaf (encoder kept)",
		out.find("Multimedia/Encoder Debug   : e0\n") != std::string::npos);
	ASSERT_TRUE("test_threadedlog_inherited_drop_summary_stays_on_the_leaf (watermark kept)",
		out.find("Multimedia/Filters/Video/watermark Debug   : w0\n") != std::string::npos);
	ASSERT_TRUE("test_threadedlog_inherited_drop_summary_stays_on_the_leaf (no watermark dropped)",
		out.find("Filters/Video/watermark Debug   : dropped") == std::string::npos);
	RETURN_TEST("test_threadedlog_inherited_drop_summary_stays_on_the_leaf", result);
}

int test_threadedlog_inherited_throttle_state_is_per_leaf() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog log(output, Level::Debug, "%c %L:");
	log << reset_component;
	IsolateLine(log);
	auto mm = log.Scope("Multimedia");
	mm->Throttle(Level::Debug, 0.0, 1);
	auto encoder = mm->Scope("Encoder");
	auto watermark = mm->Scope("Filters/Video/watermark");
	*encoder << Level::Debug << "e0" << std::endl;
	*encoder << Level::Debug << "e1" << std::endl;
	*encoder << Level::Debug << "e2" << std::endl;
	*watermark << Level::Debug << "w0" << std::endl;
	*watermark << Level::Debug << "w1" << std::endl;
	ASSERT_EQUAL("test_threadedlog_inherited_throttle_state_is_per_leaf",
		"Multimedia/Encoder Debug   : e0\n"
		"Multimedia/Filters/Video/watermark Debug   : w0\n",
		output.str());
	RETURN_TEST("test_threadedlog_inherited_throttle_state_is_per_leaf", result);
}

int test_threadedlog_throttle_drops_without_deadlock() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%L:");
	log.Throttle(0.0, 10);
	constexpr int threads = 8;
	constexpr int repeats = 100;
	std::vector<std::thread> pool;
	pool.reserve(threads);
	for (int id = 0; id < threads; ++id) {
		pool.emplace_back([&, id] {
			for (int index = 0; index < repeats; ++index)
				log << Level::Info << id << ':' << index << std::endl;
		});
	}
	for (auto& thread : pool)
		thread.join();
	log << Level::Fatal << "fatal survives" << std::endl;
	ASSERT_TRUE("test_threadedlog_throttle_drops_without_deadlock",
		output.str().find("Fatal   : fatal survives\n") != std::string::npos);
	RETURN_TEST("test_threadedlog_throttle_drops_without_deadlock", result);
}

// -------------------
// Wide
// -------------------

int test_threadedlog_filtered_wide_skips_conversion() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Info, "%L:");
	tlog << Level::Debug << std::wstring(1, static_cast<wchar_t>(0xD800)) << std::endl;
	ASSERT_EQUAL("test_threadedlog_filtered_wide_skips_conversion (no output)", std::string{}, output.str());
	tlog << Level::Info << "after filtered invalid input" << std::endl;
	ASSERT_EQUAL("test_threadedlog_filtered_wide_skips_conversion", "Info    : after filtered invalid input\n", output.str());
	RETURN_TEST("test_threadedlog_filtered_wide_skips_conversion", result);
}

int test_threadedlog_invalid_wide_releases_line_lock() {
	int result = 0;
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Info, "%L:");
	tlog << Level::Info << std::wstring(1, static_cast<wchar_t>(0xD800)) << std::endl;
	tlog << Level::Info << "after invalid input" << std::endl;
	ASSERT_EQUAL("test_threadedlog_invalid_wide_releases_line_lock",
		"Info    : \xEF\xBF\xBD\nInfo    : after invalid input\n", output.str());
	RETURN_TEST("test_threadedlog_invalid_wide_releases_line_lock", result);
}

int main() {
	int result = 0;

	// -------------------
	// Basic emit
	// -------------------
	result += test_smart_pointer_usage();
	result += test_shared_unwraps_without_dereference();
	result += test_threadedlog_basic();

	// -------------------
	// Binary span
	// -------------------
	result += test_threadedlog_span_default_is_base64();
	result += test_threadedlog_span_filtered();
	result += test_threadedlog_span_hex();
	result += test_threadedlog_span_vector_converts();
	result += test_threadedlog_every_accepted_payload();

	// -------------------
	// Color
	// -------------------
	result += test_threadedlog_colored_lines_do_not_mix();

	// -------------------
	// Components
	// -------------------
	result += test_threadedlog_component_color_override_has_priority();
	result += test_threadedlog_component_does_not_hold_line_lock();
	result += test_threadedlog_component_format_priority_and_fallback();
	result += test_threadedlog_component_formats_do_not_mix();
	result += test_threadedlog_component_header_is_sticky_and_resettable();
	result += test_threadedlog_component_without_token_preserves_legacy_output();
	result += test_threadedlog_components_are_thread_local();
	result += test_threadedlog_empty_component_does_not_push();
	result += test_threadedlog_format_change_redecides_throttle_line();
	result += test_threadedlog_push_format_overrides_component_and_restores_resolution();

	// -------------------
	// Filter lock
	// -------------------
	result += test_threadedlog_filtered_endl_no_deadlock();
	result += test_threadedlog_filtered_hot_path();
	result += test_threadedlog_filtered_multithreaded_then_info();

	// -------------------
	// Floor
	// -------------------
	result += test_threadedlog_critical_levels_are_never_filtered();
	result += test_threadedlog_enabled_and_views();

	// -------------------
	// Format
	// -------------------
	result += test_threadedlog_push_pop_format_is_line_safe();

	// -------------------
	// Groups
	// -------------------
	result += test_threadedlog_filtered_group_releases_lock();
	result += test_threadedlog_groups_do_not_mix();

	// -------------------
	// Line lock
	// -------------------
	result += test_threadedlog_deterministic_ordering();
	result += test_threadedlog_level_switch_flush();
	result += test_threadedlog_multithreaded_ordering();
	result += test_threadedlog_no_endl_sharing();

	// -------------------
	// Scope
	// -------------------
	result += test_threadedlog_component_stack_push_pop_and_join();
	result += test_threadedlog_scope_does_not_use_tls_stack();
	result += test_threadedlog_scope_format_inherits_parent_and_leaf_wins();
	result += test_threadedlog_scope_is_threadedlog_and_concurrent();
	result += test_threadedlog_scope_path_and_nested_scope();
	result += test_threadedlog_scope_shares_lock_and_path();
	result += test_threadedlog_scope_throttle_is_leaf();

	// -------------------
	// Throttle
	// -------------------
	result += test_threadedlog_flush_mid_line_preserves_lock_owner();
	result += test_threadedlog_flush_throttle_releases_lock();
	result += test_threadedlog_inherited_drop_summary_stays_on_the_leaf();
	result += test_threadedlog_inherited_throttle_state_is_per_leaf();
	result += test_threadedlog_throttle_drops_without_deadlock();

	// -------------------
	// Wide
	// -------------------
	result += test_threadedlog_filtered_wide_skips_conversion();
	result += test_threadedlog_invalid_wide_releases_line_lock();

	if (result == 0)
		std::cout << "All tests passed!" << std::endl;
	else
		std::cout << result << " tests failed." << std::endl;
	return result;
}
