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

#include <stdlib.h>

#include <lib/mongoose.h>

#include <http_audio_server/json.hpp>
#include <http_audio_server/logger.hpp>
#include <http_audio_server/server.hpp>

namespace http_audio_server {

/*
 * Class ChunkedHTTPResponseBuf
 */

ChunkedHTTPResponseBuf::ChunkedHTTPResponseBuf(mg_connection *nc)
    : m_nc(nc), m_buf(4096 + 1)
{
	setp(&m_buf[0], &m_buf[4096]);
}

ChunkedHTTPResponseBuf::~ChunkedHTTPResponseBuf() {}
ChunkedHTTPResponseBuf::int_type ChunkedHTTPResponseBuf::overflow(
    ChunkedHTTPResponseBuf::int_type ch)
{
	if (ch != ChunkedHTTPResponseBuf::traits_type::eof()) {
		assert(std::less_equal<char *>()(pptr(), epptr()));
		*pptr() = ch;
		pbump(1);
		sync();
		return ch;
	}
	return traits_type::eof();
}

int ChunkedHTTPResponseBuf::sync()
{
	const std::ptrdiff_t s = pptr() - pbase();
	mg_send_http_chunk(m_nc, pbase(), s);
	pbump(-s);
	return 0;
}

/*
 * Class Response
 */

Response::Response(mg_connection *nc) : m_nc(nc), m_sbuf(nc), m_os(&m_sbuf) {}
void Response::header(int code, const Headers &headers)
{
	// Ensure the header is only sent once
	if (m_header_sent) {
		throw std::runtime_error("HTTP header already sent!");
	}
	m_header_sent = true;

	// Assemble the extra headers
	bool first = true;
	std::stringstream extra_headers;
	for (const auto &header : headers) {
		if (!first) {
			extra_headers << "\r\n";
		}
		extra_headers << header.first << ": " << header.second;
		first = false;
	}

	// Send the header
	mg_send_head(m_nc, code, -1, extra_headers.str().c_str());
}

std::ostream &Response::stream()
{
	if (!m_header_sent) {
		throw std::runtime_error(
		    "HTTP header must be sent before sending payload!");
	}
	return m_os;
}

void Response::ok(int code, const std::string &msg)
{
	header(code, {{"Content-type", "application/json"}});
	stream() << std::setw(4) << json{{"status", "ok"}, {"msg", msg}}
	         << std::endl;
}

void Response::error(int code, const std::string &msg)
{
	header(code, {{"Content-type", "application/json"}});
	stream() << std::setw(4) << json{{"status", "error"}, {"msg", msg}}
	         << std::endl;
}

Response::~Response()
{
	if (m_header_sent) {
		m_os << std::flush;
		mg_send_http_chunk(m_nc, nullptr, 0);
	}
}

/*
 * Class HTTPServerImpl
 */

class HTTPServerImpl {
private:
	std::vector<RequestMapEntry> m_request_map;
	mg_mgr m_mgr;
	mg_connection *m_nc;

	static std::unordered_map<std::string, std::string> parse_query(
	    const char *, size_t)
	{
		// TODO
		return std::unordered_map<std::string, std::string>{};
	}

	static void event_handler(mg_connection *nc, int ev, void *ev_data)
	{
		HTTPServerImpl &self = *((HTTPServerImpl *)(nc->mgr->user_data));
		const http_message *hm = (http_message *)(ev_data);

		// Only handle HTTP requests
		if (ev != MG_EV_HTTP_REQUEST) {
			return;
		}

		// Iterate over the request map to find a suitable handler
		const std::string method(hm->method.p, hm->method.len);
		const std::string uri(hm->uri.p, hm->uri.len);

		std::cerr << method << " " << uri << std::endl;

		for (const RequestMapEntry &descr : self.m_request_map) {
			std::smatch sm;
			if (descr.method == method &&
			    std::regex_match(uri, sm, descr.regex)) {
				Request req{
				    descr, uri, std::string{hm->body.p, hm->body.len},
				    parse_query(hm->query_string.p, hm->query_string.len), sm};
				Response res(nc);
				try {
					descr.handler(req, res);
				}
				catch (std::exception e) {
					std::cerr
					    << "Caught exception in event_handler: " << e.what()
					    << std::endl;
					Response(nc).error(500, "Internal server error");
				}
				return;
			}
		}

		// Send a default error response
		Response(nc).error(404, "Requested resource \"" + uri +
		                            "\" not found for method " + method);
	}

public:
	HTTPServerImpl(const std::vector<RequestMapEntry> &request_map,
	               const std::string &host, size_t port)
	    : m_request_map(request_map)
	{
		const std::string addr = host + ":" + std::to_string(port);

		mg_mgr_init(&m_mgr, this);
		m_nc = mg_bind(&m_mgr, addr.c_str(), event_handler);
		if (m_nc) {
			mg_set_protocol_http_websocket(m_nc);
			global_logger().info(
			    "server", "Serving HTTP at " + addr + ", press CTRL+C to exit");
		}
		else {
			global_logger().fatal_error("server",
			                            "Error, cannot bind to " + addr);
			exit(1);
		}
	}

	~HTTPServerImpl() { mg_mgr_free(&m_mgr); }
	void poll(size_t timeout) { mg_mgr_poll(&m_mgr, timeout); }
};

/*
 * Class HTTPServer
 */

HTTPServer::HTTPServer(const std::vector<RequestMapEntry> &request_map,
                       const std::string &host, size_t port)
    : m_impl(std::make_unique<HTTPServerImpl>(request_map, host, port))
{
}

HTTPServer::~HTTPServer()
{
	// Implicitly call the m_impl destructor
}

void HTTPServer::poll(size_t timeout) { m_impl->poll(timeout); }
}

