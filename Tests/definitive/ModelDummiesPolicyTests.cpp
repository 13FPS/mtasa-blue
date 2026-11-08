/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual MTA dummy cache/hold/store/reset ordering without DE headers
 *****************************************************************************/
#include <game/CModelVehicleDummyPolicy.h>
#include <cstdio>
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
        using P = MtaGame::CModelVehicleDummyPolicy;
        std::map<unsigned, int> originals;
        bool                    hasModel = false;
        unsigned                holds = 0;
        int                     current = 7;
        std::string             order;
        const auto              set = [&](int desired)
        {
            return P::Mutate(
                [&](bool& first)
                {
                    order += 'C';
                    first = !hasModel;
                    hasModel = true;
                    return true;
                },
                [&]
                {
                    order += 'H';
                    Check(hasModel, "actual per-model map exists before its first hold");
                    ++holds;
                    return true;
                },
                [&]
                {
                    order += 'D';
                    originals.emplace(6, current);
                    return true;
                },
                [&]
                {
                    order += 'S';
                    current = desired;
                    return true;
                });
        };
        Check(set(7) && order == "CHDS" && holds == 1 && originals.at(6) == 7, "same-value dummy Set still captures its first original and model hold");
        order.clear();
        Check(set(11) && order == "CDS" && holds == 1 && originals.at(6) == 7 && current == 11,
              "repeated edit preserves the first default and never duplicates its model hold");
        order.clear();
        Check(!P::Mutate(
                  [&](bool&)
                  {
                      order += 'C';
                      return false;
                  },
                  [&]
                  {
                      order += 'H';
                      return true;
                  },
                  [&]
                  {
                      order += 'D';
                      return true;
                  },
                  [&]
                  {
                      order += 'S';
                      return true;
                  }) &&
                  order == "C",
              "CPU cache refusal enters neither hold nor vector mutation");
        order.clear();
        Check(!P::Mutate(
                  [&](bool& first)
                  {
                      order += 'C';
                      first = true;
                      return true;
                  },
                  [&]
                  {
                      order += 'H';
                      return false;
                  },
                  [&]
                  {
                      order += 'D';
                      return true;
                  },
                  [&]
                  {
                      order += 'S';
                      return true;
                  }) &&
                  order == "CH",
              "hold refusal preserves its cache prefix and stops before default/store callbacks");
        order.clear();
        Check(!P::Mutate(
                  [&](bool& first)
                  {
                      order += 'C';
                      first = false;
                      return true;
                  },
                  [&]
                  {
                      order += 'H';
                      return true;
                  },
                  [&]
                  {
                      order += 'D';
                      return false;
                  },
                  [&]
                  {
                      order += 'S';
                      return true;
                  }) &&
                  order == "CD",
              "default capture refusal never enters a vector store");
        order.clear();
        Check(!P::Mutate(
                  [&](bool& first)
                  {
                      order += 'C';
                      first = false;
                      return true;
                  },
                  [&]
                  {
                      order += 'H';
                      return true;
                  },
                  [&]
                  {
                      order += 'D';
                      return true;
                  },
                  [&]
                  {
                      order += 'S';
                      current = 12;
                      return false;
                  }) &&
                  order == "CDS" && current == 12 && holds == 1,
              "entered vector uncertainty adds no rollback or reference decrement");
        order.clear();
        Check(!P::RestoreOne(
                  originals,
                  [&](unsigned, int value)
                  {
                      order += 'V';
                      current = value;
                      return false;
                  },
                  [&]
                  {
                      order += 'R';
                      --holds;
                      return true;
                  },
                  true,
                  [&]
                  {
                      order += 'E';
                      originals.clear();
                  }) &&
                  order == "V" && holds == 1 && originals.at(6) == 7 && current == 7,
              "incomplete restore retains the actual hold/default evidence");
        order.clear();
        Check(!P::RestoreOne(
                  originals,
                  [&](unsigned, int value)
                  {
                      order += 'V';
                      current = value;
                      return true;
                  },
                  [&]
                  {
                      order += 'R';
                      return false;
                  },
                  true,
                  [&]
                  {
                      order += 'E';
                      originals.clear();
                  }) &&
                  order == "VR" && originals.size() == 1,
              "reference release refusal keeps the cache after acknowledged vector restoration");
        order.clear();
        Check(P::RestoreOne(
                  originals,
                  [&](unsigned, int value)
                  {
                      order += 'V';
                      Check(holds == 1, "hold survives every vector callback");
                      current = value;
                      return true;
                  },
                  [&]
                  {
                      order += 'R';
                      --holds;
                      return true;
                  },
                  false,
                  [&]
                  {
                      order += 'E';
                      originals.clear();
                  }) &&
                  order == "VR" && holds == 0 && originals.size() == 1,
              "bulk's Reset(false) keeps its map alive after the reference decrement");
        originals.clear();
        order.clear();
        holds = 1;
        Check(P::RestoreOne(
                  originals,
                  [&](unsigned, int)
                  {
                      order += 'V';
                      return true;
                  },
                  [&]
                  {
                      order += 'R';
                      --holds;
                      return true;
                  },
                  true,
                  [&]
                  {
                      order += 'E';
                      hasModel = false;
                  }) &&
                  order == "RE" && holds == 0 && !hasModel,
              "an empty per-model map still releases its first hold before erasing");
        std::map<unsigned, int> models{{1, 11}, {2, 22}};
        unsigned                called = 0;
        Check(!P::RestoreAll(models,
                             [&](unsigned key, int&)
                             {
                                 ++called;
                                 Check(models.size() == 2, "whole cache survives bulk callbacks");
                                 return key != 2;
                             }) &&
                  models.size() == 2 && called == 2,
              "failed bulk retains all entries and its accepted prefix");
        called = 0;
        Check(!P::RestoreAll(
                  models,
                  [&](unsigned, int&)
                  {
                      ++called;
                      return true;
                  },
                  [] { return false; }) &&
                  models.size() == 2 && called == 2,
              "private reference-journal acknowledgement precedes the whole-map clear");
        Check(P::RestoreAll(
                  models, [](unsigned, int&) { return true; }, [&] { return models.size() == 2; }) &&
                  models.empty(),
              "all models clear together after every acknowledged vector and hold effect");
    }
}
int main()
{
    try
    {
        Policy();
        std::puts("Shared MTA model dummy policy fixture completed");
        std::printf("Passed %u shared MTA model dummy policy checks (%u-bit; no DE native graph)\n", checks, static_cast<unsigned>(sizeof(void*) * 8));
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
