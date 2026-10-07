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

#include <StormByte/logger/exception.hxx>
#include <StormByte/logger/log.hxx>
#include <StormByte/logger/manipulators.hxx>
#include <StormByte/logger/threaded_log.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/safe/wstring.hxx>
#include <StormByte/test_handlers.h>

#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

using StormByte::Safe::String;
using StormByte::Safe::WString;
using namespace StormByte::Logger;

// -------------------
// Color
// -------------------

int test_manip_color_and_nocolor_log() {
	std::ostringstream output;
	Log log(output, Level::Info, "%c[%L]%g");
	log.Color(Level::Info, Color::Red);
	log << component("Core") << group("work") << Level::Info
		<< nocolor << "plain " << color(Color::Green) << "green" << std::endl;
	ASSERT_EQUAL("\033[31mCore[Info    ]work \033[0mplain \033[32mgreen\033[0m\n", output.str());
	log << reset_component;
	RETURN_TEST(0);
}

int test_manip_color_threadedlog() {
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%L:");
	log.Color(Level::Info, Color::Cyan);
	log << Level::Info << color << "cyan" << std::endl;
	ASSERT_EQUAL("\033[36mInfo    : cyan\033[0m\n", output.str());
	RETURN_TEST(0);
}

// -------------------
// Format
// -------------------

int test_manip_push_pop_and_component_format() {
	std::ostringstream output;
	Log log(output, Level::Info, "GENERAL[%L]");
	log.Format("Module", "COMPONENT[%L]");
	log << component("Module") << push_format("TEMP[%L]") << Level::Info << "temp" << std::endl;
	log << pop_format << Level::Info << "component" << std::endl;
	log << reset_component << Level::Info << "general" << std::endl;
	ASSERT_EQUAL("TEMP[Info    ] temp\nCOMPONENT[Info    ] component\nGENERAL[Info    ] general\n", output.str());
	RETURN_TEST(0);
}

int test_manip_push_pop_threadedlog() {
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "BASE[%L]");
	log << push_format("TEMP[%L]") << Level::Info << "temp" << std::endl;
	log << pop_format << Level::Info << "base" << std::endl;
	ASSERT_EQUAL("TEMP[Info    ] temp\nBASE[Info    ] base\n", output.str());
	RETURN_TEST(0);
}

// -------------------
// Group
// -------------------

int test_manip_group_component_reset() {
	std::ostringstream output;
	Log log(output, Level::Info, "%c[%L]%g");
	log << reset_component;
	log << component("Module") << group("line") << Level::Info << "first" << std::endl;
	log << Level::Info << "second" << std::endl;
	log << reset_component << Level::Info << "root" << std::endl;
	ASSERT_EQUAL("Module[Info    ]line first\nModule[Info    ] second\n[Info    ] root\n", output.str());
	RETURN_TEST(0);
}

int test_manip_group_component_threadedlog() {
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%c[%L]%g");
	log << component("Module") << group("line") << Level::Info << "first" << std::endl;
	ASSERT_EQUAL("Module[Info    ]line first\n", output.str());
	RETURN_TEST(0);
}

// -------------------
// Hex
// -------------------

int test_manip_hex_columns_wrap_without_header() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	log << Level::Info << hex(2) << "ABCD" << std::endl;
	ASSERT_EQUAL("Info    : 0x41 0x42\n0x43 0x44\n", output.str());
	RETURN_TEST(0);
}

int test_manip_hex_number_uses_text_bytes() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	log << Level::Info << hex << 42 << std::endl;
	ASSERT_EQUAL("Info    : 0x34 0x32\n", output.str());
	RETURN_TEST(0);
}

int test_manip_hex_string() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	log << Level::Info << hex << "AB" << std::endl;
	ASSERT_EQUAL("Info    : 0x41 0x42\n", output.str());
	RETURN_TEST(0);
}

int test_manip_hex_then_redact() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	log << Level::Info << hex << redact << "A" << std::endl;
	ASSERT_EQUAL("Info    : ****\n", output.str());
	RETURN_TEST(0);
}

int test_manip_hex_threadedlog() {
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%L:");
	log << Level::Info << hex(2) << "AB" << std::endl;
	ASSERT_EQUAL("Info    : 0x41 0x42\n", output.str());
	RETURN_TEST(0);
}

