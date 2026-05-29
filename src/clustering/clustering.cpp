#include "clustering.hpp"

#include <algorithm>
#include <cmath>
#include <direct.h>
#include <fstream>
#include <iomanip>
#include <random>
#include <unordered_map>

namespace datastr {
namespace {

double distance_squared(const std::vector<double>& a, const std::vector<double>& b) {
    double dist = 0.0;
    std::size_t n = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < n; ++i) {
        double diff = a[i] - b[i];
        dist += diff * diff;
    }
    return dist;
}

void normalize_vector(std::vector<double>& v) {
    double norm = 0.0;
    for (double val : v) {
        norm += val * val;
    }
    norm = std::sqrt(norm);
    if (norm > 0.0) {
        for (double& val : v) {
            val /= norm;
        }
    }
}

}  // namespace

VideoClusterResult cluster_videos(const std::vector<VideoFeatureVector>& video_vectors,
                                  const ClusterConfig& config) {
    VideoClusterResult result;
    if (video_vectors.empty()) {
        return result;
    }

    int num_clusters = std::min(config.num_clusters, static_cast<int>(video_vectors.size()));
    result.cluster_count = num_clusters;
    result.video_cluster_ids.resize(video_vectors.size(), -1);

    std::mt19937 rng(config.random_seed);
    std::uniform_int_distribution<int> dist(0, static_cast<int>(video_vectors.size()) - 1);

    std::vector<std::vector<double>> centroids(num_clusters);
    std::vector<bool> used(video_vectors.size(), false);
    for (int k = 0; k < num_clusters; ++k) {
        int idx;
        do {
            idx = dist(rng);
        } while (used[idx]);
        used[idx] = true;
        centroids[k] = video_vectors[idx].values;
        normalize_vector(centroids[k]);
    }

    std::vector<int> counts(num_clusters, 0);
    double max_shift = config.tolerance + 1.0;
    int iteration = 0;

    while (iteration < config.max_iterations && max_shift > config.tolerance) {
        std::fill(result.video_cluster_ids.begin(), result.video_cluster_ids.end(), -1);
        std::fill(counts.begin(), counts.end(), 0);

        for (std::size_t i = 0; i < video_vectors.size(); ++i) {
            const auto& vec = video_vectors[i].values;
            double min_dist = std::numeric_limits<double>::max();
            int best_cluster = 0;

            for (int k = 0; k < num_clusters; ++k) {
                double dist = distance_squared(vec, centroids[k]);
                if (dist < min_dist) {
                    min_dist = dist;
                    best_cluster = k;
                }
            }

            result.video_cluster_ids[i] = best_cluster;
            counts[best_cluster]++;
        }

        max_shift = 0.0;
        for (int k = 0; k < num_clusters; ++k) {
            std::vector<double> new_centroid(centroids[k].size(), 0.0);
            int count = 0;

            for (std::size_t i = 0; i < video_vectors.size(); ++i) {
                if (result.video_cluster_ids[i] == k) {
                    const auto& vec = video_vectors[i].values;
                    for (std::size_t j = 0; j < vec.size() && j < new_centroid.size(); ++j) {
                        new_centroid[j] += vec[j];
                    }
                    count++;
                }
            }

            if (count > 0) {
                for (double& val : new_centroid) {
                    val /= static_cast<double>(count);
                }
                normalize_vector(new_centroid);

                double shift = distance_squared(centroids[k], new_centroid);
                max_shift = std::max(max_shift, shift);
                centroids[k] = new_centroid;
            }
        }

        iteration++;
    }

    result.clusters.resize(num_clusters);
    for (int k = 0; k < num_clusters; ++k) {
        result.clusters[k].cluster_id = k;
        result.clusters[k].centroid = centroids[k];
        for (std::size_t i = 0; i < video_vectors.size(); ++i) {
            if (result.video_cluster_ids[i] == k) {
                result.clusters[k].video_ids.push_back(video_vectors[i].video_id);
            }
        }
    }

    return result;
}

