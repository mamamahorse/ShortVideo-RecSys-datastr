#ifndef DATASTR_SIMILARITY_HPP
#define DATASTR_SIMILARITY_HPP

#include "../common/vector_builder.hpp"

#include <string>
#include <vector>

namespace datastr {

struct SimilarUser {
    int user_id = 0;
    double similarity = 0.0;
};

struct SimilarUserResult {
    int target_user_id = 0;
    std::vector<SimilarUser> similar_users;
};

struct SimilarityConfig {
    int top_k = 20;
    int min_common_events = 5;
};

double cosine_similarity(const std::vector<double>& a, const std::vector<double>& b);

SimilarUserResult find_similar_users(int target_user_id,
                                     const std::vector<UserInterestVector>& user_vectors,
                                     const SimilarityConfig& config);

bool write_similar_users_csv(const std::string& path, const SimilarUserResult& result);

}  // namespace datastr

#endif  // DATASTR_SIMILARITY_HPP