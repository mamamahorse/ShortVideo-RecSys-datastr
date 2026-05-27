#ifndef DATASTR_POPULARITY_PREDICTOR_HPP
#define DATASTR_POPULARITY_PREDICTOR_HPP

#include <string>
#include <vector>

namespace datastr {

struct PredictorConfig {
    double alpha = 0.3;
    double beta = 0.6;
    int window_days = 14;
    int predict_days = 7;
    int min_events = 50;
    double trend_threshold = 0.05;
};

struct PopularityPrediction {
    long long video_id = 0;
    int total_events = 0;
    int day_count = 0;
    std::string daily_views;
    std::string smooth_values;
    std::string predicted_views;
    std::string trend_direction;
    double trend_value = 0.0;
    int peak_day = 0;
    int last_day = 0;
};

std::vector<PopularityPrediction> predict_popularity(
    const std::string& events_csv,
    const std::string& videos_csv,
    const PredictorConfig& config);

bool write_predictions_csv(const std::string& path,
                           const std::vector<PopularityPrediction>& predictions);

}  // namespace datastr

#endif  // DATASTR_POPULARITY_PREDICTOR_HPP