int test_manip_hex_zero_is_nohex() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	log << Level::Info << hex(0) << "AB" << std::endl;
	ASSERT_EQUAL("Info    : AB\n", output.str());
	RETURN_TEST(0);
}

int test_manip_nohex_restores_plain() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	log << Level::Info << hex << "A" << std::endl;
	log << Level::Info << nohex << "A" << std::endl;
	ASSERT_EQUAL("Info    : 0x41\nInfo    : A\n", output.str());
	RETURN_TEST(0);
}

// -------------------
// Human-readable
// -------------------

int test_manip_chainable_threadedlog() {
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Info, "%L:");
	tlog << Level::Info << humanreadable_number << humanreadable_bytes << 10240 << std::endl;
	ASSERT_EQUAL("Info    : 10 KiB\n", output.str());
	RETURN_TEST(0);
}

int test_manip_humanreadable_bytes_log() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	log << Level::Info << humanreadable_bytes << 10240 << std::endl;
	ASSERT_EQUAL("Info    : 10 KiB\n", output.str());
	RETURN_TEST(0);
}

int test_manip_humanreadable_number_log() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	log << Level::Info << humanreadable_number << 1000 << std::endl;
	ASSERT_EQUAL("Info    : 1,000\n", output.str());
	RETURN_TEST(0);
}

int test_manip_nohumanreadable_log() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	log << Level::Info << humanreadable_number << 1000 << std::endl;
	log << Level::Info << nohumanreadable << 1000 << std::endl;
	ASSERT_EQUAL("Info    : 1,000\nInfo    : 1000\n", output.str());
	RETURN_TEST(0);
}

// -------------------
// Redact
// -------------------

int test_manip_noredact_restores_plain() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	log << Level::Info << redact << "hidden" << std::endl;
	log << Level::Info << noredact << "visible" << std::endl;
	ASSERT_EQUAL("Info    : ******\nInfo    : visible\n", output.str());
	RETURN_TEST(0);
}

int test_manip_redact_affects_numbers() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	log << Level::Info << redact << 42 << " secret" << std::endl;
	ASSERT_EQUAL("Info    : *********\n", output.str());
	RETURN_TEST(0);
}

int test_manip_redact_const_char_ptr() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	const char* token = "password123";
	log << Level::Info << redact(3) << token << std::endl;
	ASSERT_EQUAL("Info    : ********123\n", output.str());
	RETURN_TEST(0);
}

int test_manip_redact_owned_string() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	log << Level::Info << redact << String{"secret"} << std::endl;
	ASSERT_EQUAL("Info    : ******\n", output.str());
	RETURN_TEST(0);
}

int test_manip_redact_empty_string() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	log << Level::Info << redact << "" << std::endl;
	ASSERT_EQUAL("Info    : \n", output.str());
	RETURN_TEST(0);
}

int test_manip_redact_first() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	log << Level::Info << redact_first(4) << "super-secret" << std::endl;
	ASSERT_EQUAL("Info    : supe********\n", output.str());
	RETURN_TEST(0);
}

int test_manip_redact_first_const_char_ptr() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	const char* token = "password123";
	log << Level::Info << redact_first(3) << token << std::endl;
	ASSERT_EQUAL("Info    : pas********\n", output.str());
	RETURN_TEST(0);
}

int test_manip_redact_first_ge_length() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	log << Level::Info << redact_first(10) << "abc" << std::endl;
	ASSERT_EQUAL("Info    : abc\n", output.str());
	RETURN_TEST(0);
}

int test_manip_redact_first_threadedlog() {
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Info, "%L:");
	tlog << Level::Info << redact_first(4) << "super-secret" << std::endl;
	tlog << Level::Info << noredact << "ok" << std::endl;
	ASSERT_EQUAL("Info    : supe********\nInfo    : ok\n", output.str());
	RETURN_TEST(0);
}

int test_manip_redact_first_zero_same_as_full() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	log << Level::Info << redact_first(0) << "abc" << std::endl;
	ASSERT_EQUAL("Info    : ***\n", output.str());
	RETURN_TEST(0);
}

int test_manip_redact_full_string() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	log << Level::Info << redact << "secret" << std::endl;
	ASSERT_EQUAL("Info    : ******\n", output.str());
	RETURN_TEST(0);
}

