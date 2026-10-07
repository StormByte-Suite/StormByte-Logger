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
#include <StormByte/byte_size.hxx>
#include <StormByte/logger/exception.hxx>
#include <StormByte/logger/threaded_log.hxx>
#include <StormByte/safe/binary.hxx>
#include <StormByte/safe/map.hxx>
#include <StormByte/safe/optional.hxx>
#include <StormByte/safe/pointers.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/safe/vector.hxx>
#include <StormByte/safe/wstring.hxx>
#include <StormByte/size.hxx>
#include <StormByte/test_handlers.h>

#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace StormByte::Logger;
using StormByte::Safe::Binary;
using StormByte::Safe::String;

static_assert(StormByte::Type::MaybeSafe<ThrottleSpec>);
static_assert(StormByte::Type::MaybeSafe<Log>);
static_assert(StormByte::Type::MaybeSafe<GroupManip>);
static_assert(StormByte::Type::MaybeSafe<ComponentManip>);
static_assert(StormByte::Type::MaybeSafe<FormatManip>);
static_assert(StormByte::Type::MaybeSafe<ColorManip>);
static_assert(StormByte::Type::MaybeSafe<ThreadedLog>);
static_assert(StormByte::Type::MaybeSafe<StormByte::Logger::Exception>);
static_assert(StormByte::Type::MaybeSafe<StormByte::Logger::ThrottleError>);
static_assert(StormByte::Type::MaybeSafe<SinkFunction>);
static_assert(!StormByte::Type::IsSafe<SinkFunction>::value);
static_assert(StormByte::Type::IsSafe<StormByte::Safe::Binary>::value);
static_assert(StormByte::Type::IsSafe<StormByte::Safe::String>::value);
static_assert(StormByte::Type::IsSafe<StormByte::Safe::WString>::value);
static_assert(StormByte::Type::IsSafe<StormByte::Safe::Optional<Level>>::value);
static_assert(StormByte::Type::IsSafe<StormByte::Safe::Vector<StormByte::Safe::String>>::value);
static_assert(StormByte::Type::IsSafe<StormByte::Safe::Map<StormByte::Safe::String, StormByte::Safe::String>>::value);

namespace {
	std::ostream& WriteNewlineThenThrow(std::ostream& output) {
		output.put('\n');
		throw std::runtime_error("stream manipulator failure");
	}

	struct SinkFunctionContext {
		std::string* output;
		int* releases;
		bool fail_once;
		bool fail_foreign_once = false;
	};

	struct CrossLoggerLockContext {
		std::promise<void> first_call;
		std::promise<void> concurrent_call;
		std::shared_future<void> release_first;
		std::atomic<bool> first_seen{false};
		std::atomic<bool> concurrent_seen{false};
		std::atomic<int> active{0};
	};

	StormByte::Safe::Status BlockFirstSinkCall(void* context, const StormByte::Safe::String&) {
		auto& sink = *static_cast<CrossLoggerLockContext*>(context);
		if (sink.active.fetch_add(1, std::memory_order_acq_rel) != 0 &&
			!sink.concurrent_seen.exchange(true, std::memory_order_acq_rel))
			sink.concurrent_call.set_value();
		if (!sink.first_seen.exchange(true, std::memory_order_acq_rel)) {
			sink.first_call.set_value();
			sink.release_first.wait();
		}
		sink.active.fetch_sub(1, std::memory_order_release);
		return StormByte::Safe::Status::Success;
	}

	void* CloneCrossLoggerContext(const void*) noexcept {
		// Promise-backed synchronization state is deliberately non-cloneable.
		return nullptr;
	}

	void ReleaseCrossLoggerContext(void*) noexcept {}

	StormByte::Safe::Status CaptureSinkFunction(void* context, const StormByte::Safe::String& text) {
		auto& sink = *static_cast<SinkFunctionContext*>(context);
		const std::string_view view = static_cast<std::string_view>(text);
		if (sink.fail_once && view == "failure") {
			sink.fail_once = false;
			throw StormByte::Logger::Exception("sink failure");
		}
		if (sink.fail_foreign_once && view == "foreign") {
			sink.fail_foreign_once = false;
			throw std::runtime_error("foreign sink failure");
		}
		sink.output->append(view);
		return StormByte::Safe::Status::Success;
	}

