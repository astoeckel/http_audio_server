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

#include <iostream>
#include <vector>

#include <ogg/ogg.h>
#include <opus/opus.h>

#include <http_audio_server/encoder.hpp>

namespace http_audio_server {

class EncoderImpl {
private:
	static constexpr size_t BUF_SIZE = 1 << 16;

	size_t m_rate;
	size_t m_n_channels;
	size_t m_frame_size;
	std::vector<float> m_buf;
	size_t m_buf_ptr;

	ogg_stream_state m_ogg_stream;
	size_t m_packet_no = 0;
	size_t m_granule = 0;

	int m_enc_error;
	OpusEncoder *m_enc = nullptr;

#pragma pack(push)
#pragma pack(1)
	struct OpusOggHead {
		uint8_t head[8] = {0x4f, 0x70, 0x75, 0x73, 0x48, 0x65, 0x61, 0x64};
		uint8_t version = 1;
		uint8_t channel_count;
		uint16_t pre_skip = 0;
		uint32_t sample_rate;
		uint16_t gain = 0;
		uint8_t mapping_family = 0;

		OpusOggHead(uint8_t channel_count, uint32_t sample_rate)
		    : channel_count(channel_count), sample_rate(sample_rate)
		{
		}
	};

	struct OpusOggTags {
		uint8_t head[8] = {0x4f, 0x70, 0x75, 0x73, 0x54, 0x61, 0x67, 0x73};
		uint32_t vendor_string_length = 0;
		uint32_t user_comment_list_length = 0;
	};
#pragma pack(pop)

	void write_page(std::ostream &os, bool flush = false)
	{
		bool has_page = false;
		ogg_page pg;
		if (flush) {
			has_page = ogg_stream_flush(&m_ogg_stream, &pg) != 0;
		}
		else {
			has_page = ogg_stream_pageout(&m_ogg_stream, &pg) != 0;
		}
		if (has_page) {
			os.write((char *)(pg.header), pg.header_len);
			os.write((char *)(pg.body), pg.body_len);
		}
	}

	void write_packet(std::ostream &os, const ogg_packet &pck,
	                  bool flush = false)
	{
		ogg_stream_packetin(&m_ogg_stream, const_cast<ogg_packet *>(&pck));
		write_page(os, flush);
	}

	void write_header(std::ostream &os) {
		// Write the first header packet
		{
			OpusOggHead head(m_n_channels, m_rate);
			write_packet(os, ogg_packet{(uint8_t *)(&head), sizeof(head), 1,
			                            0, 0, int64_t(m_packet_no++)},
			             true);
		}

		// Write the metadata packet
		{
			OpusOggTags tags;
			write_packet(os, ogg_packet{(uint8_t *)(&tags), sizeof(tags), 0,
			                            0, 0, int64_t(m_packet_no++)},
			             true);
		}
	}

public:
	EncoderImpl(size_t rate, size_t n_channels)
	    : m_rate(rate),
	      m_n_channels(n_channels),
	      m_frame_size(rate / 25),
	      m_buf(m_frame_size * m_n_channels),
	      m_buf_ptr(0)
	{
		// Create the ogg stream
		ogg_stream_init(&m_ogg_stream, 0);

		// Initialize the encoder
		m_enc = opus_encoder_create(rate, n_channels, OPUS_APPLICATION_AUDIO,
		                            &m_enc_error);
	}

	~EncoderImpl()
	{
		// Destroy the opus encoder instance
		if (m_enc != nullptr) {
			opus_encoder_destroy(m_enc);
			m_enc = nullptr;
		}

		// Destroy the ogg stream
		ogg_stream_clear(&m_ogg_stream);
	}

	void encode(float *pcm, size_t n_samples, size_t bitrate, std::ostream &os,
	            bool flush)
	{
		// Write the header packets if this has not been done yet
		if (m_packet_no == 0) {
			write_header(os);
		}

		// Advance the granule
		m_granule += n_samples;

		// Encode single packets
		uint8_t buf[BUF_SIZE];
		do {
			// Copy the input data to the internal buffer
			size_t n_floats_in =
			    std::min(m_buf.size() - m_buf_ptr, n_samples * m_n_channels);
			std::copy(pcm, pcm + n_floats_in, m_buf.begin() + m_buf_ptr);

			// If we're at the end of the stream, fill the remaining buffer with
			// zeros
			const bool at_end = flush && (n_samples <= m_frame_size);
			if (at_end) {
				std::fill(m_buf.begin() + m_buf_ptr, m_buf.end(), 0.0);
				n_samples = 0;
				m_buf_ptr = m_buf.size();
			} else {
				// Advance the buffer pointer and reduce the number of samples
				// which still have to be processed
				m_buf_ptr += n_floats_in;
				pcm += n_floats_in;
				n_samples -= n_floats_in / m_n_channels;
			}

			// If enough data for a frame has been gathered encode a frame and
			// write it into the ogg stream-
			if (m_buf_ptr == m_buf.size()) {
				opus_encoder_ctl(m_enc, OPUS_SET_BITRATE(bitrate));
				int size = opus_encode_float(m_enc, &m_buf[0], m_frame_size,
				                             buf, BUF_SIZE);
				if (size > 0) {
					write_packet(os,
					             ogg_packet{(uint8_t *)(&buf[0]), size, 0,
					                        at_end ? 1 : 0, int64_t(m_granule),
					                        int64_t(m_packet_no++)}, at_end);
				}
				m_buf_ptr = 0;
			}
		} while (n_samples > 0);

		// If the stream was flushed, reset the state
		if (flush) {
			ogg_stream_reset(&m_ogg_stream);
			m_packet_no = 0;
			m_granule = 0;
		}
	}
};

Encoder::Encoder(size_t rate, size_t n_channels)
    : m_impl(std::make_unique<EncoderImpl>(rate, n_channels))
{
}

Encoder::~Encoder()
{
	// Make sure the unique_ptr<EncoderImpl> destructor can be called
}

void Encoder::feed(float *pcm, size_t n_samples, size_t bitrate,
                   std::ostream &os)
{
	m_impl->encode(pcm, n_samples, bitrate, os, false);
}

void Encoder::finalize(size_t bitrate, std::ostream &os)
{
	m_impl->encode(nullptr, 0, bitrate, os, true);
}
}
