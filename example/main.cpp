#include <iostream>
#include "BDParser.hpp"

static std::string pts_to_string(parser::pts_t pts)
{
	auto ms = pts / 10000;
	auto hour = ms / (1000 * 60 * 60);
	auto minute = (ms / (1000 * 60)) % 60;
	auto second = (ms / 1000) % 60;
	ms = ms % 1000;

	return std::format("{:#02}:{:#02}:{:#02}.{:#03}", hour, minute, second, ms);
}

int main(int argc, char** argv)
{
	if (argc != 2) {
		std::cout << "Usage : Sample <path_to_root_BD/BDMV>" << std::endl;
		return -1;
	}

	parser::BDParser parser;
	if (!parser.parse(argv[1], true, false)) {
		std::cout << "Doesn't look like a valid BD/BDMV path or the files are corrupted" << std::endl;
		return -1;
	}

	auto& playlists = parser.playlists();
	for (const auto& playlist : playlists) {
		std::cout << std::format("\nPlaylist : {}, duration : {}\n", playlist.mpls_file_name, pts_to_string(playlist.duration));
		std::cout << std::format("    List of files:\n");
		for (const auto& item : playlist.items) {
			std::cout << std::format("        Filename : {}\n", item.file_name);
		}

		std::cout << std::format("    List of streams:\n");
		for (const auto& stream : playlist.streams) {
			std::cout << std::format("        PID : {}, type : {} ({}){}\n",
									 stream.pid, stream.type,
									 stream.is_video() ? "Video" : (stream.is_audio() ? "Audio" : "Subtitles"),
									 !stream.lang_code.empty() ? std::format(", language : {}", stream.lang_code) : "");
		}
	}

	return 0;
}
