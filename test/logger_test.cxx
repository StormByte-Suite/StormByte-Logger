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
#include <StormByte/exception.hxx>
#include <StormByte/logger/exception.hxx>
#include <StormByte/logger/log.hxx>
#include <StormByte/size.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/safe/wcstring.hxx>
#include <StormByte/safe/wstring.hxx>
#include <StormByte/test_handlers.h>

#include <clocale>
#include <iostream>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using StormByte::BinaryData;
using StormByte::ByteSize;
using StormByte::Safe::CString;
using StormByte::Size;
using StormByte::Safe::WCString;
using StormByte::Safe::String;
using StormByte::Safe::WString;
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

int log_to_stdout() {
	int result = 0;
	Log log(std::cout, Level::Info, "%L:");
	IsolateLine(log);
	log << Level::Info << "Info message" << std::endl;
	log << Level::Debug << "Debug message" << std::endl;
	log << Level::Error << "Error message" << std::endl;
	RETURN_TEST("log_to_stdout", result);
}

int test_basic_logging() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Debug, "%L:");
	IsolateLine(log);
	log << Level::Info << "Info message" << std::endl;
	log << Level::Debug << "Debug message" << std::endl;
	log << Level::Error << "Error message" << std::endl;
	ASSERT_EQUAL("test_basic_logging", std::string("Info    : Info message\nDebug   : Debug message\nError   : Error message\n"), output.str());
	RETURN_TEST("test_basic_logging", result);
}

int test_log_data() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	int i = 42;
	bool b = true;
	double d = 3.141596;
	log << Level::Info << "Info message with sample integer " << i << ", a bool " << b << " and a double " << d << std::endl;
	ASSERT_EQUAL("test_log_data", std::string("Info    : Info message with sample integer 42, a bool true and a double 3.141596\n"), output.str());
	RETURN_TEST("test_log_data", result);
}

int test_log_with_std_endl() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Debug, "%L:");
	IsolateLine(log);
	log << Level::Info << "Info message" << std::endl;
	log << Level::Debug << "Debug message" << std::endl;
	log << Level::Error << "Error message" << std::endl;
	ASSERT_EQUAL("test_log_with_std_endl", std::string("Info    : Info message\nDebug   : Debug message\nError   : Error message\n"), output.str());
	RETURN_TEST("test_log_with_std_endl", result);
}

int test_smart_pointer_usage() {
	int result = 0;
	std::ostringstream output;
	auto log = std::make_shared<StormByte::Logger::Log>(output, Level::Info, "%L:");
	IsolateLine(*log);
	log << Level::Info << "Smart pointer log message" << std::endl;
	ASSERT_EQUAL("test_smart_pointer_usage", std::string("Info    : Smart pointer log message\n"), output.str());
	RETURN_TEST("test_smart_pointer_usage", result);
}

// -------------------
// Binary span
// -------------------

int test_span_default_is_base64() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	const std::vector<std::byte> raw{
		std::byte{'H'}, std::byte{'e'}, std::byte{'l'}, std::byte{'l'}, std::byte{'o'}
	};
	log << Level::Info << std::span<const std::byte>{raw} << std::endl;
	ASSERT_EQUAL("test_span_default_is_base64",
		std::string("Info    : ") + static_cast<std::string>(StormByte::Base64Encode(raw)) + "\n", output.str());
	RETURN_TEST("test_span_default_is_base64", result);
}

int test_span_empty() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	log << Level::Info << std::span<const std::byte>{} << std::endl;
	ASSERT_EQUAL("test_span_empty", "Info    : \n", output.str());
	RETURN_TEST("test_span_empty", result);
}

int test_span_filtered_produces_no_output() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	const std::vector<std::byte> raw{std::byte{0xFF}};
	log << Level::Debug << raw << std::endl;
	ASSERT_EQUAL("test_span_filtered_produces_no_output", std::string{}, output.str());
	RETURN_TEST("test_span_filtered_produces_no_output", result);
}

int test_span_hex_dumps_raw_bytes() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	const std::vector<std::byte> raw{std::byte{0x01}, std::byte{0xAB}};
	const BinaryData dumped(raw);
	log << Level::Info << hex << raw << std::endl;
	ASSERT_EQUAL("test_span_hex_dumps_raw_bytes",
		std::string("Info    : ") + static_cast<std::string>(dumped.HexDump(Size{16})) + "\n", output.str());
	RETURN_TEST("test_span_hex_dumps_raw_bytes", result);
}

int test_span_hex_then_nohex_restores_base64() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	const std::vector<std::byte> raw{std::byte{'A'}};
	log << Level::Info << hex << raw << std::endl;
	log << Level::Info << nohex << raw << std::endl;
	ASSERT_EQUAL("test_span_hex_then_nohex_restores_base64",
		std::string("Info    : ") + static_cast<std::string>(BinaryData(raw).HexDump(Size{16})) + "\nInfo    : " + static_cast<std::string>(StormByte::Base64Encode(raw)) + "\n", output.str());
	RETURN_TEST("test_span_hex_then_nohex_restores_base64", result);
}

int test_span_vector_converts_to_span() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	const std::vector<std::byte> raw{std::byte{0x01}, std::byte{0x02}};
	log << Level::Info << raw << std::endl;
	ASSERT_EQUAL("test_span_vector_converts_to_span",
		std::string("Info    : ") + static_cast<std::string>(StormByte::Base64Encode(raw)) + "\n", output.str());
	RETURN_TEST("test_span_vector_converts_to_span", result);
}

int test_binary_data_default_is_base64() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	const BinaryData raw{std::byte{'H'}, std::byte{'i'}};
	log << Level::Info << raw << std::endl;
	ASSERT_EQUAL("test_binary_data_default_is_base64",
		std::string("Info    : ") + static_cast<std::string>(StormByte::Base64Encode(raw)) + "\n", output.str());
	RETURN_TEST("test_binary_data_default_is_base64", result);
}

int test_binary_data_hex_uses_columns() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	const BinaryData raw{std::byte{0x01}, std::byte{0xAB}, std::byte{0x02}};
	log << Level::Info << hex(2) << raw << std::endl;
	ASSERT_EQUAL("test_binary_data_hex_uses_columns",
		std::string("Info    : ") + static_cast<std::string>(raw.HexDump(Size{2})) + "\n", output.str());
	RETURN_TEST("test_binary_data_hex_uses_columns", result);
}

