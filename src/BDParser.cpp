#include <fstream>
#include <filesystem>

#include "BDParser.hpp"

namespace parser {
	[[nodiscard]] static uint16_t swap_uint16(uint16_t value) noexcept
	{
		return (value << 8) | (value >> 8);
	}

	[[nodiscard]] static uint32_t swap_uint32(uint32_t value) noexcept
	{
		value = ((value << 8) & 0xFF00FF00) | ((value >> 8) & 0xFF00FF);
		return (value << 16) | (value >> 16);
	}

	class IReader {
	public:
		IReader() = default;
		IReader(IReader&&) = delete;
		IReader(const IReader&) = delete;
		IReader& operator=(IReader&&) = delete;
		IReader& operator=(const IReader&) = delete;
		virtual ~IReader() = default;

		[[nodiscard]] virtual bool open(const std::string& path) = 0;
		virtual void read_buffer(char* buffer, std::size_t size, std::error_code& ec) = 0;
		[[nodiscard]] virtual uint32_t read_uint32(std::error_code& ec) = 0;
		[[nodiscard]] virtual uint16_t read_uint16(std::error_code& ec) = 0;
		[[nodiscard]] virtual uint8_t read_uint8(std::error_code& ec) = 0;
		virtual void skip(std::size_t size, std::error_code& ec) = 0;
		virtual void seek(std::size_t pos, std::error_code& ec) = 0;
		[[nodiscard]] virtual size_t position(std::error_code& ec) = 0;
	};

	class StreamReader final : public IReader
	{
		std::ifstream stream_;

	public:
		explicit StreamReader(const std::string& path, std::error_code& ec) {
			if (!open(path)) {
				ec = std::make_error_code(std::io_errc::stream);
			}
		}

		[[nodiscard]] bool open(const std::string& path) override {
			stream_.open(path, std::ios::in | std::ios::binary);
			return stream_.is_open();
		}

		void read_buffer(char* buffer, std::size_t size, std::error_code& ec) noexcept override {
			stream_.read(buffer, size);
			if (stream_.fail()) {
				ec = std::make_error_code(std::io_errc::stream);
			}
		}

		[[nodiscard]] uint32_t read_uint32(std::error_code& ec) noexcept override {
			uint32_t value = {};
			read_buffer(reinterpret_cast<char*>(&value), sizeof(value), ec);
			if (ec) {
				return {};
			}

			return swap_uint32(value);
		}

		[[nodiscard]] uint16_t read_uint16(std::error_code& ec) noexcept override {
			uint16_t value = {};
			read_buffer(reinterpret_cast<char*>(&value), sizeof(value), ec);
			if (ec) {
				return {};
			}

			return swap_uint16(value);
		}

		[[nodiscard]] uint8_t read_uint8(std::error_code& ec) noexcept override {
			uint8_t value = {};
			read_buffer(reinterpret_cast<char*>(&value), sizeof(value), ec);
			return value;
		}

		void skip(std::size_t size, std::error_code& ec) noexcept override {
			stream_.seekg(size, std::ios_base::cur);
			if (stream_.fail()) {
				ec = std::make_error_code(std::io_errc::stream);
			}
		}

		void seek(std::size_t pos, std::error_code& ec) noexcept override {
			stream_.seekg(pos);
			if (stream_.fail()) {
				ec = std::make_error_code(std::io_errc::stream);
			}
		}

		size_t position(std::error_code& ec) noexcept override {
			auto position = stream_.tellg();
			if (stream_.fail()) {
				ec = std::make_error_code(std::io_errc::stream);
			}

			return position;
		}
	};

	static void read_lang_code(IReader& reader, BDParser::stream_t& s, std::error_code& ec)
	{
		s.lang_code.resize(3);
		reader.read_buffer(s.lang_code.data(), 3, ec);
	}