UserClusterResult cluster_users(const std::vector<UserInterestVector>& user_vectors,
                                const ClusterConfig& config) {
    UserClusterResult result;
    if (user_vectors.empty()) {
        return result;
    }

    int num_clusters = std::min(config.num_clusters, static_cast<int>(user_vectors.size()));
    result.cluster_count = num_clusters;
    result.user_cluster_ids.resize(user_vectors.size(), -1);

    std::mt19937 rng(config.random_seed + 100);
    std::uniform_int_distribution<int> dist(0, static_cast<int>(user_vectors.size()) - 1);

    std::vector<std::vector<double>> centroids(num_clusters);
    std::vector<bool> used(user_vectors.size(), false);
    for (int k = 0; k < num_clusters; ++k) {
        int idx;
        do {
            idx = dist(rng);
        } while (used[idx]);
        used[idx] = true;
        centroids[k] = user_vectors[idx].values;
        normalize_vector(centroids[k]);
    }

    std::vector<int> counts(num_clusters, 0);
    double max_shift = config.tolerance + 1.0;
    int iteration = 0;

    while (iteration < config.max_iterations && max_shift > config.tolerance) {
        std::fill(result.user_cluster_ids.begin(), result.user_cluster_ids.end(), -1);
        std::fill(counts.begin(), counts.end(), 0);

        for (std::size_t i = 0; i < user_vectors.size(); ++i) {
            const auto& vec = user_vectors[i].values;
            double min_dist = std::numeric_limits<double>::max();
            int best_cluster = 0;

            for (int k = 0; k < num_clusters; ++k) {
                double dist = distance_squared(vec, centroids[k]);
                if (dist < min_dist) {
                    min_dist = dist;
                    best_cluster = k;
                }
            }

            result.user_cluster_ids[i] = best_cluster;
            counts[best_cluster]++;
        }

        max_shift = 0.0;
        for (int k = 0; k < num_clusters; ++k) {
            std::vector<double> new_centroid(centroids[k].size(), 0.0);
            int count = 0;

            for (std::size_t i = 0; i < user_vectors.size(); ++i) {
                if (result.user_cluster_ids[i] == k) {
                    const auto& vec = user_vectors[i].values;
                    for (std::size_t j = 0; j < vec.size() && j < new_centroid.size(); ++j) {
                        new_centroid[j] += vec[j];
                    }
                    count++;
                }
            }

            if (count > 0) {
                for (double& val : new_centroid) {
                    val /= static_cast<double>(count);
                }
                normalize_vector(new_centroid);

                double shift = distance_squared(centroids[k], new_centroid);
                max_shift = std::max(max_shift, shift);
                centroids[k] = new_centroid;
            }
        }

        iteration++;
    }

    result.clusters.resize(num_clusters);
    for (int k = 0; k < num_clusters; ++k) {
        result.clusters[k].cluster_id = k;
        result.clusters[k].centroid = centroids[k];
        for (std::size_t i = 0; i < user_vectors.size(); ++i) {
            if (result.user_cluster_ids[i] == k) {
                result.clusters[k].user_ids.push_back(user_vectors[i].user_id);
            }
        }
    }

    return result;
}

bool write_video_clusters_csv(const std::string& path, const VideoClusterResult& result) {
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

    output << "video_id,cluster_id\n";
    for (const VideoCluster& cluster : result.clusters) {
        for (long long video_id : cluster.video_ids) {
            output << video_id << ',' << cluster.cluster_id << '\n';
        }
    }
    return true;
}

bool write_user_clusters_csv(const std::string& path, const UserClusterResult& result) {
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

    output << "user_id,cluster_id\n";
    for (const UserCluster& cluster : result.clusters) {
        for (int user_id : cluster.user_ids) {
            output << user_id << ',' << cluster.cluster_id << '\n';
        }
    }
    return true;
}

}  // namespace datastr