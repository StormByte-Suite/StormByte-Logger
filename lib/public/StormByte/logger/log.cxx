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

#include <StormByte/logger/engine.hxx>
#include <StormByte/logger/log.hxx>
#include <StormByte/safe/string.hxx>

#include <string>
#include <utility>

using namespace StormByte::Logger;

Log::~Log() noexcept = default;

Log& Log::operator=(const Log&) = default;

Log& Log::operator=(Log&&) noexcept = default;

namespace {
	StormByte::Logger::ThrottleSpec make_spec(const double rate, const std::size_t burst,
		const StormByte::Logger::ThrottlePolicy policy = StormByte::Logger::ThrottlePolicy::Drop,
		const std::size_t value = 0, const std::size_t period = 0) {
		StormByte::Logger::ThrottleSpec spec;
		spec.Rate = rate;
		spec.Burst = burst;
		spec.Policy = policy;
		if (policy == StormByte::Logger::ThrottlePolicy::Sample)
			spec.SampleN = value;
		if (policy == StormByte::Logger::ThrottlePolicy::Window) {
			spec.WindowKeep = value;
			spec.WindowPeriod = period;
		}

		return spec;
	}

	bool AlwaysVisible(const Level level) noexcept {
		return level == Level::Warning || level == Level::Error || level == Level::Fatal;
	}

	std::string Native(const StormByte::Safe::String& text) {
		return std::string{static_cast<std::string_view>(text)};
	}

	StormByte::Safe::String JoinPath(const StormByte::Safe::String& base, std::string_view add) {
		const std::string_view left = static_cast<std::string_view>(base);
		if (add.empty())
			return base;
		if (left.empty())
			return StormByte::Safe::String{add};
		std::string out;
		out.reserve(left.size() + 1 + add.size());
		out.append(left);
		out.push_back('/');
		out.append(add);
		return StormByte::Safe::String{std::string_view{out}};
	}

	void BindStickyComponent(ThrottleSpec& spec, const StormByte::Safe::String& path) {
		if (!spec.Component && !static_cast<std::string_view>(path).empty())
			spec.Component = path;
	}

	StormByte::Safe::String CopyFormat(const std::string& format) {
		return StormByte::Safe::String{std::string_view{format}};
	}
}

template <typename T>
void Log::WriteValue(const T& v) {
	if (!BeginPayload())
		return;
	m_engine << v;
}

Log::Log(SinkWrite write, SinkManip manip, void* context, const Level& level, std::string_view format) {
	m_engine = std::make_shared<Engine>(write, manip, context, level, std::string{format});
}

Log::PointerType Log::Clone() const {
	return PointerType::MakePointer<Log>(*this);
}

Log::PointerType Log::Move() {
	return PointerType::MakePointer<Log>(*this);
}

Log::PointerType Log::Scope(std::string_view path) {
	auto facade = Clone();
	facade->m_scope_path = JoinPath(m_scope_path, path);
	return facade;
}

bool Log::Enabled(const Level& level) const noexcept {
	return AlwaysVisible(level) || level >= m_engine->PrintLevel();
}

bool Log::BeginPayload() {
	return true;
}

void Log::Write(std::wstring_view v) {
	if (!BeginPayload())
		return;
	m_engine << v;
}

void Log::Write(const wchar_t* v) {
	if (!BeginPayload())
		return;
	m_engine << v;
}

void Log::Write(std::span<const std::byte> v) {
	if (!BeginPayload())
		return;
	m_engine << v;
}

void Log::Write(const Level& level) {
	m_engine << level;
	if (!WillWrite() && !HasOpenOutputLine())
		return;
	m_engine->SetFacadePath(static_cast<std::string_view>(m_scope_path));
}

void Log::Write(std::ostream& (*manip)(std::ostream&)) {
	m_engine << manip;
}

void Log::Write(Log& (*manip)(Log&) noexcept) {
	manip(*this);
}

