/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep native and CPU identity across supported entity/client translation
 ****************************************************************************/
#include "CPoolsEntityDE.h"
#include "CPedPooledDE.h"
#include "NativeGameTimingAccess.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    namespace
    {
        struct AddressContext
        {
            NativeGameTimingAccess& owner;
            std::uintptr_t          address;
        };
        ClockResult ReadAddress(const void* context, std::uintptr_t& output) noexcept
        {
            const auto& current = *static_cast<const AddressContext*>(context);
            const auto  result = current.owner.CheckPedPoolAccess();
            if (result == ClockResult::Ok)
                output = current.address;
            return result;
        }
    }
    ClockResult CPoolsEntityDE::ReadEntity(std::uintptr_t address, CEntity*& output, CClientEntity** outputClient) noexcept
    {
        // Classic returns immediately for a caller-supplied null interface.
        // This is CPU argument policy, unlike a camera-observed native zero.
        if (!address)
        {
            output = nullptr;
            if (outputClient)
                *outputClient = nullptr;
            return ClockResult::Ok;
        }
        AddressContext                      context{translationOwner, address};
        CPedPooledDE::RegisteredPedIdentity first;
        auto                                result = CPedPooledDE::ReadRegisteredEntityTarget(*this, translationOwner, &ReadAddress, &context, first);
        if (result != ClockResult::Ok)
            return result;
        if (outputClient)
        {
            CClientEntity*             client = nullptr;
            NativePedPoolReader::Value native;
            NativePedReferenceToken    reference;
            result = ReadPedClientBinding(first.ped, client, native, reference);
            if (result != ClockResult::Ok)
                return result;
            // Identical SDK/native address values can recur after withdrawal.
            // The original full token and native identity must survive the
            // final client read, whose nullable association is independent.
            if (!CPedPooledDE::SameReference(first.reference, reference) || !CPedPooledDE::SameIdentity(first.native, native))
                return ClockResult::BindingChanged;
            *outputClient = client;
        }
        output = first.entity;
        return ClockResult::Ok;
    }
    void CPoolsEntityDE::RequireTranslation(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE pool entity owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result)));
    }
    CEntity* CPoolsEntityDE::GetEntity(DWORD* nativeInterface)
    {
        CEntity* entity = nullptr;
        RequireTranslation(ReadEntity(reinterpret_cast<std::uintptr_t>(nativeInterface), entity, nullptr), "GetEntity");
        return entity;
    }
    CClientEntity* CPoolsEntityDE::GetClientEntity(DWORD* nativeInterface)
    {
        CEntity*       entity = nullptr;
        CClientEntity* client = nullptr;
        RequireTranslation(ReadEntity(reinterpret_cast<std::uintptr_t>(nativeInterface), entity, &client), "GetClientEntity");
        return client;
    }
}
