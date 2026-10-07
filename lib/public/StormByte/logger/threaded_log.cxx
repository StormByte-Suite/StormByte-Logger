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
#include <StormByte/logger/exception.hxx>
#include <StormByte/logger/threaded_log.hxx>
#include <StormByte/safe/string.hxx>
#include <StormByte/safe/wstring.hxx>

#include <algorithm>
#include <exception>
#include <sstream>
#include <utility>
#include <vector>

using namespace StormByte::Logger;

ThreadedLog::~ThreadedLog() noexcept = default;

ThreadedLog& ThreadedLog::operator=(const ThreadedLog&) = default;

ThreadedLog& ThreadedLog::operator=(ThreadedLog&&) noexcept = default;

namespace {
	thread_local std::vector<const StormByte::ThreadLock*> t_line_locks;

	bool line_held(const StormByte::Safe::Shared<StormByte::ThreadLock>& lock) {
		return std::find(t_line_locks.begin(), t_line_locks.end(), lock.operator->()) != t_line_locks.end();
	}

	void claim_line(const StormByte::Safe::Shared<StormByte::ThreadLock>& lock) {
		if (line_held(lock))
			return;
		t_line_locks.push_back(lock.operator->());
		lock->Lock();
	}

	void release_line(const StormByte::Safe::Shared<StormByte::ThreadLock>& lock) {
		const auto held = std::find(t_line_locks.begin(), t_line_locks.end(), lock.operator->());
		if (held == t_line_locks.end())
			return;
		lock->Unlock();
		t_line_locks.erase(held);
	}

	[[noreturn]] void rethrow_threaded_exception(std::exception_ptr error, const char* operation) {
		try {
			std::rethrow_exception(std::move(error));
		} catch (const StormByte::Exception&) {
			throw;
		} catch (const std::exception& exception) {
			throw Exception("ThreadedLog {} failed: {}", operation, exception.what());
		} catch (...) {
			throw Exception("ThreadedLog {} failed with a non-standard exception", operation);
		}
	}

	template<typename Operation>
	auto with_line_lock(const StormByte::Safe::Shared<StormByte::ThreadLock>& lock,
		const char* description, Operation&& operation) -> decltype(operation()) {
		const bool already_held = line_held(lock);
		claim_line(lock);
		try {
			if constexpr (std::is_void_v<decltype(operation())>) {
				operation();
				if (!already_held)
					release_line(lock);
			} else {
				decltype(auto) result = operation();
				if (!already_held)
					release_line(lock);
				return std::forward<decltype(result)>(result);
			}
		} catch (...) {
			const std::exception_ptr error = std::current_exception();
			release_line(lock);
			rethrow_threaded_exception(error, description);
		}
	}

	bool manipulator_writes_newline(std::ostream& (*manip)(std::ostream&)) {
		if (manip == static_cast<std::ostream& (*)(std::ostream&)>(std::endl))
			return true;
		if (manip == static_cast<std::ostream& (*)(std::ostream&)>(std::ends))
			return false;
		std::ostringstream probe;
		try {
			probe << manip;
		} catch (...) {
		}
		try {
			return probe.str().find('\n') != std::string::npos;
		} catch (...) {
			return false;
		}
	}
}

ThreadedLog::ThreadedLog(SinkWrite write, SinkManip manip, void* context, const Level& level, std::string_view format):
	Log(write, manip, context, level, format), m_lock(StormByte::Safe::MakeShared<StormByte::ThreadLock>()) {}

ThreadedLog::ThreadedLog(StormByte::Safe::Callback&& callback, const Level& level, std::string_view format):
	Log(std::move(callback), level, format), m_lock(StormByte::Safe::MakeShared<StormByte::ThreadLock>()) {}

ThreadedLog::ThreadedLog(SinkFunction&& callback, const Level& level, std::string_view format):
	Log(std::move(callback), level, format), m_lock(StormByte::Safe::MakeShared<StormByte::ThreadLock>()) {}

Log::PointerType ThreadedLog::Clone() const {
	return PointerType::MakePointer<ThreadedLog>(*this);
}

Log::PointerType ThreadedLog::Move() {
	return PointerType::MakePointer<ThreadedLog>(*this);
}

bool ThreadedLog::BeginPayload() {
	if (!WillWrite() || !PrepareLine())
		return false;
	claim_line(m_lock);
	return true;
}