// -------------------
// Color
// -------------------

int test_all_configured_colors_emit_expected_ansi() {
	int result = 0;
	const std::vector<std::pair<Color, std::string>> colors = {
		{Color::Default, ""},
		{Color::Black, "\033[30m"},
		{Color::Red, "\033[31m"},
		{Color::Green, "\033[32m"},
		{Color::Yellow, "\033[33m"},
		{Color::Blue, "\033[34m"},
		{Color::Magenta, "\033[35m"},
		{Color::Cyan, "\033[36m"},
		{Color::Gray, "\033[90m"},
		{Color::White, "\033[37m"},
		{Color::BrightBlack, "\033[90m"},
		{Color::BrightRed, "\033[91m"},
		{Color::BrightGreen, "\033[92m"},
		{Color::BrightYellow, "\033[93m"},
		{Color::BrightBlue, "\033[94m"},
		{Color::BrightMagenta, "\033[95m"},
		{Color::BrightCyan, "\033[96m"},
		{Color::BrightWhite, "\033[97m"}
	};
	for (const auto& [configured, ansi] : colors) {
		std::ostringstream output;
		Log log(output, Level::Info, "%L:");
		IsolateLine(log);
		log.Color(Level::Info, configured);
		ASSERT_EQUAL("test_all_configured_colors_emit_expected_ansi (getter)", configured, log.Color(Level::Info));
		log << Level::Info << "value" << std::endl;
		const std::string expected = ansi + "Info    : value" + (ansi.empty() ? "" : "\033[0m") + "\n";
		ASSERT_EQUAL("test_all_configured_colors_emit_expected_ansi", expected, output.str());
	}
	RETURN_TEST("test_all_configured_colors_emit_expected_ansi", result);
}

int test_color_and_temporary_format_interoperate() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "BASE[%L]");
	IsolateLine(log);
	log.Color(Level::Info, Color::Blue);
	log << push_format("TEMP[%L]") << Level::Info << nocolor << "plain" << color << " blue" << std::endl;
	log << pop_format << Level::Info << "base" << std::endl;
	const std::string expected =
		"\033[34mTEMP[Info    ] \033[0mplain\033[34m blue\033[0m\n"
		"\033[34mBASE[Info    ] base\033[0m\n";
	ASSERT_EQUAL("test_color_and_temporary_format_interoperate", expected, output.str());
	RETURN_TEST("test_color_and_temporary_format_interoperate", result);
}

int test_color_manipulators_and_line_reset() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::LowLevel, "%L:");
	log.Color(Level::Notice, Color::Yellow);
	log << Level::Notice << nocolor << "plain " << color(Color::Green) << "green " << color << "configured" << std::endl;
	log << Level::Notice << "next" << std::endl;
	const std::string expected =
		"\033[33mNotice  : \033[0mplain \033[32mgreen \033[0m\033[33mconfigured\033[0m\n"
		"\033[33mNotice  : next\033[0m\n";
	ASSERT_EQUAL("test_color_manipulators_and_line_reset", expected, output.str());
	RETURN_TEST("test_color_manipulators_and_line_reset", result);
}

int test_colored_logger_destructor_resets_stream() {
	int result = 0;
	std::ostringstream output;
	{
		Log log(output, Level::Info, "%L:");
		IsolateLine(log);
		log.Color(Level::Info, Color::Red);
		log << Level::Info << "unterminated";
	}
	ASSERT_EQUAL("test_colored_logger_destructor_resets_stream", "\033[31mInfo    : unterminated\033[0m", output.str());
	RETURN_TEST("test_colored_logger_destructor_resets_stream", result);
}

int test_default_color_emits_no_ansi() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	log << Level::Info << color << "plain" << nocolor << " text" << std::endl;
	ASSERT_EQUAL("test_default_color_emits_no_ansi", "Info    : plain text\n", output.str());
	RETURN_TEST("test_default_color_emits_no_ansi", result);
}

int test_filtered_color_has_no_side_effects() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	log.Color(Level::Debug, Color::Red);
	log << Level::Debug << color(Color::Green) << "hidden" << std::endl;
	log << Level::Info << "visible" << std::endl;
	ASSERT_EQUAL("test_filtered_color_has_no_side_effects", "Info    : visible\n", output.str());
	RETURN_TEST("test_filtered_color_has_no_side_effects", result);
}

// -------------------
// Components
// -------------------

int test_component_color_override_has_priority() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%c[%L]");
	log << reset_component;
	IsolateLine(log);
	log.Color(Level::Info, Color::Blue);
	log.Color("Multimedia", Level::Info, Color::Red);
	log << component("Multimedia") << Level::Info << "red" << std::endl;
	log << reset_component << Level::Info << "blue" << std::endl;
	ASSERT_EQUAL("test_component_color_override_has_priority",
		"\033[31mMultimedia[Info    ] red\033[0m\n\033[34m[Info    ] blue\033[0m\n", output.str());
	RETURN_TEST("test_component_color_override_has_priority", result);
}

int test_component_format_priority_and_fallback() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "GENERAL[%L]");
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
	ASSERT_EQUAL("test_component_format_priority_and_fallback", expected, output.str());
	RETURN_TEST("test_component_format_priority_and_fallback", result);
}

int test_component_header_is_sticky_and_resettable() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%c[%L]");
	log << reset_component;
	IsolateLine(log);
	log << component("Multimedia") << Level::Info << "first" << std::endl;
	log << Level::Info << "second" << std::endl;
	log << reset_component << Level::Info << "third" << std::endl;
	ASSERT_EQUAL("test_component_header_is_sticky_and_resettable",
		"Multimedia[Info    ] first\nMultimedia[Info    ] second\n[Info    ] third\n", output.str());
	RETURN_TEST("test_component_header_is_sticky_and_resettable", result);
}

int test_component_without_token_preserves_legacy_output() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	log << reset_component;
	IsolateLine(log);
	log << component("Hidden") << Level::Info << "message" << std::endl;
	ASSERT_EQUAL("test_component_without_token_preserves_legacy_output", "Info    : message\n", output.str());
	RETURN_TEST("test_component_without_token_preserves_legacy_output", result);
}

