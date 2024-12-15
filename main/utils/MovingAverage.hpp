#include <vector>
#include <numeric>
#include <iostream>
#include <algorithm>

#define MAX_WINDOW_SIZE ((size_t)100000)
#define MIN_WINDOW_SIZE ((size_t)1)

class MovingAverage {
public:
    /// @brief Create a new moving average class with the specified window size.
    /// @param windowSize 
    explicit MovingAverage(size_t windowSize) : windowSize(windowSize), sum(0.0f) {
        windowSize = std::min(windowSize, MAX_WINDOW_SIZE);
        windowSize = std::max(windowSize, MIN_WINDOW_SIZE);
    }

    /// @brief Sets the window size after the class has been constructed.
    /// @param windowSize 
    void setWindowSize(size_t windowSize) {
        windowSize = std::min(windowSize, MAX_WINDOW_SIZE);
        windowSize = std::max(windowSize, MIN_WINDOW_SIZE);
    }

    /// @brief Add a value to the averager.
    /// @param value The new value.
    /// @return Returns the current moving average.
    float addValue(float value) {
        if (values.size() == windowSize) {
            // Remove the oldest value from the sum
            sum -= values.front();
            values.erase(values.begin());
        }

        // Add the new value
        values.push_back(value);
        sum += value;

        // Return the current moving average
        return getAverage();
    }

    /// @brief Calculates the current moving average.
    /// @return The current moving average.
    float getAverage() const {
        if (values.empty()) {
            return 0.0f; // Handle case with no values
        }
        return sum / static_cast<float>(values.size());
    }

    /// @brief Resets the average and prep for reuse.
    void reset() {
        values.clear();
        sum = 0.0f;
    }

private:
    size_t windowSize;             // Size of the moving average window
    std::vector<float> values;     // Container to store the values
    float sum;                     // Running sum of the values
};