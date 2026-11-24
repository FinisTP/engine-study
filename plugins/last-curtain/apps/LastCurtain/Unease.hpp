#pragma once

// The horror that never blocks a thing (the user, 2026-11-24 19:05, point 4): a tracking tear when an
// enemy takes its turn or a heavy blow lands, the lamps stuttering now and then, and a portrait on the
// order strip that, for a blink, is not calm. Looks only, on a fixed schedule (no clock, no rand()).
struct Unease
{
    float tear = 0.0F; // PostSettings::tear: the 3D view only, menus stay whole
    float dim = 0.0F;  // stops of exposure taken away while the lamps stutter
    int twitch = -1;   // the order strip's entry that looks wrong this moment (never the one acting, 0)

    // enemyTurn: an enemy's turn just began; heavy: a heavy blow just landed; reduced: the Giam setting.
    void Update(float dt, bool enemyTurn, bool heavy, bool reduced, int stripSize);

private:
    unsigned random_ = 2463534242u;
    float lampIn_ = 9.0F, lampLeft_ = 0.0F, faceIn_ = 11.0F, faceLeft_ = 0.0F, clock_ = 0.0F;
    float Next(float least, float most); // a number between, from the fixed sequence
};
