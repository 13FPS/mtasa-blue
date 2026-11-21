// The classic controller methods operate on two independent game-owned bytes.
// Redirect them to ordinary storage so changes can be observed through the public setters.
#include <cstdio>

static unsigned char steer, fly;
#define VAR_SteerWithMouse (&steer)
#define VAR_FlyWithMouse   (&fly)

// The unrelated vertical-aim initialization has no bearing on these preferences.
static void MemSet(void*, int, int)
{
}

class CControllerConfigManagerSA
{
public:
    CControllerConfigManagerSA();
    void SetFlyWithMouse(bool);
    void SetSteerWithMouse(bool);
    void SuspendSteerAndFlyWithMouse(bool);

private:
    void ApplySteerAndFlyWithMouseSettings();
    bool m_bSteerWithMouse, m_bFlyWithMouse, m_bSuspendSteerAndFlyWithMouse;
};
#include "MousePreferences.generated.inl"

static unsigned checks, failures;
static void     Expect(bool expectedSteer, bool expectedFly, const char* action)
{
    ++checks;
    if (steer != static_cast<unsigned char>(expectedSteer) || fly != static_cast<unsigned char>(expectedFly))
    {
        ++failures;
        std::fprintf(stderr, "%s: expected steer=%d fly=%d, got steer=%u fly=%u\n", action, expectedSteer, expectedFly, unsigned(steer), unsigned(fly));
    }
}

static void CheckPreferences(bool initialSteer, bool initialFly)
{
    steer = initialSteer;
    fly = initialFly;
    CControllerConfigManagerSA settings;
    Expect(initialSteer, initialFly, "construction");
    settings.SuspendSteerAndFlyWithMouse(true);
    Expect(false, false, "suspension");
    settings.SuspendSteerAndFlyWithMouse(false);
    Expect(initialSteer, initialFly, "resume original settings");

    settings.SuspendSteerAndFlyWithMouse(true);
    settings.SetSteerWithMouse(!initialSteer);
    Expect(false, false, "change steering during suspension");
    settings.SuspendSteerAndFlyWithMouse(false);
    Expect(!initialSteer, initialFly, "resume after steering change");

    settings.SetSteerWithMouse(initialSteer);
    settings.SuspendSteerAndFlyWithMouse(true);
    settings.SetFlyWithMouse(!initialFly);
    Expect(false, false, "change flight during suspension");
    settings.SuspendSteerAndFlyWithMouse(false);
    Expect(initialSteer, !initialFly, "resume after flight change");
}

int main()
{
    CheckPreferences(true, false);
    CheckPreferences(false, true);
    std::printf("%u/%u classic mouse preference checks passed\n", checks - failures, checks);
    return failures != 0;
}
