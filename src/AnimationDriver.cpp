#include <AnimationDriver.h>
// Debug flags
// #define DEBUG
// #define DEBUG_TIME

#ifdef DEBUG
#include <Arduino.h>
#endif
namespace AnimationDriver
{

    AnimationDriver::AnimationDriver(animation initAnim, sysTimeFunc getSysTime)
    {
        _getSysTime = getSysTime;
        updateAnimation(initAnim);
    }
    AnimationDriver::AnimationDriver(sysTimeFunc getSysTime)
    {
        _getSysTime = getSysTime;
    }

    void AnimationDriver::restart()
    {
        frameIndex = 0;
        lastStartTime = _getSysTime();
        currentTime = 0;
    }

    // Updates private timing variables
    void AnimationDriver::updateTime()
    {
        // Set current time since last animation start
        currentTime = _getSysTime() - lastStartTime;
#ifdef DEBUG_TIME
        Serial.print("LastStart: ");
        Serial.print(lastStartTime);
        Serial.print(" Current: ");
        Serial.print(currentTime);
        Serial.println();
        Serial.flush();
#endif
        // If the current time has reached the next frame time
        if (currentTime > activeAnimation.frames[frameIndex + 1].time)
        {
            frameIndex++;
            // Frame index has passed the last frame
            if (frameIndex == activeAnimation.frameCount-1)
            {
                // Move last start time forward by one animation period
                lastStartTime += activeAnimation.time;
                // Trim the extra animation period from current time
                currentTime -= activeAnimation.time;
                // Reset Frame index
                frameIndex = 0;
            }
        }
#ifdef DEBUG_TIME
        Serial.print("Frame:");
        Serial.print(frameIndex);
        Serial.println();
        Serial.flush();
#endif
    }

    // Interpolates b/w frames and updates current color state
    void AnimationDriver::interpolateColor()
    {

        animFrame *last = &activeAnimation.frames[frameIndex];
        animFrame *next = &activeAnimation.frames[frameIndex + 1];

#ifdef DEBUG_TIME
        Serial.print("Last- Time: ");
        Serial.print(last->time);
        Serial.print(" Color: ");
        Serial.print(last->color[2]);
        Serial.print(" Next- Time: ");
        Serial.print(next->time);
        Serial.print(" Color: ");
        Serial.print(next->color[2]);
        Serial.println();
        Serial.flush();
#endif
        // Linearly interpolate between current and next R,G,B values
        for (uint8_t i = 0; i < 3; i++)
        {
            color[i] = (uint8_t)((float)last->color[i] + ((float)next->color[i] - (float)last->color[i]) / ((float)next->time - (float)last->time) * (float)(currentTime - last->time));
        }
    }

    // Update the current animation and refresh index
    void AnimationDriver::updateAnimation(animation newAnim)
    {
        activeAnimation = newAnim;
        restart();
    }

    /**
     * Runs the Animation logic based on system time and calls hardware-aware function defined in parent scope
     * @param drivingFunc the function to drive hardware, arguments passed in are (uint8_t r, uint8_t g, uint8_t b)
     */
    void AnimationDriver::run(drivingFunc runLEDs)
    {
        // Update time-dependant variables
        updateTime();
        // Determine color state
        interpolateColor();
// Pass color state to parent hardware-aware function
#ifdef DEBUG
        Serial.print("R: ");
        Serial.print(color[0]);
        Serial.print("G: ");
        Serial.print(color[1]);
        Serial.print("B: ");
        Serial.print(color[2]);
        Serial.println();
        Serial.flush();
#endif
        runLEDs(color[0], color[1], color[2]);
    }

} // namespace AnimationDriver
