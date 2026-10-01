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
#include <StormByte/logger/engine.hxx>
#include <StormByte/logger/manipulators.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/safe/wstring.hxx>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <mutex>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

StormByte::Logger::Exception::~Exception() noexcept = default;

StormByte::Logger::ThrottleError::~ThrottleError() noexcept = default;

using namespace StormByte::Logger;

namespace {
	thread_local std::vector<std::string> t_component_stack;
	thread_local std::string t_facade_path;
	thread_local std::string t_group;
	thread_local std::optional<Level> t_level;

	struct LineState {
		bool decided = false;
		bool admitted = true;
		bool header_displayed = false;
		bool close_before_header = false;
		Level level = Level::Info;
		std::string component;
		std::string group;
		std::uint64_t dropped = 0;
	};

	thread_local LineState t_line;

	std::string ToStd(const StormByte::Safe::String& text) {
		return static_cast<std::string>(text);
	}

	std::string ToStdOrEmpty(const std::optional<StormByte::Safe::String>& text) {
		return text ? ToStd(*text) : std::string{};
	}

	bool IsAlwaysVisible(const Level level) noexcept {
		return level == Level::Warning || level == Level::Error || level == Level::Fatal;
	}

	std::size_t ColorIndex(const Level level) noexcept {
		return static_cast<std::size_t>(level);
	}

	std::string JoinStack(const std::vector<std::string>& stack) {
		std::string path;
		for (const auto& segment : stack) {
			if (segment.empty())
				continue;
			if (!path.empty())
				path += '/';
			path += segment;
		}
		return path;
	}

	std::string CurrentPath() {
		if (!t_facade_path.empty())
			return t_facade_path;
		return JoinStack(t_component_stack);
	}

	std::string ParentPath(const std::string& path) {
		const auto slash = path.rfind('/');
		if (slash == std::string::npos)
			return {};
		return path.substr(0, slash);
	}

	bool PathMatchesPrefix(const std::string& path, std::string_view prefix) {
		if (prefix.empty())
			return path.empty();
		if (path == prefix)
			return true;
		return path.size() > prefix.size() && path.compare(0, prefix.size(), prefix) == 0 && path[prefix.size()] == '/';
	}

	const char* AnsiColor(const StormByte::Logger::Color color) noexcept {
		switch (color) {
			case StormByte::Logger::Color::Black: return "\033[30m";
			case StormByte::Logger::Color::Red: return "\033[31m";
			case StormByte::Logger::Color::Green: return "\033[32m";
			case StormByte::Logger::Color::Yellow: return "\033[33m";
			case StormByte::Logger::Color::Blue: return "\033[34m";
			case StormByte::Logger::Color::Magenta: return "\033[35m";
			case StormByte::Logger::Color::Cyan: return "\033[36m";
			case StormByte::Logger::Color::Gray: return "\033[90m";
			case StormByte::Logger::Color::White: return "\033[37m";
			case StormByte::Logger::Color::BrightBlack: return "\033[90m";
			case StormByte::Logger::Color::BrightRed: return "\033[91m";
			case StormByte::Logger::Color::BrightGreen: return "\033[92m";
			case StormByte::Logger::Color::BrightYellow: return "\033[93m";
			case StormByte::Logger::Color::BrightBlue: return "\033[94m";
			case StormByte::Logger::Color::BrightMagenta: return "\033[95m";
			case StormByte::Logger::Color::BrightCyan: return "\033[96m";
			case StormByte::Logger::Color::BrightWhite: return "\033[97m";
			case StormByte::Logger::Color::Default: return "";
		}

		return "";
	}

	bool ManipulatorWritesNewline(std::ostream& (*manip)(std::ostream&)) {
		if (manip == static_cast<std::ostream& (*)(std::ostream&)>(std::endl))
			return true;
		if (manip == static_cast<std::ostream& (*)(std::ostream&)>(std::ends)
			|| manip == static_cast<std::ostream& (*)(std::ostream&)>(std::flush))
			return false;
		try {
			std::ostringstream probe;
			manip(probe);
			return probe.str().find('\n') != std::string::npos;
		} catch (...) {
			return false;
		}
	}