	[[nodiscard]] static bool read_stream_info(IReader& reader, std::vector<BDParser::stream_t>& streams)
	{
		std::error_code ec = {};
		auto size = reader.read_uint8(ec);
		auto pos = reader.position(ec);

		auto stream_type = reader.read_uint8(ec);
		if (ec) {
			return false;
		}

		BDParser::stream_t s;

		switch (stream_type) {
			case 1:
				s.pid = reader.read_uint16(ec);
				break;
			case 2:
			case 4:
				reader.skip(2, ec);
				s.pid = reader.read_uint16(ec);
				break;
			case 3:
				reader.skip(1, ec);
				s.pid = reader.read_uint16(ec);
				break;
			default:
				return false;
		}

		reader.seek(pos + size, ec);
		size = reader.read_uint8(ec);
		pos = reader.position(ec);
		if (ec) {
			return false;
		}

		auto it = std::find_if(streams.begin(), streams.end(), [pid = s.pid](const auto& s) {
			return s.pid == pid;
		});
		if (it != streams.end()) {
			reader.seek(pos + size, ec);
			return true;
		}

		s.type = static_cast<decltype(s.type)>(reader.read_uint8(ec));
		if (ec) {
			return false;
		}

		switch (s.type) {
			case StreamType::MPEG1_VIDEO:
			case StreamType::MPEG2_VIDEO:
			case StreamType::H264_VIDEO:
			case StreamType::H264_MVC_VIDEO:
			case StreamType::HEVC_VIDEO:
			case StreamType::VC1_VIDEO:
				{
					auto value = reader.read_uint8(ec);
					s.video_format = static_cast<decltype(s.video_format)>(value >> 4);
					s.frame_rate = static_cast<decltype(s.frame_rate)>(value & 0xf);
				}
				break;
			case StreamType::MPEG1_AUDIO:
			case StreamType::MPEG2_AUDIO:
			case StreamType::LPCM_AUDIO:
			case StreamType::AC3_AUDIO:
			case StreamType::DTS_AUDIO:
			case StreamType::AC3_TRUE_HD_AUDIO:
			case StreamType::AC3_PLUS_AUDIO:
			case StreamType::DTS_HD_AUDIO:
			case StreamType::DTS_HD_MASTER_AUDIO:
			case StreamType::AC3_PLUS_SECONDARY_AUDIO:
			case StreamType::DTS_HD_SECONDARY_AUDIO:
				{
					auto value = reader.read_uint8(ec);
					s.channel_layout = static_cast<decltype(s.channel_layout)>(value >> 4);
					s.sample_rate = static_cast<decltype(s.sample_rate)>(value & 0xf);
					read_lang_code(reader, s, ec);
				}
				break;
			case StreamType::PRESENTATION_GRAPHICS:
			case StreamType::INTERACTIVE_GRAPHICS:
				read_lang_code(reader, s, ec);
				break;
			case StreamType::SUBTITLE:
				reader.skip(1, ec);
				read_lang_code(reader, s, ec);
				break;
			default:
				break;
		}

		if (ec) {
			return false;
		}

		streams.emplace_back(s);

		reader.seek(pos + size, ec);

		return true;
	}

	[[nodiscard]] static bool read_stn_info(IReader& reader, std::vector<BDParser::stream_t>& streams)
	{
		std::error_code ec = {};
		reader.skip(4, ec);

		auto num_video = reader.read_uint8(ec);
		auto num_audio = reader.read_uint8(ec);
		auto num_pg = reader.read_uint8(ec);
		auto num_ig = reader.read_uint8(ec);
		auto num_secondary_audio = reader.read_uint8(ec);
		auto num_secondary_video = reader.read_uint8(ec);
		auto num_pip_pg = reader.read_uint8(ec);

		if (ec) {
			return false;
		}

		reader.skip(5, ec);

		if (streams.empty()) {
			streams.reserve(static_cast<size_t>(num_video) + num_audio + num_pg + num_ig +
							num_secondary_audio + num_secondary_video + num_pip_pg);
		}

		for (uint8_t i = 0; i < num_video; i++) {
			if (!(read_stream_info(reader, streams))) {
				return false;
			}
		}

		for (uint8_t i = 0; i < num_audio; i++) {
			if (!(read_stream_info(reader, streams))) {
				return false;
			}
		}

		for (uint8_t i = 0; i < (num_pg + num_pip_pg); i++) {
			if (!(read_stream_info(reader, streams))) {
				return false;
			}
		}

		for (uint8_t i = 0; i < num_ig; i++) {
			if (!(read_stream_info(reader, streams))) {
				return false;
			}
		}

		for (uint8_t i = 0; i < num_secondary_audio; i++) {
			if (!(read_stream_info(reader, streams))) {
				return false;
			}

			// Secondary Audio Extra Attributes
			const auto num_secondary_audio_extra = reader.read_uint8(ec);
			reader.skip(1, ec);
			if (num_secondary_audio_extra) {
				reader.skip(num_secondary_audio_extra, ec);
				if (num_secondary_audio_extra % 2) {
					reader.skip(1, ec);
				}
			}

			if (ec) {
				return false;
			}
		}

		for (uint8_t i = 0; i < num_secondary_video; i++) {
			if (!(read_stream_info(reader, streams))) {
				return false;
			}

			// Secondary Video Extra Attributes
			const auto num_secondary_video_extra = reader.read_uint8(ec);
			reader.skip(1, ec);
			if (num_secondary_video_extra) {
				reader.skip(num_secondary_video_extra, ec);
				if (num_secondary_video_extra % 2) {
					reader.skip(1, ec);
				}
			}

			const auto num_pip_pg_extra = reader.read_uint8(ec);
			reader.skip(1, ec);
			if (num_pip_pg_extra) {
				reader.skip(num_pip_pg_extra, ec);
				if (num_pip_pg_extra % 2) {
					reader.skip(1, ec);
				}
			}

			if (ec) {
				return false;
			}
		}

		return true;
	}

