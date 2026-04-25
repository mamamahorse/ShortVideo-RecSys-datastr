#ifndef DATASTR_BEHAVIOR_SIMULATOR_HPP
#define DATASTR_BEHAVIOR_SIMULATOR_HPP

#include "../data_builder/video_cleaner.hpp"

#include <string>
#include <vector>

namespace datastr {

struct UserProfile {
    int user_id = 0;
    int group_id = 0;
    int primary_topic = 0;
    int secondary_topic = 0;
    int planned_events = 0;
    double activity_level = 0.0;
    std::vector<double> interest_vector;
};

struct Event {
    long long event_id = 0;
    int user_id = 0;
    long long video_id = 0;
    std::size_t video_index = 0;
    long long timestamp = 0;
    double match_score = 0.0;
    int watch_sec = 0;
    double watch_ratio = 0.0;
    int is_finish = 0;
    int is_like = 0;
    int is_favorite = 0;
    int is_coin = 0;
    int is_share = 0;
    double feedback_score = 0.0;
};

struct SimulationConfig {
    int topic_count = 12;
    int user_count = 12000;
    int min_tag_frequency = 3;
    int random_seed = 20260423;
    int min_events_per_user = 20;
    int max_events_per_user = 1000;
    int candidate_count = 240;
    int recent_repeat_window = 40;
    int max_repeat_per_video_per_user = 20;
    int min_unique_videos_per_user = 10;
    double unique_video_ratio = 0.25;
    long long simulation_end_ts = 1785513600LL;
};

Catalog build_catalog(const std::vector<RawVideo>& raw_videos,
                      const SimulationConfig& config);

Catalog load_processed_videos(const std::string& csv_path,
                              const SimulationConfig& config);

std::vector<UserProfile> generate_users(const SimulationConfig& config);

std::vector<Event> simulate_events(const Catalog& catalog,
                                   const std::vector<UserProfile>& users,
                                   const SimulationConfig& config);

bool write_users_csv(const std::string& path, const std::vector<UserProfile>& users);
bool write_events_csv(const std::string& path, const std::vector<Event>& events);

}  // namespace datastr

#endif  // DATASTR_BEHAVIOR_SIMULATOR_HPP