	void* CloneSinkFunctionContext(const void* context) noexcept {
		try {
			auto clone = std::make_unique<SinkFunctionContext>(*static_cast<const SinkFunctionContext*>(context));
			return clone.release();
		} catch (...) {
			return nullptr;
		}
	}

	void ReleaseSinkFunction(void* context) noexcept {
		auto* sink = static_cast<SinkFunctionContext*>(context);
		++*sink->releases;
		delete sink;
	}

	void IsolateLine(Log& log) {
		if (!log.Enabled(Level::LowLevel))
			log << Level::LowLevel << std::endl;
	}
}

// -------------------
// Basic emit
// -------------------

int test_smart_pointer_usage() {
	std::ostringstream output;
	std::shared_ptr<StormByte::Logger::Log> log = std::make_shared<StormByte::Logger::ThreadedLog>(output, Level::Info, "%L:");
	log << Level::Info << "Smart pointer log message" << std::endl;
	ASSERT_EQUAL(std::string("Info    : Smart pointer log message\n"), output.str());
	RETURN_TEST(0);
}

int test_shared_unwraps_without_dereference() {
	std::ostringstream output;
	StormByte::Safe::Shared<ThreadedLog> log = StormByte::Safe::Shared<ThreadedLog>::MakePointer<ThreadedLog>(output, Level::Info, "%L:");
	log << Level::Info << "Hola" << std::endl;
	ASSERT_EQUAL("Info    : Hola\n", output.str());
	RETURN_TEST(0);
}

int test_threadedlog_basic() {
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Info, "%L:");
	tlog << Level::Info << "Threaded basic message" << std::endl;
	ASSERT_EQUAL(std::string("Info    : Threaded basic message\n"), output.str());
	RETURN_TEST(0);
}

int test_threadedlog_safe_function_crosses_dll_contains_logger_exception_and_continues() {
	std::string output;
	int releases = 0;
	{
		SinkFunction callback{
			new SinkFunctionContext{&output, &releases, true, true},
			&CaptureSinkFunction,
			&CloneSinkFunctionContext,
			&ReleaseSinkFunction
		};
		ThreadedLog log(std::move(callback), Level::Info, "%L:");
		log << Level::Info << "failure" << std::endl;
		log << Level::Info << "foreign" << std::endl;
		std::thread next([&] { log << Level::Info << "after" << std::endl; });
		next.join();
	}
	ASSERT_EQUAL("Info    : \nInfo    : \nInfo    : after\n", output);
	ASSERT_EQUAL(1, releases);
	RETURN_TEST(0);
}

int test_threadedlog_nested_instances_keep_distinct_locks() {
	std::ostringstream first_output;
	ThreadedLog first(first_output, Level::Info, "%L:");
	CrossLoggerLockContext context;
	std::promise<void> release_first;
	context.release_first = release_first.get_future().share();
	auto first_call = context.first_call.get_future();
	auto concurrent_call = context.concurrent_call.get_future();
	SinkFunction callback{&context, &BlockFirstSinkCall, &CloneCrossLoggerContext, &ReleaseCrossLoggerContext};
	ThreadedLog second(std::move(callback), Level::Info, "%L:");
	std::thread owner([&] {
		first << Level::Info << "held";
		second << Level::Info << "blocked sink" << std::endl;
		first << std::endl;
		first << Level::Info << "cleanup" << std::endl;
	});
	const bool entered_sink = first_call.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
	std::thread contender;
	bool overlapped = false;
	if (entered_sink) {
		contender = std::thread([&] { second << Level::Info << "contender" << std::endl; });
		overlapped = concurrent_call.wait_for(std::chrono::milliseconds(200)) == std::future_status::ready;
	}
	release_first.set_value();
	owner.join();
	if (contender.joinable())
		contender.join();
	ASSERT_TRUE(entered_sink);
	ASSERT_FALSE(overlapped);
	RETURN_TEST(0);
}

