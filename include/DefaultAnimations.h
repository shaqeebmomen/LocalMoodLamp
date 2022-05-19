#ifndef ANIMATION
#include <AnimationDriver.h>
#endif

// Macro used to generate struct for solid colors
#define SOLID_COLOR(r, g, b) ((struct AnimationDriver::animation){{                     \
                                                                      {{r, g, b}, 0},   \
                                                                      {{r, g, b}, 500}, \
                                                                  },                    \
                                                                  2,                    \
                                                                  100})

// Macro used to generate struct for breathing animation
#define BREATHE_COLOR(r, g, b, time) ((struct AnimationDriver::animation){{                                          \
                                                                              {{r, g, b}, 0},                        \
                                                                              {{r / 2, g / 2, b / 2}, time / 4},     \
                                                                              {{0, 0, 0}, time / 2},                 \
                                                                              {{r / 2, g / 2, b / 2}, time * 3 / 4}, \
                                                                              {{r, g, b}, time},                     \
                                                                          },                                         \
                                                                          5,                                         \
                                                                          time})

#define RAINBOW(time) ((struct AnimationDriver::animation){{                                    \
                                                               {{255, 0, 0}, 0},                \
                                                               {{255, 127, 0}, time * 1 / 12},  \
                                                               {{255, 255, 0}, time / 6},       \
                                                               {{127, 255, 0}, time / 4},       \
                                                               {{0, 255, 0}, time / 3},         \
                                                               {{0, 255, 127}, time * 5 / 12},  \
                                                               {{0, 255, 255}, time / 2},       \
                                                               {{0, 127, 255}, time * 7 / 12},  \
                                                               {{0, 0, 255}, time * 2 / 3},     \
                                                               {{127, 0, 255}, time * 3 / 4},   \
                                                               {{255, 0, 255}, time * 5 / 6},   \
                                                               {{255, 0, 127}, time * 11 / 12}, \
                                                               {{255, 0, 0}, time},             \
                                                           },                                   \
                                                           13,                                  \
                                                           time})
