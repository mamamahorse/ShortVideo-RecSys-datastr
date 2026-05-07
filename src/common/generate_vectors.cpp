#include "vector_builder.hpp"

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

}  // namespace

int main(int argc, char** argv) {
    using namespace datastr;

    std::string videos_csv = argc > 1 ? argv[1] : "data/processed/videos.csv";
    std::string users_csv = argc > 2 ? argv[2] : "data/simulated/users.csv";
    std::string events_csv = argc > 3 ? argv[3] : "data/simulated/events.csv";
    std::string output_dir = argc > 4 ? argv[4] : "data/outputs";

    VectorBuildConfig vector_config;
    vector_config.audience_min_events =
        argc > 5 ? parse_int_arg(argv[5], vector_config.audience_min_events)
                 : vector_config.audience_min_events;

    VideoBuildConfig video_config;
    video_config.topic_count = vector_config.topic_count;

    try {
        Catalog catalog = load_processed_videos(videos_csv, video_config);
        std::vector<VectorUser> users = load_vector_users_csv(users_csv);
        std::vector<VectorEvent> events = load_vector_events_csv(events_csv);
        if (catalog.videos.empty() || users.empty() || events.empty()) {
            std::cerr << "missing input data: videos=" << catalog.videos.size()
                      << " users=" << users.size()
                      << " events=" << events.size() << std::endl;
            return 1;
        }

        VectorBuildResult result = build_vectors(catalog, users, events, vector_config);

        bool ok = true;
        ok = write_user_interest_vectors_csv(output_dir + "/user_interest_vectors.csv",
                                             result.user_interest_vectors) &&
             ok;
        ok = write_video_audience_vectors_csv(output_dir + "/video_audience_vectors.csv",
                                              result.video_audience_vectors) &&
             ok;
        ok = write_video_feature_vectors_csv(output_dir + "/video_feature_vectors.csv",
                                             result.video_feature_vectors) &&
             ok;
        if (!ok) {
            std::cerr << "failed to write vector outputs under " << output_dir << std::endl;
            return 1;
        }

        long long videos_with_audience = 0;
        for (const VideoFeatureVector& vector : result.video_feature_vectors) {
            if (vector.has_audience) {
                ++videos_with_audience;
            }
        }

        std::cout << "user_interest_vectors=" << result.user_interest_vectors.size()
                  << " video_audience_vectors=" << result.video_audience_vectors.size()
                  << " video_feature_vectors=" << result.video_feature_vectors.size()
                  << " videos_with_audience=" << videos_with_audience
                  << " audience_min_events=" << vector_config.audience_min_events
                  << std::endl;
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }

    return 0;
}
