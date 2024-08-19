#include <fstream>
#include <filesystem>

#include "BDParser.hpp"

namespace parser {
	namespace string {
		[[nodiscard]] static bool ends_with(const std::string& str, const std::string_view suffix) noexcept
		{
			return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
		}
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

	#define check_version() (!(std::memcmp(buffer, "0300", 4)) || (!std::memcmp(buffer, "0200", 4)) || (!std::memcmp(buffer, "0100", 4)))

	bool BDParser::parse_playlist(const std::string& playlist_path, std::string_view root_path, bool skip_playlist_duplicate, bool check_m2ts_files) noexcept
	{
		std::error_code ec = {};

		StreamReader reader(playlist_path, ec);
		if (ec) {
			return false;
		}

		char buffer[9] = {};
		reader.read_buffer(buffer, 4, ec);
		if (ec || std::memcmp(buffer, "MPLS", 4)) {
			return false;
		}

		reader.read_buffer(buffer, 4, ec);
		if (ec || !check_version()) {
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

			playlist.items.emplace_back(std::move(item));
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
			if (entry.is_regular_file() && string::ends_with(entry.path().string(), ".mpls")) {
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
