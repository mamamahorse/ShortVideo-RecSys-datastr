#include "popularity_predictor.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <direct.h>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>

namespace datastr {
namespace {

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

long long parse_ll(const std::string& text) {
    return std::atoll(text.c_str());
}

int parse_int(const std::string& text) {
    return std::atoi(text.c_str());
}

void ensure_parent_dir(const std::string& path) {
    std::size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return;
    std::string parent = path.substr(0, pos);
    if (parent.empty()) return;
    std::string partial;
    for (char ch : parent) {
        partial.push_back(ch);
        if ((ch == '/' || ch == '\\') && partial.size() > 1) {
            _mkdir(partial.c_str());
        }
    }
    _mkdir(parent.c_str());
}

std::string join_ints(const std::vector<int>& values) {
    std::ostringstream output;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) output << ';';
        output << values[i];
    }
    return output.str();
}

std::string join_doubles(const std::vector<double>& values) {
    std::ostringstream output;
    output << std::fixed << std::setprecision(2);
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) output << ';';
        output << values[i];
    }
    return output.str();
}

struct VideoMeta {
    long long publish_ts = 0;
};

std::unordered_map<long long, VideoMeta> load_video_meta(const std::string& path) {
    std::unordered_map<long long, VideoMeta> meta;
    std::ifstream input(path.c_str());
    if (!input) return meta;

    std::string line;
    if (!std::getline(input, line)) return meta;

    while (std::getline(input, line)) {
        if (line.empty()) continue;
        std::vector<std::string> fields = split_simple(line, ',');
        if (fields.size() < 8) continue;
        long long video_id = parse_ll(fields[0]);
        long long publish_ts = parse_ll(fields[7]);
        meta[video_id] = {publish_ts};
    }
    return meta;
}

}  // namespace

std::vector<PopularityPrediction> predict_popularity(
    const std::string& events_csv,
    const std::string& videos_csv,
    const PredictorConfig& config) {

    std::unordered_map<long long, VideoMeta> video_meta = load_video_meta(videos_csv);

    // phase 1: aggregate events by (video_id, day_offset)
    // day_offset = days since publish, clamped to >= 0
    std::unordered_map<long long, std::unordered_map<int, int>> video_day_counts;

    std::ifstream input(events_csv.c_str());
    if (!input) {
        return {};
    }

    std::string line;
    if (!std::getline(input, line)) return {};

    while (std::getline(input, line)) {
        if (line.empty()) continue;
        std::vector<std::string> fields = split_simple(line, ',');
        if (fields.size() < 4) continue;

        long long video_id = parse_ll(fields[2]);
        long long timestamp = parse_ll(fields[3]);

        auto meta_it = video_meta.find(video_id);
        if (meta_it == video_meta.end()) continue;

        long long publish_ts = meta_it->second.publish_ts;
        int day_offset = static_cast<int>((timestamp - publish_ts) / 86400);
        if (day_offset < 0) day_offset = 0;

        ++video_day_counts[video_id][day_offset];
    }

    // phase 2: build time series, smooth, and predict
    std::vector<PopularityPrediction> results;

    for (const auto& entry : video_day_counts) {
        long long video_id = entry.first;
        const std::unordered_map<int, int>& day_map = entry.second;

        int total_events = 0;
        int max_day = 0;
        for (const auto& day_entry : day_map) {
            total_events += day_entry.second;
            if (day_entry.first > max_day) max_day = day_entry.first;
        }

        if (total_events < config.min_events) continue;
        if (max_day < 3) continue;

        // build dense daily_views array
        int day_count = max_day + 1;
        std::vector<int> daily_views(day_count, 0);
        for (const auto& day_entry : day_map) {
            daily_views[day_entry.first] = day_entry.second;
        }

        // exponential smoothing
        std::vector<double> smooth(day_count, 0.0);
        smooth[0] = static_cast<double>(daily_views[0]);
        for (int d = 1; d < day_count; ++d) {
            smooth[d] = config.alpha * daily_views[d] +
                        (1.0 - config.alpha) * smooth[d - 1];
        }

        // trend from recent window
        int window = config.window_days;
        if (window > day_count) window = day_count;
        int half = window / 2;
        if (half < 1) half = 1;

        double first_half_sum = 0.0;
        double second_half_sum = 0.0;
        for (int i = 0; i < half; ++i) {
            first_half_sum += daily_views[day_count - window + i];
            second_half_sum += daily_views[day_count - half + i];
        }
        double first_half_avg = first_half_sum / half;
        double second_half_avg = second_half_sum / half;
        double trend = 0.0;
        if (first_half_avg > 0.0) {
            trend = (second_half_avg - first_half_avg) / first_half_avg;
        }

        // predict future days
        double last_smooth = smooth[day_count - 1];
        double daily_trend = (second_half_avg - first_half_avg) / half;
        std::vector<double> predicted(config.predict_days, 0.0);
        for (int p = 0; p < config.predict_days; ++p) {
            predicted[p] = std::max(0.0,
                last_smooth + config.beta * daily_trend * (p + 1));
        }

        // trend direction string
        std::string direction = "平稳";
        if (trend > config.trend_threshold) {
            direction = "上升";
        } else if (trend < -config.trend_threshold) {
            direction = "下降";
        }

        // find peak day
        int peak_day = 0;
        int peak_views = daily_views[0];
        for (int d = 1; d < day_count; ++d) {
            if (daily_views[d] > peak_views) {
                peak_views = daily_views[d];
                peak_day = d;
            }
        }

        PopularityPrediction pred;
        pred.video_id = video_id;
        pred.total_events = total_events;
        pred.day_count = day_count;
        pred.daily_views = join_ints(daily_views);
        pred.smooth_values = join_doubles(smooth);
        pred.predicted_views = join_doubles(predicted);
        pred.trend_direction = direction;
        pred.trend_value = trend;
        pred.peak_day = peak_day;
        pred.last_day = max_day;

        results.push_back(pred);
    }

    return results;
}

bool write_predictions_csv(const std::string& path,
                           const std::vector<PopularityPrediction>& predictions) {
    ensure_parent_dir(path);
    std::ofstream output(path.c_str());
    if (!output) return false;

    output << "video_id,total_events,day_count,daily_views,smooth_values,"
              "trend_direction,trend_value,predicted_views,peak_day,last_day\n";

    for (const auto& pred : predictions) {
        output << pred.video_id << ','
               << pred.total_events << ','
               << pred.day_count << ','
               << '"' << pred.daily_views << "\","
               << '"' << pred.smooth_values << "\","
               << pred.trend_direction << ','
               << std::fixed << std::setprecision(4) << pred.trend_value << ','
               << '"' << pred.predicted_views << "\","
               << pred.peak_day << ','
               << pred.last_day << '\n';
    }

    return true;
}

}  // namespace datastr
