/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Consume the genuine Core producer borrow in a constructed CGame
 *****************************************************************************/
#pragma once
#include <game/CGamePresentationPolicy.h>
#include <stdexcept>
#include <windows.h>

namespace mta::game_de
{
    class NativePresentationPolicyAccess final
    {
    public:
        NativePresentationPolicyAccess(const NativePresentationPolicyBorrow& source, const void* identity) : borrow(source)
        {
            if (!borrow.Complete() || !identity || borrow.producerThread != GetCurrentThreadId() || borrow.lifetime->revoked || borrow.lifetime->consumed ||
                borrow.ownerClient->clientGeneration)
                throw std::runtime_error("CGame presentation constructor requires an unbound Core producer borrow");
            PresentationPolicyStateV1 state;
            if (borrow.functions.read(PresentationPolicyVersionV1, borrow.adapter, borrow.core, &state) != Status::Ok ||
                !InstalledPresentationProvider(state, borrow.providerGeneration, borrow.producerThread) || state.clientActive)
                throw std::runtime_error("CGame presentation constructor lost its installed provider");
            borrow.lifetime->consumed = true;
            PresentationPolicyClientV1 next;
            const auto result = borrow.functions.bind(PresentationPolicyVersionV1, borrow.adapter, borrow.core, borrow.providerGeneration, identity, &next);
            // Preserve a returned genuine stamp even if a broken/refusing
            // provider cannot complete construction; Core still owes retire.
            if (next.providerGeneration == borrow.providerGeneration && next.clientGeneration &&
                next.clientIdentity == reinterpret_cast<std::uintptr_t>(identity) && next.producerThread == borrow.producerThread &&
                next.size == sizeof(next) && next.version == PresentationPolicyVersionV1 && !next.reserved)
            {
                client = next;
                *borrow.ownerClient = next;
            }
            if (result != Status::Ok || !client.clientGeneration)
                throw std::runtime_error("CGame presentation client binding was refused");
        }
        NativePresentationPolicyAccess(const NativePresentationPolicyAccess&) = delete;
        NativePresentationPolicyAccess& operator=(const NativePresentationPolicyAccess&) = delete;
        ~NativePresentationPolicyAccess() { Retire(); }

        Status DisableVSync() noexcept
        {
            // The export checks the current provider/client/producer tuple
            // again. A construction snapshot can never renew a stale client.
            if (borrow.producerThread != GetCurrentThreadId())
                return Status::WrongThread;
            if (borrow.lifetime->revoked || !client.clientGeneration || !SamePresentationClient(client, *borrow.ownerClient))
                return Status::NotInitialized;
            return borrow.functions.disable(PresentationPolicyVersionV1, borrow.adapter, borrow.core, &client);
        }
        Status Retire() noexcept
        {
            if (borrow.producerThread != GetCurrentThreadId())
                return Status::WrongThread;
            if (!client.clientGeneration)
                return Status::Ok;
            if (borrow.lifetime->revoked || !SamePresentationClient(client, *borrow.ownerClient))
            {
                client = {};
                return Status::Ok;  // Core already revoked this exact client.
            }
            borrow.lifetime->revoked = true;
            const auto result = borrow.functions.retire(PresentationPolicyVersionV1, borrow.adapter, borrow.core, &client);
            if (result == Status::Ok)
            {
                *borrow.ownerClient = {};
                client = {};
            }
            // On refusal the Core-owned slot survives this object and Stop
            // retires it before Clock; native callbacks retain no object ptr.
            return result;
        }

    private:
        NativePresentationPolicyBorrow borrow;
        PresentationPolicyClientV1     client;
    };
}