int test_threadedlog_throwing_newline_manipulator_releases_lock() {
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%L:");
	log << Level::Info << "before" << &WriteNewlineThenThrow;
	std::thread next([&] { log << Level::Info << "after" << std::endl; });
	next.join();
	ASSERT_EQUAL("Info    : before\nInfo    : after\n", output.str());
	RETURN_TEST(0);
}

int test_threadedlog_throttle_error_releases_open_line_lock() {
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%L:");
	log << Level::Info << "partial";
	ASSERT_THROWS(log.Throttle(-1.0, 1), ThrottleError);
	std::thread next([&] { log << Level::Fatal << "after" << std::endl; });
	const auto ready = std::async(std::launch::async, [&next] { next.join(); });
	const bool completed = ready.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
	if (!completed) {
		log << std::endl;
		ready.wait();
	}
	log << std::endl;
	ASSERT_TRUE(completed);
	ASSERT_CONTAINS(output.str(), "Fatal   : after\n");
	RETURN_TEST(0);
}

// -------------------
// Binary span
// -------------------

int test_threadedlog_span_default_is_base64() {
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%L:");
	const std::vector<std::byte> raw{
		std::byte{'H'}, std::byte{'e'}, std::byte{'l'}, std::byte{'l'}, std::byte{'o'}
	};
	log << Level::Info << std::span<const std::byte>{raw} << std::endl;
	ASSERT_EQUAL(std::string("Info    : ") + static_cast<std::string>(StormByte::Base64Encode(raw)) + "\n", output.str());
	RETURN_TEST(0);
}

int test_threadedlog_span_filtered() {
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%L:");
	const std::vector<std::byte> raw{std::byte{0xFF}};
	log << Level::Debug << raw << std::endl;
	ASSERT_EMPTY(output.str());
	log << Level::Info << "after" << std::endl;
	ASSERT_EQUAL("Info    : after\n", output.str());
	RETURN_TEST(0);
}

int test_threadedlog_span_hex() {
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%L:");
	const std::vector<std::byte> raw{std::byte{0x01}, std::byte{0xAB}};
	const Binary dumped(raw);
	log << Level::Info << hex << raw << std::endl;
	ASSERT_EQUAL(std::string("Info    : ") + static_cast<std::string>(dumped.HexDump(StormByte::Size{16})) + "\n", output.str());
	RETURN_TEST(0);
}

int test_threadedlog_span_vector_converts() {
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%L:");
	const std::vector<std::byte> raw{std::byte{0x01}, std::byte{0x02}};
	log << Level::Info << raw << std::endl;
	ASSERT_EQUAL(std::string("Info    : ") + static_cast<std::string>(StormByte::Base64Encode(raw)) + "\n", output.str());
	RETURN_TEST(0);
}

int test_threadedlog_every_accepted_payload() {
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%L:");
	const StormByte::Safe::String owned{"owned"};
	const StormByte::Safe::String bytes{"c"};
	const StormByte::Safe::WString wide_owned{L"wide-owned"};
	const StormByte::Safe::WString wide_bytes{L"wide-c"};
	const std::string narrow_text{"string"};
	const char* literal_text = "literal";
	const StormByte::Size count{3};
	const StormByte::ByteSize octets{4};
	const Binary raw{std::byte{'Z'}};
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
		+ static_cast<std::string>(StormByte::Base64Encode(static_cast<std::span<const std::byte>>(raw)));
	ASSERT_EQUAL(std::string("Info    : ") + body + "\n", output.str());
	RETURN_TEST(0);
}

