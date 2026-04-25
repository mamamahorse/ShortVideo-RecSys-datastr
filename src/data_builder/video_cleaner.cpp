#include "video_cleaner.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <direct.h>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace datastr {
namespace {

std::string trim(const std::string& value) {
    std::size_t begin = 0;
    while (begin < value.size() &&
           (value[begin] == ' ' || value[begin] == '\t' ||
            value[begin] == '\r' || value[begin] == '\n')) {
        ++begin;
    }

    std::size_t end = value.size();
    while (end > begin &&
           (value[end - 1] == ' ' || value[end - 1] == '\t' ||
            value[end - 1] == '\r' || value[end - 1] == '\n')) {
        --end;
    }

    return value.substr(begin, end - begin);
}

void replace_all(std::string& text, const std::string& from, const std::string& to) {
    if (from.empty()) {
        return;
    }
    std::size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
}

std::vector<std::string> split_simple(const std::string& line, char delimiter) {
    std::vector<std::string> result;
    std::string current;
    for (char ch : line) {
        if (ch == delimiter) {
            result.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    result.push_back(current);
    return result;
}

std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> result;
    std::string current;
    bool in_quotes = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        char ch = line[i];
        if (ch == '"') {
            if (in_quotes && i + 1 < line.size() && line[i + 1] == '"') {
                current.push_back('"');
                ++i;
            } else {
                in_quotes = !in_quotes;
            }
        } else if (ch == ',' && !in_quotes) {
            result.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    result.push_back(current);
    return result;
}

long long parse_ll(const std::string& text) {
    std::string cleaned = trim(text);
    if (cleaned.empty()) {
        return 0;
    }
    return std::atoll(cleaned.c_str());
}

int parse_int(const std::string& text) {
    return static_cast<int>(parse_ll(text));
}

double parse_double(const std::string& text) {
    std::string cleaned = trim(text);
    if (cleaned.empty()) {
        return 0.0;
    }
    return std::atof(cleaned.c_str());
}

int get_or_add_id(std::unordered_map<std::string, int>& ids, const std::string& key) {
    std::unordered_map<std::string, int>::const_iterator found = ids.find(key);
    if (found != ids.end()) {
        return found->second;
    }
    int id = static_cast<int>(ids.size());
    ids[key] = id;
    return id;
}

bool contains_any(const std::string& text, const std::vector<std::string>& keys) {
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (text.find(keys[i]) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::vector<std::vector<std::string> > topic_keywords() {
    std::vector<std::vector<std::string> > keys(12);
    keys[0] = {"素材", "剪辑", "影视", "短片", "动画", "绿幕", "自媒体"};
    keys[1] = {"数码", "NAS", "nas", "硬盘", "电脑", "装机", "评测", "硬件"};
    keys[2] = {"AI", "ai", "软件", "工具", "教程", "自动化", "办公"};
    keys[3] = {"游戏", "手游", "主机", "攻略", "实况"};
    keys[4] = {"日常", "vlog", "VLOG", "生活", "情感", "校园"};
    keys[5] = {"美食", "烹饪", "制作", "手工"};
    keys[6] = {"科普", "学习", "课程", "考试", "知识"};
    keys[7] = {"音乐", "舞蹈", "翻唱", "演奏"};
    keys[8] = {"番剧", "二创", "鬼畜", "MMD", "mmd"};
    keys[9] = {"运动", "健身", "汽车", "赛事"};
    keys[10] = {"旅行", "户外", "摄影", "风景"};
    keys[11] = {"搞笑", "娱乐", "明星", "热点", "萌宠"};
    return keys;
}

int hash_topic(const std::string& text, int topic_count) {
    unsigned int hash = 2166136261u;
    for (unsigned char ch : text) {
        hash ^= ch;
        hash *= 16777619u;
    }
    return static_cast<int>(hash % static_cast<unsigned int>(topic_count));
}

int topic_from_text(const std::string& text, int topic_count) {
    const std::vector<std::vector<std::string> > keys = topic_keywords();
    int usable_topics = std::min(topic_count, static_cast<int>(keys.size()));
    for (int topic = 0; topic < usable_topics; ++topic) {
        if (contains_any(text, keys[topic])) {
            return topic;
        }
    }
    return -1;
}

double clamp01(double value) {
    if (value < 0.0) {
        return 0.0;
    }
    if (value > 1.0) {
        return 1.0;
    }
    return value;
}

void normalize(std::vector<double>& values) {
    double sum = 0.0;
    for (double value : values) {
        sum += value;
    }
    if (sum <= 0.0) {
        if (!values.empty()) {
            values[0] = 1.0;
        }
        return;
    }
    for (double& value : values) {
        value /= sum;
    }
}

int max_index(const std::vector<double>& values) {
    int best = 0;
    for (std::size_t i = 1; i < values.size(); ++i) {
        if (values[i] > values[best]) {
            best = static_cast<int>(i);
        }
    }
    return best;
}

double rate(long long action, long long views) {
    return static_cast<double>(action) / static_cast<double>(views + 100);
}

std::string csv_escape(const std::string& value) {
    std::string escaped = value;
    replace_all(escaped, "\"", "\"\"");
    return "\"" + escaped + "\"";
}

std::string join_ints(const std::vector<int>& values) {
    std::ostringstream output;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            output << ';';
        }
        output << values[i];
    }
    return output.str();
}

std::string join_doubles(const std::vector<double>& values) {
    std::ostringstream output;
    output << std::fixed << std::setprecision(6);
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            output << ';';
        }
        output << values[i];
    }
    return output.str();
}

std::vector<int> parse_int_list(const std::string& text) {
    std::vector<int> values;
    std::vector<std::string> parts = split_simple(text, ';');
    for (const std::string& part : parts) {
        std::string cleaned = trim(part);
        if (!cleaned.empty()) {
            values.push_back(parse_int(cleaned));
        }
    }
    return values;
}

std::vector<double> parse_double_list(const std::string& text) {
    std::vector<double> values;
    std::vector<std::string> parts = split_simple(text, ';');
    for (const std::string& part : parts) {
        std::string cleaned = trim(part);
        if (!cleaned.empty()) {
            values.push_back(parse_double(cleaned));
        }
    }
    return values;
}

void ensure_parent_dir(const std::string& path) {
    std::size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) {
        return;
    }
    std::string parent = path.substr(0, pos);
    if (parent.empty()) {
        return;
    }
    std::string partial;
    for (char ch : parent) {
        partial.push_back(ch);
        if ((ch == '/' || ch == '\\') && partial.size() > 1) {
            _mkdir(partial.c_str());
        }
    }
    _mkdir(parent.c_str());
}

}  // namespace

