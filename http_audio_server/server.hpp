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

#ifndef HTTP_AUDIO_SERVER_SERVER
#define HTTP_AUDIO_SERVER_SERVER

#include <iosfwd>
#include <functional>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

struct mg_connection;

namespace http_audio_server {

struct RequestMapEntry;

struct Request {
	const RequestMapEntry &descr;
	std::string uri;
	std::string body;
	std::unordered_map<std::string, std::string> get;
	std::smatch matcher;
};

class ChunkedHTTPResponseBuf : public std::streambuf {
private:
	mg_connection *m_nc;
	std::vector<char> m_buf;

protected:
	int_type overflow(int_type ch) override;
	int sync() override;

public:
	ChunkedHTTPResponseBuf(mg_connection *nc);
	~ChunkedHTTPResponseBuf() override;
};

struct Response {
private:
	mg_connection *m_nc;
	ChunkedHTTPResponseBuf m_sbuf;
	std::ostream m_os;

	bool m_header_sent = false;

public:
	using Headers = std::unordered_map<std::string, std::string>;

	Response(mg_connection *nc);
	~Response();
	void header(int code, const Headers &headers = Headers{});
	std::ostream &stream();
	void stream(const std::string &filename);

	void ok(int code, const std::string &msg);
	void error(int code, const std::string &msg);
};

using RequestHandler =
    std::function<void(const Request &req, Response &res)>;

struct RequestMapEntry {
	std::string method;
	std::regex regex;
	RequestHandler handler;

	RequestMapEntry(const std::string &method, const std::string &regex,
	                RequestHandler handler)
	    : method(method), regex(regex), handler(handler)
	{
	}
};

class HTTPServerImpl;

class HTTPServer {
private:
	std::unique_ptr<HTTPServerImpl> m_impl;

public:
	HTTPServer(const std::vector<RequestMapEntry> &request_map, const std::string &host = "localhost", size_t port = 4851);
	~HTTPServer();
	void poll(size_t timeout);
};
}

#endif /* HTTP_AUDIO_SERVER_SERVER */