Log& ThreadedLog::Color(const Level& level, const StormByte::Logger::Color& color) {
	return with_line_lock(m_lock, "color update", [&]() -> Log& { return Log::Color(level, color); });
}

StormByte::Logger::Color ThreadedLog::Color(const Level& level) const {
	return with_line_lock(m_lock, "color query", [&] { return Log::Color(level); });
}

Log& ThreadedLog::Color(std::string_view component, const Level& level, const StormByte::Logger::Color& color) {
	return with_line_lock(m_lock, "component color update", [&]() -> Log& { return Log::Color(component, level, color); });
}

StormByte::Logger::Color ThreadedLog::Color(std::string_view component, const Level& level) const {
	return with_line_lock(m_lock, "component color query", [&] { return Log::Color(component, level); });
}

Log& ThreadedLog::Format(std::string_view format) {
	return with_line_lock(m_lock, "format update", [&]() -> Log& { return Log::Format(format); });
}

StormByte::Safe::String ThreadedLog::Format() const {
	return with_line_lock(m_lock, "format query", [&] { return Log::Format(); });
}

Log& ThreadedLog::Format(std::string_view component, std::string_view format) {
	return with_line_lock(m_lock, "component format update", [&]() -> Log& { return Log::Format(component, format); });
}

StormByte::Safe::String ThreadedLog::Format(std::string_view component) const {
	return with_line_lock(m_lock, "component format query", [&] { return Log::Format(component); });
}

Log& ThreadedLog::Throttle(const ThrottleSpec& spec) {
	return with_line_lock(m_lock, "throttle update", [&]() -> Log& { return Log::Throttle(spec); });
}

Log& ThreadedLog::NoThrottle(const ThrottleSpec& spec) {
	return with_line_lock(m_lock, "throttle removal", [&]() -> Log& { return Log::NoThrottle(spec); });
}

Log& ThreadedLog::Throttle(double rate, std::size_t burst) {
	return with_line_lock(m_lock, "throttle update", [&]() -> Log& { return Log::Throttle(rate, burst); });
}

Log& ThreadedLog::Throttle(double rate, std::size_t burst, ThrottlePolicy policy, std::size_t value, std::size_t period) {
	return with_line_lock(m_lock, "throttle update", [&]() -> Log& { return Log::Throttle(rate, burst, policy, value, period); });
}

Log& ThreadedLog::Throttle(const Level& level, double rate, std::size_t burst) {
	return with_line_lock(m_lock, "throttle update", [&]() -> Log& { return Log::Throttle(level, rate, burst); });
}

Log& ThreadedLog::Throttle(GroupManip group, double rate, std::size_t burst) {
	return with_line_lock(m_lock, "throttle update", [&]() -> Log& { return Log::Throttle(group, rate, burst); });
}

Log& ThreadedLog::Throttle(ComponentManip component, double rate, std::size_t burst) {
	return with_line_lock(m_lock, "throttle update", [&]() -> Log& { return Log::Throttle(component, rate, burst); });
}

Log& ThreadedLog::Throttle(ComponentManip component, const Level& level, GroupManip group, double rate, std::size_t burst, ThrottlePolicy policy, std::size_t value, std::size_t period) {
	return with_line_lock(m_lock, "throttle update", [&]() -> Log& { return Log::Throttle(component, level, group, rate, burst, policy, value, period); });
}

Log& ThreadedLog::NoThrottle() {
	return with_line_lock(m_lock, "throttle removal", [&]() -> Log& { return Log::NoThrottle(); });
}

Log& ThreadedLog::NoThrottle(const Level& level) {
	return with_line_lock(m_lock, "throttle removal", [&]() -> Log& { return Log::NoThrottle(level); });
}

Log& ThreadedLog::NoThrottle(GroupManip group) {
	return with_line_lock(m_lock, "throttle removal", [&]() -> Log& { return Log::NoThrottle(group); });
}

Log& ThreadedLog::NoThrottle(ComponentManip component) {
	return with_line_lock(m_lock, "throttle removal", [&]() -> Log& { return Log::NoThrottle(component); });
}

Log& ThreadedLog::NoThrottle(ComponentManip component, const Level& level, GroupManip group) {
	return with_line_lock(m_lock, "throttle removal", [&]() -> Log& { return Log::NoThrottle(component, level, group); });
}