int test_empty_component_does_not_push() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%c[%L]");
	log << reset_component;
	IsolateLine(log);
	log << component("Multimedia") << Level::Info << "named" << std::endl;
	log << component("") << Level::Info << "still named" << std::endl;
	log << reset_component << Level::Info << "root" << std::endl;
	ASSERT_EQUAL("test_empty_component_does_not_push",
		"Multimedia[Info    ] named\nMultimedia[Info    ] still named\n[Info    ] root\n", output.str());
	RETURN_TEST("test_empty_component_does_not_push", result);
}

int test_format_change_redecides_throttle_line() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "A[%L]");
	log << reset_component;
	IsolateLine(log);
	log.Throttle(0.0, 1);
	log << Level::Info << "first";
	log.Format("B[%L]");
	log << Level::Info << "second" << std::endl;
	ASSERT_EQUAL("test_format_change_redecides_throttle_line", "A[Info    ] first\n", output.str());
	RETURN_TEST("test_format_change_redecides_throttle_line", result);
}

int test_push_format_overrides_component_and_restores_resolution() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "GENERAL[%L]");
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
	ASSERT_EQUAL("test_push_format_overrides_component_and_restores_resolution", expected, output.str());
	RETURN_TEST("test_push_format_overrides_component_and_restores_resolution", result);
}

// -------------------
// Floor
// -------------------

int test_enabled_is_floor_not_throttle() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	ASSERT_TRUE("test_enabled_is_floor_not_throttle (Info)", log.Enabled(Level::Info));
	ASSERT_TRUE("test_enabled_is_floor_not_throttle (Error)", log.Enabled(Level::Error));
	ASSERT_TRUE("test_enabled_is_floor_not_throttle (Warning below floor)", log.Enabled(Level::Warning));
	ASSERT_TRUE("test_enabled_is_floor_not_throttle (Fatal)", log.Enabled(Level::Fatal));
	ASSERT_FALSE("test_enabled_is_floor_not_throttle (Debug)", log.Enabled(Level::Debug));
	ASSERT_FALSE("test_enabled_is_floor_not_throttle (LowLevel)", log.Enabled(Level::LowLevel));
	ASSERT_FALSE("test_enabled_is_floor_not_throttle (Notice below Info)", log.Enabled(Level::Notice));
	ASSERT_EQUAL("test_enabled_is_floor_not_throttle (no I/O)", std::string(""), output.str());
	RETURN_TEST("test_enabled_is_floor_not_throttle", result);
}

int test_filtered_produces_empty_output() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Error, "%L:");
	IsolateLine(log);
	for (int i = 0; i < 100; ++i) {
		log << Level::Debug << "debug " << i << " " << true << " " << 3.14 << std::endl;
		log << Level::Info << "info " << i << std::endl;
	}
	ASSERT_EQUAL("test_filtered_produces_empty_output", std::string(""), output.str());
	RETURN_TEST("test_filtered_produces_empty_output", result);
}

int test_filtered_then_enabled_message() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	log << Level::Debug << "should not appear " << 123 << std::endl;
	log << Level::Info << "visible" << std::endl;
	log << Level::Debug << "still hidden" << std::endl;
	log << Level::Error << "error visible" << std::endl;
	ASSERT_EQUAL("test_filtered_then_enabled_message", std::string("Info    : visible\nError   : error visible\n"), output.str());
	RETURN_TEST("test_filtered_then_enabled_message", result);
}

int test_log_critical_levels_are_never_filtered() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Fatal, "%L:");
	IsolateLine(log);
	log << Level::LowLevel << "hidden low level" << std::endl;
	log << Level::Debug << "hidden debug" << std::endl;
	log << Level::Warning << "visible warning" << std::endl;
	log << Level::Notice << "hidden notice" << std::endl;
	log << Level::Info << "hidden info" << std::endl;
	log << Level::Error << "visible error" << std::endl;
	log << Level::Fatal << "visible fatal" << std::endl;
	const std::string expected =
		"Warning : visible warning\n"
		"Error   : visible error\n"
		"Fatal   : visible fatal\n";
	ASSERT_EQUAL("test_log_critical_levels_are_never_filtered", expected, output.str());
	RETURN_TEST("test_log_critical_levels_are_never_filtered", result);
}

int test_log_level_filtering() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Error, "%L:");
	IsolateLine(log);
	log << Level::Info << "Info message" << std::endl;
	log << Level::Warning << "Warning message" << std::endl;
	log << Level::Error << "Error message" << std::endl;
	ASSERT_EQUAL("test_log_level_filtering", std::string("Warning : Warning message\nError   : Error message\n"), output.str());
	RETURN_TEST("test_log_level_filtering", result);
}

// -------------------
// Format stack
// -------------------

int test_push_format_empty_and_partial_line_reset() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "BASE[%L]");
	IsolateLine(log);
	log << Level::Info << "before" << push_format("NEXT[%L]") << "after" << std::endl;
	log << push_format("") << Level::Info << "empty" << std::endl;
	log << pop_format << Level::Info << "next" << std::endl;
	const std::string expected =
		"BASE[Info    ] before\n"
		"NEXT[Info    ] after\n"
		" empty\n"
		"NEXT[Info    ] next\n";
	ASSERT_EQUAL("test_push_format_empty_and_partial_line_reset", expected, output.str());
	RETURN_TEST("test_push_format_empty_and_partial_line_reset", result);
}

int test_push_pop_format_stack() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "BASE[%L]");
	IsolateLine(log);
	log << Level::Info << "base" << std::endl;
	log << push_format("TEMP[%L]") << Level::Info << "temp" << std::endl;
	log << push_format("INNER[%L]") << Level::Info << "inner" << std::endl;
	log << pop_format << Level::Info << "temp again" << std::endl;
	log << pop_format << Level::Info << "base again" << std::endl;
	log << pop_format << Level::Info << "still base" << std::endl;
	const std::string expected =
		"BASE[Info    ] base\n"
		"TEMP[Info    ] temp\n"
		"INNER[Info    ] inner\n"
		"TEMP[Info    ] temp again\n"
		"BASE[Info    ] base again\n"
		"BASE[Info    ] still base\n";
	ASSERT_EQUAL("test_push_pop_format_stack", expected, output.str());
	RETURN_TEST("test_push_pop_format_stack", result);
}

// -------------------
// Groups
// -------------------

int test_filtered_group_has_no_side_effects() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:%g");
	IsolateLine(log);
	log << group("Hidden") << Level::Debug << "hidden" << std::endl;
	log << Level::Info << "visible" << std::endl;
	ASSERT_EQUAL("test_filtered_group_has_no_side_effects", "Info    : visible\n", output.str());
	RETURN_TEST("test_filtered_group_has_no_side_effects", result);
}

