/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the actual shared object-group cache itinerary on x86
 *****************************************************************************/
#include <game/CModelObjectGroupPolicy.h>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <map>
#include <stdexcept>
#include <string>

namespace
{
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
    }
    void Policy()
    {
        using P = MtaGame::CModelObjectGroupPolicy;
        std::map<unsigned, std::uint16_t> originals;
        std::uint16_t                     current = 17;
        std::string                       sequence;
        const auto                        set = [&](std::uint16_t requested)
        {
            return P::Set(
                requested,
                [&](std::uint16_t& value)
                {
                    sequence += 'R';
                    value = current;
                    return true;
                },
                [&](std::uint16_t value)
                {
                    sequence += 'M';
                    originals.emplace(7, value);
                    return true;
                },
                [&](std::uint16_t value)
                {
                    sequence += 'S';
                    current = value;
                    return true;
                });
        };
        Check(set(17) && sequence == "R" && originals.empty(), "same-value Set does not manufacture a default");
        sequence.clear();
        Check(set(21) && sequence == "RMS" && originals.at(7) == 17 && current == 21, "first changed Set remembers before storing");
        sequence.clear();
        Check(set(23) && sequence == "RMS" && originals.at(7) == 17 && current == 23, "repeated Set preserves the first original");
        sequence.clear();
        Check(set(23) && sequence == "R" && originals.at(7) == 17, "same-value Set leaves an existing original untouched");
        sequence.clear();
        Check(!P::Set(
                  23,
                  [&](std::uint16_t& value)
                  {
                      sequence += 'R';
                      value = 23;
                      return false;
                  },
                  [&](std::uint16_t)
                  {
                      sequence += 'M';
                      return true;
                  },
                  [&](std::uint16_t)
                  {
                      sequence += 'S';
                      return true;
                  }) &&
                  sequence == "R",
              "a refused read cannot become a same-value success through its output");
        sequence.clear();
        Check(!P::Set(
                  29,
                  [&](std::uint16_t& value)
                  {
                      sequence += 'R';
                      value = current;
                      return true;
                  },
                  [&](std::uint16_t value)
                  {
                      sequence += 'M';
                      originals.emplace(8, value);
                      return false;
                  },
                  [&](std::uint16_t)
                  {
                      sequence += 'S';
                      return true;
                  }) &&
                  sequence == "RM" && originals.at(8) == 23,
              "remember refusal prevents store and preserves an already accepted cache prefix");
        sequence.clear();
        Check(!P::Set(
                  31,
                  [&](std::uint16_t& value)
                  {
                      sequence += 'R';
                      value = current;
                      return true;
                  },
                  [&](std::uint16_t value)
                  {
                      sequence += 'M';
                      originals.emplace(9, value);
                      return true;
                  },
                  [&](std::uint16_t value)
                  {
                      sequence += 'S';
                      current = value;
                      return false;
                  }) &&
                  sequence == "RMS" && current == 31 && originals.at(9) == 23,
              "entered store uncertainty retains the original and never adds rollback or replay");
        sequence.clear();
        Check(P::RestoreOne(
                  [&](std::uint16_t&, bool& found)
                  {
                      sequence += 'F';
                      found = false;
                      return true;
                  },
                  [&](std::uint16_t)
                  {
                      sequence += 'S';
                      return true;
                  },
                  [&] { sequence += 'E'; }) &&
                  sequence == "F",
              "missing original is a clean restore no-op");
        sequence.clear();
        Check(!P::RestoreOne(
                  [&](std::uint16_t& value, bool& found)
                  {
                      sequence += 'F';
                      value = 17;
                      found = true;
                      return false;
                  },
                  [&](std::uint16_t)
                  {
                      sequence += 'S';
                      return true;
                  },
                  [&] { sequence += 'E'; }) &&
                  sequence == "F",
              "failed lookup never enters a restore store");
        sequence.clear();
        Check(!P::RestoreOne(
                  [&](std::uint16_t& value, bool& found)
                  {
                      sequence += 'F';
                      value = originals.at(7);
                      found = true;
                      return true;
                  },
                  [&](std::uint16_t value)
                  {
                      sequence += 'S';
                      current = value;
                      return false;
                  },
                  [&]
                  {
                      sequence += 'E';
                      originals.erase(7);
                  }) &&
                  sequence == "FS" && originals.at(7) == 17 && current == 17,
              "single restore retains its default until the store is acknowledged");
        sequence.clear();
        Check(P::RestoreOne(
                  [&](std::uint16_t& value, bool& found)
                  {
                      sequence += 'F';
                      value = originals.at(7);
                      found = true;
                      return true;
                  },
                  [&](std::uint16_t value)
                  {
                      sequence += 'S';
                      Check(originals.at(7) == value, "single restore's original remains alive during store");
                      current = value;
                      return true;
                  },
                  [&]
                  {
                      sequence += 'E';
                      originals.erase(7);
                  }) &&
                  sequence == "FSE" && originals.count(7) == 0 && current == 17,
              "single restore erases only after the acknowledged store");

        // A backend may qualify an absent model as a skip. The shared MTA
        // algorithm keeps every other original alive until the whole loop ends.
        originals = {{1U, std::uint16_t{10}}, {2U, std::uint16_t{20}}, {3U, std::uint16_t{30}}};
        std::map<unsigned, std::uint16_t> applied;
        unsigned                          callbacks = 0;
        Check(P::RestoreAll(originals,
                            [&](unsigned key, std::uint16_t value)
                            {
                                ++callbacks;
                                Check(originals.size() == 3, "bulk originals survive every callback");
                                if (key != 2)
                                    applied[key] = value;
                                return true;
                            }) &&
                  originals.empty() && callbacks == 3 && applied.size() == 2 && applied.at(3) == 30,
              "bulk skips absent receivers and clears only after all acknowledgements");
        originals = {{1U, std::uint16_t{10}}, {2U, std::uint16_t{20}}, {3U, std::uint16_t{30}}};
        applied.clear();
        callbacks = 0;
        Check(!P::RestoreAll(originals,
                             [&](unsigned key, std::uint16_t value)
                             {
                                 ++callbacks;
                                 applied[key] = value;
                                 return key != 2;
                             }) &&
                  originals.size() == 3 && callbacks == 2 && applied.size() == 2 && applied.count(3) == 0,
              "incomplete bulk stops at its entered prefix and retains all default evidence");
        callbacks = 0;
        bool threw = false;
        try
        {
            P::RestoreAll(originals,
                          [&](unsigned key, std::uint16_t value)
                          {
                              ++callbacks;
                              applied[key] = value;
                              if (key == 2)
                                  throw std::runtime_error("entered callback");
                              return true;
                          });
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }
        Check(threw && originals.size() == 3 && callbacks == 2, "callback exception preserves evidence without implicit recovery");

        struct Saved
        {
            std::uint16_t value;
            bool          acknowledged;
        };
        std::map<unsigned, Saved> journal{{1, {10, false}}, {2, {20, false}}, {3, {30, false}}};
        unsigned                  stores = 0;
        Check(!P::RestoreAll(journal,
                             [&](unsigned key, Saved& saved)
                             {
                                 ++stores;
                                 if (key == 2)
                                     return false;
                                 saved.acknowledged = true;
                                 return true;
                             }) &&
                  journal.at(1).acknowledged && !journal.at(2).acknowledged && stores == 2,
              "bulk owner can retain an acknowledgement journal inside the original map");
        Check(P::RestoreAll(journal,
                            [&](unsigned, Saved& saved)
                            {
                                if (!saved.acknowledged)
                                {
                                    ++stores;
                                    saved.acknowledged = true;
                                }
                                return true;
                            }) &&
                  journal.empty() && stores == 4,
              "explicit later restore can skip its known acknowledged prefix");
        for (const std::uint16_t raw :
             {std::uint16_t{0}, std::uint16_t{1}, std::uint16_t{255}, std::uint16_t{256}, std::uint16_t{0xfffe}, std::uint16_t{0xffff}})
        {
            std::uint16_t remembered = 0, stored = 0;
            Check(P::Set(
                      raw,
                      [&](std::uint16_t& value)
                      {
                          value = static_cast<std::uint16_t>(raw ^ 1);
                          return true;
                      },
                      [&](std::uint16_t value)
                      {
                          remembered = value;
                          return true;
                      },
                      [&](std::uint16_t value)
                      {
                          stored = value;
                          return true;
                      }) &&
                      stored == raw && remembered == static_cast<std::uint16_t>(raw ^ 1),
                  "all unsigned WORD bits cross Set without a clamp or sentinel reinterpretation");
        }
    }
}
int main()
{
    try
    {
        Policy();
        std::puts("Shared MTA model object-group policy fixture completed");
        std::printf("Passed %u shared MTA model object-group policy checks (%u-bit; no DE native graph)\n", checks, static_cast<unsigned>(sizeof(void*) * 8));
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