Log& ThreadedLog::FlushThrottle() {
	return with_line_lock(m_lock, "throttle flush", [&]() -> Log& { return Log::FlushThrottle(); });
}

Log& ThreadedLog::FlushThrottle(const ThrottleSpec& spec) {
	return with_line_lock(m_lock, "throttle flush", [&]() -> Log& { return Log::FlushThrottle(spec); });
}

void ThreadedLog::Write(std::wstring_view v) {
	if (!WillWrite() || !PrepareLine())
		return;
	const StormByte::Safe::String encoded{StormByte::Safe::WString{v}};
	claim_line(m_lock);
	Log::WriteValue(static_cast<std::string_view>(encoded));
}

void ThreadedLog::Write(const wchar_t* v) {
	if (!WillWrite() || !PrepareLine())
		return;
	if (!v) {
		claim_line(m_lock);
		Log::WriteValue(std::string_view{});
		return;
	}
	const StormByte::Safe::String encoded{StormByte::Safe::WString{v}};
	claim_line(m_lock);
	Log::WriteValue(static_cast<std::string_view>(encoded));
}

void ThreadedLog::Write(std::span<const std::byte> v) {
	if (!WillWrite() || !PrepareLine())
		return;
	const std::string formatted = m_engine->FormatBinary(v);
	claim_line(m_lock);
	m_engine->WritePrepared(formatted);
}

void ThreadedLog::Write(const Level& level) {
	Log::Write(level);
	if (!WillWrite() && !HasOpenOutputLine())
		return;
	claim_line(m_lock);
}

void ThreadedLog::Write(std::ostream& (*manip)(std::ostream&)) {
	const bool newline = manipulator_writes_newline(manip);
	if (WillWrite() && PrepareLine()) {
		claim_line(m_lock);
		Log::Write(manip);
	} else {
		Log::Write(manip);
	}

	if (newline)
		release_line(m_lock);
}

void ThreadedLog::Write(Log& (*manip)(Log&) noexcept) {
	if ((LineDecided() && !LineAdmitted()) || !WillWrite()) {
		Log::Write(manip);
		return;
	}

	claim_line(m_lock);
	Log::Write(manip);
	if (!WillWrite())
		release_line(m_lock);
}

void ThreadedLog::Write(RedactManip m) {
	if ((LineDecided() && !LineAdmitted()) || !WillWrite()) {
		Log::Write(m);
		return;
	}

	claim_line(m_lock);
	Log::Write(m);
	if (!WillWrite())
		release_line(m_lock);
}

void ThreadedLog::Write(HexManip m) {
	if ((LineDecided() && !LineAdmitted()) || !WillWrite()) {
		Log::Write(m);
		return;
	}

	claim_line(m_lock);
	Log::Write(m);
	if (!WillWrite())
		release_line(m_lock);
}

void ThreadedLog::Write(NoHexManip m) {
	if ((LineDecided() && !LineAdmitted()) || !WillWrite()) {
		Log::Write(m);
		return;
	}

	claim_line(m_lock);
	Log::Write(m);
	if (!WillWrite())
		release_line(m_lock);
}

void ThreadedLog::Write(ColorManip m) {
	if ((LineDecided() && !LineAdmitted()) || !WillWrite()) {
		Log::Write(m);
		return;
	}

	claim_line(m_lock);
	Log::Write(m);
	if (!WillWrite())
		release_line(m_lock);
}

void ThreadedLog::Write(NoColorManip m) {
	if ((LineDecided() && !LineAdmitted()) || !WillWrite()) {
		Log::Write(m);
		return;
	}

	claim_line(m_lock);
	Log::Write(m);
	if (!WillWrite())
		release_line(m_lock);
}

void ThreadedLog::Write(FormatManip m) {
	if (LineDecided() && !LineAdmitted()) {
		Log::Write(std::move(m));
		return;
	}

	claim_line(m_lock);
	try {
		Log::Write(std::move(m));
	} catch (...) {
		const std::exception_ptr error = std::current_exception();
		release_line(m_lock);
		rethrow_threaded_exception(error, "temporary format update");
	}

	if (!WillWrite())
		release_line(m_lock);
}

void ThreadedLog::Write(PopFormatManip m) {
	const bool already_held = line_held(m_lock);
	if (LineDecided() && !LineAdmitted()) {
		Log::Write(m);
		return;
	}

	claim_line(m_lock);
	Log::Write(m);
	if (!already_held)
		release_line(m_lock);
}
