#include <iostream>
#include "BDParser.hpp"

static std::string pts_to_string(parser::pts_t pts)
{
	auto ms = pts / 10000;
	auto hour = ms / (1000 * 60 * 60);
	auto minute = (ms / (1000 * 60)) % 60;
	auto second = (ms / 1000) % 60;
	ms = ms % 1000;

	return fmt::format("{:#02}:{:#02}:{:#02}.{:#03}", hour, minute, second, ms);
}

int main(int argc, char** argv)
{
	if (argc != 2) {
		std::cout << "Usage : Sample <path_to_root_BD/BDMV>" << std::endl;
		return -1;
	}

	parser::BDParser parser;
	if (!parser.parse(argv[1])) {
		std::cout << "Doesn't look like a valid BD/BDMV path or the files are corrupted" << std::endl;
		return -1;
	}

	auto& playlists = parser.playlists();
	for (const auto& playlist : playlists) {
		fmt::print("\nPlaylist : {}, duration : {}\n", playlist.mpls_file_name, pts_to_string(playlist.duration));
		fmt::print("    List of files:\n");
		for (const auto& item : playlist.items) {
			fmt::print("        Filename : {}\n", item.file_name);
		}

		fmt::print("    List of streams:\n");
		for (const auto& stream : playlist.streams) {
			fmt::print("        PID : {}, type : {} ({}){}\n",
					   stream.pid, stream.type,
					   stream.is_video() ? "Video" : (stream.is_audio() ? "Audio" : "Subtitles"),
					   stream.lang_code[0] ? fmt::format(", language : {}", stream.lang_code) : "");
		}
	}

	return 0;
}
