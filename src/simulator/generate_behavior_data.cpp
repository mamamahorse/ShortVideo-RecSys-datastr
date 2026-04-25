#include "behavior_simulator.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

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

    std::string input_csv = argc > 1 ? argv[1] : "data/processed/videos.csv";
    std::string output_dir = argc > 2 ? argv[2] : "data/simulated";

    SimulationConfig config;
    config.user_count = argc > 3 ? parse_int_arg(argv[3], config.user_count) : config.user_count;
    config.random_seed = argc > 4 ? parse_int_arg(argv[4], config.random_seed) : config.random_seed;

    try {
        Catalog catalog = load_processed_videos(input_csv, config);
        if (catalog.videos.empty()) {
            std::cerr << "No videos loaded from " << input_csv << std::endl;
            return 1;
        }

        std::vector<UserProfile> users = generate_users(config);
        std::vector<Event> events = simulate_events(catalog, users, config);

        bool ok = true;
        ok = write_users_csv(output_dir + "/users.csv", users) && ok;
        ok = write_events_csv(output_dir + "/events.csv", events) && ok;
        if (!ok) {
            std::cerr << "Failed to write one or more output files under " << output_dir << std::endl;
            return 1;
        }

        long long likes = 0;
        long long favorites = 0;
        long long coins = 0;
        long long shares = 0;
        for (const Event& event : events) {
            likes += event.is_like;
            favorites += event.is_favorite;
            coins += event.is_coin;
            shares += event.is_share;
        }

        std::cout << "videos=" << catalog.videos.size()
                  << " users=" << users.size()
                  << " events=" << events.size()
                  << " likes=" << likes
                  << " favorites=" << favorites
                  << " coins=" << coins
                  << " shares=" << shares
                  << std::endl;
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }

    return 0;
}