int test_group_and_color_share_the_header() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:%g");
	IsolateLine(log);
	log.Color(Level::Info, Color::Yellow);
	log << group("Decoder") << Level::Info << "open" << std::endl;
	ASSERT_EQUAL("test_group_and_color_share_the_header", "\033[33mInfo    :Decoder open\033[0m\n", output.str());
	RETURN_TEST("test_group_and_color_share_the_header", result);
}

int test_group_change_closes_partial_line() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:%g");
	IsolateLine(log);
	log << group("Decoder") << Level::Info << "before";
	log << group("Encoder") << "after" << std::endl;
	ASSERT_EQUAL("test_group_change_closes_partial_line", "Info    :Decoder before\nInfo    :Encoder after\n", output.str());
	RETURN_TEST("test_group_change_closes_partial_line", result);
}

int test_group_header_and_line_reset() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:%g");
	IsolateLine(log);
	log << group("Decoder") << Level::Info << "open" << std::endl;
	log << Level::Info << "plain" << std::endl;
	ASSERT_EQUAL("test_group_header_and_line_reset", "Info    :Decoder open\nInfo    : plain\n", output.str());
	RETURN_TEST("test_group_header_and_line_reset", result);
}

int test_group_without_token_and_empty_group() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	log << group("Decoder") << Level::Info << "open" << std::endl;
	log << group("") << Level::Info << "plain" << std::endl;
	ASSERT_EQUAL("test_group_without_token_and_empty_group", "Info    : open\nInfo    : plain\n", output.str());
	RETURN_TEST("test_group_without_token_and_empty_group", result);
}

// -------------------
// Human-readable
// -------------------

int test_escaped_percent_in_format() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "[%%] [%L]:");
	IsolateLine(log);
	log << Level::Info << "ok" << std::endl;
	std::string out = output.str();
	if (out.find("%L") != std::string::npos) {
		ASSERT_EQUAL("test_escaped_percent_in_format (leftover %L)", std::string("none"), std::string("%L"));
		RETURN_TEST("test_escaped_percent_in_format", 1);
	}
	if (out.find("[%]") == std::string::npos && out.find("[% ]") == std::string::npos) {
		if (out.find("[%") == std::string::npos) {
			ASSERT_EQUAL("test_escaped_percent_in_format (missing literal %)", std::string("found"), out);
			RETURN_TEST("test_escaped_percent_in_format", 1);
		}
	}
	if (out.find("ok") == std::string::npos) {
		ASSERT_EQUAL("test_escaped_percent_in_format (missing message)", std::string("ok"), out);
		RETURN_TEST("test_escaped_percent_in_format", 1);
	}
	if (out.find('%') == std::string::npos) {
		ASSERT_EQUAL("test_escaped_percent_in_format (no percent char)", std::string("has %"), out);
		RETURN_TEST("test_escaped_percent_in_format", 1);
	}
	RETURN_TEST("test_escaped_percent_in_format", result);
}

int test_humanreadable_bytes() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	log << Level::Info << humanreadable_bytes << 10240 << std::endl;
	ASSERT_EQUAL("test_humanreadable_bytes", std::string("Info    : 10 KiB\n"), output.str());
	RETURN_TEST("test_humanreadable_bytes", result);
}

int test_humanreadable_enable_and_disable() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	log << Level::Info << humanreadable_number << 1000 << std::endl;
	ASSERT_EQUAL("test_humanreadable_enable_and_disable (enable)", std::string("Info    : 1,000\n"), output.str());
	output.str("");
	output.clear();
	log << Level::Info << nohumanreadable << 1000 << std::endl;
	ASSERT_EQUAL("test_humanreadable_enable_and_disable (disable)", std::string("Info    : 1000\n"), output.str());
	RETURN_TEST("test_humanreadable_enable_and_disable", result);
}

int test_humanreadable_number() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	log << Level::Info << humanreadable_number << 1000 << std::endl;
	ASSERT_EQUAL("test_humanreadable_number", std::string("Info    : 1,000\n"), output.str());
	RETURN_TEST("test_humanreadable_number", result);
}

int test_nohumanreadable() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	log << Level::Info << humanreadable_number << 1000 << " " << nohumanreadable << 1000 << std::endl;
	ASSERT_EQUAL("test_nohumanreadable", std::string("Info    : 1,000 1000\n"), output.str());
	RETURN_TEST("test_nohumanreadable", result);
}

// -------------------
// Payload
// -------------------

int test_cstring_payload() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	log << Level::Info << CString{"cstring"} << std::endl;
	ASSERT_EQUAL("test_cstring_payload", "Info    : cstring\n", output.str());
	RETURN_TEST("test_cstring_payload", result);
}

int test_filtered_owned_text_is_dropped() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	log << Level::Debug << String{"hidden"} << std::endl;
	log << Level::Debug << CString{"hidden"} << std::endl;
	log << Level::Debug << WString{L"hidden"} << std::endl;
	log << Level::Debug << WCString{L"hidden"} << std::endl;
	log << Level::Debug << Size{1024} << std::endl;
	ASSERT_EQUAL("test_filtered_owned_text_is_dropped", std::string{}, output.str());
	RETURN_TEST("test_filtered_owned_text_is_dropped", result);
}

int test_size_payload() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	const Size bytes{1024};
	log << Level::Info << bytes << std::endl;
	ASSERT_EQUAL("test_size_payload",
		std::string("Info    : ") + static_cast<std::string>(bytes) + "\n", output.str());
	RETURN_TEST("test_size_payload", result);
}

int test_bytesize_payload() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	const ByteSize bytes{1024};
	log << Level::Info << bytes << std::endl;
	ASSERT_EQUAL("test_bytesize_payload",
		std::string("Info    : ") + static_cast<std::string>(bytes) + "\n", output.str());
	RETURN_TEST("test_bytesize_payload", result);
}

int test_string_payload() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	log << Level::Info << String{"owned"} << std::endl;
	ASSERT_EQUAL("test_string_payload", "Info    : owned\n", output.str());
	RETURN_TEST("test_string_payload", result);
}

