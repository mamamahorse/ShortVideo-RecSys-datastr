#include "behavior_simulator.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <direct.h>
#include <fstream>
#include <iomanip>
#include <limits>
#include <random>
#include <unordered_map>
#include <unordered_set>

namespace datastr {
namespace {

const int kSecondsPerDay = 86400;

double clamp01(double value) {
    if (value < 0.0) {
        return 0.0;
    }
    if (value > 1.0) {
        return 1.0;
    }
    return value;
}

void normalize(std::vector<double>& values) {
    double sum = 0.0;
    for (double value : values) {
        sum += value;
    }
    if (sum <= 0.0) {
        if (!values.empty()) {
            values[0] = 1.0;
        }
        return;
    }
    for (double& value : values) {
        value /= sum;
    }
}

double dot_product(const std::vector<double>& left, const std::vector<double>& right) {
    double result = 0.0;
    std::size_t n = std::min(left.size(), right.size());
    for (std::size_t i = 0; i < n; ++i) {
        result += left[i] * right[i];
    }
    return result;
}

double rate(long long action, long long views) {
    return static_cast<double>(action) / static_cast<double>(views + 100);
}

double freshness_score(const ModeledVideo& video, const SimulationConfig& config) {
    double age_days = 0.0;
    if (config.simulation_end_ts > video.publish_ts) {
        age_days = static_cast<double>(config.simulation_end_ts - video.publish_ts) /
                   static_cast<double>(kSecondsPerDay);
    }
    return 1.0 / (1.0 + age_days / 120.0);
}

int target_unique_video_count(const UserProfile& user,
                              const Catalog& catalog,
                              const SimulationConfig& config) {
    int ratio_target = static_cast<int>(std::ceil(static_cast<double>(user.planned_events) *
                                                  std::max(0.0, config.unique_video_ratio)));
    int target = std::max(config.min_unique_videos_per_user, ratio_target);
    target = std::max(1, target);
    target = std::min(target, user.planned_events);
    target = std::min(target, static_cast<int>(catalog.videos.size()));
    return target;
}

std::vector<int> sample_candidate_indices(const std::vector<int>* pool,
                                          int video_count,
                                          int candidate_count,
                                          std::mt19937& rng) {
    std::vector<int> candidates;
    int available = pool != nullptr ? static_cast<int>(pool->size()) : video_count;
    if (available <= 0) {
        return candidates;
    }

    int limit = std::max(1, candidate_count);
    if (limit >= available) {
        candidates.reserve(available);
        if (pool != nullptr) {
            candidates.assign(pool->begin(), pool->end());
        } else {
            for (int index = 0; index < video_count; ++index) {
                candidates.push_back(index);
            }
        }
        return candidates;
    }

    candidates.reserve(limit);
    std::unordered_set<int> picked;
    std::uniform_int_distribution<int> dist(0, available - 1);
    while (static_cast<int>(candidates.size()) < limit) {
        int index = pool != nullptr ? (*pool)[dist(rng)] : dist(rng);
        if (picked.insert(index).second) {
            candidates.push_back(index);
        }
    }
    return candidates;
}

void append_unique_candidates(std::vector<int>& target,
                              const std::vector<int>& extra,
                              std::unordered_set<int>& seen) {
    for (int index : extra) {
        if (seen.insert(index).second) {
            target.push_back(index);
        }
    }
}

long long choose_timestamp(const ModeledVideo& video,
                           const SimulationConfig& config,
                           std::mt19937& rng) {
    double lambda = 0.035 + (1.0 - video.quality_score) * 0.025;
    std::exponential_distribution<double> decay(lambda);
    int day_offset = static_cast<int>(decay(rng));
    if (day_offset > 720) {
        day_offset = 720;
    }

    std::uniform_real_distribution<double> unit(0.0, 1.0);
    if (video.quality_score > 0.70 && unit(rng) < 0.08) {
        std::uniform_int_distribution<int> burst_center(30, 240);
        std::uniform_int_distribution<int> burst_width(-5, 5);
        day_offset += burst_center(rng) + burst_width(rng);
        if (day_offset < 0) {
            day_offset = 0;
        }
    }

    std::uniform_int_distribution<int> seconds(0, kSecondsPerDay - 1);
    long long timestamp = video.publish_ts +
                          static_cast<long long>(day_offset) * kSecondsPerDay +
                          seconds(rng);
    if (config.simulation_end_ts > video.publish_ts && timestamp > config.simulation_end_ts) {
        timestamp = config.simulation_end_ts;
    }
    if (timestamp < video.publish_ts) {
        timestamp = video.publish_ts;
    }
    return timestamp;
}

int sample_topic(const std::vector<double>& weights, std::mt19937& rng) {
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    double target = unit(rng);
    double prefix = 0.0;
    for (std::size_t i = 0; i < weights.size(); ++i) {
        prefix += weights[i];
        if (target <= prefix) {
            return static_cast<int>(i);
        }
    }
    return static_cast<int>(weights.size() - 1);
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

}  // namespace

Catalog build_catalog(const std::vector<RawVideo>& raw_videos,
                      const SimulationConfig& config) {
    VideoBuildConfig build_config;
    build_config.topic_count = config.topic_count;
    build_config.min_tag_frequency = config.min_tag_frequency;
    return build_catalog(raw_videos, build_config);
}

Catalog load_processed_videos(const std::string& csv_path,
                              const SimulationConfig& config) {
    VideoBuildConfig build_config;
    build_config.topic_count = config.topic_count;
    build_config.min_tag_frequency = config.min_tag_frequency;
    return load_processed_videos(csv_path, build_config);
}

std::vector<UserProfile> generate_users(const SimulationConfig& config) {
    std::vector<UserProfile> users;
    users.reserve(std::max(0, config.user_count));

    int topic_count = std::max(1, config.topic_count);
    int min_events = std::max(1, config.min_events_per_user);
    int max_events = std::max(min_events, config.max_events_per_user);

    std::mt19937 rng(config.random_seed);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    std::uniform_int_distribution<int> topic_dist(0, topic_count - 1);

    for (int i = 0; i < config.user_count; ++i) {
        UserProfile user;
        user.user_id = i + 1;
        user.group_id = i % topic_count;
        user.primary_topic = user.group_id;
        user.secondary_topic = topic_dist(rng);
        if (topic_count > 1) {
            while (user.secondary_topic == user.primary_topic) {
                user.secondary_topic = topic_dist(rng);
            }
        }

        double primary_weight = 0.55 + unit(rng) * 0.20;
        double secondary_weight = 0.15 + unit(rng) * 0.15;
        double remaining = std::max(0.0, 1.0 - primary_weight - secondary_weight);
        user.interest_vector.assign(topic_count, topic_count > 2 ? remaining / (topic_count - 2) : 0.0);
        user.interest_vector[user.primary_topic] = primary_weight;
        user.interest_vector[user.secondary_topic] = secondary_weight;
        normalize(user.interest_vector);

        double activity_roll = unit(rng);
        int low_max = std::max(min_events, std::min(max_events, 80));
        int mid_min = std::max(min_events, std::min(max_events, 80));
        int mid_max = std::max(mid_min, std::min(max_events, 250));
        int high_min = std::max(min_events, std::min(max_events, 250));
        if (activity_roll < 0.70) {
            std::uniform_int_distribution<int> count_dist(min_events, low_max);
            user.planned_events = count_dist(rng);
            user.activity_level = 0.35 + unit(rng) * 0.25;
        } else if (activity_roll < 0.95) {
            std::uniform_int_distribution<int> count_dist(mid_min, mid_max);
            user.planned_events = count_dist(rng);
            user.activity_level = 0.65 + unit(rng) * 0.25;
        } else {
            std::uniform_int_distribution<int> count_dist(high_min, max_events);
            user.planned_events = count_dist(rng);
            user.activity_level = 0.90 + unit(rng) * 0.50;
        }
        users.push_back(user);
    }

    return users;
}

std::vector<Event> simulate_events(const Catalog& catalog,
                                   const std::vector<UserProfile>& users,
                                   const SimulationConfig& config) {
    std::vector<Event> events;
    if (catalog.videos.empty()) {
        return events;
    }

    long long estimated = 0;
    for (const UserProfile& user : users) {
        estimated += user.planned_events;
    }
    if (estimated > 0) {
        events.reserve(static_cast<std::size_t>(estimated));
    }

    std::mt19937 rng(config.random_seed + 17);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    std::uniform_real_distribution<double> noise(-0.03, 0.03);
    std::uniform_int_distribution<int> all_video_dist(0, static_cast<int>(catalog.videos.size()) - 1);

    long long event_id = 1;
    for (const UserProfile& user : users) {
        std::unordered_map<int, int> watch_counts;
        std::unordered_map<int, int> recent_counts;
        std::deque<int> recent_history;
        int target_unique = target_unique_video_count(user, catalog, config);

        for (int n = 0; n < user.planned_events; ++n) {
            int topic = sample_topic(user.interest_vector, rng);
            const std::vector<int>* pool = nullptr;
            if (topic >= 0 && topic < static_cast<int>(catalog.topic_to_video_indices.size()) &&
                !catalog.topic_to_video_indices[topic].empty()) {
                pool = &catalog.topic_to_video_indices[topic];
            }

            double best_score = -std::numeric_limits<double>::infinity();
            int best_index = -1;
            int candidate_count = std::max(1, config.candidate_count);
            bool needs_more_unique = static_cast<int>(watch_counts.size()) < target_unique;

            std::vector<int> candidates = sample_candidate_indices(pool,
                                                                   static_cast<int>(catalog.videos.size()),
                                                                   candidate_count,
                                                                   rng);
            std::unordered_set<int> candidate_set(candidates.begin(), candidates.end());
            if (needs_more_unique) {
                std::vector<int> backup = sample_candidate_indices(nullptr,
                                                                   static_cast<int>(catalog.videos.size()),
                                                                   candidate_count,
                                                                   rng);
                append_unique_candidates(candidates, backup, candidate_set);
            }

            bool has_unseen_candidate = false;
            for (int index : candidates) {
                int seen_count = 0;
                std::unordered_map<int, int>::const_iterator watched = watch_counts.find(index);
                if (watched != watch_counts.end()) {
                    seen_count = watched->second;
                }
                if (seen_count < config.max_repeat_per_video_per_user && seen_count == 0) {
                    has_unseen_candidate = true;
                    break;
                }
            }

            for (int index : candidates) {
                int seen_count = 0;
                std::unordered_map<int, int>::const_iterator watched = watch_counts.find(index);
                if (watched != watch_counts.end()) {
                    seen_count = watched->second;
                }
                if (seen_count >= config.max_repeat_per_video_per_user) {
                    continue;
                }
                if (needs_more_unique && has_unseen_candidate && seen_count > 0) {
                    continue;
                }

                const ModeledVideo& video = catalog.videos[index];
                double match = dot_product(user.interest_vector, video.topic_vector);
                double score = 0.65 * match + 0.25 * video.quality_score +
                               0.10 * freshness_score(video, config) + noise(rng);

                std::unordered_map<int, int>::const_iterator recent = recent_counts.find(index);
                if (recent != recent_counts.end()) {
                    score *= 1.0 / (1.0 + 0.85 * static_cast<double>(recent->second));
                }
                if (seen_count > 0) {
                    score *= 1.0 / (1.0 + 0.18 * static_cast<double>(seen_count));
                } else if (needs_more_unique) {
                    score += 0.08;
                }

                if (score > best_score) {
                    best_score = score;
                    best_index = index;
                }
            }

            if (best_index < 0) {
                for (std::size_t retry = 0; retry < catalog.videos.size(); ++retry) {
                    int index = all_video_dist(rng);
                    if (watch_counts[index] < config.max_repeat_per_video_per_user) {
                        best_index = index;
                        break;
                    }
                }
            }
            if (best_index < 0) {
                continue;
            }

            const ModeledVideo& video = catalog.videos[best_index];
            double match = dot_product(user.interest_vector, video.topic_vector);
            double base_watch = 0.15;
            if (match >= 0.55) {
                base_watch = 0.75 + unit(rng) * 0.20;
            } else if (match >= 0.30) {
                base_watch = 0.40 + unit(rng) * 0.30;
            } else {
                base_watch = 0.05 + unit(rng) * 0.25;
            }
            double watch_ratio = clamp01(base_watch + noise(rng));
            int watch_sec = static_cast<int>(std::round(video.duration_sec * watch_ratio));
            if (watch_sec > video.duration_sec) {
                watch_sec = video.duration_sec;
            }

            double match_factor = 0.5 + match;
            double like_prob = 0.0;
            double favorite_prob = 0.0;
            double coin_prob = 0.0;
            double share_prob = 0.0;
            if (watch_ratio >= 0.30) {
                double level = watch_ratio >= 0.70 ? 1.0 : 0.45;
                like_prob = std::min(0.35, (0.03 + 0.09 * level + rate(video.raw_like, video.raw_view_count)) * match_factor);
                favorite_prob = std::min(0.18, (0.01 + 0.05 * level + rate(video.raw_favorite, video.raw_view_count)) * match_factor);
                coin_prob = std::min(0.12, (0.005 + 0.035 * level + rate(video.raw_coin, video.raw_view_count)) * match_factor);
                share_prob = std::min(0.08, (0.002 + 0.018 * level + rate(video.raw_share, video.raw_view_count)) * match_factor);
            }

            Event event;
            event.event_id = event_id++;
            event.user_id = user.user_id;
            event.video_id = video.video_id;
            event.video_index = static_cast<std::size_t>(best_index);
            event.timestamp = choose_timestamp(video, config, rng);
            event.match_score = match;
            event.watch_sec = watch_sec;
            event.watch_ratio = watch_ratio;
            event.is_finish = watch_ratio >= 0.90 ? 1 : 0;
            event.is_like = unit(rng) < like_prob ? 1 : 0;
            event.is_favorite = unit(rng) < favorite_prob ? 1 : 0;
            event.is_coin = unit(rng) < coin_prob ? 1 : 0;
            event.is_share = unit(rng) < share_prob ? 1 : 0;
            event.feedback_score = 1.0 * event.watch_ratio +
                                   1.0 * event.is_finish +
                                   2.0 * event.is_like +
                                   3.0 * event.is_favorite +
                                   3.0 * event.is_coin +
                                   2.5 * event.is_share;
            events.push_back(event);

            watch_counts[best_index] += 1;
            recent_history.push_back(best_index);
            recent_counts[best_index] += 1;
            while (static_cast<int>(recent_history.size()) > std::max(1, config.recent_repeat_window)) {
                int oldest = recent_history.front();
                recent_history.pop_front();
                std::unordered_map<int, int>::iterator found = recent_counts.find(oldest);
                if (found != recent_counts.end()) {
                    found->second -= 1;
                    if (found->second <= 0) {
                        recent_counts.erase(found);
                    }
                }
            }
        }
    }

    return events;
}

bool write_users_csv(const std::string& path, const std::vector<UserProfile>& users) {
    ensure_parent_dir(path);
    std::ofstream output(path.c_str());
    if (!output) {
        return false;
    }
    output << "user_id,group_id,primary_topic,secondary_topic,activity_level,planned_events\n";
    for (const UserProfile& user : users) {
        output << user.user_id << ',' << user.group_id << ',' << user.primary_topic << ','
               << user.secondary_topic << ',' << std::fixed << std::setprecision(6)
               << user.activity_level << ',' << user.planned_events << '\n';
    }
    return true;
}

bool write_events_csv(const std::string& path, const std::vector<Event>& events) {
    ensure_parent_dir(path);
    std::ofstream output(path.c_str());
    if (!output) {
        return false;
    }
    output << "event_id,user_id,video_id,timestamp,match_score,watch_sec,watch_ratio,"
           << "is_finish,is_like,is_favorite,is_coin,is_share,feedback_score\n";
    for (const Event& event : events) {
        output << event.event_id << ',' << event.user_id << ',' << event.video_id << ','
               << event.timestamp << ',' << std::fixed << std::setprecision(6)
               << event.match_score << ',' << event.watch_sec << ','
               << event.watch_ratio << ',' << event.is_finish << ',' << event.is_like
               << ',' << event.is_favorite << ',' << event.is_coin << ','
               << event.is_share << ',' << event.feedback_score << '\n';
    }
    return true;
}

}  // namespace datastr
