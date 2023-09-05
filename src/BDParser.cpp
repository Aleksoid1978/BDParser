#include <cstdint>
#include <iostream>
#include <fstream>
#include <filesystem>

#include "BDParser.hpp"

namespace parser {
	[[nodiscard]] static bool ends_with(const std::string& str, const std::string_view suffix) noexcept
	{
		return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
	}

	[[nodiscard]] static uint16_t swap_uint16(uint16_t value) noexcept
	{
		return (value << 8) | (value >> 8);
	}

	[[nodiscard]] static uint32_t swap_uint32(uint32_t value) noexcept
	{
		value = ((value << 8) & 0xFF00FF00) | ((value >> 8) & 0xFF00FF);
		return (value << 16) | (value >> 16);
	}

	static void read_buffer(std::ifstream& stream, char* buffer, std::size_t size, std::error_code& ec) noexcept
	{
		stream.read(buffer, size);
		if (stream.fail()) {
			ec = std::make_error_code(std::errc::io_error);
		}
	}

	[[nodiscard]] static uint32_t read_uint32(std::ifstream& stream, std::error_code& ec) noexcept
	{
		uint32_t value = {};
		read_buffer(stream, reinterpret_cast<char*>(&value), sizeof(value), ec);
		if (ec) {
			return {};
		}

		return swap_uint32(value);
	}

	[[nodiscard]] static uint16_t read_uint16(std::ifstream& stream, std::error_code& ec) noexcept
	{
		uint16_t value = {};
		read_buffer(stream, reinterpret_cast<char*>(&value), sizeof(value), ec);
		if (ec) {
			return {};
		}

		return swap_uint16(value);
	}

	[[nodiscard]] static uint8_t read_uint8(std::ifstream& stream, std::error_code& ec) noexcept
	{
		uint8_t value = {};
		read_buffer(stream, reinterpret_cast<char*>(&value), sizeof(value), ec);
		return value;
	}

	void skip(std::ifstream& stream, std::size_t size) noexcept
	{
		stream.seekg(size, std::ios_base::cur);
	}

	void seek(std::ifstream& stream, std::size_t pos) noexcept
	{
		stream.seekg(pos);
	}

	[[nodiscard]] static size_t position(std::ifstream& stream) noexcept
	{
		return stream.tellg();
	}

	static void read_lang_code(std::ifstream& stream, BDParser::stream_t& s, std::error_code& ec)
	{
		read_buffer(stream, s.lang_code, 3, ec);
	}