int test_string_view_and_wstring_view_payloads() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	const std::string owned = "owned";
	const std::string_view sv = owned;
	const std::wstring wowned = L"wide";
	const std::wstring_view wv = wowned;
	log << Level::Info << sv << " " << wv << std::endl;
	log << Level::Debug << sv << wv << std::endl;
	ASSERT_EQUAL("test_string_view_and_wstring_view_payloads", "Info    : owned wide\n", output.str());
	RETURN_TEST("test_string_view_and_wstring_view_payloads", result);
}

int test_wcstring_payload() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	log << Level::Info << WCString{L"wide-c"} << std::endl;
	ASSERT_EQUAL("test_wcstring_payload", "Info    : wide-c\n", output.str());
	RETURN_TEST("test_wcstring_payload", result);
}

int test_wstring_payload() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	log << Level::Info << WString{L"owned-wide"} << std::endl;
	ASSERT_EQUAL("test_wstring_payload", "Info    : owned-wide\n", output.str());
	RETURN_TEST("test_wstring_payload", result);
}

int test_every_accepted_payload() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	const char ch = 'A';
	const signed char sch = -2;
	const unsigned char uch = 7;
	const short sh = -3;
	const unsigned short ush = 4;
	const unsigned int ui = 5;
	const long lg = -6;
	const unsigned long ul = 8;
	const long long ll = -9;
	const unsigned long long ull = 10;
	const float fl = 1.5f;
	const long double ld = 2.5L;
	char mutable_text[] = "buf";
	const std::string narrow_text{"std"};
	const char* literal_text = "literal";
	const char* null_narrow = nullptr;
	const wchar_t* null_wide = nullptr;
	log << Level::Info
		<< false << " "
		<< ch << " " << sch << " " << uch << " "
		<< sh << " " << ush << " " << ui << " "
		<< lg << " " << ul << " " << ll << " " << ull << " "
		<< fl << " " << ld << " "
		<< mutable_text << " " << narrow_text << " " << literal_text << " "
		<< null_narrow << null_wide
		<< std::endl;
	const std::string body =
		std::string("false ")
		+ std::to_string(ch) + ' '
		+ std::to_string(sch) + ' '
		+ std::to_string(uch) + ' '
		+ std::to_string(sh) + ' '
		+ std::to_string(ush) + ' '
		+ std::to_string(ui) + ' '
		+ std::to_string(lg) + ' '
		+ std::to_string(ul) + ' '
		+ std::to_string(ll) + ' '
		+ std::to_string(ull) + ' '
		+ std::to_string(fl) + ' '
		+ std::to_string(ld)
		+ " buf std literal ";
	ASSERT_EQUAL("test_every_accepted_payload", std::string("Info    : ") + body + "\n", output.str());
	RETURN_TEST("test_every_accepted_payload", result);
}

int test_pointer_owners_stream() {
	int result = 0;
	std::ostringstream output;
	StormByte::Safe::Shared<Log> empty_shared;
	StormByte::Safe::Unique<Log> empty_unique;
	std::shared_ptr<Log> empty_std_shared;
	std::unique_ptr<Log> empty_std_unique;
	empty_shared << Level::Info << "skip" << std::endl;
	empty_unique << Level::Info << "skip" << std::endl;
	empty_std_shared << Level::Info << "skip" << std::endl;
	empty_std_unique << Level::Info << "skip" << std::endl;

	auto shared = StormByte::Safe::Shared<Log>::MakePointer<Log>(output, Level::Info, "%L:");
	IsolateLine(*shared);
	shared << Level::Info << "shared" << std::endl;
	auto unique = StormByte::Safe::Unique<Log>::MakePointer<Log>(output, Level::Info, "%L:");
	IsolateLine(*unique);
	unique << Level::Info << "unique" << std::endl;
	ASSERT_EQUAL("test_pointer_owners_stream", "Info    : shared\nInfo    : unique\n", output.str());
	RETURN_TEST("test_pointer_owners_stream", result);
}

int test_logger_exception_path() {
	int result = 0;
	const Exception formatted("failed {}", 3);
	const ThrottleError plain("nope");
	ASSERT_EQUAL("test_logger_exception_path", std::string("StormByte.Logger: failed 3"), std::string(formatted.what()));
	ASSERT_EQUAL("test_logger_exception_path (leaf)", std::string("StormByte.Logger: nope"), std::string(plain.what()));
	RETURN_TEST("test_logger_exception_path", result);
}

// -------------------
// Scope
// -------------------

int test_component_stack_push_pop_and_join() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%c %L:");
	log << reset_component;
	IsolateLine(log);
	log << component("Multimedia") << component("Decoder") << Level::Info << "nested" << std::endl;
	log << pop_component << Level::Info << "parent" << std::endl;
	log << reset_component << Level::Info << "root" << std::endl;
	ASSERT_EQUAL("test_component_stack_push_pop_and_join",
		"Multimedia/Decoder Info    : nested\nMultimedia Info    : parent\n Info    : root\n",
		output.str());
	RETURN_TEST("test_component_stack_push_pop_and_join", result);
}

int test_scope_does_not_use_tls_stack() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%c %L:");
	log << reset_component;
	IsolateLine(log);
	log << component("TLS");
	auto scoped = log.Scope("Multimedia");
	scoped << Level::Info << "scoped" << std::endl;
	log << Level::Info << "stack" << std::endl;
	ASSERT_EQUAL("test_scope_does_not_use_tls_stack",
		"Multimedia Info    : scoped\nTLS Info    : stack\n", output.str());
	log << reset_component;
	RETURN_TEST("test_scope_does_not_use_tls_stack", result);
}

int test_scope_format_inherits_parent_and_leaf_wins() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "ROOT %L:");
	log << reset_component;
	IsolateLine(log);
	log.Format("Multimedia", "MM %c %L:");
	auto dec = log.Scope("Multimedia/Decoder");
	dec << Level::Info << "inherited" << std::endl;
	dec->Format("DEC %c %L:");
	dec << Level::Info << "leaf" << std::endl;
	log << Level::Info << "root" << std::endl;
	ASSERT_EQUAL("test_scope_format_inherits_parent_and_leaf_wins",
		"MM Multimedia/Decoder Info    : inherited\nDEC Multimedia/Decoder Info    : leaf\nROOT Info    : root\n",
		output.str());
	RETURN_TEST("test_scope_format_inherits_parent_and_leaf_wins", result);
}

