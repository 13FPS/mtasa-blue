/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual MTA AllocateForBlock order without guessed cleanup
 ****************************************************************************/
#include "NativeAnimationAssociationGroupReplacement.h"
#include "NativeAnimationStaticAssociationProfile.h"
#include "NativeStaticAssociationArrayProfile.h"
#include "NativeAnimationBlockMemory.h"
#include "NativeAnimationBlockFactoryMemory.h"
#include "NativeAnimationBlockProfile.h"
#include "NativeStreamingCompletionContext.h"
#include <algorithm>
#include <cstring>
#include <intrin.h>
#include <limits>
#include <vector>
namespace mta::game_de
{
    // Only this source-specific publisher can lend a genuine fresh record to
    // an admitted current group. A matching raw address is never authority.
    struct NativeStaticAssociationArrayPublicationAccess
    {
        using S = NativeCustomAnimSequenceStorage;
        static bool Pin(S& s, std::uint64_t generation, std::uintptr_t group, const NativeModelReferenceHeap::Value& heap) noexcept
        {
            if (s.kind != S::Kind::StaticAnimationAssociationArray || s.generation != generation || !group || !s.base || s.tag != s.base + 8 ||
                s.capacity > ((std::numeric_limits<std::size_t>::max)() - 8) / 32 || s.bytes != 8 + s.capacity * 32 ||
                !NativeModelReferenceHeap::Same(s.origin, heap) || !NativeModelReferenceMap::PrivateWritable(s.base, s.bytes))
                return false;
            std::uint64_t cookie = 0;
            if (!NativeModelReferenceMap::Copy(s.base, &cookie, 8) || cookie != s.capacity)
                return false;
            auto expected = S::State::LiveUnpublished;
            if (!s.state.compare_exchange_strong(expected, S::State::PublicationPinned, std::memory_order_acq_rel))
                return false;
            s.hierarchyAffinity = group;
            return true;
        }
        static ClockResult RecheckPublished(std::uintptr_t image, const S& s, std::uint64_t generation, std::uintptr_t group,
                                            NativeAnimationManagerMemory::CheckOwner owner) noexcept
        {
            if (s.kind != S::Kind::StaticAnimationAssociationArray || s.generation != generation || s.hierarchyAffinity != group ||
                s.GetState() != S::State::EnginePublished || !s.base || s.tag != s.base + 8 ||
                s.capacity > ((std::numeric_limits<std::size_t>::max)() - 8) / 32 || s.bytes != 8 + s.capacity * 32)
                return ClockResult::OwnerConflict;
            NativeModelReferenceHeap::Value heap;
            auto                            result = NativeStaticAssociationArrayStorageCalls::CaptureRetirementHeap(image, owner, heap);
            if (result != ClockResult::Ok)
                return result;
            if (!NativeModelReferenceHeap::Same(s.origin, heap) || !NativeModelReferenceMap::PrivateWritable(s.base, s.bytes))
                return ClockResult::BindingChanged;
            std::uint64_t cookie = 0;
            if (!NativeModelReferenceMap::Copy(s.base, &cookie, 8))
                return ClockResult::ReadFailure;
            return cookie == s.capacity ? ClockResult::Ok : ClockResult::BindingChanged;
        }
        static void Reject(S& s) noexcept
        {
            s.hierarchyAffinity = 0;
            s.state.store(S::State::LiveUnpublished, std::memory_order_release);
        }
        static void Accept(S& s) noexcept { s.state.store(S::State::EnginePublished, std::memory_order_release); }
    };
    struct NativeAnimationAssociationGroupReplacement::Data
    {
        NativeAnimationAssociationGroupReplacementCalls::Receipt        receipt;
        NativeAnimationManagerMemory::AssociationGroupReplacementLayout expected;
        NativeAnimationAssociationGroupChildBorrowInvalidator           invalidator;
        NativeAnimationBlockIdentity                                    blockIdentity;
        NativeAnimationBlockValues                                      blockValues;
        NativeCustomAnimSequenceStorage*                                publishedStorage = nullptr;
        std::vector<unsigned char>                                      oldRows;
        std::vector<std::uintptr_t>                                     oldNodes;
    };
    NativeAnimationAssociationGroupReplacement::NativeAnimationAssociationGroupReplacement() noexcept = default;
    NativeAnimationAssociationGroupReplacement::~NativeAnimationAssociationGroupReplacement() = default;
    namespace AssociationGroupReplacementDetail
    {
        namespace C = NativeAnimationAssociationGroupReplacementCalls;
        namespace M = NativeModelReferenceMap;
        namespace Context = NativeStreamingCompletionContext;
        using State = C::Record::State;
        struct Busy
        {
            std::atomic<bool>& value;
            ~Busy() { value.store(false, std::memory_order_release); }
        };
        bool Enter(std::atomic<bool>& value) noexcept
        {
            bool expected = false;
            return value.compare_exchange_strong(expected, true, std::memory_order_acq_rel);
        }
        template <class T>
        T Field(const std::array<unsigned char, 32>& bytes, std::size_t offset) noexcept
        {
            T out{};
            std::memcpy(&out, bytes.data() + offset, sizeof(out));
            return out;
        }
        template <class T>
        void Put(std::array<unsigned char, 32>& bytes, std::size_t offset, T value) noexcept
        {
            std::memcpy(bytes.data() + offset, &value, sizeof(value));
        }
        ClockResult InvokeOriginal(C::RetireFunction entry, std::uintptr_t group, std::uint32_t& exception) noexcept
        {
            __try
            {
                entry(group);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        ClockResult Qword(std::uintptr_t address, std::uint64_t before, std::uint64_t desired, std::uint64_t& observed, bool& accepted,
                          std::uint32_t& exception) noexcept
        {
            LONG64 a = 0, b = 0;
            std::memcpy(&a, &before, 8);
            std::memcpy(&b, &desired, 8);
            accepted = false;
            __try
            {
                const auto previous = InterlockedCompareExchange64(reinterpret_cast<volatile LONG64*>(address), b, a);
                std::memcpy(&observed, &previous, 8);
                accepted = observed == before;
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        ClockResult Dword(std::uintptr_t address, std::uint32_t before, std::uint32_t desired, std::uint32_t& observed, bool& accepted,
                          std::uint32_t& exception) noexcept
        {
            LONG a = 0, b = 0;
            std::memcpy(&a, &before, 4);
            std::memcpy(&b, &desired, 4);
            accepted = false;
            __try
            {
                const auto previous = InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), b, a);
                std::memcpy(&observed, &previous, 4);
                accepted = observed == before;
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
        ClockResult RetireProfile(std::uintptr_t image, C::CheckOwner owner, NativeModelReferenceHeap::Value& heap) noexcept
        {
            std::array<unsigned char, NativeAnimationStaticAssociationProfile::Retire.size()> bytes{};
            if (!NativeImageAccess::ReadImage(image, image + 0xfeb580, bytes.data(), bytes.size(), false, true) ||
                bytes != NativeAnimationStaticAssociationProfile::Retire)
                return ClockResult::BindingChanged;
            return NativeStaticAssociationArrayStorageCalls::CaptureRetirementHeap(image, owner, heap);
        }
    }
    struct NativeAnimationAssociationGroupReplacementAccess
    {
        using R = NativeAnimationAssociationGroupReplacement;
        using D = R::Data;
        using C = NativeAnimationAssociationGroupReplacementCalls::Receipt;
        using O = NativeAnimationAssociationGroupReplacementCalls::CheckOwner;
        using State = R::State;
        static ClockResult Finish(R& r, ClockResult result, C* out) noexcept
        {
            if (r.data)
            {
                r.data->receipt.result = result;
                r.data->receipt.state = r.GetState();
                if (out)
                    *out = r.data->receipt;
            }
            return result;
        }
        static ClockResult Check(std::uintptr_t image, R& r, std::uint64_t generation, State required, O owner) noexcept
        {
            if (!r.data || !generation || r.data->receipt.generation != generation || r.GetState() != required)
                return ClockResult::OwnerConflict;
            auto result = NativeStreamingCompletionContext::Owner(image, owner);
            if (result != ClockResult::Ok)
                return result;
            return NativeAnimationManagerMemory({image}, owner).RecheckAssociationGroupReplacementLayout(r.data->expected);
        }
        static void Invalidate(D& d) noexcept
        {
            if (d.receipt.childBorrowsInvalidated)
                return;
            d.receipt.childBorrowsInvalidated = true;
            d.invalidator();
        }
        static ClockResult Bookend(std::uintptr_t image, R& r, O owner) noexcept
        {
            NativeAnimationManagerMemory::AssociationGroupReplacementLayout observed;
            auto result = NativeAnimationManagerMemory({image}, owner).ReadAssociationGroupReplacementLayout(r.data->expected.group, observed);
            if (result == ClockResult::Ok)
            {
                r.data->receipt.observed = observed;
                result = NativeAnimationManagerMemory({image}, owner).RecheckAssociationGroupReplacementLayout(r.data->expected);
            }
            if (result != ClockResult::Ok)
                r.state.store(State::FaultDebt);
            return result;
        }
        static ClockResult Block(std::uintptr_t image, D& d, O owner) noexcept
        {
            NativeAnimationBlockValues current;
            auto                       result = NativeAnimationBlockMemory({image}, owner).Read(d.blockIdentity, current);
            if (result != ClockResult::Ok)
                return result;
            return current.index == d.blockValues.index && current.animationCount == d.blockValues.animationCount &&
                           current.idOffset == d.blockValues.idOffset && current.loaded == d.blockValues.loaded
                       ? ClockResult::Ok
                       : ClockResult::BindingChanged;
        }
        static ClockResult Retire(std::uintptr_t image, R& r, std::uint64_t generation, const NativeAnimationAssociationGroupReferenceToken& token, O owner,
                                  NativeAnimationAssociationGroupChildBorrowInvalidator invalidator, C* out,
                                  NativeAnimationAssociationGroupReplacementCalls::InvokeRetire invoke) noexcept
        {
            namespace F = AssociationGroupReplacementDetail;
            if (out)
                *out = {};
            if (!generation || token.ownerGeneration != generation || !token.locator)
                return ClockResult::OwnerConflict;
            if (!F::Enter(r.busy))
                return ClockResult::Busy;
            F::Busy busy{r.busy};
            if (r.data || r.GetState() != State::Empty)
                return ClockResult::OwnerConflict;
            if (!F::Context::TryEnter())
                return ClockResult::Busy;
            F::Context::Scope                                               gate;
            NativeAnimationManagerMemory                                    memory({image}, owner);
            NativeAnimationManagerMemory::AssociationGroupReplacementLayout source;
            auto                                                            result = F::Context::Owner(image, owner);
            if (result != ClockResult::Ok)
                return result;
            result = memory.ReadAssociationGroupReplacementLayout(token, source);
            if (result != ClockResult::Ok)
                return result;
            try
            {
                r.data = std::make_unique<D>();
                r.data->oldRows.resize(static_cast<std::size_t>(source.childCapacity) * 32);
                r.data->oldNodes.reserve(static_cast<std::size_t>(source.childCapacity));
            }
            catch (...)
            {
                r.data.reset();
                return ClockResult::WriteFailure;
            }
            auto& d = *r.data;
            d.expected = source;
            d.receipt.before = source;
            d.receipt.generation = generation;
            d.invalidator = invalidator;
            if (!source.children)
            {
                r.state.store(State::OldChildrenRetired);
                d.receipt.retireVerified = true;
                return Finish(r, ClockResult::Ok, out);
            }
            NativeModelReferenceHeap::Value heap;
            if ((result = F::RetireProfile(image, owner, heap)) != ClockResult::Ok)
                return Finish(r, result, out);
            if (!d.oldRows.empty())
            {
                if (!F::M::Copy(source.children, d.oldRows.data(), d.oldRows.size()))
                    return Finish(r, ClockResult::ReadFailure, out);
                std::uintptr_t table = 0, entry = 0;
                std::memcpy(&table, d.oldRows.data(), 8);
                if (table != image + NativeStaticAssociationArrayProfile::VtableRva || !NativeModelReferenceHeap::ReadOnlyImage(image, table, 8) ||
                    !NativeImageAccess::ReadImage(image, table, &entry, 8) || entry != image + NativeStaticAssociationArrayProfile::DeletingDestructorRva)
                    return Finish(r, ClockResult::BindingChanged, out);
                const auto parentBase = source.groups - 8, parentBytes = 8 + static_cast<std::size_t>(source.groupCapacity) * 32;
                for (std::size_t i = 0; i < source.childCapacity; ++i)
                {
                    std::uintptr_t nodes = 0;
                    std::memcpy(&nodes, d.oldRows.data() + i * 32 + 16, 8);
                    if (nodes && (!F::M::PrivateWritable(nodes, 1) || !F::M::Disjoint(nodes, 1, source.children - 8, 8 + d.oldRows.size()) ||
                                  !F::M::Disjoint(nodes, 1, parentBase, parentBytes)))
                        return Finish(r, ClockResult::BindingChanged, out);
                    if (nodes)
                        d.oldNodes.push_back(nodes);
                }
                std::sort(d.oldNodes.begin(), d.oldNodes.end());
                if (std::adjacent_find(d.oldNodes.begin(), d.oldNodes.end()) != d.oldNodes.end())
                    return Finish(r, ClockResult::BindingChanged, out);
                // Compare all consumed row facts again with bounded stack chunks;
                // no metadata allocation or old-child dereference follows entry.
                std::array<unsigned char, 256> chunk{};
                for (std::size_t offset = 0; offset < d.oldRows.size(); offset += chunk.size())
                {
                    const auto n = (std::min)(chunk.size(), d.oldRows.size() - offset);
                    if (!F::M::Copy(source.children + offset, chunk.data(), n))
                        return Finish(r, ClockResult::ReadFailure, out);
                    if (std::memcmp(chunk.data(), d.oldRows.data() + offset, n))
                        return Finish(r, ClockResult::BindingChanged, out);
                }
            }
            if ((result = memory.RecheckAssociationGroupReplacementLayout(source)) != ClockResult::Ok ||
                (result = NativeModelReferenceHeap::Recheck(image, heap)) != ClockResult::Ok)
                return Finish(r, result, out);
            Invalidate(d);
            if ((result = memory.RecheckAssociationGroupReplacementLayout(source)) != ClockResult::Ok)
                return Finish(r, result, out);
            r.state.store(State::RetireEntered);
            d.receipt.retireEntered = true;
            result = (invoke ? invoke : &F::InvokeOriginal)(reinterpret_cast<NativeAnimationAssociationGroupReplacementCalls::RetireFunction>(image + 0xfeb580),
                                                            token.locator, d.receipt.exceptionCode);
            d.receipt.retireReturned = result == ClockResult::Ok;
            if (result != ClockResult::Ok)
            {
                r.state.store(State::FaultDebt);
                return Finish(r, result, out);
            }
            // The returned whole original consumed its old cookie/rows and
            // nulled the parent. Never inspect or reconstruct the freed child.
            d.receipt.oldChildrenConsumed = true;
            d.expected.children = 0;
            d.expected.childCapacity = 0;
            F::Put(d.expected.parent, 8, std::uintptr_t{0});
            F::Put(d.expected.parent, 16, std::uint32_t{0});
            if ((result = Bookend(image, r, owner)) != ClockResult::Ok)
                return Finish(r, result, out);
            if ((result = NativeModelReferenceHeap::Recheck(image, heap)) != ClockResult::Ok)
            {
                r.state.store(State::FaultDebt);
                return Finish(r, result, out);
            }
            d.receipt.retireVerified = true;
            r.state.store(State::OldChildrenRetired);
            return Finish(r, ClockResult::Ok, out);
        }
        static ClockResult PublishBlock(std::uintptr_t image, R& r, std::uint64_t generation, const NativeAnimationBlockIdentity& block, O owner, C* out,
                                        NativeAnimationAssociationGroupReplacementCalls::InvokeQword invoke) noexcept
        {
            namespace F = AssociationGroupReplacementDetail;
            if (out)
                *out = {};
            if (!F::Enter(r.busy))
                return ClockResult::Busy;
            F::Busy busy{r.busy};
            if (!F::Context::TryEnter())
                return ClockResult::Busy;
            F::Context::Scope gate;
            auto              result = Check(image, r, generation, State::OldChildrenRetired, owner);
            if (result != ClockResult::Ok)
                return Finish(r, result, out);
            if (block.ownerGeneration != generation)
                return Finish(r, ClockResult::OwnerConflict, out);
            auto&                      d = *r.data;
            NativeAnimationBlockValues values;
            if ((result = NativeAnimationBlockMemory({image}, owner).Read(block, values)) != ClockResult::Ok)
                return Finish(r, result, out);
            if (values.animationCount > static_cast<std::uint32_t>((std::numeric_limits<std::int32_t>::max)()))
                return Finish(r, ClockResult::InvalidArgument, out);
            d.blockIdentity = block;
            d.blockValues = values;
            d.receipt.block = image + NativeAnimationBlockProfile::BankRva + static_cast<std::uintptr_t>(block.index) * NativeAnimationBlockProfile::RowBytes;
            d.receipt.fullCount = values.animationCount;
            auto& s = d.receipt.blockStore;
            s.address = d.expected.group.locator;
            s.before = F::Field<std::uint64_t>(d.expected.parent, 0);
            s.target = d.receipt.block;
            Invalidate(d);
            if ((result = NativeAnimationManagerMemory({image}, owner).RecheckAssociationGroupReplacementLayout(d.expected)) != ClockResult::Ok)
                return Finish(r, result, out);
            r.state.store(State::BlockStoreEntered);
            s.entered = true;
            result = (invoke ? invoke : &F::Qword)(s.address, s.before, s.target, s.observed, s.accepted, s.exceptionCode);
            s.returned = result == ClockResult::Ok;
            if (result != ClockResult::Ok || !s.accepted || s.observed != s.before)
            {
                r.state.store(State::FaultDebt);
                return Finish(r, result == ClockResult::Ok ? ClockResult::OwnerConflict : result, out);
            }
            F::Put(d.expected.parent, 0, s.target);
            if ((result = Bookend(image, r, owner)) != ClockResult::Ok || (result = Block(image, d, owner)) != ClockResult::Ok)
            {
                r.state.store(State::FaultDebt);
                return Finish(r, result, out);
            }
            s.verified = true;
            r.state.store(State::BlockPublished);
            return Finish(r, ClockResult::Ok, out);
        }
        static ClockResult PublishChildren(std::uintptr_t image, R& r, std::uint64_t generation, NativeCustomAnimSequenceStorage& storage, O owner, C* out,
                                           NativeAnimationAssociationGroupReplacementCalls::InvokeQword invoke) noexcept
        {
            namespace F = AssociationGroupReplacementDetail;
            if (out)
                *out = {};
            if (!F::Enter(r.busy))
                return ClockResult::Busy;
            F::Busy busy{r.busy};
            if (!F::Context::TryEnter())
                return ClockResult::Busy;
            F::Context::Scope gate;
            auto              result = Check(image, r, generation, State::BlockPublished, owner);
            if (result != ClockResult::Ok)
                return Finish(r, result, out);
            auto& d = *r.data;
            if ((result = Block(image, d, owner)) != ClockResult::Ok)
                return Finish(r, result, out);
            if (storage.OwnerGeneration() != generation || storage.GetKind() != NativeCustomAnimSequenceStorage::Kind::StaticAnimationAssociationArray ||
                storage.Capacity() != d.receipt.fullCount)
                return Finish(r, ClockResult::OwnerConflict, out);
            // ReadFreshRow qualifies the complete issued fresh array twice,
            // including every other row's constructor-defined bytes. One row
            // request preserves that full admission without quadratic rescans.
            if (d.receipt.fullCount)
            {
                NativeStaticAssociationArrayStorageCalls::FreshRow row;
                if ((result = NativeStaticAssociationArrayStorageCalls::ReadFreshRow(image, storage, generation, 0, owner, row)) != ClockResult::Ok)
                    return Finish(r, result, out);
            }
            // A zero-cookie array has no row to read. Pin still requires the
            // genuine issued base/tag/extent and current heap before exposure.
            NativeModelReferenceHeap::Value heap;
            if ((result = NativeStaticAssociationArrayStorageCalls::CaptureRetirementHeap(image, owner, heap)) != ClockResult::Ok)
                return Finish(r, result, out);
            if ((result = NativeAnimationManagerMemory({image}, owner).RecheckAssociationGroupReplacementLayout(d.expected)) != ClockResult::Ok)
                return Finish(r, result, out);
            if (!NativeStaticAssociationArrayPublicationAccess::Pin(storage, generation, d.expected.group.locator, heap))
                return Finish(r, ClockResult::OwnerConflict, out);
            auto& s = d.receipt.childStore;
            s.address = d.expected.group.locator + 8;
            s.before = F::Field<std::uint64_t>(d.expected.parent, 8);
            s.target = storage.IssuedTag();
            r.state.store(State::ChildStoreEntered);
            s.entered = true;
            result = (invoke ? invoke : &F::Qword)(s.address, s.before, s.target, s.observed, s.accepted, s.exceptionCode);
            s.returned = result == ClockResult::Ok;
            if (s.returned && !s.accepted)
                NativeStaticAssociationArrayPublicationAccess::Reject(storage);
            if (s.returned && s.accepted)
            {
                // Keep the actual issued CPU origin through synchronous per-H
                // execution, even though native group ownership is accepted.
                d.publishedStorage = &storage;
                NativeStaticAssociationArrayPublicationAccess::Accept(storage);
            }
            if (result != ClockResult::Ok || !s.accepted || s.observed != s.before)
            {
                r.state.store(State::FaultDebt);
                return Finish(r, result == ClockResult::Ok ? ClockResult::OwnerConflict : result, out);
            }
            d.expected.children = s.target;
            d.expected.childCapacity = storage.Capacity();
            F::Put(d.expected.parent, 8, s.target);
            if ((result = Bookend(image, r, owner)) != ClockResult::Ok || (result = Block(image, d, owner)) != ClockResult::Ok)
            {
                r.state.store(State::FaultDebt);
                return Finish(r, result, out);
            }
            s.verified = true;
            r.state.store(State::ChildrenPublished);
            return Finish(r, ClockResult::Ok, out);
        }
        static ClockResult PublishFullCount(std::uintptr_t image, R& r, std::uint64_t generation, O owner, C* out,
                                            NativeAnimationAssociationGroupReplacementCalls::InvokeDword invoke) noexcept
        {
            namespace F = AssociationGroupReplacementDetail;
            if (out)
                *out = {};
            if (!F::Enter(r.busy))
                return ClockResult::Busy;
            F::Busy busy{r.busy};
            if (!F::Context::TryEnter())
                return ClockResult::Busy;
            F::Context::Scope gate;
            auto              result = Check(image, r, generation, State::ChildrenPublished, owner);
            if (result != ClockResult::Ok)
                return Finish(r, result, out);
            auto& d = *r.data;
            if ((result = Block(image, d, owner)) != ClockResult::Ok)
                return Finish(r, result, out);
            auto& s = d.receipt.countStore;
            s.address = d.expected.group.locator + 16;
            s.before = F::Field<std::uint32_t>(d.expected.parent, 16);
            s.target = d.receipt.fullCount;
            r.state.store(State::CountStoreEntered);
            s.entered = true;
            std::uint32_t observed = 0;
            result = (invoke ? invoke : &F::Dword)(s.address, static_cast<std::uint32_t>(s.before), d.receipt.fullCount, observed, s.accepted, s.exceptionCode);
            s.observed = observed;
            s.returned = result == ClockResult::Ok;
            if (result != ClockResult::Ok || !s.accepted || s.observed != s.before)
            {
                r.state.store(State::FaultDebt);
                return Finish(r, result == ClockResult::Ok ? ClockResult::OwnerConflict : result, out);
            }
            F::Put(d.expected.parent, 16, d.receipt.fullCount);
            if ((result = Bookend(image, r, owner)) != ClockResult::Ok || (result = Block(image, d, owner)) != ClockResult::Ok)
            {
                r.state.store(State::FaultDebt);
                return Finish(r, result, out);
            }
            s.verified = true;
            r.state.store(State::Published);
            return Finish(r, ClockResult::Ok, out);
        }
        static ClockResult FinishPublished(std::uintptr_t image, R& r, std::uint64_t generation, O owner, C* out) noexcept
        {
            namespace F = AssociationGroupReplacementDetail;
            if (out)
                *out = {};
            if (!F::Enter(r.busy))
                return ClockResult::Busy;
            F::Busy busy{r.busy};
            if (!F::Context::TryEnter())
                return ClockResult::Busy;
            F::Context::Scope gate;
            auto              result = Check(image, r, generation, State::Published, owner);
            if (result != ClockResult::Ok)
                return Finish(r, result, out);
            auto& d = *r.data;
            if (!d.publishedStorage || d.publishedStorage->OwnerGeneration() != generation ||
                d.publishedStorage->GetState() != NativeCustomAnimSequenceStorage::State::EnginePublished ||
                d.publishedStorage->IssuedTag() != d.expected.children || d.publishedStorage->Capacity() != d.receipt.fullCount)
                return Finish(r, ClockResult::OwnerConflict, out);
            if ((result = Block(image, d, owner)) != ClockResult::Ok)
                return Finish(r, result, out);
            r.state.store(State::Finished);
            return Finish(r, ClockResult::Ok, out);
        }
    };
    ClockResult NativeAnimationAssociationGroupReplacement::ResolvePublishedRowForInitialization(
        std::uintptr_t image, std::uint64_t generation, const NativeCustomAnimSequenceStorage& storage, std::uint32_t index,
        NativeAnimationManagerMemory::CheckOwner owner, NativeAnimationStaticAssociationReferenceToken& output) const noexcept
    {
        if (!data || GetState() != State::Published || !generation || data->receipt.generation != generation || data->publishedStorage != &storage ||
            index >= data->receipt.fullCount || storage.IssuedTag() != data->expected.children || storage.Capacity() != data->receipt.fullCount)
            return ClockResult::OwnerConflict;
        auto result = NativeStaticAssociationArrayPublicationAccess::RecheckPublished(image, storage, generation, data->expected.group.locator, owner);
        if (result != ClockResult::Ok)
            return result;
        if ((result = NativeAnimationManagerMemory({image}, owner).RecheckAssociationGroupReplacementLayout(data->expected)) != ClockResult::Ok)
            return result;
        NativeAnimationStaticAssociationReferenceToken next{generation, data->expected.group.index, index,
                                                            data->expected.children + static_cast<std::uintptr_t>(index) * 32};
        if ((result = NativeStreamingCompletionContext::Owner(image, owner)) != ClockResult::Ok)
            return result;
        output = next;
        return ClockResult::Ok;
    }
    ClockResult NativeAnimationAssociationGroupReplacement::ResolvePublishedInitializationOrigin(
        std::uintptr_t image, std::uint64_t generation, const NativeCustomAnimSequenceStorage& storage, std::uint32_t index,
        const NativeAnimationHierarchyReferenceToken& hierarchy, NativeAnimationManagerMemory::CheckOwner owner,
        NativeAnimationStaticAssociationReferenceToken& output) const noexcept
    {
        if (hierarchy.ownerGeneration != generation || !hierarchy.locator)
            return ClockResult::OwnerConflict;
        NativeAnimationStaticAssociationReferenceToken row, confirmed;
        auto                                           result = ResolvePublishedRowForInitialization(image, generation, storage, index, owner, row);
        if (result != ClockResult::Ok)
            return result;
        // The source loop selects H from this exact block/index. A valid raw
        // hierarchy elsewhere in the bank is not this row's initialization.
        // The existing helper's loaded-block domain is an explicit refusal
        // here, never a claim that generic Create skips an unloaded block.
        std::uintptr_t                    selected = 0, after = 0;
        NativeAnimationBlockFactoryMemory block({image}, owner);
        if ((result = block.ReadHierarchy(data->blockIdentity, index, selected)) != ClockResult::Ok)
            return result;
        if (!selected || selected != hierarchy.locator)
            return ClockResult::BindingChanged;
        if ((result = ResolvePublishedRowForInitialization(image, generation, storage, index, owner, confirmed)) != ClockResult::Ok ||
            (result = block.ReadHierarchy(data->blockIdentity, index, after)) != ClockResult::Ok)
            return result;
        if (row.ownerGeneration != confirmed.ownerGeneration || row.groupIndex != confirmed.groupIndex || row.index != confirmed.index ||
            row.locator != confirmed.locator || after != selected)
            return ClockResult::BindingChanged;
        output = row;
        return ClockResult::Ok;
    }
    namespace NativeAnimationAssociationGroupReplacementCalls
    {
        ClockResult RetireCurrentChildren(std::uintptr_t image, Record& r, std::uint64_t generation, const NativeAnimationAssociationGroupReferenceToken& group,
                                          CheckOwner owner, NativeAnimationAssociationGroupChildBorrowInvalidator invalidator, Receipt* out,
                                          InvokeRetire invoke) noexcept
        {
            return NativeAnimationAssociationGroupReplacementAccess::Retire(image, r, generation, group, owner, invalidator, out, invoke);
        }
        ClockResult PublishBlock(std::uintptr_t image, Record& r, std::uint64_t generation, const NativeAnimationBlockIdentity& block, CheckOwner owner,
                                 Receipt* out, InvokeQword invoke) noexcept
        {
            return NativeAnimationAssociationGroupReplacementAccess::PublishBlock(image, r, generation, block, owner, out, invoke);
        }
        ClockResult PublishChildren(std::uintptr_t image, Record& r, std::uint64_t generation, NativeCustomAnimSequenceStorage& storage, CheckOwner owner,
                                    Receipt* out, InvokeQword invoke) noexcept
        {
            return NativeAnimationAssociationGroupReplacementAccess::PublishChildren(image, r, generation, storage, owner, out, invoke);
        }
        ClockResult PublishFullCount(std::uintptr_t image, Record& r, std::uint64_t generation, CheckOwner owner, Receipt* out, InvokeDword invoke) noexcept
        {
            return NativeAnimationAssociationGroupReplacementAccess::PublishFullCount(image, r, generation, owner, out, invoke);
        }
        ClockResult FinishPublished(std::uintptr_t image, Record& r, std::uint64_t generation, CheckOwner owner, Receipt* out) noexcept
        {
            return NativeAnimationAssociationGroupReplacementAccess::FinishPublished(image, r, generation, owner, out);
        }
    }
}
