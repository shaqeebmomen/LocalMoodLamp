#include <stdint.h>
#define ANIMATION // Used to stop duplicate imports

namespace AnimationDriver
{

    // Structure that holds a single frame of an animation
    struct animFrame
    {
        uint8_t color[3]; // Color at this frame
        uint32_t time;    // time in ms from the animation's start where this frame occurs
    };

    // Structure that holds an entire animation
    struct animation
    {
        animFrame frames[20]; // List of frames (fixed size array)
        uint8_t frameCount;   // Number of entries with useful data in the frames buffer
        uint32_t time;        // Total runtime of this animation (redundant with "time" member of last relevant item in frames array)
    };

    // Typedef for parent function that will call actually drive the LEDs
    typedef void (*drivingFunc)(uint8_t, uint8_t, uint8_t);
    // Typedef for system time function
    typedef unsigned long (*sysTimeFunc)();

    class AnimationDriver
    {
    private:
        unsigned long currentTime;   // Current timestamp within animation
        unsigned long lastStartTime; // System time of last animation start
        animation activeAnimation;   // current animation running
        uint8_t frameIndex;          // index of the current frame
        // Internal Color state
        uint8_t color[3];
        sysTimeFunc _getSysTime;
        void updateTime();       // Update current time within animation
        void interpolateColor(); // Calculates current color

    public:
        AnimationDriver(animation, sysTimeFunc);
        AnimationDriver(sysTimeFunc);
        void updateAnimation(animation);
        void run(drivingFunc); // Takes a pointer to the parent function that runs hardware
        void restart();        // Used to reset all time-dependant logic
    };

} // Namespace AnimationDriver