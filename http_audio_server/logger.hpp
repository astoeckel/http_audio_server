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

/**
 * @file logger.hpp
 *
 * Implements a simple logging system used to log messages.
 *
 * @author Andreas Stöckel
 */

#ifndef HTTP_AUDIO_SERVER_LOGGER_HPP
#define HTTP_AUDIO_SERVER_LOGGER_HPP

#include <ctime>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <memory>
#include <string>

namespace http_audio_server {
/*
 * Forward declarations.
 */
class Logger;
class LoggerImpl;
class LogStreamBackendImpl;

/**
 * The LogSeverity enum holds the LogSeverity of a log message. Higher
 * severities are associated with higher values.
 */
enum LogSeverity : int32_t {
	DEBUG = 10,
	INFO = 20,
	WARNING = 30,
	ERROR = 40,
	FATAL_ERROR = 50
};

/**
 * The Backend class is the abstract base class that must be implemented by
 * any log backend.
 */
class LogBackend {
public:
	/**
	 * Pure virtual function which is called whenever a new log message should
	 * be logged.
	 */
	virtual void log(LogSeverity lvl, std::time_t time,
	                 const std::string &module, const std::string &message) = 0;
	virtual ~LogBackend() {}
};

/**
 * Implementation of the LogBackend class which loggs to the given output
 * stream.
 */
class LogStreamBackend : public LogBackend {
private:
	std::unique_ptr<LogStreamBackendImpl> m_impl;

public:
	LogStreamBackend(std::ostream &os, bool use_color = false);

	void log(LogSeverity lvl, std::time_t time, const std::string &module,
	         const std::string &message) override;
	~LogStreamBackend() override;
};

/**
 * Writes a log file to the "logs/" directory including the current date, time,
 * and a short random string.
 */
class LogFileBackend : public LogStreamBackend {
private:
	std::ofstream m_stream;

public:
	LogFileBackend(const std::string &prefix = "http_audio_server_");
	~LogFileBackend() override;
};

/**
 * The Logger class is the frontend class that should be used to log messages.
 * A global instance which logs to std::cerr can be accessed using the
 * global_logger() function.
 */
class Logger {
private:
	std::unique_ptr<LoggerImpl> m_impl;

public:
	/**
	 * Default constructor, adds no backend.
	 */
	Logger();

	/**
	 * Creates a new Logger instance which logs into the given backend.
	 */
	Logger(std::shared_ptr<LogBackend> backend,
	       LogSeverity lvl = LogSeverity::INFO);

	/**
	 * Returns the number of attached backends.
	 */
	size_t backend_count() const;

	/**
	 * Adds another logger backend with the given log level and returns the
	 * index of the newly added backend.
	 */
	int add_backend(std::shared_ptr<LogBackend> backend,
	                 LogSeverity lvl = LogSeverity::INFO);

	/**
	 * Sets the minimum level for the backend with the given index. Negative
	 * indices allow to access the backend list in reverse order. Per default
	 * the log level of the last added backend is updated.
	 */
	void min_level(LogSeverity lvl, int idx = -1);

	/**
	 * Returns the log level for the backend with the given index. Negative
	 * indices allow to access the backend list in reverse order.
	 */
	LogSeverity min_level(int idx = -1);

	/**
	 * Returns the number of messages that have been captured with at least the
	 * given level.
	 */
	size_t count(LogSeverity lvl = LogSeverity::DEBUG) const;

	void log(LogSeverity lvl, std::time_t time, const std::string &module,
	         const std::string &message);
	void debug(const std::string &module, const std::string &message);
	void info(const std::string &module, const std::string &message);
	void warn(const std::string &module, const std::string &message);
	void error(const std::string &module, const std::string &message);
	void fatal_error(const std::string &module, const std::string &message);
};

Logger &global_logger();
}

#endif /* HTTP_AUDIO_SERVER_LOGGER_HPP */
