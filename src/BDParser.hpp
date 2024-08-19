#ifndef BDPARSER_HPP
#define BDPARSER_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <format>

namespace parser {
	using pts_t = uint64_t;

	enum class StreamType {
		Unknown                  = 0,
		MPEG1_VIDEO              = 0x01,
		MPEG2_VIDEO              = 0x02,
		H264_VIDEO               = 0x1B,
		H264_MVC_VIDEO           = 0x20,
		HEVC_VIDEO               = 0x24,
		VC1_VIDEO                = 0xEA,
		MPEG1_AUDIO              = 0x03,
		MPEG2_AUDIO              = 0x04,
		MPEG2_AAC_AUDIO          = 0x0F,
		MPEG4_AAC_AUDIO          = 0x11,
		LPCM_AUDIO               = 0x80,
		AC3_AUDIO                = 0x81,
		AC3_PLUS_AUDIO           = 0x84,
		AC3_PLUS_SECONDARY_AUDIO = 0xA1,
		AC3_TRUE_HD_AUDIO        = 0x83,
		DTS_AUDIO                = 0x82,
		DTS_HD_AUDIO             = 0x85,
		DTS_HD_SECONDARY_AUDIO   = 0xA2,
		DTS_HD_MASTER_AUDIO      = 0x86,
		PRESENTATION_GRAPHICS    = 0x90,
		INTERACTIVE_GRAPHICS     = 0x91,
		SUBTITLE                 = 0x92
	};

	enum class VideoFormat {
		Unknown           = 0,
		VideoFormat_480i  = 1,
		VideoFormat_576i  = 2,
		VideoFormat_480p  = 3,
		VideoFormat_1080i = 4,
		VideoFormat_720p  = 5,
		VideoFormat_1080p = 6,
		VideoFormat_576p  = 7,
		VideoFormat_2160p = 8,
	};

	enum class FrameRate {
		Unknown          = 0,
		FrameRate_23_976 = 1,
		FrameRate_24     = 2,
		FrameRate_25     = 3,
		FrameRate_29_97  = 4,
		FrameRate_50     = 6,
		FrameRate_59_94  = 7
	};

	enum class AspectRatio {
		Unknown          = 0,
		AspectRatio_4_3  = 2,
		AspectRatio_16_9 = 3,
		AspectRatio_2_21 = 4
	};

	enum class ChannelLayout {
		Unknown              = 0,
		ChannelLayout_MONO   = 1,
		ChannelLayout_STEREO = 3,
		ChannelLayout_MULTI  = 6,
		ChannelLayout_COMBO  = 12
	};

	enum class SampleRate {
		Unknown           = 0,
		SampleRate_48     = 1,
		SampleRate_96     = 4,
		SampleRate_192    = 5,
		SampleRate_48_192 = 12,
		SampleRate_48_96  = 14
	};

	enum class StreamFormat {
		Video,
		Audio,
		Subtitles
	};

	class BDParser final {
		bool parse_playlist(const std::string& playlist_path, std::string_view root_path, bool skip_playlist_duplicate, bool check_m2ts_files) noexcept;

	public:
		BDParser() = default;
		BDParser(BDParser&&) = delete;
		BDParser(const BDParser&) = delete;
		BDParser& operator=(BDParser&&) = delete;
		BDParser& operator=(const BDParser&) = delete;
		~BDParser() = default;

		[[nodiscard]] bool parse(std::string_view path, bool skip_playlist_duplicate, bool check_m2ts_files);

		struct stream_t {
			uint16_t pid = {};
			StreamType type = {};
			std::string lang_code;

			// Valid for video types
			VideoFormat video_format = {};
			FrameRate frame_rate = {};
			AspectRatio aspect_ratio = {};

			// Valid for audio types
			ChannelLayout channel_layout = {};
			SampleRate sample_rate = {};

			StreamFormat format() const noexcept {
				if (video_format != VideoFormat::Unknown) {
					return StreamFormat::Video;
				} else if (channel_layout != ChannelLayout::Unknown) {
					return StreamFormat::Audio;
				}

				return StreamFormat::Subtitles;
			}

			bool operator==(const stream_t& other) const {
				return pid == other.pid && type == other.type && lang_code == other.lang_code &&
					video_format == other.video_format && frame_rate == other.frame_rate && aspect_ratio == other.aspect_ratio &&
					channel_layout == other.channel_layout && sample_rate == other.sample_rate;
			}
		};

