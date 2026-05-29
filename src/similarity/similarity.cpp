#include "similarity.hpp"

#include <algorithm>
#include <cmath>
#include <direct.h>
#include <fstream>
#include <iomanip>
#include <queue>
#include <unordered_map>

namespace datastr {

double cosine_similarity(const std::vector<double>& a, const std::vector<double>& b) {
    double dot = 0.0;
    double norm_a = 0.0;
    double norm_b = 0.0;
    std::size_t n = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < n; ++i) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    if (norm_a == 0.0 || norm_b == 0.0) {
        return 0.0;
    }
    return dot / (std::sqrt(norm_a) * std::sqrt(norm_b));
}

SimilarUserResult find_similar_users(int target_user_id,
                                     const std::vector<UserInterestVector>& user_vectors,
                                     const SimilarityConfig& config) {
    SimilarUserResult result;
    result.target_user_id = target_user_id;

    std::unordered_map<int, int> user_id_to_index;
    int target_index = -1;
    for (std::size_t i = 0; i < user_vectors.size(); ++i) {
        user_id_to_index[user_vectors[i].user_id] = static_cast<int>(i);
        if (user_vectors[i].user_id == target_user_id) {
            target_index = static_cast<int>(i);
        }
    }

    if (target_index < 0) {
        return result;
    }

    const std::vector<double>& target_vector = user_vectors[target_index].values;
    int top_k = std::max(1, config.top_k);

    auto compare = [](const SimilarUser& a, const SimilarUser& b) {
        return a.similarity > b.similarity;
    };
    std::priority_queue<SimilarUser, std::vector<SimilarUser>, decltype(compare)> min_heap(compare);

    for (std::size_t i = 0; i < user_vectors.size(); ++i) {
        if (static_cast<int>(i) == target_index) {
            continue;
        }

        const UserInterestVector& other = user_vectors[i];
        if (other.event_count < config.min_common_events) {
            continue;
        }

        double sim = cosine_similarity(target_vector, other.values);

        if (min_heap.size() < static_cast<std::size_t>(top_k)) {
            min_heap.push(SimilarUser{other.user_id, sim});
        } else if (sim > min_heap.top().similarity) {
            min_heap.pop();
            min_heap.push(SimilarUser{other.user_id, sim});
        }
    }

    while (!min_heap.empty()) {
        result.similar_users.push_back(min_heap.top());
        min_heap.pop();
    }

    std::reverse(result.similar_users.begin(), result.similar_users.end());

    return result;
}

bool write_similar_users_csv(const std::string& path, const SimilarUserResult& result) {
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

    output << "target_user_id,similar_user_id,similarity\n";
    for (const SimilarUser& user : result.similar_users) {
        output << result.target_user_id << ',' << user.user_id << ','
               << std::fixed << std::setprecision(6) << user.similarity << '\n';
    }
    return true;
}

}  // namespace datastr