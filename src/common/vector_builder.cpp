#include "vector_builder.hpp"

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

int parse_int(const std::string& text) {
    return std::atoi(text.c_str());
}

long long parse_ll(const std::string& text) {
    return std::atoll(text.c_str());
}

double parse_double(const std::string& text) {
    return std::atof(text.c_str());
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

void normalize_sum(std::vector<double>& values) {
    double sum = 0.0;
    for (double value : values) {
        sum += value;
    }
    if (sum <= 0.0) {
        return;
    }
    for (double& value : values) {
        value /= sum;
    }
}

void normalize_l2(std::vector<double>& values) {
    double norm = 0.0;
    for (double value : values) {
        norm += value * value;
    }
    if (norm <= 0.0) {
        return;
    }
    norm = std::sqrt(norm);
    for (double& value : values) {
        value /= norm;
    }
}

double topic_value(const ModeledVideo& video, int index) {
    if (index < 0 || index >= static_cast<int>(video.topic_vector.size())) {
        return 0.0;
    }
    return video.topic_vector[index];
}

}  // namespace

std::vector<VectorUser> load_vector_users_csv(const std::string& path) {
    std::ifstream input(path.c_str());
    std::vector<VectorUser> users;
    if (!input) {
        return users;
    }

    std::string line;
    if (!std::getline(input, line)) {
        return users;
    }

    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }
        std::vector<std::string> fields = split_simple(line, ',');
        if (fields.size() < 2) {
            continue;
        }
        VectorUser user;
        user.user_id = parse_int(fields[0]);
        user.group_id = parse_int(fields[1]);
        users.push_back(user);
    }
    return users;
}

std::vector<VectorEvent> load_vector_events_csv(const std::string& path) {
    std::ifstream input(path.c_str());
    std::vector<VectorEvent> events;
    if (!input) {
        return events;
    }

    std::string line;
    if (!std::getline(input, line)) {
        return events;
    }

    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }
        std::vector<std::string> fields = split_simple(line, ',');
        if (fields.size() < 13) {
            continue;
        }
        VectorEvent event;
        event.user_id = parse_int(fields[1]);
        event.video_id = parse_ll(fields[2]);
        event.feedback_score = parse_double(fields[12]);
        events.push_back(event);
    }
    return events;
}

