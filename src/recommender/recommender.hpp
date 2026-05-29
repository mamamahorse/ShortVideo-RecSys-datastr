#ifndef DATASTR_RECOMMENDER_HPP
#define DATASTR_RECOMMENDER_HPP

#include "../common/vector_builder.hpp"
#include "../data_builder/video_cleaner.hpp"

#include <string>
#include <vector>

namespace datastr {

struct RecommendedVideo {
    long long video_id = 0;
    double recommend_score = 0.0;
    double interest_match = 0.0;
    double collaborative_score = 0.0;
    double quality_score = 0.0;
    double freshness_score = 0.0;
};

struct RecommendationResult {
    int user_id = 0;
    std::vector<RecommendedVideo> videos;
};

struct RecommendConfig {
    int top_n = 50;
    int candidate_pool_size = 200;
    int max_similar_users = 10;
    double interest_weight = 0.45;
    double collaborative_weight = 0.25;
    double quality_weight = 0.20;
    double freshness_weight = 0.10;
    long long current_timestamp = 1735000000LL;
};

RecommendationResult recommend_videos(int user_id,
                                      const Catalog& catalog,
                                      const std::vector<UserInterestVector>& user_vectors,
                                      const std::vector<VideoFeatureVector>& video_vectors,
                                      const std::vector<VectorEvent>& events,
                                      const RecommendConfig& config);

bool write_recommendations_csv(const std::string& path, const RecommendationResult& result);

}  // namespace datastr

#endif  // DATASTR_RECOMMENDER_HPP