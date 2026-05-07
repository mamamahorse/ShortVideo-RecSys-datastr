#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "../src/common/vector_builder.hpp"

namespace {

void expect_true(bool value, const std::string& message) {
    if (!value) {
        std::cerr << "FAIL: " << message << std::endl;
        std::exit(1);
    }
}

void expect_near(double actual, double expected, double eps, const std::string& message) {
    if (std::fabs(actual - expected) > eps) {
        std::cerr << "FAIL: " << message << " actual=" << actual
                  << " expected=" << expected << std::endl;
        std::exit(1);
    }
}

double l2_norm(const std::vector<double>& values) {
    double total = 0.0;
    for (double value : values) {
        total += value * value;
    }
    return std::sqrt(total);
}

}  // namespace

int main() {
    using namespace datastr;

    Catalog catalog;
    catalog.topic_to_video_indices.assign(12, std::vector<int>());

    ModeledVideo video_a;
    video_a.video_id = 101;
    video_a.topic_id = 0;
    video_a.topic_vector.assign(12, 0.0);
    video_a.topic_vector[0] = 1.0;
    video_a.quality_score = 0.8;
    video_a.duration_sec = 100;
    catalog.videos.push_back(video_a);

    ModeledVideo video_b;
    video_b.video_id = 202;
    video_b.topic_id = 1;
    video_b.topic_vector.assign(12, 0.0);
    video_b.topic_vector[1] = 1.0;
    video_b.quality_score = 0.2;
    video_b.duration_sec = 200;
    catalog.videos.push_back(video_b);

    std::vector<VectorUser> users;
    users.push_back(VectorUser{1, 0});
    users.push_back(VectorUser{2, 1});

    std::vector<VectorEvent> events;
    events.push_back(VectorEvent{1, 101, 2.0});
    events.push_back(VectorEvent{2, 202, 3.0});
    events.push_back(VectorEvent{2, 101, 1.0});

    VectorBuildConfig config;
    config.topic_count = 12;
    config.group_count = 12;
    config.audience_min_events = 2;

    VectorBuildResult result = build_vectors(catalog, users, events, config);

    expect_true(result.user_interest_vectors.size() == 2,
                "should build one interest vector per user");
    expect_near(result.user_interest_vectors[0].values[0], 1.0, 0.000001,
                "user 1 should fully prefer topic 0");
    expect_near(result.user_interest_vectors[1].values[0], 0.25, 0.000001,
                "user 2 should keep weaker feedback on topic 0");
    expect_near(result.user_interest_vectors[1].values[1], 0.75, 0.000001,
                "user 2 should keep stronger feedback on topic 1");

    expect_true(result.video_audience_vectors.size() == 2,
                "should build one audience vector per video");
    expect_near(result.video_audience_vectors[0].values[0], 2.0 / 3.0, 0.000001,
                "video A audience should include group 0 feedback");
    expect_near(result.video_audience_vectors[0].values[1], 1.0 / 3.0, 0.000001,
                "video A audience should include group 1 feedback");
    expect_near(result.video_audience_vectors[1].values[1], 1.0, 0.000001,
                "video B audience should be all group 1");

    expect_true(result.video_feature_vectors.size() == 2,
                "should build one feature vector per video");
    expect_true(result.video_feature_vectors[0].has_audience,
                "video A should use audience after reaching the event threshold");
    expect_true(!result.video_feature_vectors[1].has_audience,
                "video B should skip audience below the event threshold");
    expect_true(result.video_feature_vectors[0].values[12] > 0.0,
                "video A feature should contain audience dimensions");
    expect_near(result.video_feature_vectors[1].values[12], 0.0, 0.000001,
                "video B feature should keep audience dimensions zero");
    expect_near(l2_norm(result.video_feature_vectors[0].values), 1.0, 0.000001,
                "video feature vectors should be L2-normalized for fast dot products");

    std::cout << "vector builder tests passed" << std::endl;
    return 0;
}