int test_threadedlog_safe_containers_keep_one_logical_line() {
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%L:");
	const StormByte::Safe::Optional<String> present{String{"value"}};
	const StormByte::Safe::Optional<String> empty{std::nullopt};
	const StormByte::Safe::Vector<StormByte::Safe::Optional<String>> values{present, empty};
	const StormByte::Safe::Map<String, StormByte::Safe::Optional<String>> entries(std::map<
		String, StormByte::Safe::Optional<String>>{
			{String{"alpha"}, present},
			{String{"beta"}, empty}
		});
	log << Level::Info << values << " " << entries << std::endl;
	const std::string expected =
		"Info    : [value, (empty Safe::Optional)] {\n"
		"\talpha: value\n\tbeta: (empty Safe::Optional)\n}\n";
	ASSERT_EQUAL(expected, output.str());
	RETURN_TEST(0);
}

// -------------------
// Color
// -------------------

int test_threadedlog_colored_lines_do_not_mix() {
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
		ASSERT_TRUE(line.starts_with("\033[36mInfo    : T"));
		ASSERT_TRUE(line.ends_with("\033[0m"));
		ASSERT_TRUE(line.find("\033[36m") != std::string::npos);
		++count;
	}
	ASSERT_EQUAL(threads * repeats, count);
	RETURN_TEST(0);
}

// -------------------
// Components
// -------------------

int test_threadedlog_component_color_override_has_priority() {
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%c[%L]");
	log << reset_component;
	IsolateLine(log);
	log.Color(Level::Info, Color::Blue);
	log.Color("Multimedia", Level::Info, Color::Red);
	log << component("Multimedia") << Level::Info << "red" << std::endl;
	log << reset_component << Level::Info << "blue" << std::endl;
	ASSERT_EQUAL("\033[31mMultimedia[Info    ] red\033[0m\n\033[34m[Info    ] blue\033[0m\n", output.str());
	RETURN_TEST(0);
}

int test_threadedlog_component_does_not_hold_line_lock() {
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Info, "%c[%L]");
	tlog << reset_component;
	tlog << component("Filtered") << Level::Debug << "hidden" << std::endl;
	tlog << Level::Info << "visible" << std::endl;
	ASSERT_EQUAL("Filtered[Info    ] visible\n", output.str());
	RETURN_TEST(0);
}

int test_threadedlog_component_format_priority_and_fallback() {
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
	ASSERT_EQUAL(expected, output.str());
	RETURN_TEST(0);
}

int test_threadedlog_component_formats_do_not_mix() {
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
		ASSERT_TRUE(std::regex_match(line, std::regex("^[AB]\\[Info    \\] message$")));
		++count;
	}
	ASSERT_EQUAL(threads * repeats, count);
	RETURN_TEST(0);
}

int test_threadedlog_component_header_is_sticky_and_resettable() {
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%c[%L]");
	log << reset_component;
	IsolateLine(log);
	log << component("Multimedia") << Level::Info << "first" << std::endl;
	log << Level::Info << "second" << std::endl;
	log << reset_component << Level::Info << "third" << std::endl;
	ASSERT_EQUAL("Multimedia[Info    ] first\nMultimedia[Info    ] second\n[Info    ] third\n", output.str());
	RETURN_TEST(0);
}

int test_threadedlog_component_without_token_preserves_legacy_output() {
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%L:");
	log << reset_component;
	IsolateLine(log);
	log << component("Hidden") << Level::Info << "message" << std::endl;
	ASSERT_EQUAL("Info    : message\n", output.str());
	RETURN_TEST(0);
}

int test_threadedlog_components_are_thread_local() {
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
		ASSERT_TRUE(std::regex_match(line, std::regex("^C[0-3]\\[Info    \\] message$")));
		++count;
	}
	ASSERT_EQUAL(threads * repeats, count);
	RETURN_TEST(0);
}

int test_threadedlog_empty_component_does_not_push() {
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%c[%L]");
	log << reset_component;
	IsolateLine(log);
	log << component("Multimedia") << Level::Info << "named" << std::endl;
	log << component("") << Level::Info << "still named" << std::endl;
	log << reset_component << Level::Info << "root" << std::endl;
	ASSERT_EQUAL("Multimedia[Info    ] named\nMultimedia[Info    ] still named\n[Info    ] root\n", output.str());
	RETURN_TEST(0);
}