	int PatternSpecificity(const ThrottleSpec& spec) noexcept {
		const bool component = spec.Component.has_value();
		const bool level = spec.Level.has_value();
		const bool group = spec.Group.has_value();
		int score = 0;
		if (component && level && group)
			score = 7;
		else if (component && group)
			score = 6;
		else if (component && level)
			score = 5;
		else if (component)
			score = 4;
		else if (level && group)
			score = 3;
		else if (group)
			score = 2;
		else if (level)
			score = 1;
		if (component)
			score = score * 1000 + static_cast<int>(spec.Component->size());
		return score;
	}

	bool SameSelectors(const ThrottleSpec& left, const ThrottleSpec& right) noexcept {
		return left.Component == right.Component && left.Level == right.Level && left.Group == right.Group;
	}

	bool Matches(const ThrottleSpec& spec, const std::string& component, const Level level, const std::string& group) {
		if (spec.Level && *spec.Level != level)
			return false;
		if (spec.Group && static_cast<std::string_view>(*spec.Group) != group)
			return false;
		if (!spec.Component)
			return true;
		return PathMatchesPrefix(component, static_cast<std::string_view>(*spec.Component));
	}

	bool Selects(const ThrottleSpec& filter, const ThrottleSpec& rule) {
		return (!filter.Component || (rule.Component && *filter.Component == *rule.Component)) &&
			(!filter.Level || (rule.Level && *filter.Level == *rule.Level)) &&
			(!filter.Group || (rule.Group && *filter.Group == *rule.Group));
	}

	void ValidateThrottle(const ThrottleSpec& spec) {
		if (!std::isfinite(spec.Rate) || spec.Rate < 0.0 || (spec.Rate > 0.0 && spec.Burst == 0))
			throw StormByte::Logger::ThrottleError("Invalid throttle rate or burst");
		if (spec.Policy == ThrottlePolicy::Sample && spec.SampleN < 2)
			throw StormByte::Logger::ThrottleError("Invalid throttle sample period");
		if (spec.Policy == ThrottlePolicy::Window &&
			(spec.WindowPeriod == 0 || spec.WindowKeep == 0 || spec.WindowKeep > spec.WindowPeriod))
			throw StormByte::Logger::ThrottleError("Invalid throttle window");
	}

	std::int64_t NowNanoseconds() noexcept {
		return std::chrono::duration_cast<std::chrono::nanoseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count();
	}

	bool ConsumeFiniteCredit(ThrottleRuleState& state) noexcept {
		auto credits = state.finite_credits.load(std::memory_order_relaxed);
		while (credits != 0 && !state.finite_credits.compare_exchange_weak(
			credits, credits - 1, std::memory_order_acq_rel, std::memory_order_relaxed)) {}

		return credits != 0;
	}

