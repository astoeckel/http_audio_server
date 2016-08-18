/*
 *  HTTP Streaming Audio Server
 *  Copyright (C) 2016  Andreas Stöckel
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <map>
#include <mutex>
#include <vector>

#include <http_audio_server/string_utils.hpp>
#include <http_audio_server/logger.hpp>
#include <http_audio_server/terminal.hpp>

namespace http_audio_server {
/*
 * Class LogStreamBackendImpl
 */

class LogStreamBackendImpl {
private:
	std::ostream &m_os;
	Terminal m_terminal;

public:
	LogStreamBackendImpl(std::ostream &os, bool use_color)
	    : m_os(os), m_terminal(use_color)
	{
	}

	void log(LogSeverity lvl, std::time_t time, const std::string &module,
	         const std::string &message)
	{
		{
			char time_str[41];
			std::strftime(time_str, 40, "%Y-%m-%d %H:%M:%S",
			              std::localtime(&time));
			m_os << m_terminal.italic() << time_str << m_terminal.reset();
		}

		m_os << " [" << module << "] ";

		if (lvl <= LogSeverity::DEBUG) {
			m_os << m_terminal.color(Terminal::BLACK, true) << "debug";
		}
		else if (lvl <= LogSeverity::INFO) {
			m_os << m_terminal.color(Terminal::BLUE, true) << "info";
		}
		else if (lvl <= LogSeverity::WARNING) {
			m_os << m_terminal.color(Terminal::MAGENTA, true) << "warning";
		}
		else if (lvl <= LogSeverity::ERROR) {
			m_os << m_terminal.color(Terminal::RED, true) << "error";
		}
		else {
			m_os << m_terminal.color(Terminal::RED, true) << "fatal error";
		}

		m_os << m_terminal.reset() << ": " << message << std::endl;
	}
};

/*
 * Class LogStreamBackend
 */

LogStreamBackend::LogStreamBackend(std::ostream &os, bool use_color)
    : m_impl(std::make_unique<LogStreamBackendImpl>(os, use_color))
{
}

void LogStreamBackend::log(LogSeverity lvl, std::time_t time,
                           const std::string &module,
                           const std::string &message)
{
	m_impl->log(lvl, time, module, message);
}

LogStreamBackend::~LogStreamBackend()
{
	// Do nothing here, just required for the unique_ptr destructor
}

/*
 * Class LogFileBackend
 */

static std::string make_log_filename(const std::string &prefix)
{
	static const std::string TAR_DIR = "logs";

	// Create the target directory
	if (mkdir(TAR_DIR.c_str(), S_IRWXU) == -1 && errno != EEXIST) {
		throw std::runtime_error("Error while creating logging subdirectory!");
	}

	// Build and return the filename
	std::string fn;
	std::time_t cur_time = time(nullptr);
	char time_str[41];
	std::strftime(time_str, 40, "%Y-%m-%d_%H_%M_%S", std::localtime(&cur_time));
	return TAR_DIR + "/" + prefix + "_" + time_str + "_" +
	       random_alphanum_string(4) + ".log";
}

LogFileBackend::LogFileBackend(const std::string &prefix)
    : LogStreamBackend(m_stream, false), m_stream(make_log_filename(prefix))
{
}

LogFileBackend::~LogFileBackend() {}
/*
 * Class LoggerImpl
 */

class LoggerImpl {
private:
	std::vector<std::tuple<std::shared_ptr<LogBackend>, LogSeverity>>
	    m_backends;
	std::mutex m_logger_mtx;
	LogSeverity m_min_level = LogSeverity::INFO;
	std::map<LogSeverity, size_t> m_counts;

	size_t backend_idx(int idx) const
	{
		idx = (idx < 0) ? int(m_backends.size()) + idx : idx;
		if (idx < 0 || size_t(idx) >= m_backends.size()) {
			throw std::invalid_argument("Given index out of range");
		}
		return idx;
	}

public:
	size_t backend_count() const { return m_backends.size(); }
	int add_backend(std::shared_ptr<LogBackend> backend, LogSeverity lvl)
	{
		std::lock_guard<std::mutex> lock(m_logger_mtx);
		m_backends.emplace_back(std::move(backend), lvl);
		return m_backends.size() - 1;
	}

	void min_level(LogSeverity lvl, int idx)
	{
		std::get<1>(m_backends[backend_idx(idx)]) = lvl;
	}

	LogSeverity min_level(int idx)
	{
		return std::get<1>(m_backends[backend_idx(idx)]);
	}

	void log(LogSeverity lvl, std::time_t time, const std::string &module,
	         const std::string &message)
	{
		std::lock_guard<std::mutex> lock(m_logger_mtx);

		// Update the statistics
		auto it = m_counts.find(lvl);
		if (it != m_counts.end()) {
			it->second++;
		}
		else {
			m_counts.emplace(lvl, 1);
		}

		// Actually issue the elements
		for (auto &backend : m_backends) {
			if (lvl >= std::get<1>(backend)) {
				std::get<0>(backend)->log(lvl, time, module, message);
			}
		}
	}

	void log(LogSeverity lvl, const std::string &module,
	         const std::string &message)
	{
		log(lvl, time(nullptr), module, message);
	}

	size_t count(LogSeverity lvl) const
	{
		size_t res = 0;
		auto it = m_counts.lower_bound(lvl);
		for (; it != m_counts.end(); it++) {
			res += it->second;
		}
		return res;
	}
};

/*
 * Class Logger
 */

Logger::Logger() : m_impl(std::make_unique<LoggerImpl>()) {}
Logger::Logger(std::shared_ptr<LogBackend> backend, LogSeverity lvl) : Logger()
{
	add_backend(std::move(backend), lvl);
}

size_t Logger::backend_count() const { return m_impl->backend_count(); }
size_t Logger::count(LogSeverity lvl) const { return m_impl->count(lvl); }
int Logger::add_backend(std::shared_ptr<LogBackend> backend, LogSeverity lvl)
{
	return m_impl->add_backend(backend, lvl);
}

void Logger::min_level(LogSeverity lvl, int idx)
{
	return m_impl->min_level(lvl, idx);
}

LogSeverity Logger::min_level(int idx) { return m_impl->min_level(idx); }
void Logger::log(LogSeverity lvl, std::time_t time, const std::string &module,
                 const std::string &message)
{
	m_impl->log(lvl, time, module, message);
}

void Logger::debug(const std::string &module, const std::string &message)
{
	m_impl->log(LogSeverity::DEBUG, module, message);
}

void Logger::info(const std::string &module, const std::string &message)
{
	m_impl->log(LogSeverity::INFO, module, message);
}

void Logger::warn(const std::string &module, const std::string &message)
{
	m_impl->log(LogSeverity::WARNING, module, message);
}

void Logger::error(const std::string &module, const std::string &message)
{
	m_impl->log(LogSeverity::ERROR, module, message);
}

void Logger::fatal_error(const std::string &module, const std::string &message)
{
	m_impl->log(LogSeverity::FATAL_ERROR, module, message);
}

/*
 * Functions
 */

Logger &global_logger()
{
	static std::mutex global_logger_mtx;
	static Logger logger;

	// Add the default backends to the logger if this has not been done yet
	std::lock_guard<std::mutex> lock(global_logger_mtx);
	if (logger.backend_count() == 0) {
		logger.add_backend(std::make_shared<LogStreamBackend>(std::cout, true),
		                   LogSeverity::INFO);
	}
	return logger;
}
}