int test_threadedlog_format_change_redecides_throttle_line() {
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "A[%L]");
	log << reset_component;
	IsolateLine(log);
	log.Throttle(0.0, 1);
	log << Level::Info << "first";
	log.Format("B[%L]");
	log << Level::Info << "second" << std::endl;
	ASSERT_EQUAL("A[Info    ] first\n", output.str());
	RETURN_TEST(0);
}

int test_threadedlog_push_format_overrides_component_and_restores_resolution() {
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
	ASSERT_EQUAL(expected, output.str());
	RETURN_TEST(0);
}

int test_threadedlog_malformed_format_tokens_remain_literal() {
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "bad[%Q %");
	log << Level::Info << "first" << std::endl;
	log.Format("good[%L]");
	log << Level::Info << "after format change" << std::endl;
	ASSERT_EQUAL("bad[%Q % first\ngood[Info    ] after format change\n", output.str());
	RETURN_TEST(0);
}

// -------------------
// Filter lock
// -------------------

int test_threadedlog_filtered_endl_no_deadlock() {
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Info, "%L:");
	for (int i = 0; i < 50; ++i)
		tlog << Level::Debug << "hidden " << i << std::endl;
	tlog << Level::Info << "after filtered" << std::endl;
	ASSERT_EQUAL(std::string("Info    : after filtered\n"), output.str());
	RETURN_TEST(0);
}

int test_threadedlog_filtered_hot_path() {
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
	ASSERT_EQUAL(threads, completed.load(std::memory_order_acquire));
	ASSERT_EMPTY(output.str());
	tlog << Level::Info << "after filtered hot path" << std::endl;
	ASSERT_EQUAL("Info    : after filtered hot path\n", output.str());
	RETURN_TEST(0);
}

int test_threadedlog_filtered_multithreaded_then_info() {
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
	ASSERT_EQUAL(threads, done.load());
	tlog << Level::Info << "ok" << std::endl;
	ASSERT_EQUAL(std::string("Info    : ok\n"), output.str());
	RETURN_TEST(0);
}

// -------------------
// Floor
// -------------------

int test_threadedlog_critical_levels_are_never_filtered() {
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
	ASSERT_EQUAL(expected, output.str());
	RETURN_TEST(0);
}

int test_threadedlog_enabled_and_views() {
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%L:");
	ASSERT_TRUE(log.Enabled(Level::Info));
	ASSERT_TRUE(log.Enabled(Level::Warning));
	ASSERT_FALSE(log.Enabled(Level::Debug));
	const std::string owned = "owned";
	const std::wstring wowned = L"wide";
	log << Level::Info << std::string_view{owned} << " " << std::wstring_view{wowned} << std::endl;
	log << Level::Debug << std::string_view{owned} << std::endl;
	ASSERT_EQUAL("Info    : owned wide\n", output.str());
	RETURN_TEST(0);
}

// -------------------
// Format
// -------------------

int test_threadedlog_push_pop_format_is_line_safe() {
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
		ASSERT_TRUE(std::regex_match(line, std::regex("^T[0-3]\\[Info    \\] message$")));
		++count;
	}
	ASSERT_EQUAL(threads * repeats, count);
	tlog << pop_format;
	tlog << Level::Info << "after empty pop" << std::endl;
	ASSERT_TRUE(output.str().ends_with("BASE[Info    ] after empty pop\n"));
	RETURN_TEST(0);
}

// -------------------
// Groups
// -------------------

int test_threadedlog_filtered_group_releases_lock() {
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Info, "%g[%L]");
	tlog << group("Hidden") << Level::Debug << "hidden" << std::endl;
	tlog << Level::Info << "visible" << std::endl;
	ASSERT_EQUAL("[Info    ] visible\n", output.str());
	RETURN_TEST(0);
}

int test_threadedlog_groups_do_not_mix() {
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
		ASSERT_TRUE(std::regex_match(line, std::regex("^G[0-3]\\[Info    \\] message$")));
		++count;
	}
	ASSERT_EQUAL(threads * repeats, count);
	RETURN_TEST(0);
}

