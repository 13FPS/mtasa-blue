/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Strict dependency receiver for unchanged actual SA Terminate body
 ****************************************************************************/
#pragma once
struct TerminateProbe
{
    bool     pending = false;
    unsigned closes = 0, queries = 0, destructions = 0;
};
class CAnimManagerSA
{
public:
    explicit CAnimManagerSA(TerminateProbe& probe) noexcept : probe(probe) {}
    void CloseCustomAnimationCleanup() noexcept { ++probe.closes; }
    bool HasPendingCustomAnimationCleanup() const noexcept
    {
        // Any query before the actual method closes its creator is a protocol
        // failure. The receiver supplies no native game or cleanup behavior.
        if (probe.closes <= probe.queries)
            std::abort();
        ++probe.queries;
        return probe.pending;
    }

private:
    TerminateProbe& probe;
};
class CGameSA
{
public:
    CGameSA(CAnimManagerSA* manager, TerminateProbe& probe) noexcept : m_pAnimManager(manager), probe(probe) {}
    ~CGameSA() { ++probe.destructions; }
    CAnimManagerSA* GetAnimManager() const noexcept { return m_pAnimManager; }
    void            Terminate();

private:
    CAnimManagerSA* m_pAnimManager;
    TerminateProbe& probe;
};
// MTA_ACTUAL_SA_TERMINATE_BODY
inline void RunActualTerminateChecks()
{
    TerminateProbe clear;
    CAnimManagerSA clearManager(clear);
    auto*          clearGame = new CGameSA(&clearManager, clear);
    clearGame->Terminate();
    Check(clear.closes == 1 && clear.queries == 1 && clear.destructions == 1,
          "actual SA Terminate closes creator, queries cleanup, then deletes only clear backend");
    TerminateProbe retained;
    retained.pending = true;
    CAnimManagerSA retainedManager(retained);
    auto*          retainedGame = new CGameSA(&retainedManager, retained);
    retainedGame->Terminate();
    Check(retained.closes == 1 && retained.queries == 1 && !retained.destructions,
          "actual SA Terminate retains its manager/Game object while cleanup or tickets remain pending");
    retained.pending = false;
    retainedGame->Terminate();
    Check(retained.closes == 2 && retained.queries == 2 && retained.destructions == 1,
          "later explicit clear teardown can delete once after creator-close and new pending query");
}