int test_manip_redact_keep_last() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	log << Level::Info << redact(4) << "super-secret" << std::endl;
	ASSERT_EQUAL("Info    : ********cret\n", output.str());
	RETURN_TEST(0);
}

int test_manip_redact_keep_last_ge_length() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	log << Level::Info << redact(10) << "abc" << std::endl;
	ASSERT_EQUAL("Info    : abc\n", output.str());
	RETURN_TEST(0);
}

int test_manip_redact_keep_last_zero_same_as_full() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	log << Level::Info << redact(0) << "abc" << std::endl;
	ASSERT_EQUAL("Info    : ***\n", output.str());
	RETURN_TEST(0);
}

int test_manip_redact_stays_active() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	log << Level::Info << redact(2) << "one" << " " << "two" << std::endl;
	log << Level::Info << "three" << std::endl;
	log << Level::Info << noredact << "four" << std::endl;
	ASSERT_EQUAL("Info    : *ne *wo\nInfo    : ***ee\nInfo    : four\n", output.str());
	RETURN_TEST(0);
}

int test_manip_redact_string() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	log << Level::Info << redact << String{"secret"} << std::endl;
	ASSERT_EQUAL("Info    : ******\n", output.str());
	RETURN_TEST(0);
}

int test_manip_redact_then_change_keep() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	log << Level::Info << redact << "abcdef" << std::endl;
	log << Level::Info << redact(2) << "abcdef" << std::endl;
	log << Level::Info << redact << "abcdef" << std::endl;
	ASSERT_EQUAL("Info    : ******\nInfo    : ****ef\nInfo    : ******\n", output.str());
	RETURN_TEST(0);
}

int test_manip_redact_threadedlog() {
	std::ostringstream output;
	ThreadedLog tlog(output, Level::Info, "%L:");
	tlog << Level::Info << redact(4) << "super-secret" << std::endl;
	tlog << Level::Info << noredact << "ok" << std::endl;
	ASSERT_EQUAL("Info    : ********cret\nInfo    : ok\n", output.str());
	RETURN_TEST(0);
}

int test_manip_redact_owned_wstring() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	log << Level::Info << redact << WString{L"secret"} << std::endl;
	ASSERT_EQUAL("Info    : ******\n", output.str());
	RETURN_TEST(0);
}

int test_manip_redact_with_humanreadable_independent() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	log << Level::Info << humanreadable_number << redact << 1000 << " token" << std::endl;
	log << Level::Info << noredact << nohumanreadable << 1000 << std::endl;
	ASSERT_EQUAL("Info    : ***********\nInfo    : 1000\n", output.str());
	RETURN_TEST(0);
}

int test_manip_redact_wstring() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	std::wstring wide = L"secret";
	log << Level::Info << redact << wide << std::endl;
	ASSERT_EQUAL("Info    : ******\n", output.str());
	RETURN_TEST(0);
}

int test_manip_redact_wstring_owned() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	log << Level::Info << redact << WString{L"secret"} << std::endl;
	ASSERT_EQUAL("Info    : ******\n", output.str());
	RETURN_TEST(0);
}

int test_owned_text_preserves_embedded_nulls() {
	const std::string_view narrow_view{"A\0B", 3};
	const std::wstring_view wide_view{L"C\0D", 3};
	const String narrow{narrow_view};
	const WString wide{wide_view};
	const std::string expected{"Info    : A\0B|C\0D\n", 18};

	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	log << Level::Info << narrow << "|" << wide << std::endl;
	ASSERT_EQUAL(expected, output.str());

	std::ostringstream threaded_output;
	ThreadedLog threaded_log(threaded_output, Level::Info, "%L:");
	threaded_log << Level::Info << narrow << "|" << wide << std::endl;
	ASSERT_EQUAL(expected, threaded_output.str());
	RETURN_TEST(0);
}

// -------------------
// Throttle
// -------------------

int test_manip_throttle_level_selector() {
	static_assert(StormByte::Type::SameAs<decltype(ThrottleSpec::Component), StormByte::Safe::Optional<String>>);
	static_assert(StormByte::Type::SameAs<decltype(ThrottleSpec::Group), StormByte::Safe::Optional<String>>);
	ThrottleSpec spec{.Component = std::nullopt, .Level = Level::Info, .Group = std::nullopt};
	spec.Component.emplace(String{"Core"});
	ASSERT_TRUE(spec.Level == Level::Info);
	ASSERT_TRUE(spec.Component.has_value());
	ASSERT_FALSE(spec.Group.has_value());
	RETURN_TEST(0);
}