	[[nodiscard]] static bool read_stream_info(std::ifstream& stream, std::vector<BDParser::stream_t>& streams)
	{
		std::error_code ec = {};
		auto size = read_uint8(stream, ec);
		auto pos = position(stream);

		auto stream_type = read_uint8(stream, ec);
		if (ec) {
			return false;
		}

		BDParser::stream_t s;

		switch (stream_type) {
			case 1:
				s.pid = read_uint16(stream, ec);
				break;
			case 2:
			case 4:
				skip(stream, 2);
				s.pid = read_uint16(stream, ec);
				break;
			case 3:
				skip(stream, 1);
				s.pid = read_uint16(stream, ec);
				break;
			default:
				return false;
		}

		seek(stream, pos + size);
		size = read_uint8(stream, ec);
		pos = position(stream);
		if (ec) {
			return false;
		}

		auto it = std::find_if(streams.begin(), streams.end(), [pid = s.pid](const auto& s) {
			return s.pid == pid;
		});
		if (it != streams.end()) {
			seek(stream, pos + size);
			return true;
		}

		s.type = static_cast<decltype(s.type)>(read_uint8(stream, ec));
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
					auto value = read_uint8(stream, ec);
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
					auto value = read_uint8(stream, ec);
					s.channel_layout = static_cast<decltype(s.channel_layout)>(value >> 4);
					s.sample_rate = static_cast<decltype(s.sample_rate)>(value & 0xf);
					read_lang_code(stream, s, ec);
				}
				break;
			case StreamType::PRESENTATION_GRAPHICS:
			case StreamType::INTERACTIVE_GRAPHICS:
				read_lang_code(stream, s, ec);
				break;
			case StreamType::SUBTITLE:
				skip(stream, 1);
				read_lang_code(stream, s, ec);
				break;
		}

		if (ec) {
			return false;
		}

		streams.emplace_back(s);

		seek(stream, pos + size);

		return true;
	}

	[[nodiscard]] static bool read_stn_info(std::ifstream& stream, std::vector<BDParser::stream_t>& streams)
	{
		skip(stream, 4);

		std::error_code ec = {};
		auto num_video = read_uint8(stream, ec);
		auto num_audio = read_uint8(stream, ec);
		auto num_pg = read_uint8(stream, ec);
		auto num_ig = read_uint8(stream, ec);
		auto num_secondary_audio = read_uint8(stream, ec);
		auto num_secondary_video = read_uint8(stream, ec);
		auto num_pip_pg = read_uint8(stream, ec);

		if (ec) {
			return false;
		}

		skip(stream, 5);

		if (streams.empty()) {
			streams.reserve(static_cast<size_t>(num_video) + num_audio + num_pg + num_ig +
							num_secondary_audio + num_secondary_video + num_pip_pg);
		}

		for (uint8_t i = 0; i < num_video; i++) {
			if (!(read_stream_info(stream, streams))) {
				return false;
			}
		}

		for (uint8_t i = 0; i < num_audio; i++) {
			if (!(read_stream_info(stream, streams))) {
				return false;
			}
		}

		for (uint8_t i = 0; i < (num_pg + num_pip_pg); i++) {
			if (!(read_stream_info(stream, streams))) {
				return false;
			}
		}

		for (uint8_t i = 0; i < num_ig; i++) {
			if (!(read_stream_info(stream, streams))) {
				return false;
			}
		}

		for (uint8_t i = 0; i < num_secondary_audio; i++) {
			if (!(read_stream_info(stream, streams))) {
				return false;
			}

			// Secondary Audio Extra Attributes
			const auto num_secondary_audio_extra = read_uint8(stream, ec);
			skip(stream, 1);
			if (num_secondary_audio_extra) {
				skip(stream, num_secondary_audio_extra);
				if (num_secondary_audio_extra % 2) {
					skip(stream, 1);
				}
			}

			if (ec) {
				return false;
			}
		}

		for (uint8_t i = 0; i < num_secondary_video; i++) {
			if (!(read_stream_info(stream, streams))) {
				return false;
			}

			// Secondary Video Extra Attributes
			const auto num_secondary_video_extra = read_uint8(stream, ec);
			skip(stream, 1);
			if (num_secondary_video_extra) {
				skip(stream, num_secondary_video_extra);
				if (num_secondary_video_extra % 2) {
					skip(stream, 1);
				}
			}

			const auto num_pip_pg_extra = read_uint8(stream, ec);
			skip(stream, 1);
			if (num_pip_pg_extra) {
				skip(stream, num_pip_pg_extra);
				if (num_pip_pg_extra % 2) {
					skip(stream, 1);
				}
			}

			if (ec) {
				return false;
			}
		}

		return true;
	}

	#define check_version() (!(std::memcmp(buffer, "0300", 4)) || (!std::memcmp(buffer, "0200", 4)) || (!std::memcmp(buffer, "0100", 4)))

	bool BDParser::parse_playlist(const std::string& playlist_path, std::string_view root_path) noexcept
	{
		std::ifstream stream(playlist_path, std::ios::in | std::ios::binary);
		if (!stream.is_open()) {
			return false;
		}

		std::error_code ec = {};
		char buffer[9] = {};
		read_buffer(stream, buffer, 4, ec);
		if (ec || std::memcmp(buffer, "MPLS", 4)) {
			return false;
		}

		read_buffer(stream, buffer, 4, ec);
		if (ec || !check_version()) {
			return false;
		}

		auto playlist_start_address = read_uint32(stream, ec);
		if (ec) {
			return false;
		}

		seek(stream, playlist_start_address);
		skip(stream, 6); // length + reserved_for_future_use
		auto number_of_playlist_items = read_uint16(stream, ec);
		if (ec) {
			return false;
		}

		playlist_t playlist;
		playlist.mpls_file_name = playlist_path;

		playlist_start_address += 10;
		for (uint16_t i = 0; i < number_of_playlist_items; i++) {
			seek(stream, playlist_start_address);
			playlist_start_address += read_uint16(stream, ec);
			read_buffer(stream, buffer, 9, ec);
			if (ec || std::memcmp(&buffer[5], "M2TS", 4)) {
				return false;
			}

			playlist_item_t item;
			item.file_name = fmt::format("{}/STREAM/{}{}{}{}{}.M2TS", root_path,
										 buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);
			if (!std::filesystem::exists(item.file_name, ec)) {
				return false;
			}

			read_buffer(stream, buffer, 3, ec);
			if (ec) {
				return false;
			}
			bool multi_angle = (buffer[1] >> 4) & 0x1;
			item.start_pts = static_cast<pts_t>(20000.0 * read_uint32(stream, ec) / 90);
			item.end_pts = static_cast<pts_t>(20000.0 * read_uint32(stream, ec) / 90);
			if (ec) {
				return false;
			}

			item.start_time = playlist.duration;
			playlist.duration += (item.end_pts - item.start_pts);

			skip(stream, 12);
			uint8_t angle_count = 1;
			if (multi_angle) {
				angle_count = read_uint8(stream, ec);
				if (angle_count < 1) {
					angle_count = 1;
				}
				skip(stream, 1);
			}
			for (uint8_t j = 1; j < angle_count; j++) {
				skip(stream, 10);
			}
			if (ec) {
				return false;
			}

			if (!read_stn_info(stream, playlist.streams)) {
				return false;
			}

			playlist.items.emplace_back(std::move(item));
		}

		if (!playlist.duration) {
			return false;
		}

		playlists_.emplace_back(std::move(playlist));

		return true;
	}

	bool BDParser::parse(std::string_view path)
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
			if (entry.is_regular_file() && ends_with(entry.path().string(), ".mpls")) {
				if (!parse_playlist(entry.path().string(), path)) {
					return false;
				}
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
