/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve accepted MTA setter prefixes and block facade lifetimes
 ****************************************************************************/
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

namespace
{
    struct MoveItineraryFailure
    {
        const char* stage;
    };
    struct MoveItineraryOperations;
    struct MoveItineraryBlock
    {
        MoveItineraryOperations& operations;
        int                      index;
        ~MoveItineraryBlock();
        bool IsLoaded();
        void Request(EModelRequestType, bool);
        void AddRef();
    };
    struct MoveItineraryOperations
    {
        using BlockName = std::string;
        enum class Fault
        {
            None,
            Request,
            AddRef,
            NativeStore,
            Reapply
        };
        std::vector<std::string> trace;
        std::vector<std::string> names;
        bool                     firstLoaded = true, secondLoaded = true, blockLoaded = false, valid = true, referenced = false;
        bool                     nullFirst = false, nullFinal = false;
        int                      loads = 0, lookups = 0, refs = 0, custom = 7, nativeGroup = MOVE_MAN, reapplyEntries = 0;
        unsigned                 requests = 0;
        EModelRequestType        requestType = NON_BLOCKING;
        bool                     allowBlockingFail = false;
        Fault                    fault = Fault::None;
        MoveItineraryOperations()
        {
            trace.reserve(32);
            names.reserve(2);
        }
        void ClearOverride()
        {
            trace.emplace_back("clear");
            custom = 0;
        }
        bool IsGroupLoaded(std::uint32_t)
        {
            trace.emplace_back("loaded" + std::to_string(++loads));
            return loads == 1 ? firstLoaded : secondLoaded;
        }
        bool IsGroupValid(std::uint32_t)
        {
            trace.emplace_back("valid");
            return valid;
        }
        std::unique_ptr<MoveItineraryBlock> GetAnimationBlock(const std::string& name)
        {
            trace.emplace_back("get" + std::to_string(++lookups));
            names.push_back(name);
            if ((lookups == 1 && nullFirst) || (firstLoaded && nullFinal) || (lookups == 2 && nullFinal))
                return nullptr;
            return std::unique_ptr<MoveItineraryBlock>(new MoveItineraryBlock{*this, lookups});
        }
        bool HasBlockReference(const std::string&)
        {
            trace.emplace_back("has");
            return referenced;
        }
        void RecordBlockReference(const std::string&)
        {
            trace.emplace_back("record");
            referenced = true;
        }
        void StoreOverride(std::uint32_t group)
        {
            trace.emplace_back("cache");
            custom = static_cast<int>(group);
        }
        void StoreNativeGroup(std::uint32_t group)
        {
            trace.emplace_back("group");
            nativeGroup = static_cast<int>(group);
            if (fault == Fault::NativeStore)
                throw MoveItineraryFailure{"native-store"};
        }
        void Reapply()
        {
            trace.emplace_back("reapply");
            ++reapplyEntries;
            if (fault == Fault::Reapply)
                throw MoveItineraryFailure{"reapply"};
        }
    };
    MoveItineraryBlock::~MoveItineraryBlock()
    {
        operations.trace.emplace_back("destroy" + std::to_string(index));
    }
    bool MoveItineraryBlock::IsLoaded()
    {
        operations.trace.emplace_back("block_loaded" + std::to_string(index));
        return operations.blockLoaded;
    }
    void MoveItineraryBlock::Request(EModelRequestType type, bool allowFail)
    {
        operations.trace.emplace_back("request" + std::to_string(index));
        ++operations.requests;
        operations.requestType = type;
        operations.allowBlockingFail = allowFail;
        ++operations.refs;
        if (operations.fault == MoveItineraryOperations::Fault::Request)
            throw MoveItineraryFailure{"request"};
    }
    void MoveItineraryBlock::AddRef()
    {
        operations.trace.emplace_back("add" + std::to_string(index));
        ++operations.refs;
        if (operations.fault == MoveItineraryOperations::Fault::AddRef)
            throw MoveItineraryFailure{"add-ref"};
    }
    void CheckMoveTrace(const MoveItineraryOperations& operations, std::initializer_list<const char*> expected, const char* why)
    {
        std::vector<std::string> values;
        for (const char* value : expected)
            values.emplace_back(value);
        Check(operations.trace == values, why);
    }
    void ApplyMoveExpectFailure(MoveItineraryOperations& operations, const char* stage)
    {
        bool sameFailure = false;
        try
        {
            MtaGame::CPlayerMoveAnimPolicy::SetMoveAnim(MOVE_PLAYER_F, operations);
        }
        catch (const MoveItineraryFailure& failure)
        {
            sameFailure = std::strcmp(failure.stage, stage) == 0;
        }
        Check(sameFailure, "an entered effect exception propagates unchanged; the itinerary neither retries nor relabels it");
    }
    void TestPlayerMoveAnimItinerary()
    {
        using MovePolicy = MtaGame::CPlayerMoveAnimPolicy;
        using Fault = MoveItineraryOperations::Fault;
        {
            MoveItineraryOperations operations;
            MovePolicy::SetMoveAnim(71, operations);
            CheckMoveTrace(operations, {}, "invalid groups enter no owner operation");
            Check(operations.custom == 7 && operations.nativeGroup == MOVE_MAN && !operations.refs && !operations.reapplyEntries,
                  "invalid input preserves both CPU override and native state");
        }
        {
            MoveItineraryOperations operations;
            operations.referenced = true;
            operations.refs = 9;
            MovePolicy::SetMoveAnim(MOVE_DEFAULT, operations);
            CheckMoveTrace(operations, {"clear"}, "default clears only the wrapper override");
            Check(!operations.custom && operations.nativeGroup == MOVE_MAN && operations.refs == 9 && operations.referenced,
                  "default never rewrites native group, references or module intent");
        }
        {
            MoveItineraryOperations operations;
            MovePolicy::SetMoveAnim(MOVE_PLAYER_F, operations);
            CheckMoveTrace(operations, {"loaded1", "valid", "get1", "has", "record", "add1", "cache", "group", "reapply", "destroy1"},
                           "preloaded success records intent before AddRef and commits cache/group/original in order");
            Check(operations.custom == MOVE_PLAYER_F && operations.nativeGroup == MOVE_PLAYER_F && operations.refs == 1 && operations.reapplyEntries == 1,
                  "preloaded success acquires exactly one reference and enters the original once");
        }
        {
            MoveItineraryOperations operations;
            operations.referenced = true;
            operations.refs = 8;
            MovePolicy::SetMoveAnim(MOVE_PLAYER_M, operations);
            CheckMoveTrace(operations, {"loaded1", "valid", "get1", "has", "cache", "group", "reapply", "destroy1"},
                           "an already-recorded name still performs the final block lookup");
            Check(operations.lookups == 1 && operations.refs == 8 && operations.reapplyEntries == 1, "a recorded block does not add another native reference");
        }
        {
            MoveItineraryOperations operations;
            operations.firstLoaded = false;
            MovePolicy::SetMoveAnim(MOVE_SKATE, operations);
            CheckMoveTrace(operations,
                           {"loaded1", "get1", "block_loaded1", "request1", "record", "loaded2", "destroy1", "valid", "get2", "has", "cache", "group",
                            "reapply", "destroy2"},
                           "blocking load rechecks first, retires its first facade, then validates and reacquires the final facade");
            Check(operations.requests == 1 && operations.requestType == BLOCKING && operations.allowBlockingFail,
                  "the setter preserves Request(BLOCKING,true), including the allowed blocking failure");
            Check(operations.refs == 1 && operations.referenced && operations.lookups == 2 && operations.custom == MOVE_SKATE &&
                      operations.nativeGroup == MOVE_SKATE && operations.names == std::vector<std::string>{"skate", "skate"},
                  "the request's accepted reference suppresses the later AddRef without suppressing the later lookup");
        }
        {
            MoveItineraryOperations operations;
            operations.firstLoaded = false;
            operations.blockLoaded = true;
            MovePolicy::SetMoveAnim(MOVE_PLAYER_F, operations);
            CheckMoveTrace(
                operations,
                {"loaded1", "get1", "block_loaded1", "loaded2", "destroy1", "valid", "get2", "has", "record", "add2", "cache", "group", "reapply", "destroy2"},
                "a block already loaded skips Request while preserving group recheck and final AddRef");
            Check(!operations.requests && operations.refs == 1 && operations.lookups == 2, "block-loaded and group-loaded are distinct observations");
        }
        {
            MoveItineraryOperations operations;
            operations.firstLoaded = false;
            operations.secondLoaded = false;
            MovePolicy::SetMoveAnim(MOVE_PLAYER_F, operations);
            CheckMoveTrace(operations, {"loaded1", "get1", "block_loaded1", "request1", "record", "loaded2", "destroy1"},
                           "a failed group load ends before validity and native group commit");
            Check(operations.refs == 1 && operations.referenced && operations.custom == 7 && operations.nativeGroup == MOVE_MAN,
                  "load failure preserves the accepted request/reference prefix");
        }
        {
            MoveItineraryOperations operations;
            operations.firstLoaded = false;
            operations.secondLoaded = false;
            operations.nullFirst = true;
            MovePolicy::SetMoveAnim(MOVE_PLAYER_F, operations);
            CheckMoveTrace(operations, {"loaded1", "get1", "loaded2"}, "a null load lookup still performs the required second group observation");
            Check(!operations.refs && !operations.referenced && operations.custom == 7, "a null first lookup does not invent a request or reference");
        }
        {
            MoveItineraryOperations operations;
            operations.valid = false;
            MovePolicy::SetMoveAnim(MOVE_PLAYER_F, operations);
            CheckMoveTrace(operations, {"loaded1", "valid"}, "invalid associations stop before the final block lookup");
            Check(!operations.lookups && !operations.refs && operations.custom == 7, "invalid associations enter no later mutation");
        }
        {
            MoveItineraryOperations operations;
            operations.nullFinal = true;
            MovePolicy::SetMoveAnim(MOVE_PLAYER_F, operations);
            CheckMoveTrace(operations, {"loaded1", "valid", "get1"}, "a missing final block is not bypassed by reference intent");
            Check(!operations.referenced && !operations.refs && operations.custom == 7 && !operations.reapplyEntries, "a null final lookup commits nothing");
        }
        {
            MoveItineraryOperations operations;
            operations.firstLoaded = false;
            operations.fault = Fault::Request;
            ApplyMoveExpectFailure(operations, "request");
            CheckMoveTrace(operations, {"loaded1", "get1", "block_loaded1", "request1", "destroy1"},
                           "request exception retires only the CPU facade before propagating");
            Check(operations.refs == 1 && !operations.referenced && operations.custom == 7,
                  "unknown Request completion is not recorded as a returned request or rolled back");
        }
        {
            MoveItineraryOperations operations;
            operations.fault = Fault::AddRef;
            ApplyMoveExpectFailure(operations, "add-ref");
            CheckMoveTrace(operations, {"loaded1", "valid", "get1", "has", "record", "add1", "destroy1"},
                           "AddRef exception leaves source-ordered module intent");
            Check(operations.refs == 1 && operations.referenced && operations.custom == 7 && !operations.reapplyEntries,
                  "accepted AddRef prefix is not replayed or undone");
        }
        {
            MoveItineraryOperations operations;
            operations.fault = Fault::NativeStore;
            ApplyMoveExpectFailure(operations, "native-store");
            CheckMoveTrace(operations, {"loaded1", "valid", "get1", "has", "record", "add1", "cache", "group", "destroy1"},
                           "native-store exception cannot enter the later original");
            Check(operations.custom == MOVE_PLAYER_F && operations.nativeGroup == MOVE_PLAYER_F && operations.refs == 1 && !operations.reapplyEntries,
                  "CPU/native accepted stores remain intact without a rollback");
        }
        {
            MoveItineraryOperations operations;
            operations.fault = Fault::Reapply;
            ApplyMoveExpectFailure(operations, "reapply");
            CheckMoveTrace(operations, {"loaded1", "valid", "get1", "has", "record", "add1", "cache", "group", "reapply", "destroy1"},
                           "an entered original exception is propagated once with typed facade cleanup");
            Check(operations.custom == MOVE_PLAYER_F && operations.nativeGroup == MOVE_PLAYER_F && operations.referenced && operations.refs == 1 &&
                      operations.reapplyEntries == 1,
                  "reapply uncertainty retains every accepted prefix and enters no replay");
        }
        {
            MoveItineraryOperations operations;
            operations.firstLoaded = false;
            operations.nullFirst = true;
            MovePolicy::SetMoveAnim(MOVE_PLAYER_F, operations);
            CheckMoveTrace(operations, {"loaded1", "get1", "loaded2", "valid", "get2", "has", "record", "add2", "cache", "group", "reapply", "destroy2"},
                           "a later admitted group load may continue despite the first missing facade");
            Check(operations.lookups == 2 && !operations.requests && operations.refs == 1 && operations.reapplyEntries == 1,
                  "fresh group observation, not the earlier nullable facade, controls continuation");
        }
    }
}