	class BitReader {
		const uint8_t* m_data;
		size_t m_size;
		size_t m_bitPos = 0;

	public:
		BitReader(const uint8_t* data, size_t size) : m_data(data), m_size(size) {}

		uint32_t read_bits(uint8_t bits) {
			uint32_t result = 0;
			while (bits--) {
				result <<= 1;
				if (m_bitPos >= m_size * 8) break;
				result |= (m_data[m_bitPos / 8] >> (7 - (m_bitPos % 8))) & 1;
				m_bitPos++;
			}
			return result;
		}

		uint32_t read_uint32() {
			uint32_t result = 0;
			for (int i = 0; i < 4; i++) {
				result <<= 8;
				if (m_bitPos / 8 < m_size) {
					result |= m_data[m_bitPos / 8];
					m_bitPos += 8;
				}
			}
			return result;
		}
	};

	static bool read_cpi_info(IReader& reader, BDParser::sync_points& sync_points_by_pid)
	{
		std::error_code ec = {};

		struct ClpiEpCoarse {
			uint32_t ref_ep_fine_id;
			uint16_t pts_ep;
			uint32_t spn_ep;
		};

		struct ClpiEpFine {
			uint8_t is_angle_change_point;
			uint8_t i_end_position_offset;
			uint16_t pts_ep;
			uint32_t spn_ep;
		};

		struct ClpiEpMapEntry {
			uint16_t pid;
			uint8_t ep_stream_type;
			uint16_t num_ep_coarse;
			uint32_t num_ep_fine;
			uint32_t ep_map_stream_start_addr;
			std::vector<ClpiEpCoarse> coarse;
			std::vector<ClpiEpFine> fine;
		};

		auto cpi_start_address = reader.position(ec);
		if (ec) return false;

		auto len = reader.read_uint32(ec);
		if (ec || len == 0) return false;

		reader.skip(1, ec);
		auto type = reader.read_uint8(ec) & 0xF;
		auto ep_map_pos = cpi_start_address + 4 + 2;
		reader.skip(1, ec);
		auto num_stream_pid = reader.read_uint8(ec);

		std::vector<uint8_t> buf(num_stream_pid * 12);
		reader.read_buffer(reinterpret_cast<char*>(buf.data()), buf.size(), ec);
		if (ec) return false;

		BitReader br(buf.data(), buf.size());
		std::vector<ClpiEpMapEntry> clpi_ep_map_list;
		clpi_ep_map_list.reserve(num_stream_pid);

		for (uint8_t i = 0; i < num_stream_pid; i++) {
			ClpiEpMapEntry em;
			em.pid = static_cast<uint16_t>(br.read_bits(16));
			br.read_bits(10);
			em.ep_stream_type = static_cast<uint8_t>(br.read_bits(4));
			em.num_ep_coarse = static_cast<uint16_t>(br.read_bits(16));
			em.num_ep_fine = br.read_bits(18);
			em.ep_map_stream_start_addr = br.read_uint32() + ep_map_pos;

			em.coarse.resize(em.num_ep_coarse);
			em.fine.resize(em.num_ep_fine);
			clpi_ep_map_list.emplace_back(std::move(em));
		}

		for (auto& em : clpi_ep_map_list) {
			reader.seek(em.ep_map_stream_start_addr, ec);
			if (ec) return false;

			auto fine_start = reader.read_uint32(ec);

			std::vector<uint8_t> coarse_buf(em.num_ep_coarse * 8);
			reader.read_buffer(reinterpret_cast<char*>(coarse_buf.data()), coarse_buf.size(), ec);
			if (ec) return false;

			BitReader br_coarse(coarse_buf.data(), coarse_buf.size());
			for (uint16_t j = 0; j < em.num_ep_coarse; j++) {
				em.coarse[j].ref_ep_fine_id = br_coarse.read_bits(18);
				em.coarse[j].pts_ep = static_cast<uint16_t>(br_coarse.read_bits(14));
				em.coarse[j].spn_ep = br_coarse.read_uint32();
			}

			reader.seek(em.ep_map_stream_start_addr + fine_start, ec);
			if (ec) return false;

			std::vector<uint8_t> fine_buf(em.num_ep_fine * 4);
			reader.read_buffer(reinterpret_cast<char*>(fine_buf.data()), fine_buf.size(), ec);
			if (ec) return false;

			BitReader gb_fine(fine_buf.data(), fine_buf.size());
			for (uint32_t j = 0; j < em.num_ep_fine; j++) {
				em.fine[j].is_angle_change_point = static_cast<uint8_t>(gb_fine.read_bits(1));
				em.fine[j].i_end_position_offset = static_cast<uint8_t>(gb_fine.read_bits(3));
				em.fine[j].pts_ep = static_cast<uint16_t>(gb_fine.read_bits(11));
				em.fine[j].spn_ep = gb_fine.read_bits(17);
			}
		}

		if (!clpi_ep_map_list.empty()) {
			for (auto& entry : clpi_ep_map_list) {
				auto& sync_points = sync_points_by_pid[entry.pid];
				sync_points.reserve(entry.num_ep_fine);

				for (uint16_t coarse_index = 0; coarse_index < entry.num_ep_coarse; coarse_index++) {
					auto& coarse = entry.coarse[coarse_index];
					auto start = coarse.ref_ep_fine_id;
					auto end = (coarse_index < entry.num_ep_coarse - 1)
						? entry.coarse[coarse_index + 1].ref_ep_fine_id
						: entry.num_ep_fine;
					auto coarse_pts = static_cast<uint64_t>(coarse.pts_ep & ~0x01) << 18;

					for (uint32_t fine_index = start; fine_index < end; fine_index++) {
						auto pts = coarse_pts + (static_cast<uint64_t>(entry.fine[fine_index].pts_ep) << 8);
						auto offset = (static_cast<uint64_t>(coarse.spn_ep & ~0x1FFFF) + entry.fine[fine_index].spn_ep) * 192 + 4;

						auto reftime = pts * 2000 / 9;
						sync_points.emplace_back(reftime, offset);
					}
				}
			}
		}

		return true;
	}

