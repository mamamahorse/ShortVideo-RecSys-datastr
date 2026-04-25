#ifndef DATASTR_VIDEO_CLEANER_HPP
#define DATASTR_VIDEO_CLEANER_HPP

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace datastr {

struct RawVideo {
    long long aid = 0;
    std::string bvid;
    std::string title;
    std::string category;
    std::string author;
    int duration = 0;
    long long pubdate = 0;
    long long view_count = 0;
    long long favorite = 0;
    long long coin = 0;
    long long share = 0;
    long long like = 0;
    std::string tag;
};

struct ModeledVideo {
    long long video_id = 0;
    std::string bvid;
    std::string title;
    int category_id = 0;
    std::string category_name;
    std::vector<int> tag_ids;
    int duration_sec = 0;
    long long publish_ts = 0;
    long long raw_view_count = 0;
    long long raw_like = 0;
    long long raw_favorite = 0;
    long long raw_coin = 0;
    long long raw_share = 0;
    double quality_score = 0.0;
    int topic_id = 0;
    std::vector<double> topic_vector;
};

struct Catalog {
    std::vector<ModeledVideo> videos;
    std::vector<std::vector<int> > topic_to_video_indices;
    std::unordered_map<std::string, int> category_to_id;
    std::unordered_map<std::string, int> tag_to_id;
};

struct VideoBuildConfig {
    int topic_count = 12;
    int min_tag_frequency = 3;
};

std::vector<std::string> split_tags(const std::string& raw_tags);

std::vector<RawVideo> load_raw_videos(const std::string& csv_path,
                                      std::size_t max_rows);

Catalog build_catalog(const std::vector<RawVideo>& raw_videos,
                      const VideoBuildConfig& config);

Catalog load_processed_videos(const std::string& csv_path,
                              const VideoBuildConfig& config);

bool write_videos_csv(const std::string& path, const Catalog& catalog);

}  // namespace datastr

#endif  // DATASTR_VIDEO_CLEANER_HPP