int test_manip_throttle_invalid_configuration() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	ThrottleSpec spec;
	spec.Rate = 1.0;
	ASSERT_THROWS(log.Throttle(spec), StormByte::Logger::ThrottleError);
	ASSERT_EMPTY(output.str());
	RETURN_TEST(0);
}

int test_manip_throttle_policies_log() {
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	ThrottleSpec spec;
	spec.Policy = ThrottlePolicy::Window;
	spec.WindowKeep = 1;
	spec.WindowPeriod = 2;
	log.Throttle(spec);
	log << Level::Info << "one" << std::endl;
	log << Level::Info << "two" << std::endl;
	log << Level::Info << "three" << std::endl;
	ASSERT_EQUAL("Info    : one\nInfo    : dropped 1 messages\nInfo    : three\n", output.str());
	RETURN_TEST(0);
}

int test_manip_throttle_threadedlog() {
	std::ostringstream output;
	ThreadedLog log(output, Level::Info, "%L:");
	log.Throttle(0.0, 1);
	log << Level::Info << "one" << std::endl;
	log << Level::Info << "two" << std::endl;
	log << Level::Fatal << "fatal" << std::endl;
	ASSERT_EQUAL("Info    : one\nFatal   : fatal\n", output.str());
	RETURN_TEST(0);
}

int main() {
	int result = 0;

	// -------------------
	// Color
	// -------------------
	result += test_manip_color_and_nocolor_log();
	result += test_manip_color_threadedlog();

	// -------------------
	// Format
	// -------------------
	result += test_manip_push_pop_and_component_format();
	result += test_manip_push_pop_threadedlog();

	// -------------------
	// Group
	// -------------------
	result += test_manip_group_component_reset();
	result += test_manip_group_component_threadedlog();

	// -------------------
	// Hex
	// -------------------
	result += test_manip_hex_columns_wrap_without_header();
	result += test_manip_hex_number_uses_text_bytes();
	result += test_manip_hex_string();
	result += test_manip_hex_then_redact();
	result += test_manip_hex_threadedlog();
	result += test_manip_hex_zero_is_nohex();
	result += test_manip_nohex_restores_plain();

	// -------------------
	// Human-readable
	// -------------------
	result += test_manip_chainable_threadedlog();
	result += test_manip_humanreadable_bytes_log();
	result += test_manip_humanreadable_number_log();
	result += test_manip_nohumanreadable_log();

	// -------------------
	// Redact
	// -------------------
	result += test_manip_noredact_restores_plain();
	result += test_manip_redact_affects_numbers();
	result += test_manip_redact_const_char_ptr();
	result += test_manip_redact_owned_string();
	result += test_manip_redact_empty_string();
	result += test_manip_redact_first();
	result += test_manip_redact_first_const_char_ptr();
	result += test_manip_redact_first_ge_length();
	result += test_manip_redact_first_threadedlog();
	result += test_manip_redact_first_zero_same_as_full();
	result += test_manip_redact_full_string();
	result += test_manip_redact_keep_last();
	result += test_manip_redact_keep_last_ge_length();
	result += test_manip_redact_keep_last_zero_same_as_full();
	result += test_manip_redact_stays_active();
	result += test_manip_redact_string();
	result += test_manip_redact_then_change_keep();
	result += test_manip_redact_threadedlog();
	result += test_manip_redact_owned_wstring();
	result += test_manip_redact_with_humanreadable_independent();
	result += test_manip_redact_wstring();
	result += test_manip_redact_wstring_owned();
	result += test_owned_text_preserves_embedded_nulls();

	// -------------------
	// Throttle
	// -------------------
	result += test_manip_throttle_level_selector();
	result += test_manip_throttle_invalid_configuration();
	result += test_manip_throttle_policies_log();
	result += test_manip_throttle_threadedlog();

	if (result == 0)
		std::cout << "All tests passed!" << std::endl;
	else
		std::cout << result << " tests failed." << std::endl;
	return result;
}