int test_scope_path_and_nested_scope() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%c %L:");
	log << reset_component;
	IsolateLine(log);
	auto mm = log.Scope("Multimedia");
	auto dec = mm->Scope("Decoder");
	auto abs = log.Scope("Multimedia/Encoder");
	log << Level::Info << "root" << std::endl;
	mm << Level::Info << "mm" << std::endl;
	dec << Level::Info << "dec" << std::endl;
	abs << Level::Info << "enc" << std::endl;
	ASSERT_EQUAL("test_scope_path_and_nested_scope",
		" Info    : root\nMultimedia Info    : mm\nMultimedia/Decoder Info    : dec\nMultimedia/Encoder Info    : enc\n",
		output.str());
	RETURN_TEST("test_scope_path_and_nested_scope", result);
}

int test_scope_throttle_binds_to_leaf() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%c %L:");
	log << reset_component;
	IsolateLine(log);
	auto dec = log.Scope("Multimedia/Decoder");
	dec->Throttle(0.0, 1);
	dec << Level::Info << "one" << std::endl;
	dec << Level::Info << "two" << std::endl;
	log << Level::Info << "root still free" << std::endl;
	ASSERT_EQUAL("test_scope_throttle_binds_to_leaf",
		"Multimedia/Decoder Info    : one\n Info    : root still free\n", output.str());
	RETURN_TEST("test_scope_throttle_binds_to_leaf", result);
}

// -------------------
// Throttle
// -------------------

int test_flush_throttle_emits_orphaned_summary() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	ThrottleSpec spec;
	spec.Policy = ThrottlePolicy::Window;
	spec.WindowKeep = 1;
	spec.WindowPeriod = 2;
	log.Throttle(spec);
	log << Level::Info << "first" << std::endl;
	log << Level::Info << "dropped" << std::endl;
	log.FlushThrottle();
	ASSERT_EQUAL("test_flush_throttle_emits_orphaned_summary",
		"Info    : first\nNotice  : dropped 1 messages\n", output.str());
	RETURN_TEST("test_flush_throttle_emits_orphaned_summary", result);
}

int test_flush_throttle_selects_component_only() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%c[%L]:");
	log << reset_component;
	IsolateLine(log);
	ThrottleSpec a;
	a.Component = String{"A"};
	a.Burst = 1;
	ThrottleSpec b = a;
	b.Component = String{"B"};
	log.Throttle(a);
	log.Throttle(b);
	log << reset_component << component("A") << Level::Info << "a0" << std::endl;
	log << Level::Info << "a1" << std::endl;
	log << reset_component << component("B") << Level::Info << "b0" << std::endl;
	log << Level::Info << "b1" << std::endl;
	ThrottleSpec filter;
	filter.Component = String{"A"};
	log.FlushThrottle(filter);
	ASSERT_TRUE("test_flush_throttle_selects_component_only (A)", output.str().find("A[Notice  ]: dropped 1 messages\n") != std::string::npos);
	ASSERT_TRUE("test_flush_throttle_selects_component_only (B absent)", output.str().find("B[Info    ]: dropped") == std::string::npos);
	RETURN_TEST("test_flush_throttle_selects_component_only", result);
}

int test_inherited_drop_summary_stays_on_the_leaf() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Debug, "%c %L:");
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
	ASSERT_TRUE("test_inherited_drop_summary_stays_on_the_leaf (encoder kept)",
		out.find("Multimedia/Encoder Debug   : e0\n") != std::string::npos);
	ASSERT_TRUE("test_inherited_drop_summary_stays_on_the_leaf (watermark kept)",
		out.find("Multimedia/Filters/Video/watermark Debug   : w0\n") != std::string::npos);
	ASSERT_TRUE("test_inherited_drop_summary_stays_on_the_leaf (no watermark dropped)",
		out.find("Filters/Video/watermark Debug   : dropped") == std::string::npos);
	RETURN_TEST("test_inherited_drop_summary_stays_on_the_leaf", result);
}

int test_inherited_throttle_state_is_per_leaf() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Debug, "%c %L:");
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
	ASSERT_EQUAL("test_inherited_throttle_state_is_per_leaf",
		"Multimedia/Encoder Debug   : e0\n"
		"Multimedia/Filters/Video/watermark Debug   : w0\n",
		output.str());
	RETURN_TEST("test_inherited_throttle_state_is_per_leaf", result);
}

int test_throttle_component_and_group_isolation() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%c[%L]%g ");
	log << reset_component;
	IsolateLine(log);
	ThrottleSpec component_rule;
	component_rule.Component = String{"A"};
	component_rule.Burst = 1;
	log.Throttle(component_rule);
	log << reset_component << component("A") << Level::Info << "a0" << std::endl;
	log << Level::Info << "a1" << std::endl;
	log << reset_component << component("B") << Level::Info << "b0" << std::endl;
	log << Level::Info << "b1" << std::endl;
	ThrottleSpec group_rule;
	group_rule.Group = String{"x"};
	group_rule.Burst = 1;
	log.Throttle(group_rule);
	log << group("x") << Level::Info << "x0" << std::endl;
	log << group("x") << Level::Info << "x1" << std::endl;
	log << group("y") << Level::Info << "y0" << std::endl;
	ASSERT_EQUAL("test_throttle_component_and_group_isolation", std::string("A[Info    ]  a0\nB[Info    ]  b0\nB[Info    ]  b1\nB[Info    ]x  x0\nB[Info    ]y  y0\n"), output.str());
	RETURN_TEST("test_throttle_component_and_group_isolation", result);
}

int test_throttle_drop_burst() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	log.Throttle(0.0, 2);
	for (int index = 0; index < 5; ++index)
		log << Level::Info << index << std::endl;
	ASSERT_EQUAL("test_throttle_drop_burst", "Info    : 0\nInfo    : 1\n", output.str());
	RETURN_TEST("test_throttle_drop_burst", result);
}

int test_throttle_empty_lines_are_counted() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	ThrottleSpec spec;
	spec.Policy = ThrottlePolicy::Window;
	spec.WindowKeep = 1;
	spec.WindowPeriod = 2;
	log.Throttle(spec);
	log << Level::Info << std::endl;
	log << Level::Info << std::endl;
	log << Level::Info << std::endl;
	ASSERT_EQUAL("test_throttle_empty_lines_are_counted",
		"\nInfo    : dropped 1 messages\n\n", output.str());
	RETURN_TEST("test_throttle_empty_lines_are_counted", result);
}