	bool ConsumeRateCredit(ThrottleRuleState& state, const ThrottleSpec& spec) noexcept {
		if (spec.Rate == 0.0 && spec.Burst == 0)
			return true;
		if (spec.Rate == 0.0)
			return ConsumeFiniteCredit(state);
		const auto interval = std::max<std::int64_t>(1, static_cast<std::int64_t>(1'000'000'000.0 / spec.Rate));
		const auto now = NowNanoseconds();
		const auto capacity = static_cast<std::int64_t>(spec.Burst - 1) * interval;
		auto next = state.next_token_ns.load(std::memory_order_relaxed);
		for (;;) {
			const auto minimum = now - capacity;
			const auto reservation = std::max(next, minimum);
			if (reservation > now)
				return false;
			if (state.next_token_ns.compare_exchange_weak(
				next, reservation + interval, std::memory_order_acq_rel, std::memory_order_relaxed))
				return true;
		}
	}

	ThrottleRule MakeRule(const ThrottleSpec& spec) {
		ThrottleRule rule;
		rule.spec = spec;
		rule.state = std::make_shared<ThrottleRuleState>();
		rule.state->finite_credits.store(spec.Burst, std::memory_order_relaxed);
		rule.leaf_mutex = std::make_shared<std::mutex>();
		rule.leaf_states = std::make_shared<std::unordered_map<std::string, std::shared_ptr<ThrottleRuleState>>>();
		return rule;
	}
}

std::string Engine::CurrentTime() const noexcept {
	try {
		auto now = std::chrono::system_clock::now();
		std::time_t rawtime = std::chrono::system_clock::to_time_t(now);
		struct tm timeinfo{};
#ifdef WINDOWS
		localtime_s(&timeinfo, &rawtime);
#elifdef UNIX
		localtime_r(&rawtime, &timeinfo);
#else
		#error "Unsupported platform for CurrentTime()"
#endif
		char timebuf[64];
		std::size_t tn = std::strftime(timebuf, sizeof(timebuf), "%d/%m/%Y %H:%M:%S", &timeinfo);
		return std::string(timebuf, tn);
	} catch (...) {
		return std::string();
	}
}

Engine::Engine(SinkWrite write, SinkManip manip, void* context, const Level& level, const std::string& format):
	m_write(write),
	m_manip(manip),
	m_context(context),
	m_print_level(level),
	m_current_level(std::nullopt),
	m_enabled(true),
	m_format(format),
	m_human_readable_format(Detail::HumanReadable::Raw),
	m_redact_active(false),
	m_redact_count(0),
	m_redact_keep_first(false),
	m_hex_active(false),
	m_hex_columns(16),
	m_throttle_table(std::make_shared<const ThrottleTable>()) {
}

Engine::~Engine() noexcept {
	reset_color();
}

void Engine::SetFacadePath(std::string_view path) noexcept {
	t_facade_path.assign(path.data(), path.size());
}

std::shared_ptr<const ThrottleTable> Engine::LoadThrottleTable() const noexcept {
#ifdef WINDOWS
	return m_throttle_table.load(std::memory_order_acquire);
#elifdef __GLIBCXX__
	return m_throttle_table.load(std::memory_order_acquire);
#else
	return std::atomic_load_explicit(&m_throttle_table, std::memory_order_acquire);
#endif
}

void Engine::StoreThrottleTable(std::shared_ptr<const ThrottleTable> table) noexcept {
#ifdef WINDOWS
	m_throttle_table.store(std::move(table), std::memory_order_release);
#elifdef __GLIBCXX__
	m_throttle_table.store(std::move(table), std::memory_order_release);
#else
	std::atomic_store_explicit(&m_throttle_table, std::move(table), std::memory_order_release);
#endif
}

const Level& Engine::CurrentLevel() const noexcept {
	return t_level ? *t_level : m_print_level;
}

bool Engine::Enabled() const noexcept {
	if (!t_level)
		return m_enabled.load(std::memory_order_acquire);
	return IsAlwaysVisible(*t_level) || *t_level >= m_print_level;
}

void Engine::Color(const Level& level, const StormByte::Logger::Color& color) noexcept {
	if (ColorIndex(level) < m_level_colors.size())
		m_level_colors[ColorIndex(level)] = color;
}

void Engine::Color(const std::string& component, const Level& level, const StormByte::Logger::Color& color) {
	if (!component.empty())
		m_component_colors[component][ColorIndex(level)] = color;
}

StormByte::Logger::Color Engine::Color(const Level& level) const noexcept {
	if (ColorIndex(level) < m_level_colors.size())
		return m_level_colors[ColorIndex(level)];
	return StormByte::Logger::Color::Default;
}

StormByte::Logger::Color Engine::Color(const std::string& component, const Level& level) const noexcept {
	for (std::string path = component; !path.empty(); path = ParentPath(path)) {
		if (const auto found = m_component_colors.find(path); found != m_component_colors.end())
			return found->second[ColorIndex(level)];
	}
	return Color(level);
}

const std::string& Engine::effective_format() const noexcept {
	if (!m_format_stack.empty())
		return m_format_stack.back();
	const auto& component = t_line.decided ? t_line.component : CurrentPath();
	for (std::string path = component; !path.empty(); path = ParentPath(path)) {
		if (const auto found = m_component_formats.find(path); found != m_component_formats.end())
			return found->second;
	}
	return m_format;
}

const std::string& Engine::Format() const noexcept {
	return effective_format();
}

const std::string& Engine::Format(const std::string& component) const noexcept {
	for (std::string path = component; !path.empty(); path = ParentPath(path)) {
		if (const auto found = m_component_formats.find(path); found != m_component_formats.end())
			return found->second;
	}
	return m_format;
}

void Engine::Format(const std::string& format) {
	if (t_line.header_displayed) {
		reset_color();
		sink_manip(std::endl);
		t_line.header_displayed = false;
		m_content_color.reset();
		m_content_nocolor = false;
		t_group.clear();
		reset_line_state();
	}

	m_format = format;
}

void Engine::Format(const std::string& component, const std::string& format) {
	if (component.empty()) {
		Format(format);
		return;
	}

	if (t_line.header_displayed) {
		reset_color();
		sink_manip(std::endl);
		t_line.header_displayed = false;
		m_content_color.reset();
		m_content_nocolor = false;
		t_group.clear();
		reset_line_state();
	}

	if (format.empty())
		m_component_formats.erase(component);
	else
		m_component_formats[component] = format;
}

void Engine::Throttle(const ThrottleSpec& spec) {
	ValidateThrottle(spec);
	auto current = LoadThrottleTable();
	auto next = std::make_shared<ThrottleTable>(*current);
	auto rule = MakeRule(spec);
	auto found = std::find_if(next->rules.begin(), next->rules.end(), [&](const ThrottleRule& candidate) {
		return SameSelectors(candidate.spec, spec);
	});
	if (found == next->rules.end())
		next->rules.push_back(std::move(rule));
	else
		*found = std::move(rule);
	StoreThrottleTable(std::shared_ptr<const ThrottleTable>(std::move(next)));
}

std::shared_ptr<ThrottleRuleState> Engine::LeafThrottleState(
	const ThrottleRule& rule, const std::string& path) {
	if (!rule.leaf_mutex || !rule.leaf_states)
		return rule.state;
	std::lock_guard<std::mutex> lock(*rule.leaf_mutex);
	auto& slot = (*rule.leaf_states)[path];
	if (!slot) {
		slot = std::make_shared<ThrottleRuleState>();
		slot->finite_credits.store(rule.spec.Burst, std::memory_order_relaxed);
	}
	return slot;
}

void Engine::NoThrottle(const ThrottleSpec& spec) {
	auto current = LoadThrottleTable();
	auto next = std::make_shared<ThrottleTable>(*current);
	next->rules.erase(std::remove_if(next->rules.begin(), next->rules.end(), [&](const ThrottleRule& candidate) {
		return SameSelectors(candidate.spec, spec);
	}), next->rules.end());
	StoreThrottleTable(std::shared_ptr<const ThrottleTable>(std::move(next)));
}

void Engine::NoThrottleAll() noexcept {
	StoreThrottleTable(std::make_shared<const ThrottleTable>());
}

void Engine::FlushThrottle() {
	FlushThrottle(ThrottleSpec{});
}

void Engine::FlushThrottle(const ThrottleSpec& filter) {
	const auto table = LoadThrottleTable();
	const auto saved = t_line;
	if (t_line.header_displayed) {
		reset_color();
		sink_char('\n');
		reset_line_state();
	}

	const auto emit = [&](const ThrottleRule& rule, const std::string& component,
		const std::shared_ptr<ThrottleRuleState>& state) {
		if (!state)
			return;
		const auto dropped = state->dropped.exchange(0, std::memory_order_acq_rel);
		if (dropped == 0)
			return;
		t_line.decided = true;
		t_line.admitted = true;
		t_line.level = rule.spec.Level.value_or(Level::Notice);
		t_line.component = component;
		t_line.group = ToStdOrEmpty(rule.spec.Group);
		t_line.dropped = 0;
		BeginOutputLine();
		print_header();
		sync_content_color();
		sink_write("dropped ");
		sink_write(std::to_string(dropped));
		sink_write(" messages");
		reset_color();
		sink_char('\n');
		reset_line_state();
	};

	for (const auto& rule : table->rules) {
		if (!Selects(filter, rule.spec))
			continue;
		if (rule.leaf_states && rule.leaf_mutex) {
			std::lock_guard<std::mutex> lock(*rule.leaf_mutex);
			for (const auto& [path, state] : *rule.leaf_states)
				emit(rule, path, state);
		} else {
			emit(rule, ToStdOrEmpty(rule.spec.Component), rule.state);
		}
	}

	t_line = saved;
}

bool Engine::PrepareLine() {
	if (t_line.decided)
		return t_line.admitted;
	t_line.decided = true;
	t_line.level = t_level.value_or(m_print_level);
	t_line.component = CurrentPath();
	t_line.group = t_group;
	t_line.admitted = true;
	t_line.dropped = 0;
	if (t_line.level == Level::Error || t_line.level == Level::Fatal)
		return true;
	const auto table = LoadThrottleTable();
	const ThrottleRule* selected = nullptr;
	int selected_specificity = -1;
	for (const auto& rule : table->rules) {
		if (Matches(rule.spec, t_line.component, t_line.level, t_line.group)) {
			const int specificity = PatternSpecificity(rule.spec);
			if (specificity > selected_specificity) {
				selected = &rule;
				selected_specificity = specificity;
			}
		}
	}

	if (selected == nullptr)
		return true;
	auto state_ptr = LeafThrottleState(*selected, t_line.component);
	auto& state = *state_ptr;
	bool admitted = true;
	if (selected->spec.Policy == ThrottlePolicy::Sample) {
		const auto index = state.sample_count.fetch_add(1, std::memory_order_relaxed);
		admitted = index % selected->spec.SampleN == 0;
	} else if (selected->spec.Policy == ThrottlePolicy::Window) {
		const auto index = state.window_count.fetch_add(1, std::memory_order_relaxed);
		admitted = index % selected->spec.WindowPeriod < selected->spec.WindowKeep;
	}

	if (admitted)
		admitted = ConsumeRateCredit(state, selected->spec);
	if (!admitted) {
		state.dropped.fetch_add(1, std::memory_order_relaxed);
		t_line.admitted = false;
		return false;
	}

	t_line.dropped = state.dropped.exchange(0, std::memory_order_acq_rel);
	return true;
}

bool Engine::LineAdmitted() const noexcept {
	return t_line.admitted;
}

bool Engine::LineDecided() const noexcept {
	return t_line.decided;
}

bool Engine::HasOpenOutputLine() const noexcept {
	return t_line.header_displayed;
}

void Engine::BeginOutputLine() noexcept {
	if (t_line.close_before_header) {
		reset_color();
		sink_char('\n');
		t_line.close_before_header = false;
		t_line.header_displayed = false;
	}

	t_line.header_displayed = true;
}

void Engine::close_deferred_line() noexcept {
	if (!t_line.close_before_header)
		return;
	reset_color();
	sink_char('\n');
	t_line.close_before_header = false;
	t_line.header_displayed = false;
}

void Engine::reset_line_state() noexcept {
	t_line = {};
	t_facade_path.clear();
}

void Engine::write_drop_summary() noexcept {
	if (t_line.dropped == 0)
		return;
	print_header();
	sync_content_color();
	sink_write("dropped ");
	sink_write(std::to_string(t_line.dropped));
	sink_write(" messages");
	reset_color();
	sink_char('\n');
	t_line.dropped = 0;
}

Engine& Engine::operator<<(const Level& level) noexcept {
	if (t_level) {
		if (level != *t_level && (IsAlwaysVisible(*t_level) || *t_level >= m_print_level) && t_line.header_displayed) {
			reset_color();
			reset_line_state();
			t_line.close_before_header = true;
			t_group.clear();
		}
	}

	t_level = level;
	m_content_color.reset();
	m_content_nocolor = false;
	m_enabled.store(IsAlwaysVisible(level) || level >= m_print_level, std::memory_order_release);
	return *this;
}

Engine& Engine::operator<<(std::ostream& (*manip)(std::ostream&)) noexcept {
	if (ManipulatorWritesNewline(manip)) {
		// A newline closes the line. The next admitted payload prints a new header.
		// std::endl is forwarded only when this line is actually emitted, so the
		// caller flushes and the text shows up at this instant. A filtered or
		// throttled line never touches the stream: there is nothing to flush.
		if (Enabled() && PrepareLine()) {
			write_drop_summary();
			reset_color();
			if (t_line.admitted)
				sink_manip(manip);
		}

		t_line.header_displayed = false;
		m_content_color.reset();
		m_content_nocolor = false;
		t_group.clear();
		reset_line_state();
		return *this;
	}

	if (Enabled())
		sink_manip(manip);
	return *this;
}

Engine& Engine::operator<<(ColorManip manip) noexcept {
	if (!Enabled())
		return *this;
	m_content_nocolor = false;
	m_content_color = manip.value;
	if (t_line.header_displayed)
		sync_content_color();
	return *this;
}

Engine& Engine::operator<<(NoColorManip) noexcept {
	if (!Enabled())
		return *this;
	m_content_color.reset();
	m_content_nocolor = true;
	if (t_line.header_displayed)
		sync_content_color();
	return *this;
}

Engine& Engine::operator<<(FormatManip manip) {
	if (t_line.header_displayed) {
		reset_color();
		sink_manip(std::endl);
		t_line.header_displayed = false;
		m_content_color.reset();
		m_content_nocolor = false;
		t_group.clear();
		reset_line_state();
	}

	m_format_stack.push_back(ToStd(manip.format));
	return *this;
}

Engine& Engine::operator<<(PopFormatManip) noexcept {
	if (m_format_stack.empty())
		return *this;
	if (t_line.header_displayed) {
		reset_color();
		sink_manip(std::endl);
		t_line.header_displayed = false;
		m_content_color.reset();
		m_content_nocolor = false;
		t_group.clear();
		reset_line_state();
	}

	m_format_stack.pop_back();
	return *this;
}

Engine& Engine::operator<<(GroupManip manip) {
	if (t_line.header_displayed) {
		reset_color();
		sink_manip(std::endl);
		t_line.header_displayed = false;
		m_content_color.reset();
		m_content_nocolor = false;
		reset_line_state();
	}

	t_group = ToStd(manip.name);
	return *this;
}

Engine& Engine::operator<<(ComponentManip manip) {
	if (!manip.name.empty())
		t_component_stack.push_back(ToStd(manip.name));
	return *this;
}

Engine& Engine::operator<<(PopComponentManip) {
	if (!t_component_stack.empty())
		t_component_stack.pop_back();
	return *this;
}

Engine& Engine::operator<<(ResetComponentManip) {
	t_component_stack.clear();
	return *this;
}

void Engine::print_time(std::string& out) const noexcept {
	out += CurrentTime();
}

void Engine::print_level(std::string& out) const noexcept {
	constexpr std::size_t fixed_width = 8;
	const char* const level_str = LevelToString(t_line.decided ? t_line.level : t_level.value_or(m_print_level));
	out += level_str;
	const std::size_t length = std::char_traits<char>::length(level_str);
	if (length < fixed_width)
		out.append(fixed_width - length, ' ');
}

void Engine::print_thread_id(std::string& out) const noexcept {
	thread_local std::ostringstream id;
	id.str({});
	id.clear();
	id << std::this_thread::get_id();
	out += id.str();
}

void Engine::print_header() noexcept {
	std::string header;
	header.reserve(160);
	const std::string& fmt = effective_format();
	const auto component = t_line.decided ? t_line.component : CurrentPath();
	append_color(header, Color(component, t_line.level));
	for (std::size_t i = 0; i < fmt.size(); ++i) {
		if (fmt[i] == '%' && (i + 1) < fmt.size()) {
			const char spec = fmt[i + 1];
			switch (spec) {
				case '%':
					header.push_back('%');
					++i;
					break;
				case 'L':
					print_level(header);
					++i;
					break;

				case 'T':
					print_time(header);
					++i;
					break;
				case 'i':
					print_thread_id(header);
					++i;
					break;
				case 'g':
					header += t_line.decided ? t_line.group : t_group;
					++i;
					break;
				case 'c':
					header += component;
					++i;
					break;
				default:
					header.push_back('%');
					break;
			}
		} else {
			header.push_back(fmt[i]);
		}
	}

	header.push_back(' ');
	sink_write(header);
}

void Engine::append_color(std::string& out, const StormByte::Logger::Color color) noexcept {
	if (m_active_color == std::optional<StormByte::Logger::Color>{color})
		return;
	if (m_active_color) {
		out += "\033[0m";
		m_active_color.reset();
	}

	if (color != StormByte::Logger::Color::Default) {
		out += AnsiColor(color);
		m_active_color = color;
	}
}

void Engine::emit_color(const StormByte::Logger::Color color) noexcept {
	std::string sequence;
	append_color(sequence, color);
	sink_write(sequence);
}

void Engine::sync_content_color() noexcept {
	const auto level = t_line.decided ? t_line.level : t_level.value_or(m_print_level);
	const auto component = t_line.decided ? t_line.component : CurrentPath();
	const auto configured = Color(component, level);
	if (m_content_nocolor)
		emit_color(StormByte::Logger::Color::Default);
	else if (m_content_color)
		emit_color(*m_content_color);
	else
		emit_color(configured);
}

void Engine::reset_color() noexcept {
	if (m_active_color) {
		sink_write("\033[0m");
		m_active_color.reset();
	}
}

void Engine::print_message(const std::string& message) noexcept {
	if (!Enabled())
		return;
	write_text(message);
}

void Engine::print_message(const wchar_t& value) {
	const wchar_t raw[1] = { value };
	const StormByte::Safe::String encoded{StormByte::Safe::WString{std::wstring_view{raw, 1}}};
	print_message(ToStd(encoded));
}

namespace StormByte::Logger {
	template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<bool>(const bool& value);
	template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<short>(const short& value);
	template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<unsigned short>(const unsigned short& value);
	template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<int>(const int& value);
	template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<unsigned int>(const unsigned int& value);
	template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<long>(const long& value);
	template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<unsigned long>(const unsigned long& value);
	template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<long long>(const long long& value);
	template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<unsigned long long>(const unsigned long long& value);
	template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<float>(const float& value);
	template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<double>(const double& value);
	template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<long double>(const long double& value);
	template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<char>(const char& value);
	template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<signed char>(const signed char& value);
	template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<unsigned char>(const unsigned char& value);
	template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<wchar_t>(const wchar_t& value);
	template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<std::string>(const std::string& value);
	template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<std::wstring>(const std::wstring& value);
	template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<const char*>(const char* const& value);
	template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<const wchar_t*>(const wchar_t* const& value);
	template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<std::string_view>(const std::string_view& value);
	template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<std::wstring_view>(const std::wstring_view& value);
	template STORMBYTE_LOGGER_PRIVATE Engine& Engine::operator<<<std::span<const std::byte>>(const std::span<const std::byte>& value);
}