	static bool read_program_info(IReader& reader, std::vector<BDParser::stream_t>& streams)
	{
		std::error_code ec = {};

		auto program_info_start = reader.position(ec);
		if (ec) return false;

		reader.skip(4, ec);  // length
		reader.skip(1, ec);  // reserved_for_word_align
		auto number_of_program_sequences = reader.read_uint8(ec);
		if (ec) return false;

		for (uint8_t i = 0; i < number_of_program_sequences; i++) {
			reader.skip(4, ec);  // SPN_program_sequence_start
			reader.skip(2, ec);  // program_map_PID
			auto number_of_streams_in_ps = reader.read_uint8(ec);
			reader.skip(1, ec);  // reserved_for_future_use
			if (ec) return false;

			for (uint8_t stream_index = 0; stream_index < number_of_streams_in_ps; stream_index++) {
				BDParser::stream_t s;
				s.pid = reader.read_uint16(ec);  // stream_PID

				auto pos = reader.position(ec);
				if (ec) return false;

				// StreamCodingInfo
				auto len = reader.read_uint8(ec);  // length
				s.type = static_cast<decltype(s.type)>(reader.read_uint8(ec));

				auto it = std::find_if(streams.begin(), streams.end(), [pid = s.pid](const auto& stream) {
					return stream.pid == pid;
				});

				if (it == streams.end()) {
					it = streams.insert(streams.end(), s);
				}

				switch (s.type) {
					case StreamType::MPEG1_VIDEO:
					case StreamType::MPEG2_VIDEO:
					case StreamType::H264_VIDEO:
					case StreamType::H264_MVC_VIDEO:
					case StreamType::HEVC_VIDEO:
					case StreamType::VC1_VIDEO: {
							auto temp = reader.read_uint8(ec);
							it->video_format = static_cast<decltype(it->video_format)>(temp >> 4);
							it->frame_rate = static_cast<decltype(it->frame_rate)>(temp & 0xf);
							temp = reader.read_uint8(ec);
							it->aspect_ratio = static_cast<decltype(it->aspect_ratio)>(temp >> 4);
							break;
						}
					case StreamType::MPEG1_AUDIO:
					case StreamType::MPEG2_AUDIO:
					case StreamType::LPCM_AUDIO:
					case StreamType::AC3_AUDIO:
					case StreamType::DTS_AUDIO:
					case StreamType::AC3_TRUE_HD_AUDIO:
					case StreamType::AC3_PLUS_AUDIO:
					case StreamType::DTS_HD_AUDIO:
					case StreamType::DTS_HD_MASTER_AUDIO:
					case StreamType::AC3_PLUS_SECONDARY_AUDIO:
					case StreamType::DTS_HD_SECONDARY_AUDIO: {
							auto temp = reader.read_uint8(ec);
							it->channel_layout = static_cast<decltype(it->channel_layout)>(temp >> 4);
							it->sample_rate = static_cast<decltype(it->sample_rate)>(temp & 0xf);

							if (it->lang_code.empty()) {
								it->lang_code.resize(3);
								reader.read_buffer(it->lang_code.data(), 3, ec);
							} else {
								reader.skip(3, ec);
							}
							break;
						}
					case StreamType::PRESENTATION_GRAPHICS:
					case StreamType::INTERACTIVE_GRAPHICS: {
							if (it->lang_code.empty()) {
								it->lang_code.resize(3);
								reader.read_buffer(it->lang_code.data(), 3, ec);
							} else {
								reader.skip(3, ec);
							}
							break;
						}
					case StreamType::SUBTITLE: {
							reader.skip(1, ec);  // bd_char_code
							if (it->lang_code.empty()) {
								it->lang_code.resize(3);
								reader.read_buffer(it->lang_code.data(), 3, ec);
							} else {
								reader.skip(3, ec);
							}
							break;
						}
					default:
						break;
				}

				reader.seek(pos + len + 1, ec);  // +1 for length byte
				if (ec) return false;
			}
		}

		return true;
	}

