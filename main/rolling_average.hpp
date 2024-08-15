#include <vector>

class RollingAverage {
public:
    RollingAverage(int windowSize) : windowSize(windowSize) {}

    float rollingAverage(float value) {
        values.push_back(value);

        if (values.size() > windowSize) {
            // Remove the first item since it's not out of the window
            values.erase(values.begin());
        }

        float sum = 0.0;
        for (float v : values) {
            sum += v;
        }

        return sum / values.size();
    }

    void reset() {
        values.empty();
    }

private:
    std::vector<float> values;
    int windowSize;
};