std::vector<std::string> split_tags(const std::string& raw_tags) {
    std::string normalized = raw_tags;
    replace_all(normalized, "，", ",");
    replace_all(normalized, "；", ",");
    replace_all(normalized, ";", ",");
    replace_all(normalized, "/", ",");
    replace_all(normalized, "|", ",");

    std::vector<std::string> parts = split_simple(normalized, ',');
    std::vector<std::string> result;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        std::string tag = trim(parts[i]);
        if (!tag.empty()) {
            result.push_back(tag);
        }
    }
    return result;
}

std::vector<RawVideo> load_raw_videos(const std::string& csv_path,
                                      std::size_t max_rows) {
    std::ifstream input(csv_path.c_str());
    if (!input) {
        throw std::runtime_error("cannot open csv file: " + csv_path);
    }

    std::vector<RawVideo> videos;
    std::string line;
    if (!std::getline(input, line)) {
        return videos;
    }

    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }
        std::vector<std::string> fields = split_csv_line(line);
        if (fields.size() < 17) {
            continue;
        }

        RawVideo video;
        video.aid = parse_ll(fields[0]);
        video.bvid = trim(fields[1]);
        video.title = trim(fields[2]);
        video.category = trim(fields[3]);
        video.author = trim(fields[4]);
        video.duration = parse_int(fields[5]);
        video.pubdate = parse_ll(fields[6]);
        video.view_count = parse_ll(fields[7]);
        video.favorite = parse_ll(fields[10]);
        video.coin = parse_ll(fields[11]);
        video.share = parse_ll(fields[12]);
        video.like = parse_ll(fields[13]);
        video.tag = trim(fields[16]);
        videos.push_back(video);

        if (max_rows > 0 && videos.size() >= max_rows) {
            break;
        }
    }

    return videos;
}

