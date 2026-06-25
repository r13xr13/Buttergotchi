/*
Thanks to thoses developers for their projects:
* @7h30th3r0n3 : https://github.com/7h30th3r0n3/Evil-M5Core2 and https://github.com/7h30th3r0n3/PwnGridSpam
* @viniciusbo : https://github.com/viniciusbo/m5-palnagotchi
* @sduenasg : https://github.com/sduenasg/pio_palnagotchi

Thanks to @bmorcelli for his help doing a better code.
*/
#include <Arduino.h>

enum Mood : uint8_t {
    MOOD_SLEEPING = 0,
    MOOD_AWAKENING = 1,
    MOOD_AWAKE = 2,
    MOOD_OBSERVING_R = 3,
    MOOD_OBSERVING_L = 4,
    MOOD_OBSERVING_HAPPY_R = 5,
    MOOD_OBSERVING_HAPPY_L = 6,
    MOOD_INTENSE = 7,
    MOOD_COOL = 8,
    MOOD_HAPPY = 9,
    MOOD_GRATEFUL = 10,
    MOOD_EXCITED = 11,
    MOOD_SMART = 12,
    MOOD_FRIENDLY = 13,
    MOOD_MOTIVATED = 14,
    MOOD_DEMOTIVATED = 15,
    MOOD_BORED = 16,
    MOOD_SAD = 17,
    MOOD_LONELY = 18,
    MOOD_BROKEN = 19,
    MOOD_DEBUGGING = 20,
    MOOD_ANGRY = 21,
    MOOD_HELPING = 22,
};

void setMood(uint8_t mood, String face = "", String phrase = "", bool broken = false);
uint8_t getCurrentMoodId();
int getNumberOfMoods();
String getCurrentMoodFace();
String getCurrentMoodPhrase();
bool isCurrentMoodBroken();