int test_throttle_level_change_redecides_line() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	ThrottleSpec spec;
	spec.Level = Level::Info;
	spec.Burst = 1;
	log.Throttle(spec);
	log << Level::Info << "first";
	log << Level::Warning << "second" << std::endl;
	ASSERT_EQUAL("test_throttle_level_change_redecides_line", "Info    : first\nWarning : second\n", output.str());
	RETURN_TEST("test_throttle_level_change_redecides_line", result);
}

int test_throttle_off_preserves_output() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	log << Level::Info << "one" << std::endl;
	log << Level::Warning << "two" << std::endl;
	ASSERT_EQUAL("test_throttle_off_preserves_output", "Info    : one\nWarning : two\n", output.str());
	RETURN_TEST("test_throttle_off_preserves_output", result);
}

int test_throttle_precedence_and_no_throttle() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%c[%L]%g:");
	log << reset_component;
	IsolateLine(log);
	ThrottleSpec global;
	global.Burst = 10;
	log.Throttle(global);
	ThrottleSpec component_rule = global;
	component_rule.Component = String{"A"};
	component_rule.Burst = 2;
	log.Throttle(component_rule);
	ThrottleSpec other_rule = global;
	other_rule.Component = String{"B"};
	other_rule.Burst = 1;
	log.Throttle(other_rule);
	log << reset_component << component("A") << group("x") << Level::Info << "a0" << std::endl;
	log << Level::Info << "a1" << std::endl;
	log << Level::Info << "a2" << std::endl;
	log << reset_component << component("B") << group("x") << Level::Info << "b0" << std::endl;
	log << Level::Info << "b1" << std::endl;
	log.NoThrottle(component("B"));
	log << Level::Info << "b2" << std::endl;
	const std::string out = output.str();
	ASSERT_TRUE("test_throttle_precedence_and_no_throttle (component)", out.find("A[Info    ]x: a0") != std::string::npos);
	ASSERT_TRUE("test_throttle_precedence_and_no_throttle (root)", out.find("B[Info    ]x: b0") != std::string::npos);
	ASSERT_EQUAL("test_throttle_precedence_and_no_throttle (reset)", std::string("B[Info    ]: b2\n"), out.substr(out.rfind("B[Info    ]:")));
	RETURN_TEST("test_throttle_precedence_and_no_throttle", result);
}

int test_throttle_rejects_invalid_specs() {
	int result = 0;
	Log log(std::cout, Level::Info);
	IsolateLine(log);
	ThrottleSpec spec;
	spec.Rate = -1.0;
	bool threw = false;
	try { log.Throttle(spec); } catch (const StormByte::Logger::ThrottleError& ex) {
		threw = true;
		ASSERT_TRUE("test_throttle_rejects_invalid_specs (component)", std::string(ex.what()).find("StormByte.Logger:") == 0);
	}
	ASSERT_TRUE("test_throttle_rejects_invalid_specs (negative rate)", threw);
	spec = {};
	spec.Rate = 1.0;
	try { log.Throttle(spec); } catch (const StormByte::Logger::ThrottleError& ex) {
		threw = true;
		ASSERT_TRUE("test_throttle_rejects_invalid_specs (component)", std::string(ex.what()).find("StormByte.Logger:") == 0);
	}
	ASSERT_TRUE("test_throttle_rejects_invalid_specs (zero burst)", threw);
	spec = {};
	spec.Rate = 100.0;
	spec.Burst = 1;
	threw = false;
	try { log.Throttle(spec); } catch (const StormByte::Exception&) { threw = true; }
	ASSERT_TRUE("test_throttle_rejects_invalid_specs (valid rate)", !threw);
	spec = {};
	spec.Policy = ThrottlePolicy::Sample;
	spec.SampleN = 1;
	threw = false;
	try { log.Throttle(spec); } catch (const StormByte::Exception&) { threw = true; }
	ASSERT_TRUE("test_throttle_rejects_invalid_specs (sample)", threw);
	spec = {};
	spec.Policy = ThrottlePolicy::Window;
	spec.WindowKeep = 2;
	spec.WindowPeriod = 1;
	threw = false;
	try { log.Throttle(spec); } catch (const StormByte::Exception&) { threw = true; }
	ASSERT_TRUE("test_throttle_rejects_invalid_specs (window)", threw);
	RETURN_TEST("test_throttle_rejects_invalid_specs", result);
}

int test_throttle_sample_and_window() {
	int result = 0;
	std::ostringstream sample_output;
	Log sample(sample_output, Level::Info, "%L:");
	IsolateLine(sample);
	ThrottleSpec sample_spec;
	sample_spec.Policy = ThrottlePolicy::Sample;
	sample_spec.SampleN = 10;
	sample.Throttle(sample_spec);
	for (int index = 0; index < 20; ++index)
		sample << Level::Info << index << std::endl;
	ASSERT_EQUAL("test_throttle_sample_and_window (sample)",
		"Info    : 0\nInfo    : dropped 9 messages\nInfo    : 10\n", sample_output.str());
	std::ostringstream window_output;
	Log window(window_output, Level::Info, "%L:");
	IsolateLine(window);
	ThrottleSpec window_spec;
	window_spec.Policy = ThrottlePolicy::Window;
	window_spec.WindowKeep = 2;
	window_spec.WindowPeriod = 5;
	window.Throttle(window_spec);
	for (int index = 0; index < 6; ++index)
		window << Level::Info << index << std::endl;
	ASSERT_EQUAL("test_throttle_sample_and_window (window)",
		"Info    : 0\nInfo    : 1\nInfo    : dropped 3 messages\nInfo    : 5\n", window_output.str());
	RETURN_TEST("test_throttle_sample_and_window", result);
}

int test_throttle_summary_preserves_context() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%c[%L]%g ");
	log << reset_component;
	IsolateLine(log);
	ThrottleSpec spec;
	spec.Component = String{"A"};
	spec.Level = Level::Info;
	spec.Group = String{"g"};
	spec.Policy = ThrottlePolicy::Window;
	spec.WindowKeep = 1;
	spec.WindowPeriod = 2;
	log.Throttle(spec);
	log << component("A") << group("g") << Level::Info << "first" << std::endl;
	log << group("g") << Level::Info << "dropped" << std::endl;
	log << group("g") << Level::Info << "third" << std::endl;
	ASSERT_EQUAL("test_throttle_summary_preserves_context",
		"A[Info    ]g  first\nA[Info    ]g  dropped 1 messages\nA[Info    ]g  third\n", output.str());
	RETURN_TEST("test_throttle_summary_preserves_context", result);
}

