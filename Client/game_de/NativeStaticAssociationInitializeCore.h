/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Closed stack origin for reuse of the original static initializer core
 ****************************************************************************/
#pragma once
#include "NativeStaticAssociationInitializeCalls.h"
#include "NativeModelBankReader.h"
namespace mta::game_de
{
    struct NativeStaticAssociationInitializeOwnershipAccess;
    struct NativeStaticAssociationInitializeCoreAccess;
    // This is a noncopyable synchronous capability, not a provider or a public
    // raw-pointer/skip-gate API. Only the closed group/temp bridge creates it.
    // Keeping it separate also preserves public-ped link dependencies.
    class NativeStaticAssociationTemporaryOrigin final
    {
    public:
        NativeStaticAssociationTemporaryOrigin(const NativeStaticAssociationTemporaryOrigin&) = delete;
        NativeStaticAssociationTemporaryOrigin& operator=(const NativeStaticAssociationTemporaryOrigin&) = delete;
        ClockResult                             Recheck() const noexcept { return recheck ? recheck(context) : ClockResult::NotInitialized; }
        ClockResult                             Claim() const noexcept { return claim ? claim(context) : ClockResult::NotInitialized; }
        void                                    Entered() const noexcept
        {
            if (entered)
                entered(context);
        }
        const NativeModelBankReader::Value& Model() const noexcept { return model; }

    private:
        friend struct NativeStaticAssociationInitializeOwnershipAccess;
        friend struct NativeStaticAssociationInitializeCoreAccess;
        NativeStaticAssociationTemporaryOrigin() noexcept = default;
        std::uintptr_t                                           image = 0;
        NativeStaticAssociationInitializeCalls::Arguments        input;
        NativeStaticAssociationInitializeCalls::CheckOwner       owner = nullptr;
        NativeStaticAssociationInitializeCalls::RecheckArguments recheckArguments = nullptr;
        const void*                                              context = nullptr;
        ClockResult (*recheck)(const void*) noexcept = nullptr;
        ClockResult (*claim)(const void*) noexcept = nullptr;
        void (*entered)(const void*) noexcept = nullptr;
        NativeModelBankReader::Value model{};
    };
    struct NativeStaticAssociationInitializeCoreAccess
    {
        // The argument cannot be constructed from observations or raw tags.
        // Its only constructor path already owns WithTemporary's native scope.
        static ClockResult Initialize(const NativeStaticAssociationTemporaryOrigin&, NativeStaticAssociationInitializeCalls::Receipt*,
                                      NativeStaticAssociationInitializeCalls::Invoke) noexcept;
    };
}
