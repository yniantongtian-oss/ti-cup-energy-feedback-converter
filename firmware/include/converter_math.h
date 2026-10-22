#ifndef CONVERTER_MATH_H
#define CONVERTER_MATH_H

static inline float converter_clampf(float value, float minimum, float maximum) {
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

#endif