		struct playlist_item_t {
			std::string file_name;

			pts_t start_pts = {};
			pts_t end_pts = {};
			pts_t start_time = {};

			bool operator==(const playlist_item_t& other) const {
				return file_name == other.file_name &&
					start_pts == other.start_pts && end_pts == other.end_pts &&
					start_time == other.start_time;
			}
		};

		struct playlist_t {
			std::string mpls_file_name;
			pts_t duration = {};

			std::vector<playlist_item_t> items;
			std::vector<stream_t> streams;

			bool operator==(const playlist_t& other) const {
				return duration == other.duration && items == other.items && streams == other.streams;
			}
		};

	private:
		std::vector<playlist_t> playlists_;

	public:
		const std::vector<playlist_t>& playlists() noexcept {
			return playlists_;
		}
	};
} // namespace parser

// format helpers

template<>
struct std::formatter<parser::StreamType> : std::formatter<std::string_view>
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
	constexpr auto format(parser::StreamType type, FormatContext& ctx) const {
		return std::formatter<std::string_view>::format(toString(type), ctx);
	}
};

template<>
struct std::formatter<parser::StreamFormat> : std::formatter<std::string_view>
{
	static constexpr auto toString(parser::StreamFormat type) {
#define UNPACK_VALUE(VALUE) case parser::StreamFormat::VALUE: return #VALUE; break;
		switch (type) {
			UNPACK_VALUE(Video);
			UNPACK_VALUE(Audio);
			UNPACK_VALUE(Subtitles);

			default:
				return "Unknown";
		};
#undef UNPACK_VALUE
	}

public:
	template<typename FormatContext>
	constexpr auto format(parser::StreamFormat type, FormatContext& ctx) const {
		return std::formatter<std::string_view>::format(toString(type), ctx);
	}
};

template<>
struct std::formatter<parser::FrameRate> : std::formatter<std::string_view>
{
	static constexpr auto toString(parser::FrameRate rate) {
		switch (rate) {
			case parser::FrameRate::FrameRate_23_976: return "23.976";
			case parser::FrameRate::FrameRate_24:     return "24";
			case parser::FrameRate::FrameRate_25:     return "25";
			case parser::FrameRate::FrameRate_29_97:  return "29.97";
			case parser::FrameRate::FrameRate_50:     return "50";
			case parser::FrameRate::FrameRate_59_94:  return "59.94";
			default:
				return "Unknown";
		};
	}

public:
	template<typename FormatContext>
	constexpr auto format(parser::FrameRate rate, FormatContext& ctx) const {
		return std::formatter<std::string_view>::format(toString(rate), ctx);
	}
};

template<>
struct std::formatter<parser::VideoFormat> : std::formatter<std::string_view>
{
	static constexpr auto toString(parser::VideoFormat format) {
		switch (format) {
			case parser::VideoFormat::VideoFormat_480i:  return "480i";
			case parser::VideoFormat::VideoFormat_576i:  return "576i";
			case parser::VideoFormat::VideoFormat_480p:  return "480";
			case parser::VideoFormat::VideoFormat_1080i: return "1080i";
			case parser::VideoFormat::VideoFormat_720p:  return "720";
			case parser::VideoFormat::VideoFormat_1080p: return "1080";
			case parser::VideoFormat::VideoFormat_576p:  return "576";
			case parser::VideoFormat::VideoFormat_2160p: return "4k";
			default:
				return "Unknown";
		};
	}

public:
	template<typename FormatContext>
	constexpr auto format(parser::VideoFormat format, FormatContext& ctx) const {
		return std::formatter<std::string_view>::format(toString(format), ctx);
	}
};

template<>
struct std::formatter<parser::BDParser::stream_t> : std::formatter<std::string_view>
{
public:
	template<typename FormatContext>
	constexpr auto format(const parser::BDParser::stream_t& stream, FormatContext& ctx) const {
		auto info = std::format("PID : {}, type : {} ({}{})",
							   stream.pid, stream.type, stream.format(),
							   stream.format() == parser::StreamFormat::Video ? std::format(" {}@{}", stream.video_format, stream.frame_rate) : "");
		if (!stream.lang_code.empty()) {
			info += std::format(", language : {}", stream.lang_code);
		}
		return std::formatter<std::string_view>::format(info, ctx);
	}
};

#endif // BDPARSER_HPP
