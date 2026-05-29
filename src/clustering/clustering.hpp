#ifndef DATASTR_CLUSTERING_HPP
#define DATASTR_CLUSTERING_HPP

#include "../common/vector_builder.hpp"

#include <string>
#include <vector>

namespace datastr {

struct VideoCluster {
    int cluster_id = 0;
    std::vector<long long> video_ids;
    std::vector<double> centroid;
};

struct UserCluster {
    int cluster_id = 0;
    std::vector<int> user_ids;
    std::vector<double> centroid;
};

struct VideoClusterResult {
    int cluster_count = 0;
    std::vector<VideoCluster> clusters;
    std::vector<int> video_cluster_ids;
};

struct UserClusterResult {
    int cluster_count = 0;
    std::vector<UserCluster> clusters;
    std::vector<int> user_cluster_ids;
};

struct ClusterConfig {
    int max_iterations = 100;
    int num_clusters = 12;
    double tolerance = 1e-6;
    int random_seed = 42;
};

VideoClusterResult cluster_videos(const std::vector<VideoFeatureVector>& video_vectors,
                                  const ClusterConfig& config);

UserClusterResult cluster_users(const std::vector<UserInterestVector>& user_vectors,
                                const ClusterConfig& config);

bool write_video_clusters_csv(const std::string& path, const VideoClusterResult& result);

bool write_user_clusters_csv(const std::string& path, const UserClusterResult& result);

}  // namespace datastr

#endif  // DATASTR_CLUSTERING_HPP