#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "../src/simulator/behavior_simulator.hpp"

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

double sum_vector(const std::vector<double>& values) {
    double total = 0.0;
    for (double value : values) {
        total += value;
    }
    return total;
}

}  // namespace

int main() {
    using namespace datastr;

    std::vector<std::string> tags = split_tags("NAS;storage|AI/tool");
    expect_true(tags.size() == 4, "split_tags should support common separators");
    expect_true(tags[0] == "NAS", "split_tags should trim the first tag");
    expect_true(tags[3] == "tool", "split_tags should keep the last valid tag");

    std::vector<RawVideo> raw;
    RawVideo nas;
    nas.aid = 1;
    nas.bvid = "BV1";
    nas.title = "NAS setup tutorial";
    nas.category = "NAS";
    nas.author = "author-a";
    nas.duration = 120;
    nas.pubdate = 1700000000;
    nas.view_count = 100000;
    nas.favorite = 5000;
    nas.coin = 1800;
    nas.share = 600;
    nas.like = 9000;
    nas.tag = "NAS,storage";
    raw.push_back(nas);

    RawVideo ai;
    ai.aid = 2;
    ai.bvid = "BV2";
    ai.title = "AI tool tutorial";
    ai.category = "AI";
    ai.author = "author-b";
    ai.duration = 60;
    ai.pubdate = 1700000500;
    ai.view_count = 20000;
    ai.favorite = 1200;
    ai.coin = 300;
    ai.share = 500;
    ai.like = 2500;
    ai.tag = "AI,tool";
    raw.push_back(ai);

    RawVideo unknown;
    unknown.aid = 3;
    unknown.bvid = "BV3";
    unknown.title = "unclear title";
    unknown.category = "";
    unknown.author = "author-c";
    unknown.duration = 30;
    unknown.pubdate = 1700001000;
    unknown.view_count = 100;
    unknown.favorite = 0;
    unknown.coin = 0;
    unknown.share = 0;
    unknown.like = 1;
    unknown.tag = "";
    raw.push_back(unknown);

    SimulationConfig config;
    config.topic_count = 12;
    config.user_count = 30;
    config.min_tag_frequency = 1;
    config.random_seed = 7;
    config.min_events_per_user = 3;
    config.max_events_per_user = 8;
    config.simulation_end_ts = 1730000000;

    Catalog catalog = build_catalog(raw, config);
    expect_true(catalog.videos.size() == 3, "build_catalog should preserve all videos");
    expect_true(catalog.videos[0].topic_id == 1, "NAS video should map to hardware topic");
    expect_true(catalog.videos[1].topic_id == 2, "AI tool video should map to software topic");
    expect_near(sum_vector(catalog.videos[0].topic_vector), 1.0, 0.000001,
                "topic vector should be normalized");
    expect_true(catalog.videos[0].quality_score > catalog.videos[2].quality_score,
                "high engagement video should receive higher quality score");
    expect_true(!catalog.videos[2].tag_ids.empty(),
                "video with empty tag should receive a category fallback tag");

    expect_true(write_videos_csv("data/outputs/test_processed_videos.csv", catalog),
                "write_videos_csv should save processed videos");
    Catalog reloaded = load_processed_videos("data/outputs/test_processed_videos.csv", config);
    expect_true(reloaded.videos.size() == catalog.videos.size(),
                "load_processed_videos should restore every processed video");
    expect_true(reloaded.videos[0].tag_ids.size() == catalog.videos[0].tag_ids.size(),
                "processed videos should preserve tag ids");
    expect_near(sum_vector(reloaded.videos[0].topic_vector), 1.0, 0.000001,
                "processed videos should preserve normalized topic vectors");
    expect_true(!reloaded.topic_to_video_indices[reloaded.videos[0].topic_id].empty(),
                "processed videos should rebuild topic inverted index");

    std::vector<UserProfile> users = generate_users(config);
    expect_true(users.size() == 30, "generate_users should create requested users");
    expect_near(sum_vector(users[0].interest_vector), 1.0, 0.000001,
                "user interest vector should be normalized");
    expect_true(users[0].activity_level > 0.0, "user activity should be positive");

    std::vector<Event> events = simulate_events(catalog, users, config);
    expect_true(!events.empty(), "simulate_events should create behavior events");
    for (const Event& event : events) {
        const ModeledVideo& video = catalog.videos[event.video_index];
        expect_true(event.timestamp >= video.publish_ts,
                    "event timestamp should not be earlier than publish time");
        expect_true(event.watch_sec >= 0 && event.watch_sec <= video.duration_sec,
                    "watch seconds should stay within video duration");
        expect_true(event.watch_ratio >= 0.0 && event.watch_ratio <= 1.0,
                    "watch ratio should be clamped to [0, 1]");
        if (event.is_like || event.is_favorite || event.is_coin || event.is_share) {
            expect_true(event.watch_ratio >= 0.30,
                        "strong feedback should require at least 30 percent watch ratio");
        }
    }

    std::vector<RawVideo> repeated_raw;
    for (int i = 0; i < 30; ++i) {
        RawVideo item;
        item.aid = 1000 + i;
        item.bvid = "BVR" + std::to_string(i);
        item.title = i == 0 ? "AI tool premium" : "AI tool backup " + std::to_string(i);
        item.category = "AI";
        item.author = "stress-user";
        item.duration = 60 + i;
        item.pubdate = 1700100000 + i * 100;
        item.view_count = i == 0 ? 500000 : 1000 + i;
        item.favorite = i == 0 ? 30000 : 5;
        item.coin = i == 0 ? 15000 : 2;
        item.share = i == 0 ? 5000 : 1;
        item.like = i == 0 ? 80000 : 20;
        item.tag = "AI,tool";
        repeated_raw.push_back(item);
    }

    SimulationConfig repeat_config;
    repeat_config.topic_count = 12;
    repeat_config.user_count = 1;
    repeat_config.min_tag_frequency = 1;
    repeat_config.random_seed = 19;
    repeat_config.min_events_per_user = 45;
    repeat_config.max_events_per_user = 45;
    repeat_config.candidate_count = 240;
    repeat_config.max_repeat_per_video_per_user = 20;
    repeat_config.min_unique_videos_per_user = 10;
    repeat_config.unique_video_ratio = 0.25;
    repeat_config.simulation_end_ts = 1735000000;

    Catalog repeat_catalog = build_catalog(repeated_raw, repeat_config);
    std::vector<UserProfile> repeat_users(1);
    repeat_users[0].user_id = 1;
    repeat_users[0].group_id = 2;
    repeat_users[0].primary_topic = 2;
    repeat_users[0].secondary_topic = 3;
    repeat_users[0].planned_events = 45;
    repeat_users[0].activity_level = 1.0;
    repeat_users[0].interest_vector.assign(12, 0.0);
    repeat_users[0].interest_vector[2] = 0.85;
    repeat_users[0].interest_vector[3] = 0.15;

    std::vector<Event> repeat_events = simulate_events(repeat_catalog, repeat_users, repeat_config);
    std::unordered_map<long long, int> repeat_counts;
    for (const Event& event : repeat_events) {
        repeat_counts[event.video_id] += 1;
    }

    int max_repeat = 0;
    for (const std::pair<const long long, int>& entry : repeat_counts) {
        if (entry.second > max_repeat) {
            max_repeat = entry.second;
        }
    }
    expect_true(max_repeat <= repeat_config.max_repeat_per_video_per_user,
                "simulate_events should cap repeated watches for one user-video pair");
    expect_true(static_cast<int>(repeat_counts.size()) >= 10,
                "simulate_events should keep a minimum unique video coverage per user");

    SimulationConfig default_config;
    expect_true(default_config.user_count == 12000,
                "default simulation should generate the first 12000 users");

    std::cout << "behavior simulator tests passed, events=" << events.size() << std::endl;
    return 0;
}