void Log::Write(RedactManip m) {
	m_engine->SetRedact(true, m.count, m.keep_first);
}

void Log::Write(HexManip m) {
	m_engine->SetHex(true, m.columns);
}

void Log::Write(NoHexManip) {
	m_engine->SetHex(false, 0);
}

Log& Log::Color(const Level& level, const StormByte::Logger::Color& color) {
	if (static_cast<std::string_view>(m_scope_path).empty())
		m_engine->Color(level, color);
	else
		m_engine->Color(Native(m_scope_path), level, color);
	return *this;
}

StormByte::Logger::Color Log::Color(const Level& level) const {
	if (static_cast<std::string_view>(m_scope_path).empty())
		return m_engine->Color(level);
	return m_engine->Color(Native(m_scope_path), level);
}

Log& Log::Color(std::string_view component, const Level& level, const StormByte::Logger::Color& color) {
	m_engine->Color(std::string{component}, level, color);
	return *this;
}

StormByte::Logger::Color Log::Color(std::string_view component, const Level& level) const {
	return m_engine->Color(std::string{component}, level);
}

Log& Log::Format(std::string_view format) {
	if (static_cast<std::string_view>(m_scope_path).empty())
		m_engine->Format(std::string{format});
	else
		m_engine->Format(Native(m_scope_path), std::string{format});
	return *this;
}

StormByte::Safe::String Log::Format() const {
	if (static_cast<std::string_view>(m_scope_path).empty())
		return CopyFormat(m_engine->Format());
	return CopyFormat(static_cast<const Engine&>(*m_engine).Format(Native(m_scope_path)));
}

Log& Log::Format(std::string_view component, std::string_view format) {
	m_engine->Format(std::string{component}, std::string{format});
	return *this;
}

StormByte::Safe::String Log::Format(std::string_view component) const {
	return CopyFormat(static_cast<const Engine&>(*m_engine).Format(std::string{component}));
}

Log& Log::Throttle(const ThrottleSpec& spec) {
	ThrottleSpec bound = spec;
	BindStickyComponent(bound, m_scope_path);
	m_engine->Throttle(bound);
	return *this;
}

Log& Log::NoThrottle(const ThrottleSpec& spec) {
	ThrottleSpec bound = spec;
	BindStickyComponent(bound, m_scope_path);
	m_engine->NoThrottle(bound);
	return *this;
}

Log& Log::Throttle(double rate, std::size_t burst) {
	return Throttle(make_spec(rate, burst));
}

Log& Log::Throttle(double rate, std::size_t burst, ThrottlePolicy policy, std::size_t value, std::size_t period) {
	return Throttle(make_spec(rate, burst, policy, value, period));
}

Log& Log::Throttle(const Level& level, double rate, std::size_t burst) {
	auto spec = make_spec(rate, burst);
	spec.Level = level;
	return Throttle(spec);
}

Log& Log::Throttle(GroupManip group, double rate, std::size_t burst) {
	auto spec = make_spec(rate, burst);
	spec.Group = std::move(group.name);
	return Throttle(spec);
}

Log& Log::Throttle(ComponentManip component, double rate, std::size_t burst) {
	auto spec = make_spec(rate, burst);
	spec.Component = std::move(component.name);
	return Throttle(spec);
}

Log& Log::Throttle(ComponentManip component, const Level& level, GroupManip group, double rate, std::size_t burst, ThrottlePolicy policy, std::size_t value, std::size_t period) {
	auto spec = make_spec(rate, burst, policy, value, period);
	spec.Component = std::move(component.name);
	spec.Level = level;
	spec.Group = std::move(group.name);
	return Throttle(spec);
}

Log& Log::NoThrottle() {
	m_engine->NoThrottleAll();
	return *this;
}

Log& Log::NoThrottle(const Level& level) {
	ThrottleSpec spec;
	spec.Level = level;
	return NoThrottle(spec);
}

