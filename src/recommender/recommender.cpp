#include "recommender.hpp"

#include <algorithm>
#include <cmath>
#include <direct.h>
#include <fstream>
#include <iomanip>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace datastr {
namespace {

double dot_product(const std::vector<double>& a, const std::vector<double>& b) {
    double result = 0.0;
    std::size_t n = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < n; ++i) {
        result += a[i] * b[i];
    }
    return result;
}

double calculate_freshness(const ModeledVideo& video, long long current_ts) {
    if (current_ts <= video.publish_ts) {
        return 1.0;
    }
    double age_days = static_cast<double>(current_ts - video.publish_ts) / 86400.0;
    return 1.0 / (1.0 + age_days / 120.0);
}

std::vector<int> find_similar_users(int user_id,
                                    const std::vector<UserInterestVector>& user_vectors,
                                    int top_k) {
    std::vector<int> similar_user_ids;
    if (top_k <= 0) {
        return similar_user_ids;
    }

    int target_index = -1;
    for (std::size_t i = 0; i < user_vectors.size(); ++i) {
        if (user_vectors[i].user_id == user_id) {
            target_index = static_cast<int>(i);
            break;
        }
    }

    if (target_index < 0) {
        return similar_user_ids;
    }

    const std::vector<double>& target_vec = user_vectors[target_index].values;

    using ScorePair = std::pair<double, int>;
    auto compare = [](const ScorePair& a, const ScorePair& b) {
        return a.first > b.first;
    };
    std::priority_queue<ScorePair, std::vector<ScorePair>, decltype(compare)> min_heap(compare);

    for (std::size_t i = 0; i < user_vectors.size(); ++i) {
        if (static_cast<int>(i) == target_index) {
            continue;
        }

        const auto& other = user_vectors[i];
        double dot = 0.0, norm_a = 0.0, norm_b = 0.0;
        std::size_t n = std::min(target_vec.size(), other.values.size());
        for (std::size_t j = 0; j < n; ++j) {
            dot += target_vec[j] * other.values[j];
            norm_a += target_vec[j] * target_vec[j];
            norm_b += other.values[j] * other.values[j];
        }
        double sim = (norm_a > 0 && norm_b > 0) ? dot / (std::sqrt(norm_a) * std::sqrt(norm_b)) : 0.0;

        if (min_heap.size() < static_cast<std::size_t>(top_k)) {
            min_heap.push({sim, other.user_id});
        } else if (sim > min_heap.top().first) {
            min_heap.pop();
            min_heap.push({sim, other.user_id});
        }
    }

    while (!min_heap.empty()) {
        similar_user_ids.push_back(min_heap.top().second);
        min_heap.pop();
    }
    std::reverse(similar_user_ids.begin(), similar_user_ids.end());
    return similar_user_ids;
}

}  // namespace