int test_throttle_warning_but_not_error_or_fatal() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Fatal, "%L:");
	IsolateLine(log);
	ThrottleSpec spec;
	spec.Burst = 1;
	log.Throttle(spec);
	log << Level::Warning << "warning 1" << std::endl;
	log << Level::Warning << "warning 2" << std::endl;
	log << Level::Error << "error" << std::endl;
	log << Level::Fatal << "fatal" << std::endl;
	ASSERT_EQUAL("test_throttle_warning_but_not_error_or_fatal",
		"Warning : warning 1\nError   : error\nFatal   : fatal\n", output.str());
	RETURN_TEST("test_throttle_warning_but_not_error_or_fatal", result);
}

// -------------------
// Wide
// -------------------

int test_invalid_wide_string_is_replaced() {
	int result = 0;
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	log << Level::Info << std::wstring(1, static_cast<wchar_t>(0xD800)) << std::endl;
	log << Level::Info << "after invalid input" << std::endl;
	ASSERT_EQUAL("test_invalid_wide_string_is_replaced",
		"Info    : \xEF\xBF\xBD\nInfo    : after invalid input\n", output.str());
	RETURN_TEST("test_invalid_wide_string_is_replaced", result);
}

int test_wide_string_logging_is_locale_independent() {
	int result = 0;
	const char* previous = std::setlocale(LC_ALL, nullptr);
	std::string previous_locale = previous ? previous : "C";
	std::setlocale(LC_ALL, "C");
	std::ostringstream output;
	Log log(output, Level::Info, "%L:");
	IsolateLine(log);
	log << Level::Info << L"café" << std::endl;
	std::setlocale(LC_ALL, previous_locale.c_str());
	ASSERT_EQUAL("test_wide_string_logging_is_locale_independent", std::string("Info    : café\n"), output.str());
	RETURN_TEST("test_wide_string_logging_is_locale_independent", result);
}

int main() {
	int result = 0;

	// -------------------
	// Basic emit
	// -------------------
	result += log_to_stdout();
	result += test_basic_logging();
	result += test_log_data();
	result += test_log_with_std_endl();
	result += test_smart_pointer_usage();

	// -------------------
	// Binary span
	// -------------------
	result += test_span_default_is_base64();
	result += test_span_empty();
	result += test_span_filtered_produces_no_output();
	result += test_span_hex_dumps_raw_bytes();
	result += test_binary_data_default_is_base64();
	result += test_binary_data_hex_uses_columns();
	result += test_span_hex_then_nohex_restores_base64();
	result += test_span_vector_converts_to_span();

	// -------------------
	// Color
	// -------------------
	result += test_all_configured_colors_emit_expected_ansi();
	result += test_color_and_temporary_format_interoperate();
	result += test_color_manipulators_and_line_reset();
	result += test_colored_logger_destructor_resets_stream();
	result += test_default_color_emits_no_ansi();
	result += test_filtered_color_has_no_side_effects();

	// -------------------
	// Components
	// -------------------
	result += test_component_color_override_has_priority();
	result += test_component_format_priority_and_fallback();
	result += test_component_header_is_sticky_and_resettable();
	result += test_component_without_token_preserves_legacy_output();
	result += test_empty_component_does_not_push();
	result += test_format_change_redecides_throttle_line();
	result += test_push_format_overrides_component_and_restores_resolution();

	// -------------------
	// Floor
	// -------------------
	result += test_enabled_is_floor_not_throttle();
	result += test_filtered_produces_empty_output();
	result += test_filtered_then_enabled_message();
	result += test_log_critical_levels_are_never_filtered();
	result += test_log_level_filtering();

	// -------------------
	// Format stack
	// -------------------
	result += test_push_format_empty_and_partial_line_reset();
	result += test_push_pop_format_stack();

	// -------------------
	// Groups
	// -------------------
	result += test_filtered_group_has_no_side_effects();
	result += test_group_and_color_share_the_header();
	result += test_group_change_closes_partial_line();
	result += test_group_header_and_line_reset();
	result += test_group_without_token_and_empty_group();

	// -------------------
	// Human-readable
	// -------------------
	result += test_escaped_percent_in_format();
	result += test_humanreadable_bytes();
	result += test_humanreadable_enable_and_disable();
	result += test_humanreadable_number();
	result += test_nohumanreadable();

	// -------------------
	// Payload
	// -------------------
	result += test_cstring_payload();
	result += test_filtered_owned_text_is_dropped();
	result += test_size_payload();
	result += test_bytesize_payload();
	result += test_string_payload();
	result += test_string_view_and_wstring_view_payloads();
	result += test_wcstring_payload();
	result += test_wstring_payload();
	result += test_every_accepted_payload();
	result += test_pointer_owners_stream();
	result += test_logger_exception_path();

	// -------------------
	// Scope
	// -------------------
	result += test_component_stack_push_pop_and_join();
	result += test_scope_does_not_use_tls_stack();
	result += test_scope_format_inherits_parent_and_leaf_wins();
	result += test_scope_path_and_nested_scope();
	result += test_scope_throttle_binds_to_leaf();

	// -------------------
	// Throttle
	// -------------------
	result += test_flush_throttle_emits_orphaned_summary();
	result += test_flush_throttle_selects_component_only();
	result += test_inherited_drop_summary_stays_on_the_leaf();
	result += test_inherited_throttle_state_is_per_leaf();
	result += test_throttle_component_and_group_isolation();
	result += test_throttle_drop_burst();
	result += test_throttle_empty_lines_are_counted();
	result += test_throttle_level_change_redecides_line();
	result += test_throttle_off_preserves_output();
	result += test_throttle_precedence_and_no_throttle();
	result += test_throttle_rejects_invalid_specs();
	result += test_throttle_sample_and_window();
	result += test_throttle_summary_preserves_context();
	result += test_throttle_warning_but_not_error_or_fatal();

	// -------------------
	// Wide
	// -------------------
	result += test_invalid_wide_string_is_replaced();
	result += test_wide_string_logging_is_locale_independent();

	if (result == 0)
		std::cout << "All tests passed!" << std::endl;
	else
		std::cout << result << " tests failed." << std::endl;
	return result;
}