Log& Log::NoThrottle(GroupManip group) {
	ThrottleSpec spec;
	spec.Group = std::move(group.name);
	return NoThrottle(spec);
}

Log& Log::NoThrottle(ComponentManip component) {
	ThrottleSpec spec;
	spec.Component = std::move(component.name);
	return NoThrottle(spec);
}

Log& Log::NoThrottle(ComponentManip component, const Level& level, GroupManip group) {
	ThrottleSpec spec;
	spec.Component = std::move(component.name);
	spec.Level = level;
	spec.Group = std::move(group.name);
	return NoThrottle(spec);
}

Log& Log::FlushThrottle() {
	m_engine->FlushThrottle();
	return *this;
}

Log& Log::FlushThrottle(const ThrottleSpec& spec) {
	m_engine->FlushThrottle(spec);
	return *this;
}

void Log::Write(ColorManip manip) {
	*m_engine << manip;
}

void Log::Write(NoColorManip manip) {
	*m_engine << manip;
}

void Log::Write(FormatManip manip) {
	*m_engine << std::move(manip);
}

void Log::Write(PopFormatManip manip) {
	*m_engine << manip;
}

void Log::Write(GroupManip manip) {
	*m_engine << std::move(manip);
}

void Log::Write(ComponentManip manip) {
	*m_engine << std::move(manip);
}

void Log::Write(PopComponentManip manip) {
	*m_engine << manip;
}

void Log::Write(ResetComponentManip manip) {
	*m_engine << manip;
}

bool Log::WillWrite() const noexcept {
	return m_engine->Enabled();
}

bool Log::PrepareLine() {
	return m_engine->PrepareLine();
}

bool Log::HasOpenOutputLine() const noexcept {
	return m_engine->HasOpenOutputLine();
}

bool Log::LineDecided() const noexcept {
	return m_engine->LineDecided();
}

bool Log::LineAdmitted() const noexcept {
	return m_engine->LineAdmitted();
}

template STORMBYTE_LOGGER_INSTANTIATE void Log::WriteValue<bool>(const bool& v);
template STORMBYTE_LOGGER_INSTANTIATE void Log::WriteValue<char>(const char& v);
template STORMBYTE_LOGGER_INSTANTIATE void Log::WriteValue<signed char>(const signed char& v);
template STORMBYTE_LOGGER_INSTANTIATE void Log::WriteValue<unsigned char>(const unsigned char& v);
template STORMBYTE_LOGGER_INSTANTIATE void Log::WriteValue<short>(const short& v);
template STORMBYTE_LOGGER_INSTANTIATE void Log::WriteValue<unsigned short>(const unsigned short& v);
template STORMBYTE_LOGGER_INSTANTIATE void Log::WriteValue<int>(const int& v);
template STORMBYTE_LOGGER_INSTANTIATE void Log::WriteValue<unsigned int>(const unsigned int& v);
template STORMBYTE_LOGGER_INSTANTIATE void Log::WriteValue<long>(const long& v);
template STORMBYTE_LOGGER_INSTANTIATE void Log::WriteValue<unsigned long>(const unsigned long& v);
template STORMBYTE_LOGGER_INSTANTIATE void Log::WriteValue<long long>(const long long& v);
template STORMBYTE_LOGGER_INSTANTIATE void Log::WriteValue<unsigned long long>(const unsigned long long& v);
template STORMBYTE_LOGGER_INSTANTIATE void Log::WriteValue<float>(const float& v);
template STORMBYTE_LOGGER_INSTANTIATE void Log::WriteValue<double>(const double& v);
template STORMBYTE_LOGGER_INSTANTIATE void Log::WriteValue<long double>(const long double& v);
template STORMBYTE_LOGGER_INSTANTIATE void Log::WriteValue<std::string_view>(const std::string_view& v);
template STORMBYTE_LOGGER_INSTANTIATE void Log::WriteValue<const char*>(const char* const& v);
