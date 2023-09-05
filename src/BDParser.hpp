#ifndef BDPARSER_HPP
#define BDPARSER_HPP

#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>

namespace parser {
	using pts_t = uint64_t;

	enum class StreamType {
		Unknown = 0,
		MPEG1_VIDEO = 0x01,
		MPEG2_VIDEO = 0x02,
		H264_VIDEO = 0x1B,
		H264_MVC_VIDEO = 0x20,
		HEVC_VIDEO = 0x24,
		VC1_VIDEO = 0xEA,
		MPEG1_AUDIO = 0x03,
		MPEG2_AUDIO = 0x04,
		MPEG2_AAC_AUDIO = 0x0F,
		MPEG4_AAC_AUDIO = 0x11,
		LPCM_AUDIO = 0x80,
		AC3_AUDIO = 0x81,
		AC3_PLUS_AUDIO = 0x84,
		AC3_PLUS_SECONDARY_AUDIO = 0xA1,
		AC3_TRUE_HD_AUDIO = 0x83,
		DTS_AUDIO = 0x82,
		DTS_HD_AUDIO = 0x85,
		DTS_HD_SECONDARY_AUDIO = 0xA2,
		DTS_HD_MASTER_AUDIO = 0x86,
		PRESENTATION_GRAPHICS = 0x90,
		INTERACTIVE_GRAPHICS = 0x91,
		SUBTITLE = 0x92
	};

	enum class VideoFormat {
		Unknown = 0,
		VideoFormat_480i = 1,
		VideoFormat_576i = 2,
		VideoFormat_480p = 3,
		VideoFormat_1080i = 4,
		VideoFormat_720p = 5,
		VideoFormat_1080p = 6,
		VideoFormat_576p = 7,
		VideoFormat_2160p = 8,
	};

	enum class FrameRate {
		Unknown = 0,
		FrameRate_23_976 = 1,
		FrameRate_24 = 2,
		FrameRate_25 = 3,
		FrameRate_29_97 = 4,
		FrameRate_50 = 6,
		FrameRate_59_94 = 7
	};

	enum class AspectRatio {
		Unknown = 0,
		AspectRatio_4_3 = 2,
		AspectRatio_16_9 = 3,
		AspectRatio_2_21 = 4
	};

	enum class ChannelLayout {
		Unknown = 0,
		ChannelLayout_MONO = 1,
		ChannelLayout_STEREO = 3,
		ChannelLayout_MULTI = 6,
		ChannelLayout_COMBO = 12
	};

	enum class SampleRate {
		Unknown = 0,
		SampleRate_48 = 1,
		SampleRate_96 = 4,
		SampleRate_192 = 5,
		SampleRate_48_192 = 12,
		SampleRate_48_96 = 14
	};

	class BDParser final {
		bool parse_playlist(const std::string& playlist_path, std::string_view root_path) noexcept;

	public:
		BDParser() = default;
		BDParser(BDParser&&) = delete;
		BDParser(const BDParser&) = delete;
		BDParser& operator=(BDParser&&) = delete;
		BDParser& operator=(const BDParser&) = delete;
		~BDParser() = default;

		[[nodiscard]] bool parse(std::string_view path);

		struct stream_t {
			uint16_t pid = {};
			StreamType type = {};
			char lang_code[4] = {};

			// Valid for video types
			VideoFormat video_format = {};
			FrameRate frame_rate = {};
			AspectRatio aspect_ratio = {};

			// Valid for audio types
			ChannelLayout channel_layout = {};
			SampleRate sample_rate = {};

			bool is_video() const noexcept {
				return video_format != VideoFormat::Unknown;
			}
			bool is_audio() const noexcept {
				return channel_layout != ChannelLayout::Unknown;
			}
			bool is_subtitles() const noexcept {
				return !is_video() && !is_audio();
			}
		};

		struct playlist_item_t {
			std::string file_name;

			pts_t start_pts = {};
			pts_t end_pts = {};
			pts_t start_time = {};
		};

		struct playlist_t {
			std::string mpls_file_name;
			pts_t duration = {};

			std::vector<playlist_item_t> items;
			std::vector<stream_t> streams;
		};

	private:
		std::vector<playlist_t> playlists_;

	public:
		const std::vector<playlist_t>& playlists() noexcept {
			return playlists_;
		}
	};
} // namespace parser

// format helpers for parser::StreamType
template<>
struct fmt::formatter<parser::StreamType> : fmt::formatter<std::string_view>
{
	static constexpr auto toString(parser::StreamType type) {
#define UNPACK_VALUE(VALUE) case parser::StreamType::VALUE: return #VALUE; break;
		switch (type) {
			UNPACK_VALUE(MPEG1_VIDEO);
			UNPACK_VALUE(MPEG2_VIDEO);
			UNPACK_VALUE(H264_VIDEO);
			UNPACK_VALUE(H264_MVC_VIDEO);
			UNPACK_VALUE(HEVC_VIDEO);
			UNPACK_VALUE(VC1_VIDEO);

			UNPACK_VALUE(MPEG1_AUDIO);
			UNPACK_VALUE(MPEG2_AUDIO);
			UNPACK_VALUE(LPCM_AUDIO);
			UNPACK_VALUE(AC3_AUDIO);
			UNPACK_VALUE(DTS_AUDIO);
			UNPACK_VALUE(AC3_TRUE_HD_AUDIO);
			UNPACK_VALUE(AC3_PLUS_AUDIO);
			UNPACK_VALUE(DTS_HD_AUDIO);
			UNPACK_VALUE(DTS_HD_MASTER_AUDIO);
			UNPACK_VALUE(AC3_PLUS_SECONDARY_AUDIO);
			UNPACK_VALUE(DTS_HD_SECONDARY_AUDIO);

			UNPACK_VALUE(PRESENTATION_GRAPHICS);
			UNPACK_VALUE(INTERACTIVE_GRAPHICS);
			UNPACK_VALUE(SUBTITLE);

			default:
				return "Unknown";
		};
#undef UNPACK_VALUE
	}

public:
	template<typename FormatContext>
	constexpr auto format(const parser::StreamType& type, FormatContext& ctx) {
		return fmt::formatter<std::string_view>::format(toString(type), ctx);
	}
};

#endif // BDPARSER_HPP