// -------------------
// Line lock
// -------------------

int test_threadedlog_deterministic_ordering() {
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
		ASSERT_EQUAL("Info    : T" + std::to_string(idx), line);
		++idx;
	}
	ASSERT_EQUAL(threads, idx);
	RETURN_TEST(0);
}

int test_threadedlog_level_switch_flush() {
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Debug, "%L:");
	tlog << Level::Info << "part1";
	tlog << Level::Debug << "part2" << std::endl;
	const std::string out = output.str();
	ASSERT_CONTAINS(out, "part1");
	ASSERT_CONTAINS(out, "part2");
	RETURN_TEST(0);
}

int test_threadedlog_multithreaded_ordering() {
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
		ASSERT_TRUE(std::regex_match(line, r));
		++count;
	}
	ASSERT_EQUAL(threads * repeats, count);
	RETURN_TEST(0);
}

int test_threadedlog_no_endl_sharing() {
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
	ASSERT_EQUAL(threads, count);
	RETURN_TEST(0);
}

// -------------------
// Scope
// -------------------

int test_threadedlog_component_stack_push_pop_and_join() {
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%c %L:");
	log << reset_component;
	IsolateLine(log);
	log << component("Multimedia") << component("Decoder") << Level::Info << "nested" << std::endl;
	log << pop_component << Level::Info << "parent" << std::endl;
	log << reset_component << Level::Info << "root" << std::endl;
	ASSERT_EQUAL("Multimedia/Decoder Info    : nested\nMultimedia Info    : parent\n Info    : root\n", output.str());
	RETURN_TEST(0);
}

int test_threadedlog_scope_does_not_use_tls_stack() {
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%c %L:");
	log << reset_component;
	IsolateLine(log);
	log << component("TLS");
	auto scoped = log.Scope("Multimedia");
	scoped << Level::Info << "scoped" << std::endl;
	log << Level::Info << "stack" << std::endl;
	ASSERT_EQUAL("Multimedia Info    : scoped\nTLS Info    : stack\n", output.str());
	log << reset_component;
	RETURN_TEST(0);
}

int test_threadedlog_scope_format_inherits_parent_and_leaf_wins() {
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
	ASSERT_EQUAL(
		"MM Multimedia/Decoder Info    : inherited\nDEC Multimedia/Decoder Info    : leaf\nROOT Info    : root\n",
		output.str());
	RETURN_TEST(0);
}

int test_threadedlog_scope_is_threadedlog_and_concurrent() {
	std::ostringstream output;
	auto root = std::make_shared<ThreadedLog>(output, Level::Info, "%c %L:");
	auto scoped = root->Scope("Buffer/Pipeline");
	ASSERT_NOT_NULL(dynamic_cast<ThreadedLog*>(scoped.get()));
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
	ASSERT_EQUAL(static_cast<std::size_t>(kThreads * kLines), lines);
	RETURN_TEST(0);
}

int test_threadedlog_scope_path_and_nested_scope() {
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
	ASSERT_EQUAL(
		" Info    : root\nMultimedia Info    : mm\nMultimedia/Decoder Info    : dec\nMultimedia/Encoder Info    : enc\n",
		output.str());
	RETURN_TEST(0);
}

int test_threadedlog_scope_shares_lock_and_path() {
	std::ostringstream output;
	auto log = std::make_shared<ThreadedLog>(output, Level::Info, "%c %L:");
	*log << reset_component;
	IsolateLine(*log);
	auto dec = log->Scope("Multimedia/Decoder");
	dec << Level::Info << "dec" << std::endl;
	log << Level::Info << "root" << std::endl;
	ASSERT_EQUAL("Multimedia/Decoder Info    : dec\n Info    : root\n", output.str());
	RETURN_TEST(0);
}

