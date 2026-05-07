#ifndef DATASTR_VECTOR_BUILDER_HPP
#define DATASTR_VECTOR_BUILDER_HPP

#include "../data_builder/video_cleaner.hpp"

#include <string>
#include <vector>

namespace datastr {

struct VectorUser {
    int user_id = 0;
    int group_id = 0;
};

struct VectorEvent {
    int user_id = 0;
    long long video_id = 0;
    double feedback_score = 0.0;
};

struct VectorBuildConfig {
    int topic_count = 12;
    int group_count = 12;
    int audience_min_events = 10;
    double topic_weight_with_audience = 0.60;
    double audience_weight = 0.25;
    double topic_weight_without_audience = 0.85;
    double quality_weight = 0.10;
    double duration_weight = 0.05;
};

struct UserInterestVector {
    int user_id = 0;
    int group_id = 0;
    int event_count = 0;
    std::vector<double> values;
};

struct VideoAudienceVector {
    long long video_id = 0;
    int event_count = 0;
    bool has_audience = false;
    std::vector<double> values;
};

struct VideoFeatureVector {
    long long video_id = 0;
    int topic_id = 0;
    int event_count = 0;
    bool has_audience = false;
    std::vector<double> values;
};

struct VectorBuildResult {
    std::vector<UserInterestVector> user_interest_vectors;
    std::vector<VideoAudienceVector> video_audience_vectors;
    std::vector<VideoFeatureVector> video_feature_vectors;
};

std::vector<VectorUser> load_vector_users_csv(const std::string& path);
std::vector<VectorEvent> load_vector_events_csv(const std::string& path);

VectorBuildResult build_vectors(const Catalog& catalog,
                                const std::vector<VectorUser>& users,
                                const std::vector<VectorEvent>& events,
                                const VectorBuildConfig& config);

bool write_user_interest_vectors_csv(const std::string& path,
                                     const std::vector<UserInterestVector>& vectors);
bool write_video_audience_vectors_csv(const std::string& path,
                                      const std::vector<VideoAudienceVector>& vectors);
bool write_video_feature_vectors_csv(const std::string& path,
                                     const std::vector<VideoFeatureVector>& vectors);

}  // namespace datastr

#endif  // DATASTR_VECTOR_BUILDER_HPP
