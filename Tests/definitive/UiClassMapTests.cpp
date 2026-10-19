/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Production startup dictionary lookup over guarded owned buffers
 ****************************************************************************/
#include "../../Client/game_de/NativeUiClassMapAccess.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
    using Access = mta::game_de::NativeUiClassMapAccess;
    unsigned checks = 0;

    void Check(bool condition, const char* description)
    {
        ++checks;
        if (!condition)
        {
            std::fprintf(stderr, "FAIL after %u startup UI dictionary checks: %s\n", checks, description);
            std::exit(EXIT_FAILURE);
        }
    }

    template <typename T, std::size_t N>
    void Put(std::array<unsigned char, N>& bytes, std::size_t offset, T value)
    {
        Check(offset <= N && sizeof(value) <= N - offset, "owned fixture field fits its independent byte layout");
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
    }

    // Fixture offsets and bucket positions come from the independently saved
    // native lookup contract, rather than using the helper to construct its own
    // expected answer. No UClass address is dereferenced by this fixture.
    static_assert(sizeof(std::uintptr_t) == 8 && sizeof(char16_t) == 2);
    static_assert(mta::game_de::NativeUiClassMapProfile::keyLoadingHash == 0xad613fa4 &&
                  mta::game_de::NativeUiClassMapProfile::keySimpleFadeHash == 0x72aad08d);
    constexpr std::uintptr_t loadingClass = 0x11110000, fadeClass = 0x22220000;
    using Header = std::array<unsigned char, 0x50>;
    using Entry = std::array<unsigned char, 0x20>;

    struct Fixture;
    Fixture* current = nullptr;

    struct Fixture
    {
        enum class Mutation
        {
            None,
            Header,
            Bucket,
            Path,
            Key
        };
        Header                    header{};
        std::vector<Entry>        entries;
        std::vector<std::int32_t> buckets;
        std::array<char16_t, 12>  loading{u'L', u'o', u'a', u'd', u'i', u'n', u'g', 0};
        std::array<char16_t, 12>  fade{u'S', u'i', u'm', u'p', u'l', u'e', u'F', u'a', u'd', u'e', 0};
        std::array<char16_t, 12>  missing{u'M', u'i', u's', u's', u'i', u'n', u'g', 0};
        unsigned                  reads = 0, headerReads = 0, bucketReads = 0, pathReads = 0, keyReads = 0, denied = 0, failRead = 0;
        std::size_t               headerLimit = 0x50, entryLimit = (std::numeric_limits<std::size_t>::max)(), keyLimit = 24;
        Mutation                  mutation = Mutation::None;
        std::uintptr_t            watchedBucket = 0;

        explicit Fixture(std::size_t slots = 2) : entries(slots), buckets(8, -1)
        {
            current = this;
            Put(header, 0x00, reinterpret_cast<std::uintptr_t>(entries.data()));
            Put(header, 0x08, static_cast<std::int32_t>(slots));
            Put(header, 0x0c, std::int32_t{-713});  // Unproved capacity is deliberately not a validation input.
            Put(header, 0x34, std::int32_t{0});
            Put(header, 0x38, std::int32_t{0});
            Put(header, 0x40, std::uintptr_t{0});
            Put(header, 0x48, std::int32_t{1});
            for (std::size_t index = 0; index < slots; ++index)
                Node(index, missing.data(), 8, 0x33330000, index + 1 < slots ? static_cast<std::int32_t>(index + 1) : -1);
            if (slots)
                Node(0, loading.data(), 8, loadingClass, -1);
            if (slots > 1)
                Node(1, fade.data(), 11, fadeClass, -1);
            watchedBucket = Address() + 0x38;
        }

        std::uintptr_t Address() const { return reinterpret_cast<std::uintptr_t>(header.data()); }

        void Node(std::size_t index, const char16_t* text, std::int32_t units, std::uintptr_t value, std::int32_t next)
        {
            Check(index < entries.size(), "owned fixture node index exists");
            Put(entries[index], 0x00, reinterpret_cast<std::uintptr_t>(text));
            Put(entries[index], 0x08, units);
            Put(entries[index], 0x10, value);
            Put(entries[index], 0x18, next);
        }

        void Heap()
        {
            // These positions are fixed expected residues of the independently
            // established hashes under an eight-bucket mask.
            buckets[4] = 0;
            buckets[5] = 1;
            Put(header, 0x40, reinterpret_cast<std::uintptr_t>(buckets.data()));
            Put(header, 0x48, std::int32_t{8});
            watchedBucket = reinterpret_cast<std::uintptr_t>(buckets.data() + 4);
        }

        static bool In(std::uintptr_t at, std::size_t bytes, const void* begin, std::size_t length) noexcept
        {
            const auto base = reinterpret_cast<std::uintptr_t>(begin);
            return bytes <= length && at >= base && at - base <= length - bytes;
        }

        bool Read(std::uintptr_t at, void* output, std::size_t bytes) noexcept
        {
            ++reads;
            const bool allowed = In(at, bytes, header.data(), headerLimit) ||
                                 In(at, bytes, entries.data(), entryLimit < entries.size() * sizeof(Entry) ? entryLimit : entries.size() * sizeof(Entry)) ||
                                 In(at, bytes, buckets.data(), buckets.size() * sizeof(std::int32_t)) || In(at, bytes, loading.data(), keyLimit) ||
                                 In(at, bytes, fade.data(), keyLimit) || In(at, bytes, missing.data(), keyLimit);
            if (!allowed || reads == failRead)
            {
                ++denied;
                // A guarded source can fail after writing a partial local
                // sample. The caller's Value must still remain untouched.
                std::memset(output, 0xa7, bytes);
                return false;
            }
            std::memcpy(output, reinterpret_cast<const void*>(at), bytes);
            if (at == Address() && bytes == header.size() && ++headerReads == 2 && mutation == Mutation::Header)
            {
                const std::int32_t changedCount = 3;
                std::memcpy(static_cast<unsigned char*>(output) + 8, &changedCount, sizeof(changedCount));
            }
            if (at == watchedBucket && bytes == sizeof(std::int32_t) && ++bucketReads == 2 && mutation == Mutation::Bucket)
            {
                const std::int32_t changedIndex = 1;
                std::memcpy(output, &changedIndex, sizeof(changedIndex));
            }
            if (!entries.empty() && at == reinterpret_cast<std::uintptr_t>(entries.data()) && bytes == sizeof(Entry) && ++pathReads == 2 &&
                mutation == Mutation::Path)
            {
                const std::uintptr_t changedValue = 0x44440000;
                std::memcpy(static_cast<unsigned char*>(output) + 0x10, &changedValue, sizeof(changedValue));
            }
            if (at == reinterpret_cast<std::uintptr_t>(loading.data()) && ++keyReads == 2 && mutation == Mutation::Key)
            {
                const char16_t changedUnit = u'B';
                std::memcpy(output, &changedUnit, sizeof(changedUnit));
            }
            return true;
        }

        static bool GuardedRead(std::uintptr_t at, void* output, std::size_t bytes) noexcept { return current && current->Read(at, output, bytes); }

        bool Lookup(Access::Key key, Access::Value& output) { return Access(Address(), &GuardedRead).Lookup(key, output); }
    };

    void Refuses(Fixture& fixture, Access::Key key = Access::Key::Loading)
    {
        Access::Value output{0x77770000, true};
        Check(!fixture.Lookup(key, output) && output.classAddress == 0x77770000 && output.keyPresent,
              "invalid, changing or unreadable map refuses without changing either caller output field");
    }

    void ValidLookups()
    {
        {
            Fixture fixture;
            fixture.Node(0, fixture.loading.data(), 8, loadingClass, 1);
            Access::Value value;
            Check(fixture.Lookup(Access::Key::Loading, value) && value.keyPresent && value.classAddress == loadingClass,
                  "inline bucket finds Loading despite an unproved negative capacity field");
            Check(fixture.Lookup(Access::Key::SimpleFade, value) && value.keyPresent && value.classAddress == fadeClass,
                  "inline collision chain finds SimpleFade using its exact eleven-unit key");
        }
        {
            Fixture fixture;
            fixture.Heap();
            Access::Value value;
            Check(fixture.Lookup(Access::Key::Loading, value) && value.keyPresent && value.classAddress == loadingClass &&
                      fixture.Lookup(Access::Key::SimpleFade, value) && value.keyPresent && value.classAddress == fadeClass,
                  "independent native hash constants choose distinct known heap buckets");
        }
        {
            Fixture fixture;
            fixture.Node(0, fixture.missing.data(), 8, 0xdeadbeef, 1);
            fixture.Node(1, fixture.loading.data(), 8, 0, -1);
            Access::Value value{0x77770000, false};
            Check(fixture.Lookup(Access::Key::Loading, value) && value.keyPresent && value.classAddress == 0,
                  "a matched null raw class after a collision remains distinct from an absent key");
            Check(fixture.Lookup(Access::Key::SimpleFade, value) && !value.keyPresent && value.classAddress == 0,
                  "a terminated unmatched chain publishes absence rather than an opaque class from another entry");
        }
        for (const bool allFree : {false, true})
        {
            Fixture fixture;
            Put(fixture.header, 0x08, std::int32_t{allFree ? 2 : 0});
            Put(fixture.header, 0x34, std::int32_t{allFree ? 2 : 0});
            Put(fixture.header, 0x00, std::uintptr_t{1});
            Put(fixture.header, 0x40, std::uintptr_t{1});
            Put(fixture.header, 0x48, std::int32_t{0});
            Access::Value value{0x77770000, true};
            Check(fixture.Lookup(Access::Key::Loading, value) && !value.keyPresent && value.classAddress == 0 && fixture.reads == fixture.headerReads &&
                      !fixture.denied,
                  "zero-slot and all-free headers are coherent empty maps without following irrelevant child pointers");
        }
        {
            Fixture fixture;
            Put(fixture.header, 0x38, std::int32_t{-1});
            Access::Value value{0x77770000, true};
            Check(fixture.Lookup(Access::Key::Loading, value) && !value.keyPresent && value.classAddress == 0,
                  "minus-one bucket terminates with ordinary absence");
        }
        for (const bool upper : {false, true})
        {
            Fixture fixture;
            for (std::size_t i = 0; i != 7; ++i)
            {
                auto& unit = fixture.loading[i];
                if (upper && unit >= u'a' && unit <= u'z')
                    unit = static_cast<char16_t>(unit - (u'a' - u'A'));
                if (!upper && unit >= u'A' && unit <= u'Z')
                    unit = static_cast<char16_t>(unit + (u'a' - u'A'));
            }
            Access::Value value;
            Check(fixture.Lookup(Access::Key::Loading, value) && value.keyPresent && value.classAddress == loadingClass,
                  "native ASCII case folding accepts uppercase and lowercase spellings");
        }
        {
            Fixture fixture;
            fixture.loading[1] = u'\u00d6';
            Access::Value value{0x77770000, true};
            Check(fixture.Lookup(Access::Key::Loading, value) && !value.keyPresent && value.classAddress == 0,
                  "a differing non-ASCII unit does not acquire ASCII case-fold equivalence");
            fixture.loading[1] = u'o';
            Put(fixture.entries[0], 8, std::int32_t{7});
            Check(fixture.Lookup(Access::Key::Loading, value) && !value.keyPresent,
                  "a different FString count cannot match Loading without its exact NUL-inclusive length");
        }
    }

    void RefusalAndFaults()
    {
        for (unsigned invalid = 0; invalid != 18; ++invalid)
        {
            Fixture fixture;
            if (invalid == 0)
                Put(fixture.header, 8, std::int32_t{-1});
            if (invalid == 1)
                Put(fixture.header, 8, std::int32_t{4097});
            if (invalid == 2)
                Put(fixture.header, 0x34, std::int32_t{-1});
            if (invalid == 3)
                Put(fixture.header, 0x34, std::int32_t{3});
            if (invalid == 4)
                Put(fixture.header, 0x48, std::int32_t{0});
            if (invalid == 5)
                Put(fixture.header, 0x48, std::int32_t{3});
            if (invalid == 6)
                Put(fixture.header, 0x48, std::int32_t{8192});
            if (invalid == 7)
                Put(fixture.header, 0, std::uintptr_t{0});
            if (invalid == 8)
                Put(fixture.header, 0x38, std::int32_t{-2});
            if (invalid == 9)
                Put(fixture.header, 0x38, std::int32_t{2});
            if (invalid == 10)
                Put(fixture.entries[0], 8, std::int32_t{-1});
            if (invalid == 11)
                fixture.loading[7] = u'x';
            if (invalid == 12)
                fixture.loading[2] = 0;
            if (invalid == 13)
                fixture.headerLimit = 79;
            if (invalid == 14)
                fixture.entryLimit = 31;
            if (invalid == 15)
                fixture.keyLimit = 15;
            if (invalid == 16)
                Put(fixture.entries[0], 0, std::uintptr_t{1});
            if (invalid == 17)
                Put(fixture.header, 0x48, std::int32_t{-1});
            Refuses(fixture);
        }
        for (const std::int32_t next : {0, -2, 2})
        {
            Fixture fixture;
            fixture.Node(0, fixture.missing.data(), 8, 0, next);
            Refuses(fixture);
        }
        {
            Fixture       fixture;
            Access::Value value{0x77770000, true};
            Check(!Access(fixture.Address(), nullptr).Lookup(Access::Key::Loading, value) &&
                      !Access(0, &Fixture::GuardedRead).Lookup(Access::Key::Loading, value) && value.classAddress == 0x77770000 && value.keyPresent,
                  "null reader and null map cannot publish a fabricated result");
            Refuses(fixture, static_cast<Access::Key>(7));
            const auto before = fixture.reads;
            Check(!Access((std::numeric_limits<std::uintptr_t>::max)() - 39, &Fixture::GuardedRead).Lookup(Access::Key::Loading, value) &&
                      fixture.reads == before && value.classAddress == 0x77770000 && value.keyPresent,
                  "header address overflow refuses before invoking the guarded callback");
        }
        for (unsigned overflow = 0; overflow != 3; ++overflow)
        {
            Fixture fixture;
            if (overflow == 0)
            {
                Put(fixture.header, 0x00, (std::numeric_limits<std::uintptr_t>::max)() - 15);
                Put(fixture.header, 0x38, std::int32_t{1});
            }
            if (overflow == 1)
            {
                fixture.Heap();
                Put(fixture.header, 0x40, (std::numeric_limits<std::uintptr_t>::max)() - 1);
            }
            if (overflow == 2)
                Put(fixture.entries[0], 0, (std::numeric_limits<std::uintptr_t>::max)() - 7);
            Refuses(fixture);
        }
        unsigned completeReads = 0;
        {
            Fixture       fixture;
            Access::Value value;
            Check(fixture.Lookup(Access::Key::Loading, value), "obtain a complete production helper trace for every-position read fault injection");
            completeReads = fixture.reads;
        }
        for (unsigned failure = 1; failure <= completeReads; ++failure)
        {
            Fixture fixture;
            fixture.failRead = failure;
            Refuses(fixture);
            Check(fixture.denied == 1, "each read-position failure is actually reached and reported by the guarded callback");
        }
        for (const auto mutation : {Fixture::Mutation::Header, Fixture::Mutation::Bucket, Fixture::Mutation::Path, Fixture::Mutation::Key})
        {
            Fixture fixture;
            fixture.mutation = mutation;
            Refuses(fixture);
        }
        {
            Fixture fixture;
            fixture.Node(0, fixture.missing.data(), 8, 0, 1);
            fixture.Node(1, fixture.loading.data(), 8, loadingClass, -1);
            fixture.mutation = Fixture::Mutation::Path;
            Refuses(fixture);
        }
    }

    void Bounds()
    {
        for (const std::size_t length : {64u, 65u})
        {
            Fixture fixture(length);
            for (std::size_t index = 0; index < length; ++index)
                fixture.Node(index, fixture.missing.data(), 8, 0, index + 1 < length ? static_cast<std::int32_t>(index + 1) : -1);
            fixture.Node(length - 1, fixture.loading.data(), 8, loadingClass, -1);
            if (length == 65)
                Refuses(fixture);
            else
            {
                Access::Value value;
                Check(fixture.Lookup(Access::Key::Loading, value) && value.keyPresent && value.classAddress == loadingClass,
                      "a valid exactly-64-node collision chain remains inside the conservative bound");
                fixture.Node(length - 1, fixture.missing.data(), 8, 0, -1);
                Check(fixture.Lookup(Access::Key::Loading, value) && !value.keyPresent && value.classAddress == 0,
                      "a bounded long unmatched chain can coherently report absence");
            }
        }
        {
            Fixture fixture(4096);
            fixture.buckets.assign(4096, -1);
            fixture.buckets[4004] = 4095;  // Loading native hash low12 bits = 0xfa4.
            fixture.Node(4095, fixture.loading.data(), 8, loadingClass, -1);
            Put(fixture.header, 0x34, std::int32_t{4095});
            Put(fixture.header, 0x40, reinterpret_cast<std::uintptr_t>(fixture.buckets.data()));
            Put(fixture.header, 0x48, std::int32_t{4096});
            Access::Value value;
            Check(fixture.Lookup(Access::Key::Loading, value) && value.keyPresent && value.classAddress == loadingClass,
                  "maximum admitted sparse slot and hash bounds resolve a real high index with native bucket residue");
        }
    }
}

int main()
{
    ValidLookups();
    RefusalAndFaults();
    Bounds();
    std::puts("Production startup UI dictionary fixture completed");
    std::printf("Passed %u startup UI dictionary checks\n", checks);
    return EXIT_SUCCESS;
}
