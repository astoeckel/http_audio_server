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

#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <list>
#include <random>
#include <sstream>
#include <thread>

#include <http_audio_server/decoder.hpp>
#include <http_audio_server/encoder.hpp>
#include <http_audio_server/json.hpp>
#include <http_audio_server/logger.hpp>
#include <http_audio_server/metadata.hpp>
#include <http_audio_server/process.hpp>
#include <http_audio_server/server.hpp>
#include <http_audio_server/string_utils.hpp>

using namespace http_audio_server;

bool cancel = false;
void signal_handler(int)
{
	if (cancel) {
		exit(1);
	}
	cancel = true;
}

class Stream {
private:
	std::list<std::tuple<std::string, double, std::shared_ptr<Decoder>>>
	    m_decoders;
	Encoder m_encoder;
	size_t m_bytes_tranferred = 0;
	size_t m_n_samples = 0;
	std::vector<uint8_t> m_buf;
	size_t m_bitrate;

	static std::ostringstream::pos_type size_of_stream(
	    const std::ostringstream &ss)
	{
		std::streambuf *buf = ss.rdbuf();
		std::ostringstream::pos_type original =
		    buf->pubseekoff(0, ss.cur, ss.out);
		std::ostringstream::pos_type end = buf->pubseekoff(0, ss.end, ss.out);
		buf->pubseekpos(original, ss.out);
		return end;
	}

public:
	Stream(size_t m_bitrate) : m_encoder(48000, 2), m_bitrate(m_bitrate) {}
	void append(const std::string &filename, double offs = 0.0)
	{
		m_decoders.emplace_back(filename, offs, nullptr);
	}

	void advance(double seconds, std::ostream &os)
	{
		std::vector<json> metadata;
		size_t samples = seconds * 48000;
		size_t n_bytes = samples * 2 * sizeof(float);

		std::ostringstream os_buf_data;
		while (n_bytes > 0 && !m_decoders.empty()) {
			// Lazily create the decoder if it does not exist yet
			auto &entry = m_decoders.front();
			const std::string &fn = std::get<0>(entry);
			const double offs = std::get<1>(entry);
			std::shared_ptr<Decoder> &dec = std::get<2>(entry);
			if (!dec) {
				dec = std::make_shared<Decoder>(fn, offs);
				metadata.emplace_back(json{
				    {"start", double(m_n_samples) / 48000.0},
				    {"filename", fn},
				    {"meta", metadata_from_file(fn).to_json()},
				});
			}

			// Read the data
			if (dec->read(n_bytes, m_buf)) {
				const size_t n_samples_read = m_buf.size() / sizeof(float) / 2;
				n_bytes -= m_buf.size();
				m_encoder.feed((float *)(&m_buf[0]), n_samples_read, m_bitrate,
				               os_buf_data);
				m_n_samples += n_samples_read;
			}

			// Remove the current decoder if we're at the end of the file
			if (m_buf.size() < n_bytes) {
				m_decoders.pop_front();
			}
			m_buf.clear();
		}
		// Finalise the encoder if this stream is done
		if (m_decoders.empty()) {
			m_encoder.finalize(m_bitrate, os_buf_data);
		}

		// Dump the metadata segment and its size
		const std::string smeta = json(metadata).dump();
		const uint32_t smeta_size = smeta.size();
		os << "meta";
		os.write((char *)&smeta_size, sizeof(smeta_size));
		os << smeta;

		// Dump the data segment and its size
		const uint32_t data_size = size_of_stream(os_buf_data);
		os << "data";
		os.write((char *)&data_size, sizeof(data_size));
		os << os_buf_data.str();
	}
};

int main()
{
	signal(SIGINT, signal_handler);

	// Make sure ffmpeg and ffprobe are found
	if (std::get<0>(Process::exec("ffmpeg", {"-version"})) != 0) {
		global_logger().fatal_error(
		    "main",
		    "ffmpeg binary not found. Please make sure a reasonably recent "
		    "version of ffmpeg is installed.");
		return 1;
	}
	if (std::get<0>(Process::exec("ffprobe", {"-version"})) != 0) {
		global_logger().fatal_error(
		    "main",
		    "ffprobe binary not found. Please make sure a reasonably recent "
		    "version of ffmpeg is installed.");
		return 1;
	}

	std::unordered_map<std::string, std::shared_ptr<Stream>> streams;

	auto handle_index = [](const Request &, Response &res) {
		res.header(200, {{"Content-Type", "text/html; charset=utf-8"}});
		std::ifstream is("../static/index.html");
		Process::generic_pipe(is, res.stream());
	};

	auto handle_stream_create = [&](const Request &, Response &res) {
		std::string stream_id = random_alphanum_string();
		streams.emplace(stream_id, std::make_shared<Stream>(196000));
		res.header(200, {{"Content-Type", "text/plain"}});
		res.stream() << stream_id << std::endl;
	};

	auto handle_stream_append = [&](const Request &req, Response &res) {
		const std::string stream_id = req.matcher[1];
		auto it = streams.find(stream_id);
		if (it != streams.end()) {
			json resource = json::parse(req.body);
			auto fn = resource.find("filename");
			if (fn == resource.end()) {
				res.error(400, "Invalid query");
				return;
			}
			res.ok(200, "Appended file " + fn->get<std::string>());
			it->second->append(*fn);
		}
		else {
			res.error(404, "Stream id \"" + stream_id + "\" not found");
		}
	};

	auto handle_stream_advance = [&](const Request &req, Response &res) {
		const std::string stream_id = req.matcher[1];
		auto it = streams.find(stream_id);
		if (it != streams.end()) {
			res.header(200, {{"Content-Type", "audio/webm"}});
			it->second->advance(5.0, res.stream());
		}
		else {
			res.error(404, "Stream id \"" + stream_id + "\" not found");
		}
	};

	auto handle_stream_destroy = [&](const Request &req, Response &res) {
		const std::string stream_id = req.matcher[1];
		auto it = streams.find(stream_id);
		if (it != streams.end()) {
			res.ok(200, {"Stream successfully erased"});
			streams.erase(it);
		}
		else {
			res.error(404, "Stream id \"" + stream_id + "\" not found");
		}
	};

	HTTPServer server(
	    {RequestMapEntry("GET", "^/(index.html)?$", handle_index),
	     RequestMapEntry("POST", "^/stream/create$", handle_stream_create),
	     RequestMapEntry("POST", "^/stream/([A-Za-z0-9]+)/append$",
	                     handle_stream_append),
	     RequestMapEntry("POST", "^/stream/([A-Za-z0-9]+)/advance$",
	                     handle_stream_advance),
	     RequestMapEntry("POST", "^/stream/([A-Za-z0-9]+)/destroy$",
	                     handle_stream_destroy)});

	while (!cancel) {
		server.poll(1000);
	}

	return 0;
}
