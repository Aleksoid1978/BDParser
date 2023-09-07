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

	class IOInterface {
	public:
		IOInterface() = default;
		IOInterface(IOInterface&&) = delete;
		IOInterface(const IOInterface&) = delete;
		IOInterface& operator=(IOInterface&&) = delete;
		IOInterface& operator=(const IOInterface&) = delete;
		virtual ~IOInterface() = default;

		[[nodiscard]] virtual bool open(const std::string& path) = 0;
		virtual void read_buffer(char* buffer, std::size_t size, std::error_code& ec) = 0;
		[[nodiscard]] virtual uint32_t read_uint32(std::error_code& ec) = 0;
		[[nodiscard]] virtual uint16_t read_uint16(std::error_code& ec) = 0;
		[[nodiscard]] virtual uint8_t read_uint8(std::error_code& ec) = 0;
		virtual void skip(std::size_t size, std::error_code& ec) = 0;
		virtual void seek(std::size_t pos, std::error_code& ec) = 0;
		[[nodiscard]] virtual size_t position(std::error_code& ec) = 0;
	};

	class IFStreamIO final : public IOInterface
	{
		std::ifstream stream_;

	public:
		explicit IFStreamIO(const std::string& path, std::error_code& ec) {
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

	static void read_lang_code(IFStreamIO& io, BDParser::stream_t& s, std::error_code& ec)
	{
		io.read_buffer(s.lang_code, 3, ec);
	}

	[[nodiscard]] static bool read_stream_info(IFStreamIO& io, std::vector<BDParser::stream_t>& streams)
	{
		std::error_code ec = {};
		auto size = io.read_uint8(ec);
		auto pos = io.position(ec);

		auto stream_type = io.read_uint8(ec);
		if (ec) {
			return false;
		}

		BDParser::stream_t s;

		switch (stream_type) {
			case 1:
				s.pid = io.read_uint16(ec);
				break;
			case 2:
			case 4:
				io.skip(2, ec);
				s.pid = io.read_uint16(ec);
				break;
			case 3:
				io.skip(1, ec);
				s.pid = io.read_uint16(ec);
				break;
			default:
				return false;
		}

		io.seek(pos + size, ec);
		size = io.read_uint8(ec);
		pos = io.position(ec);
		if (ec) {
			return false;
		}

		auto it = std::find_if(streams.begin(), streams.end(), [pid = s.pid](const auto& s) {
			return s.pid == pid;
		});
		if (it != streams.end()) {
			io.seek(pos + size, ec);
			return true;
		}

		s.type = static_cast<decltype(s.type)>(io.read_uint8(ec));
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
					auto value = io.read_uint8(ec);
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
					auto value = io.read_uint8(ec);
					s.channel_layout = static_cast<decltype(s.channel_layout)>(value >> 4);
					s.sample_rate = static_cast<decltype(s.sample_rate)>(value & 0xf);
					read_lang_code(io, s, ec);
				}
				break;
			case StreamType::PRESENTATION_GRAPHICS:
			case StreamType::INTERACTIVE_GRAPHICS:
				read_lang_code(io, s, ec);
				break;
			case StreamType::SUBTITLE:
				io.skip(1, ec);
				read_lang_code(io, s, ec);
				break;
		}

		if (ec) {
			return false;
		}

		streams.emplace_back(s);

		io.seek(pos + size, ec);

		return true;
	}

	[[nodiscard]] static bool read_stn_info(IFStreamIO& io, std::vector<BDParser::stream_t>& streams)
	{
		std::error_code ec = {};
		io.skip(4, ec);

		auto num_video = io.read_uint8(ec);
		auto num_audio = io.read_uint8(ec);
		auto num_pg = io.read_uint8(ec);
		auto num_ig = io.read_uint8(ec);
		auto num_secondary_audio = io.read_uint8(ec);
		auto num_secondary_video = io.read_uint8(ec);
		auto num_pip_pg = io.read_uint8(ec);

		if (ec) {
			return false;
		}

		io.skip(5, ec);

		if (streams.empty()) {
			streams.reserve(static_cast<size_t>(num_video) + num_audio + num_pg + num_ig +
							num_secondary_audio + num_secondary_video + num_pip_pg);
		}

		for (uint8_t i = 0; i < num_video; i++) {
			if (!(read_stream_info(io, streams))) {
				return false;
			}
		}

		for (uint8_t i = 0; i < num_audio; i++) {
			if (!(read_stream_info(io, streams))) {
				return false;
			}
		}

		for (uint8_t i = 0; i < (num_pg + num_pip_pg); i++) {
			if (!(read_stream_info(io, streams))) {
				return false;
			}
		}

		for (uint8_t i = 0; i < num_ig; i++) {
			if (!(read_stream_info(io, streams))) {
				return false;
			}
		}

		for (uint8_t i = 0; i < num_secondary_audio; i++) {
			if (!(read_stream_info(io, streams))) {
				return false;
			}

			// Secondary Audio Extra Attributes
			const auto num_secondary_audio_extra = io.read_uint8(ec);
			io.skip(1, ec);
			if (num_secondary_audio_extra) {
				io.skip(num_secondary_audio_extra, ec);
				if (num_secondary_audio_extra % 2) {
					io.skip(1, ec);
				}
			}

			if (ec) {
				return false;
			}
		}

		for (uint8_t i = 0; i < num_secondary_video; i++) {
			if (!(read_stream_info(io, streams))) {
				return false;
			}

			// Secondary Video Extra Attributes
			const auto num_secondary_video_extra = io.read_uint8(ec);
			io.skip(1, ec);
			if (num_secondary_video_extra) {
				io.skip(num_secondary_video_extra, ec);
				if (num_secondary_video_extra % 2) {
					io.skip(1, ec);
				}
			}

			const auto num_pip_pg_extra = io.read_uint8(ec);
			io.skip(1, ec);
			if (num_pip_pg_extra) {
				io.skip(num_pip_pg_extra, ec);
				if (num_pip_pg_extra % 2) {
					io.skip(1, ec);
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
		std::error_code ec = {};

		IFStreamIO io(playlist_path, ec);
		if (ec) {
			return false;
		}

		char buffer[9] = {};
		io.read_buffer(buffer, 4, ec);
		if (ec || std::memcmp(buffer, "MPLS", 4)) {
			return false;
		}

		io.read_buffer(buffer, 4, ec);
		if (ec || !check_version()) {
			return false;
		}

		auto playlist_start_address = io.read_uint32(ec);
		if (ec) {
			return false;
		}

		io.seek(playlist_start_address, ec);
		io.skip(6, ec);
		auto number_of_playlist_items = io.read_uint16(ec);
		if (ec) {
			return false;
		}

		playlist_t playlist;
		playlist.mpls_file_name = playlist_path;

		playlist_start_address += 10;
		for (uint16_t i = 0; i < number_of_playlist_items; i++) {
			io.seek(playlist_start_address, ec);
			playlist_start_address += io.read_uint16(ec) + 2;
			io.read_buffer(buffer, 9, ec);
			if (ec || std::memcmp(&buffer[5], "M2TS", 4)) {
				return false;
			}

			playlist_item_t item;
			item.file_name = fmt::format("{}/STREAM/{}{}{}{}{}.M2TS", root_path,
										 buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);
			if (!std::filesystem::exists(item.file_name, ec)) {
				return false;
			}
			if (std::find_if(playlist.items.begin(), playlist.items.end(), [&](const auto& _item) {
						return item.file_name == _item.file_name;
					}) != playlist.items.end()) {
				// Ignore playlists with duplicate files
				return false;
			}

			io.read_buffer(buffer, 3, ec);
			if (ec) {
				return false;
			}
			bool multi_angle = (buffer[1] >> 4) & 0x1;
			item.start_pts = static_cast<pts_t>(20000.0 * io.read_uint32(ec) / 90);
			item.end_pts = static_cast<pts_t>(20000.0 * io.read_uint32(ec) / 90);
			if (ec) {
				return false;
			}

			item.start_time = playlist.duration;
			playlist.duration += (item.end_pts - item.start_pts);

			io.skip(12, ec);
			uint8_t angle_count = 1;
			if (multi_angle) {
				angle_count = io.read_uint8(ec);
				if (angle_count < 1) {
					angle_count = 1;
				}
				io.skip(1, ec);
			}
			for (uint8_t j = 1; j < angle_count; j++) {
				io.skip(10, ec);
			}
			if (ec) {
				return false;
			}

			if (!read_stn_info(io, playlist.streams)) {
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
			if (entry.is_regular_file() && string::ends_with(entry.path().string(), ".mpls")) {
				parse_playlist(entry.path().string(), path);
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