int test_threadedlog_scope_throttle_is_leaf() {
	std::ostringstream output;
	auto log = std::make_shared<ThreadedLog>(output, Level::Info, "%c %L:");
	*log << reset_component;
	IsolateLine(*log);
	auto dec = log->Scope("Multimedia/Decoder");
	dec->Throttle(0.0, 1);
	dec << Level::Info << "one" << std::endl;
	dec << Level::Info << "two" << std::endl;
	log << Level::Info << "root" << std::endl;
	ASSERT_EQUAL("Multimedia/Decoder Info    : one\n Info    : root\n", output.str());
	RETURN_TEST(0);
}

// -------------------
// Throttle
// -------------------

int test_threadedlog_flush_mid_line_preserves_lock_owner() {
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
	ASSERT_CONTAINS(output.str(), "Info    : first\n");
	ASSERT_CONTAINS(output.str(), "Info    : dropped 1 messages\n");
	ASSERT_CONTAINS(output.str(), "Fatal   : after\n");
	ASSERT_CONTAINS(output.str(), "Fatal   : other\n");
	RETURN_TEST(0);
}

int test_threadedlog_flush_throttle_releases_lock() {
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
	ASSERT_CONTAINS(output.str(), "Fatal   : fatal after flush\n");
	RETURN_TEST(0);
}

int test_threadedlog_inherited_drop_summary_stays_on_the_leaf() {
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
	ASSERT_CONTAINS(out, "Multimedia/Encoder Debug   : e0\n");
	ASSERT_CONTAINS(out, "Multimedia/Filters/Video/watermark Debug   : w0\n");
	ASSERT_NOT_CONTAINS(out, "Filters/Video/watermark Debug   : dropped");
	RETURN_TEST(0);
}

int test_threadedlog_inherited_throttle_state_is_per_leaf() {
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
	ASSERT_EQUAL(
		"Multimedia/Encoder Debug   : e0\n"
		"Multimedia/Filters/Video/watermark Debug   : w0\n",
		output.str());
	RETURN_TEST(0);
}

int test_threadedlog_throttle_drops_without_deadlock() {
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
	ASSERT_CONTAINS(output.str(), "Fatal   : fatal survives\n");
	RETURN_TEST(0);
}

// -------------------
// Wide
// -------------------

int test_threadedlog_filtered_wide_skips_conversion() {
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Info, "%L:");
	tlog << Level::Debug << std::wstring(1, static_cast<wchar_t>(0xD800)) << std::endl;
	ASSERT_EMPTY(output.str());
	tlog << Level::Info << "after filtered invalid input" << std::endl;
	ASSERT_EQUAL("Info    : after filtered invalid input\n", output.str());
	RETURN_TEST(0);
}

int test_threadedlog_invalid_wide_releases_line_lock() {
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Info, "%L:");
	tlog << Level::Info << std::wstring(1, static_cast<wchar_t>(0xD800)) << std::endl;
	tlog << Level::Info << "after invalid input" << std::endl;
	ASSERT_EQUAL("Info    : \xEF\xBF\xBD\nInfo    : after invalid input\n", output.str());
	RETURN_TEST(0);
}

int main() {
	int result = 0;

	// -------------------
	// Basic emit
	// -------------------
	result += test_smart_pointer_usage();
	result += test_shared_unwraps_without_dereference();
	result += test_threadedlog_basic();
	result += test_threadedlog_safe_function_crosses_dll_contains_logger_exception_and_continues();
	result += test_threadedlog_nested_instances_keep_distinct_locks();
	result += test_threadedlog_throwing_newline_manipulator_releases_lock();
	result += test_threadedlog_throttle_error_releases_open_line_lock();

	// -------------------
	// Binary span
	// -------------------
	result += test_threadedlog_span_default_is_base64();
	result += test_threadedlog_span_filtered();
	result += test_threadedlog_span_hex();
	result += test_threadedlog_span_vector_converts();
	result += test_threadedlog_every_accepted_payload();
	result += test_threadedlog_safe_containers_keep_one_logical_line();

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
	result += test_threadedlog_malformed_format_tokens_remain_literal();

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
