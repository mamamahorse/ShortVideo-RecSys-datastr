#include "popularity_predictor.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int parse_int_arg(const char* value, int fallback) {
    if (value == nullptr) return fallback;
    int parsed = std::atoi(value);
    return parsed > 0 ? parsed : fallback;
}

double parse_double_arg(const char* value, double fallback) {
    if (value == nullptr) return fallback;
    double parsed = std::atof(value);
    return parsed > 0.0 ? parsed : fallback;
}

}  // namespace

int main(int argc, char** argv) {
    using namespace datastr;

    std::string events_csv = argc > 1 ? argv[1] : "data/simulated/events.csv";
    std::string videos_csv = argc > 2 ? argv[2] : "data/processed/videos.csv";
    std::string output_path = argc > 3 ? argv[3] : "data/outputs/popularity_predictions.csv";

    PredictorConfig config;
    config.alpha = argc > 4 ? parse_double_arg(argv[4], config.alpha) : config.alpha;
    config.beta = argc > 5 ? parse_double_arg(argv[5], config.beta) : config.beta;
    config.window_days = argc > 6 ? parse_int_arg(argv[6], config.window_days) : config.window_days;
    config.predict_days = argc > 7 ? parse_int_arg(argv[7], config.predict_days) : config.predict_days;
    config.min_events = argc > 8 ? parse_int_arg(argv[8], config.min_events) : config.min_events;

    try {
        std::vector<PopularityPrediction> predictions =
            predict_popularity(events_csv, videos_csv, config);

        if (predictions.empty()) {
            std::cerr << "no videos with enough data for prediction" << std::endl;
            return 1;
        }

        if (!write_predictions_csv(output_path, predictions)) {
            std::cerr << "failed to write " << output_path << std::endl;
            return 1;
        }

        // summary stats
        int rising = 0, flat = 0, declining = 0;
        for (const auto& p : predictions) {
            if (p.trend_direction == "上升") ++rising;
            else if (p.trend_direction == "下降") ++declining;
            else ++flat;
        }

        std::cout << "predictions=" << predictions.size()
                  << " rising=" << rising
                  << " flat=" << flat
                  << " declining=" << declining
                  << " alpha=" << config.alpha
                  << " beta=" << config.beta
                  << " window=" << config.window_days
                  << " predict_days=" << config.predict_days
                  << " min_events=" << config.min_events
                  << std::endl;
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }

    return 0;
}