	bool BDParser::parse_playlist(const std::string& playlist_path, std::string_view root_path, bool skip_playlist_duplicate, bool check_m2ts_files) noexcept
	{
		std::error_code ec = {};

		StreamReader reader(playlist_path, ec);
		if (ec) {
			return false;
		}

		char buffer[9] = {};
		reader.read_buffer(buffer, 8, ec);
		if (ec || std::memcmp(buffer, "MPLS", 4)) {
			return false;
		}

		auto check_version = [](auto buffer) {
			return !(std::memcmp(buffer, "0300", 4)) || (!std::memcmp(buffer, "0200", 4)) || (!std::memcmp(buffer, "0100", 4));
		};

		if (!check_version(buffer + 4)) {
			return false;
		}

		auto playlist_start_address = reader.read_uint32(ec);
		if (ec) {
			return false;
		}

		reader.seek(playlist_start_address, ec);
		reader.skip(6, ec);
		auto number_of_playlist_items = reader.read_uint16(ec);
		if (ec) {
			return false;
		}

		playlist_t playlist;
		playlist.mpls_file_name = playlist_path;

		playlist_start_address += 10;
		for (uint16_t i = 0; i < number_of_playlist_items; i++) {
			reader.seek(playlist_start_address, ec);
			playlist_start_address += reader.read_uint16(ec) + 2;
			reader.read_buffer(buffer, 9, ec);
			if (ec || std::memcmp(&buffer[5], "M2TS", 4)) {
				return false;
			}

			playlist_item_t item;
			item.file_name = std::format("{}/STREAM/{}{}{}{}{}.M2TS", root_path,
										 buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);
			if (check_m2ts_files && !std::filesystem::exists(item.file_name, ec)) {
				return false;
			}
			if (std::find_if(playlist.items.begin(), playlist.items.end(), [&](const auto& _item) {
						return item.file_name == _item.file_name;
					}) != playlist.items.end()) {
				// Ignore playlists with duplicate files
				return false;
			}

			// Build CLPI path
			std::string clpi_path = std::format("{}/CLIPINF/{}{}{}{}{}.CLPI",
												root_path,
												buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);

			reader.read_buffer(buffer, 3, ec);
			if (ec) {
				return false;
			}
			bool multi_angle = (buffer[1] >> 4) & 0x1;
			item.start_pts = static_cast<pts_t>(20000.0 * reader.read_uint32(ec) / 90);
			item.end_pts = static_cast<pts_t>(20000.0 * reader.read_uint32(ec) / 90);
			if (ec) {
				return false;
			}

			item.start_time = playlist.duration;
			playlist.duration += (item.end_pts - item.start_pts);

			reader.skip(12, ec);
			uint8_t angle_count = 1;
			if (multi_angle) {
				angle_count = reader.read_uint8(ec);
				if (angle_count < 1) {
					angle_count = 1;
				}
				reader.skip(1, ec);
			}
			for (uint8_t j = 1; j < angle_count; j++) {
				reader.skip(10, ec);
			}
			if (ec) {
				return false;
			}

			if (!read_stn_info(reader, playlist.streams)) {
				return false;
			}

			if (std::filesystem::exists(clpi_path, ec)) {
				StreamReader clpi_reader(clpi_path, ec);
				if (!ec) {
					// Read CLPI header
					char hdmv_buff[8] = {};
					clpi_reader.read_buffer(hdmv_buff, 8, ec);
					if (!ec && !memcmp(hdmv_buff, "HDMV", 4) and check_version(hdmv_buff + 4)) {
						// Read CPI info
						clpi_reader.skip(4, ec);  // skip sequence_info_start_address
						auto program_info_start = clpi_reader.read_uint32(ec);
						auto cpi_start = clpi_reader.read_uint32(ec);
						if (!ec) {
							if (program_info_start) {
								clpi_reader.seek(program_info_start, ec);
								if (!ec) {
									read_program_info(clpi_reader, playlist.streams);
								}
							}

							if (cpi_start) {
								clpi_reader.seek(cpi_start, ec);
								if (!ec) {
									read_cpi_info(clpi_reader, item.sync_points_by_pid);
								}
							}
						}
					}
				}
			}

			playlist.items.emplace_back(std::move(item));

			std::sort(playlist.streams.begin(), playlist.streams.end(), [&](const auto& a, const auto& b) {
				return a.pid < b.pid;
			});
		}

		if (!playlist.duration) {
			return false;
		}

		if (skip_playlist_duplicate) {
			for (const auto& item : playlists_) {
				if (playlist == item) {
					return false;
				}
			}
		}

		playlists_.emplace_back(std::move(playlist));

		return true;
	}

	bool BDParser::parse(std::string_view path, bool skip_playlist_duplicate, bool check_m2ts_files)
	{
		constexpr std::string_view check_paths[] = {
			"index.bdmv",
			"CLIPINF",
			"PLAYLIST",
			"STREAM"
		};

		// Checking required paths
		std::error_code ec = {};
		for (auto& check_path : check_paths) {
			if (!std::filesystem::exists(path / std::filesystem::path(check_path), ec)) {
				return false;
			}
		}

		playlists_.clear();

		// Read playlists
		std::filesystem::path playlist_path = path / std::filesystem::path("PLAYLIST");
		for (const auto& entry : std::filesystem::directory_iterator(playlist_path)) {
			if (entry.is_regular_file() && entry.path().string().ends_with(".mpls")) {
				parse_playlist(entry.path().string(), path, skip_playlist_duplicate, check_m2ts_files);
			}
		}

		if (playlists_.empty()) {
			return false;
		}

		std::sort(playlists_.begin(), playlists_.end(), [&](const auto& a, const auto& b) {
			return a.duration > b.duration;
		});

		return true;
	}
}
