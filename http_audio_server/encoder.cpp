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

#include <opus/opus.h>

#include <mkvmuxer/mkvmuxer.h>

#include <http_audio_server/encoder.hpp>

namespace http_audio_server {

using namespace mkvmuxer;

class BufferMkvWriter : public IMkvWriter {
private:
	std::vector<uint8_t> m_buf;
	size_t m_bytes_written = 0;

public:
	int32 Write(const void *buf, uint32 len) override
	{
		m_buf.insert(m_buf.end(), (uint8_t *)buf, (uint8_t *)buf + len);
		m_bytes_written += len;
		return 0;
	}

	int64 Position() const override { return m_bytes_written; }
	int32 Position(int64) override { return -1; }
	bool Seekable() const override { return false; }
	void ElementStartNotify(uint64, int64) override {}
	BufferMkvWriter() {}
	~BufferMkvWriter() override{};

	void dump(std::ostream &os)
	{
		os.write((char *)&m_buf[0], m_buf.size());
		m_buf.clear();
	}
};

class EncoderImpl {
private:
	static constexpr size_t BUF_SIZE = 1 << 16;

	size_t m_rate;
	size_t m_n_channels;
	size_t m_frame_size;
	std::vector<float> m_buf;
	size_t m_buf_ptr;
	size_t m_granule = 0;
	bool m_done = false;

	BufferMkvWriter m_mkv_writer;
	Segment m_mkv_segment;
	uint64_t m_mkv_track_id;
	AudioTrack *m_mkv_audio_track;

	int m_enc_error;
	OpusEncoder *m_enc = nullptr;

#pragma pack(push)
#pragma pack(1)
	struct OpusMkvCodecPrivate {
		uint8_t head[8] = {0x4f, 0x70, 0x75, 0x73, 0x48, 0x65, 0x61, 0x64};
		uint8_t version = 1;
		uint8_t channel_count;
		uint16_t pre_skip = 0;
		uint32_t sample_rate;
		uint16_t gain = 0;
		uint8_t mapping_family = 0;

		OpusMkvCodecPrivate(uint8_t channel_count, uint32_t sample_rate)
		    : channel_count(channel_count), sample_rate(sample_rate)
		{
		}
	};
#pragma pack(pop)

public:
	EncoderImpl(size_t rate, size_t n_channels)
	    : m_rate(rate),
	      m_n_channels(n_channels),
	      m_frame_size(rate / 25),
	      m_buf(m_frame_size * m_n_channels),
	      m_buf_ptr(0)
	{
		// Initialize the mkv segment
		m_mkv_segment.Init(&m_mkv_writer);
		m_mkv_segment.set_mode(Segment::kLive);

		// Add a single audio track
		m_mkv_track_id = m_mkv_segment.AddAudioTrack(rate, n_channels, 0);
		m_mkv_audio_track = static_cast<AudioTrack *>(
		    m_mkv_segment.GetTrackByNumber(m_mkv_track_id));
		m_mkv_audio_track->set_codec_id(Tracks::kOpusCodecId);
		m_mkv_audio_track->set_bit_depth(16);

		// Write the Opus private data
		OpusMkvCodecPrivate private_data(n_channels, rate);
		m_mkv_audio_track->SetCodecPrivate((uint8_t *)&private_data,
		                                   sizeof(OpusMkvCodecPrivate));

		// Initialize the encoder
		m_enc = opus_encoder_create(rate, n_channels, OPUS_APPLICATION_AUDIO,
		                            &m_enc_error);
	}

	void encode(float *pcm, size_t n_samples, size_t bitrate, std::ostream &os,
	            bool flush)
	{
		// Do nothing if we're already done!
		if (m_done) {
			return;
		}

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
			}
			else {
				// Advance the buffer pointer and reduce the number of samples
				// which still have to be processed
				m_buf_ptr += n_floats_in;
				pcm += n_floats_in;
				n_samples -= n_floats_in / m_n_channels;
			}

			// If enough data for a frame has been gathered encode a frame and
			// write it into the mkv/webm stream
			if (m_buf_ptr == m_buf.size()) {
				opus_encoder_ctl(m_enc, OPUS_SET_BITRATE(bitrate));
				int size = opus_encode_float(m_enc, &m_buf[0], m_frame_size,
				                             buf, BUF_SIZE);
				if (size > 0) {
					uint64_t ts = (m_granule * 1000 * 1000 * 1000) / m_rate;
					m_mkv_segment.AddFrame(buf, size, m_mkv_track_id, ts, true);
				}
				m_buf_ptr = 0;
				m_granule += m_frame_size;
			}
		} while (n_samples > 0);

		// If the stream was flushed, reset the state
		if (flush) {
			m_mkv_segment.Finalize();
			m_granule = 0;
			m_done = true;
		}

		// Dump all buffered data into the given output stream
		m_mkv_writer.dump(os);
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