RecommendationResult recommend_videos(int user_id,
                                      const Catalog& catalog,
                                      const std::vector<UserInterestVector>& user_vectors,
                                      const std::vector<VideoFeatureVector>& video_vectors,
                                      const std::vector<VectorEvent>& events,
                                      const RecommendConfig& config) {
    RecommendationResult result;
    result.user_id = user_id;

    if (catalog.videos.empty() || user_vectors.empty()) {
        return result;
    }

    int user_index = -1;
    std::unordered_map<int, int> user_id_map;
    for (std::size_t i = 0; i < user_vectors.size(); ++i) {
        user_id_map[user_vectors[i].user_id] = static_cast<int>(i);
        if (user_vectors[i].user_id == user_id) {
            user_index = static_cast<int>(i);
        }
    }

    if (user_index < 0) {
        return result;
    }

    const UserInterestVector& target_user = user_vectors[user_index];
    std::unordered_set<long long> watched_videos;
    for (const VectorEvent& event : events) {
        if (event.user_id == user_id) {
            watched_videos.insert(event.video_id);
        }
    }

    std::unordered_map<long long, int> video_id_map;
    for (std::size_t i = 0; i < catalog.videos.size(); ++i) {
        video_id_map[catalog.videos[i].video_id] = static_cast<int>(i);
    }

    std::vector<int> similar_users = find_similar_users(user_id, user_vectors, config.max_similar_users);
    std::unordered_map<long long, double> collaborative_scores;
    for (int sim_user_id : similar_users) {
        for (const VectorEvent& event : events) {
            if (event.user_id == sim_user_id && event.feedback_score > 0) {
                collaborative_scores[event.video_id] += event.feedback_score;
            }
        }
    }

    double max_collab = 1.0;
    for (const auto& entry : collaborative_scores) {
        max_collab = std::max(max_collab, entry.second);
    }

    std::unordered_set<int> candidate_indices;
    int primary_topic = -1;
    double max_interest = 0.0;
    for (std::size_t i = 0; i < target_user.values.size(); ++i) {
        if (target_user.values[i] > max_interest) {
            max_interest = target_user.values[i];
            primary_topic = static_cast<int>(i);
        }
    }

    if (primary_topic >= 0 && primary_topic < static_cast<int>(catalog.topic_to_video_indices.size())) {
        const auto& topic_videos = catalog.topic_to_video_indices[primary_topic];
        int sample_size = std::min(static_cast<int>(topic_videos.size()), config.candidate_pool_size / 2);
        for (int i = 0; i < sample_size; ++i) {
            candidate_indices.insert(topic_videos[i]);
        }
    }

    for (const auto& entry : collaborative_scores) {
        if (candidate_indices.size() >= static_cast<std::size_t>(config.candidate_pool_size)) {
            break;
        }
        auto found = video_id_map.find(entry.first);
        if (found != video_id_map.end()) {
            candidate_indices.insert(found->second);
        }
    }

    for (std::size_t i = 0; i < catalog.videos.size() && candidate_indices.size() < static_cast<std::size_t>(config.candidate_pool_size); ++i) {
        candidate_indices.insert(static_cast<int>(i));
    }

    using VideoScore = std::pair<double, RecommendedVideo>;
    auto compare = [](const VideoScore& a, const VideoScore& b) {
        return a.first > b.first;
    };
    std::priority_queue<VideoScore, std::vector<VideoScore>, decltype(compare)> min_heap(compare);

    for (int idx : candidate_indices) {
        const ModeledVideo& video = catalog.videos[idx];
        if (watched_videos.count(video.video_id)) {
            continue;
        }

        double interest_match = 0.0;
        if (idx < static_cast<int>(video_vectors.size())) {
            const VideoFeatureVector& feature = video_vectors[idx];
            interest_match = dot_product(target_user.values, feature.values);
        } else {
            interest_match = dot_product(target_user.values, video.topic_vector);
        }

        double collab_score = 0.0;
        auto collab_found = collaborative_scores.find(video.video_id);
        if (collab_found != collaborative_scores.end()) {
            collab_score = collab_found->second / max_collab;
        }

        double freshness = calculate_freshness(video, config.current_timestamp);

        double score = config.interest_weight * interest_match +
                       config.collaborative_weight * collab_score +
                       config.quality_weight * video.quality_score +
                       config.freshness_weight * freshness;

        RecommendedVideo rv;
        rv.video_id = video.video_id;
        rv.recommend_score = score;
        rv.interest_match = interest_match;
        rv.collaborative_score = collab_score;
        rv.quality_score = video.quality_score;
        rv.freshness_score = freshness;

        if (min_heap.size() < static_cast<std::size_t>(config.top_n)) {
            min_heap.push({score, rv});
        } else if (score > min_heap.top().first) {
            min_heap.pop();
            min_heap.push({score, rv});
        }
    }

    while (!min_heap.empty()) {
        result.videos.push_back(min_heap.top().second);
        min_heap.pop();
    }
    std::reverse(result.videos.begin(), result.videos.end());

    return result;
}

bool write_recommendations_csv(const std::string& path, const RecommendationResult& result) {
    std::size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        std::string parent = path.substr(0, pos);
        std::string partial;
        for (char ch : parent) {
            partial.push_back(ch);
            if ((ch == '/' || ch == '\\') && partial.size() > 1) {
                _mkdir(partial.c_str());
            }
        }
        _mkdir(parent.c_str());
    }

    std::ofstream output(path.c_str());
    if (!output) {
        return false;
    }

    output << "user_id,video_id,recommend_score,interest_match,collaborative_score,quality_score,freshness_score\n";
    for (const RecommendedVideo& rv : result.videos) {
        output << result.user_id << ',' << rv.video_id << ','
               << std::fixed << std::setprecision(6)
               << rv.recommend_score << ',' << rv.interest_match << ','
               << rv.collaborative_score << ',' << rv.quality_score << ','
               << rv.freshness_score << '\n';
    }
    return true;
}

}  // namespace datastr