Catalog build_catalog(const std::vector<RawVideo>& raw_videos,
                      const VideoBuildConfig& config) {
    Catalog catalog;
    int topic_count = std::max(1, config.topic_count);
    catalog.topic_to_video_indices.assign(topic_count, std::vector<int>());

    std::unordered_map<std::string, int> tag_frequency;
    for (const RawVideo& raw : raw_videos) {
        std::vector<std::string> tags = split_tags(raw.tag);
        if (tags.empty()) {
            std::string category = trim(raw.category);
            if (category.empty()) {
                category = "unknown-category";
            }
            tags.push_back("category:" + category);
        }
        for (const std::string& tag : tags) {
            tag_frequency[tag] += 1;
        }
    }

    std::vector<double> raw_quality;
    raw_quality.reserve(raw_videos.size());
    for (const RawVideo& raw : raw_videos) {
        double score = 0.45 * std::log(1.0 + static_cast<double>(raw.view_count))
                     + 0.20 * rate(raw.like, raw.view_count)
                     + 0.15 * rate(raw.favorite, raw.view_count)
                     + 0.15 * rate(raw.coin, raw.view_count)
                     + 0.05 * rate(raw.share, raw.view_count);
        raw_quality.push_back(score);
    }

    double min_quality = raw_quality.empty() ? 0.0 : raw_quality[0];
    double max_quality = raw_quality.empty() ? 1.0 : raw_quality[0];
    for (double value : raw_quality) {
        min_quality = std::min(min_quality, value);
        max_quality = std::max(max_quality, value);
    }
    double quality_range = std::max(0.000001, max_quality - min_quality);

    for (std::size_t i = 0; i < raw_videos.size(); ++i) {
        const RawVideo& raw = raw_videos[i];
        ModeledVideo video;
        video.video_id = raw.aid;
        video.bvid = raw.bvid;
        video.title = raw.title;
        video.category_name = trim(raw.category);
        if (video.category_name.empty()) {
            video.category_name = "unknown-category";
        }
        video.category_id = get_or_add_id(catalog.category_to_id, video.category_name);
        video.duration_sec = std::max(1, raw.duration);
        video.publish_ts = raw.pubdate;
        video.raw_view_count = raw.view_count;
        video.raw_like = raw.like;
        video.raw_favorite = raw.favorite;
        video.raw_coin = raw.coin;
        video.raw_share = raw.share;
        video.quality_score = clamp01((raw_quality[i] - min_quality) / quality_range);
        video.topic_vector.assign(topic_count, 0.0);

        int category_topic = topic_from_text(video.category_name, topic_count);
        if (category_topic < 0) {
            category_topic = hash_topic(video.category_name, topic_count);
        }
        video.topic_vector[category_topic] += 0.60;

        std::vector<std::string> tags = split_tags(raw.tag);
        if (tags.empty()) {
            tags.push_back("category:" + video.category_name);
        }

        for (std::string tag : tags) {
            if (tag_frequency[tag] < config.min_tag_frequency) {
                tag = "rare-tag";
            }
            int tag_id = get_or_add_id(catalog.tag_to_id, tag);
            video.tag_ids.push_back(tag_id);
            int tag_topic = topic_from_text(tag, topic_count);
            if (tag_topic >= 0) {
                video.topic_vector[tag_topic] += 0.20;
            }
        }

        int title_topic = topic_from_text(video.title, topic_count);
        if (title_topic >= 0) {
            video.topic_vector[title_topic] += 0.08;
        }

        normalize(video.topic_vector);
        video.topic_id = max_index(video.topic_vector);

        int video_index = static_cast<int>(catalog.videos.size());
        catalog.topic_to_video_indices[video.topic_id].push_back(video_index);
        catalog.videos.push_back(video);
    }

    return catalog;
}