VectorBuildResult build_vectors(const Catalog& catalog,
                                const std::vector<VectorUser>& users,
                                const std::vector<VectorEvent>& events,
                                const VectorBuildConfig& config) {
    VectorBuildResult result;
    int topic_count = std::max(1, config.topic_count);
    int group_count = std::max(1, config.group_count);
    int feature_dim = topic_count + group_count + 2;

    result.user_interest_vectors.reserve(users.size());
    std::unordered_map<int, int> user_id_to_index;
    for (std::size_t i = 0; i < users.size(); ++i) {
        UserInterestVector vector;
        vector.user_id = users[i].user_id;
        vector.group_id = users[i].group_id;
        vector.values.assign(topic_count, 0.0);
        user_id_to_index[users[i].user_id] = static_cast<int>(i);
        result.user_interest_vectors.push_back(vector);
    }

    result.video_audience_vectors.reserve(catalog.videos.size());
    result.video_feature_vectors.reserve(catalog.videos.size());
    std::unordered_map<long long, int> video_id_to_index;
    int max_duration = 1;
    for (std::size_t i = 0; i < catalog.videos.size(); ++i) {
        const ModeledVideo& video = catalog.videos[i];
        video_id_to_index[video.video_id] = static_cast<int>(i);
        max_duration = std::max(max_duration, video.duration_sec);

        VideoAudienceVector audience;
        audience.video_id = video.video_id;
        audience.values.assign(group_count, 0.0);
        result.video_audience_vectors.push_back(audience);
    }

    for (const VectorEvent& event : events) {
        std::unordered_map<int, int>::const_iterator user_found =
            user_id_to_index.find(event.user_id);
        std::unordered_map<long long, int>::const_iterator video_found =
            video_id_to_index.find(event.video_id);
        if (user_found == user_id_to_index.end() || video_found == video_id_to_index.end()) {
            continue;
        }

        int user_index = user_found->second;
        int video_index = video_found->second;
        double feedback = std::max(0.0, event.feedback_score);
        const ModeledVideo& video = catalog.videos[video_index];

        result.user_interest_vectors[user_index].event_count += 1;
        result.video_audience_vectors[video_index].event_count += 1;

        for (int topic = 0; topic < topic_count; ++topic) {
            result.user_interest_vectors[user_index].values[topic] +=
                feedback * topic_value(video, topic);
        }

        int group_id = users[user_index].group_id;
        if (group_id >= 0 && group_id < group_count) {
            result.video_audience_vectors[video_index].values[group_id] += feedback;
        }
    }

    for (UserInterestVector& vector : result.user_interest_vectors) {
        normalize_sum(vector.values);
    }
    for (VideoAudienceVector& vector : result.video_audience_vectors) {
        vector.has_audience = vector.event_count >= config.audience_min_events;
        normalize_sum(vector.values);
    }

    double duration_denominator = std::log(1.0 + static_cast<double>(max_duration));
    for (std::size_t i = 0; i < catalog.videos.size(); ++i) {
        const ModeledVideo& video = catalog.videos[i];
        const VideoAudienceVector& audience = result.video_audience_vectors[i];
        bool use_audience = audience.has_audience;

        double topic_weight = use_audience ? config.topic_weight_with_audience
                                           : config.topic_weight_without_audience;
        double audience_weight = use_audience ? config.audience_weight : 0.0;

        VideoFeatureVector feature;
        feature.video_id = video.video_id;
        feature.topic_id = video.topic_id;
        feature.event_count = audience.event_count;
        feature.has_audience = use_audience;
        feature.values.assign(feature_dim, 0.0);

        for (int topic = 0; topic < topic_count; ++topic) {
            feature.values[topic] = topic_weight * topic_value(video, topic);
        }
        for (int group = 0; group < group_count; ++group) {
            feature.values[topic_count + group] = audience_weight * audience.values[group];
        }
        feature.values[topic_count + group_count] =
            config.quality_weight * std::max(0.0, std::min(1.0, video.quality_score));
        feature.values[topic_count + group_count + 1] =
            config.duration_weight *
            (duration_denominator > 0.0
                 ? std::log(1.0 + static_cast<double>(std::max(1, video.duration_sec))) /
                       duration_denominator
                 : 0.0);
        normalize_l2(feature.values);
        result.video_feature_vectors.push_back(feature);
    }

    return result;
}

bool write_user_interest_vectors_csv(const std::string& path,
                                     const std::vector<UserInterestVector>& vectors) {
    ensure_parent_dir(path);
    std::ofstream output(path.c_str());
    if (!output) {
        return false;
    }
    output << "user_id,group_id,event_count,interest_vector\n";
    for (const UserInterestVector& vector : vectors) {
        output << vector.user_id << ',' << vector.group_id << ',' << vector.event_count
               << ",\"" << join_doubles(vector.values) << "\"\n";
    }
    return true;
}

bool write_video_audience_vectors_csv(const std::string& path,
                                      const std::vector<VideoAudienceVector>& vectors) {
    ensure_parent_dir(path);
    std::ofstream output(path.c_str());
    if (!output) {
        return false;
    }
    output << "video_id,event_count,has_audience,audience_vector\n";
    for (const VideoAudienceVector& vector : vectors) {
        output << vector.video_id << ',' << vector.event_count << ','
               << (vector.has_audience ? 1 : 0) << ",\""
               << join_doubles(vector.values) << "\"\n";
    }
    return true;
}

bool write_video_feature_vectors_csv(const std::string& path,
                                     const std::vector<VideoFeatureVector>& vectors) {
    ensure_parent_dir(path);
    std::ofstream output(path.c_str());
    if (!output) {
        return false;
    }
    output << "video_id,topic_id,event_count,has_audience,feature_vector\n";
    for (const VideoFeatureVector& vector : vectors) {
        output << vector.video_id << ',' << vector.topic_id << ',' << vector.event_count
               << ',' << (vector.has_audience ? 1 : 0) << ",\""
               << join_doubles(vector.values) << "\"\n";
    }
    return true;
}

}  // namespace datastr
