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

#include <map>
#include <regex>
#include <string>

#include <http_audio_server/metadata.hpp>
#include <http_audio_server/process.hpp>

namespace http_audio_server {

json Metadata::to_json() const
{
	json res;
	res["title"] = title;
	res["album"] = album;
	res["artist"] = artist;
	res["date"] = date;
	res["track_number"] = track_number;
	res["track_total"] = track_total;
	res["disc_number"] = disc_number;
	res["disc_total"] = disc_total;
	res["duration"] = duration;
	res["format"] = format;
	return res;
}

static void flaten(const json &o, std::map<std::string, json> &tar,
                   const std::string &prefix = "")
{
	for (auto it = o.begin(); it != o.end(); ++it) {
		std::string tar_key =
		    prefix.empty() ? it.key() : prefix + "::" + it.key();
		if (it.value().is_object()) {
			flaten(it.value(), tar, tar_key);
		}
		else {
			tar.emplace(tar_key, it.value());
		}
	}
}

static json get_json(const std::regex &regex, std::map<std::string, json> &data)
{
	for (auto it = data.begin(); it != data.end(); it++) {
		std::smatch sm;
		if (std::regex_match(it->first, sm, regex)) {
			json res = it->second;
			data.erase(it);
			return res;
		}
	}
	return nullptr;
}

template <typename T>
static T get_number(const std::regex &regex, std::map<std::string, json> &data,
                    T default_value = T())
{
	json j = get_json(regex, data);
	if (j != nullptr) {
		if (j.is_number()) {
			return j.get<int>();
		}
		else if (j.is_string()) {
			return std::stold(j.get<std::string>());
		}
	}
	return default_value;
}

static std::string get_string(const std::regex &regex,
                              std::map<std::string, json> &data,
                              const std::string &default_value = std::string())
{
	json j = get_json(regex, data);
	if (j != nullptr) {
		if (j.is_string()) {
			return j;
		}
		return j.dump();
	}
	return default_value;
}

Metadata metadata_from_file(const std::string &filename)
{
	using namespace std::regex_constants;

	Metadata res;

	auto pres = Process::exec(
	    "ffprobe", {"-show_format", "-print_format", "json", filename});
	if (std::get<0>(pres) == 0) {
		// Fetch the data
		std::map<std::string, json> data;
		flaten(json::parse(std::get<1>(pres)), data);

		res.title = get_string(std::regex("format::tags::title", icase), data);
		res.album = get_string(std::regex("format::tags::album", icase), data);
		res.artist =
		    get_string(std::regex("format::tags::artist", icase), data);
		res.date = get_string(std::regex("format::tags::date", icase), data);
		res.format = get_string(std::regex("format::format_name", icase), data);
		res.track_number =
		    get_number(std::regex("format::tags::track", icase), data, -1);
		res.track_total = get_number(
		    std::regex("format::tags::track_total", icase), data, -1);
		res.disc_number =
		    get_number(std::regex("format::tags::disc", icase), data, -1);
		res.disc_total =
		    get_number(std::regex("format::tags::disc_total", icase), data, -1);
		res.duration =
		    get_number(std::regex("format::duration", icase), data, -1.0);
	}

	return res;
}
}