Catalog load_processed_videos(const std::string& csv_path,
                              const VideoBuildConfig& config) {
    std::ifstream input(csv_path.c_str());
    if (!input) {
        throw std::runtime_error("cannot open processed videos file: " + csv_path);
    }

    Catalog catalog;
    int topic_count = std::max(1, config.topic_count);
    catalog.topic_to_video_indices.assign(topic_count, std::vector<int>());

    std::string line;
    if (!std::getline(input, line)) {
        return catalog;
    }

    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }

        std::vector<std::string> fields = split_csv_line(line);
        if (fields.size() < 16) {
            continue;
        }

        ModeledVideo video;
        video.video_id = parse_ll(fields[0]);
        video.bvid = trim(fields[1]);
        video.title = trim(fields[2]);
        video.category_id = parse_int(fields[3]);
        video.category_name = trim(fields[4]);
        video.tag_ids = parse_int_list(fields[5]);
        video.duration_sec = std::max(1, parse_int(fields[6]));
        video.publish_ts = parse_ll(fields[7]);
        video.raw_view_count = parse_ll(fields[8]);
        video.raw_like = parse_ll(fields[9]);
        video.raw_favorite = parse_ll(fields[10]);
        video.raw_coin = parse_ll(fields[11]);
        video.raw_share = parse_ll(fields[12]);
        video.quality_score = clamp01(parse_double(fields[13]));
        video.topic_id = parse_int(fields[14]);
        video.topic_vector = parse_double_list(fields[15]);
        if (video.topic_vector.empty()) {
            video.topic_vector.assign(topic_count, 0.0);
            video.topic_vector[std::max(0, std::min(topic_count - 1, video.topic_id))] = 1.0;
        }

        int video_index = static_cast<int>(catalog.videos.size());
        if (video.topic_id >= 0 && video.topic_id < topic_count) {
            catalog.topic_to_video_indices[video.topic_id].push_back(video_index);
        }
        catalog.category_to_id[video.category_name] = video.category_id;
        catalog.videos.push_back(video);
    }

    return catalog;
}

bool write_videos_csv(const std::string& path, const Catalog& catalog) {
    ensure_parent_dir(path);
    std::ofstream output(path.c_str());
    if (!output) {
        return false;
    }
    output << "video_id,bvid,title,category_id,category_name,tag_ids,duration_sec,publish_ts,"
           << "raw_view_count,raw_like,raw_favorite,raw_coin,raw_share,quality_score,"
           << "topic_id,topic_vector\n";
    for (const ModeledVideo& video : catalog.videos) {
        output << video.video_id << ',' << video.bvid << ','
               << csv_escape(video.title) << ',' << video.category_id << ','
               << csv_escape(video.category_name) << ',' << csv_escape(join_ints(video.tag_ids)) << ','
               << video.duration_sec << ',' << video.publish_ts << ','
               << video.raw_view_count << ',' << video.raw_like << ','
               << video.raw_favorite << ',' << video.raw_coin << ','
               << video.raw_share << ',' << std::fixed << std::setprecision(6)
               << video.quality_score << ',' << video.topic_id << ','
               << csv_escape(join_doubles(video.topic_vector)) << '\n';
    }
    return true;
}

}  // namespace datastr
