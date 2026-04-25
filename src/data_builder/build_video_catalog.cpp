#include "video_cleaner.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int parse_int_arg(const char* value, int fallback) {
    if (value == nullptr) {
        return fallback;
    }
    int parsed = std::atoi(value);
    return parsed > 0 ? parsed : fallback;
}

std::size_t parse_size_arg(const char* value, std::size_t fallback) {
    if (value == nullptr) {
        return fallback;
    }
    long long parsed = std::atoll(value);
    return parsed > 0 ? static_cast<std::size_t>(parsed) : fallback;
}

}  // namespace

int main(int argc, char** argv) {
    using namespace datastr;

    std::string input_csv = argc > 1 ? argv[1] : "data/raw/bilibili_10w_pro.csv";
    std::string output_csv = argc > 2 ? argv[2] : "data/processed/videos.csv";

    VideoBuildConfig config;
    config.topic_count = argc > 3 ? parse_int_arg(argv[3], config.topic_count) : config.topic_count;
    std::size_t max_videos = argc > 4 ? parse_size_arg(argv[4], 0) : 0;
    config.min_tag_frequency = argc > 5 ? parse_int_arg(argv[5], config.min_tag_frequency)
                                        : config.min_tag_frequency;

    try {
        std::vector<RawVideo> raw_videos = load_raw_videos(input_csv, max_videos);
        if (raw_videos.empty()) {
            std::cerr << "No videos loaded from " << input_csv << std::endl;
            return 1;
        }

        Catalog catalog = build_catalog(raw_videos, config);
        if (!write_videos_csv(output_csv, catalog)) {
            std::cerr << "Failed to write processed videos to " << output_csv << std::endl;
            return 1;
        }

        std::cout << "processed_videos=" << catalog.videos.size()
                  << " categories=" << catalog.category_to_id.size()
                  << " tags=" << catalog.tag_to_id.size()
                  << " output=" << output_csv
                  << std::endl;
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }

    return 0;
}
