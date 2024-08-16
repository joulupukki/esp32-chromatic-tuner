/*=============================================================================
    Copyright (c) 2024 Boyd Timothy. All rights reserved.

    Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <vector>
#include <algorithm>

class ExponentialSmoother {
public:
    ExponentialSmoother(float amount) : amount(amount) {
        amount = std::min(amount, 1.0f);
        amount = std::max(amount, 0.0f);
    }

    float smooth(float value) {
        float newValue = value;
        if (values.size() > 0) {
            float last = values.back();
            newValue = (amount * value) + (1.0 - amount) * last;
            if (values.size() > 2) {
                values.erase(values.begin()); // remove the first item
            }
        }
        values.push_back(value);
        return newValue;
    }

    void reset() {
        values.empty();
    }

private:
    std::vector<float> values;
    float amount;
};