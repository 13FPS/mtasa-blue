/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve source WORD signedness and immediate opaque pointer reads
 ****************************************************************************/
#include "CAnimBlendStaticAssociationDE.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    namespace
    {
        void Require(ClockResult result, const char* operation)
        {
            if (result != ClockResult::Ok)
                throw std::runtime_error(std::string("DE static-association owner refused ") + operation + ": " +
                                         std::to_string(static_cast<unsigned>(result)));
        }
    }
    NativeAnimationStaticAssociationValues CAnimBlendStaticAssociationDE::Read(const char* operation) const
    {
        const auto                             token = reference;
        NativeAnimationStaticAssociationValues value;
        Require(native.ReadAnimationStaticAssociation(token, value), operation);
        return value;
    }
    unsigned short CAnimBlendStaticAssociationDE::GetNumBlendNodes()
    {
        static_assert(sizeof(unsigned short) == sizeof(std::uint16_t));
        return Read("GetNumBlendNodes").numBlendNodes;
    }
    short CAnimBlendStaticAssociationDE::GetAnimID()
    {
        static_assert(sizeof(short) == sizeof(std::int16_t));
        return Read("GetAnimID").animId;
    }
    short CAnimBlendStaticAssociationDE::GetAnimGroup()
    {
        return Read("GetAnimGroup").animGroup;
    }
    short CAnimBlendStaticAssociationDE::GetFlags()
    {
        // The actual SDK returns signed short flags, retaining every WORD
        // bit rather than filtering a known enum or sanitizing its sign.
        return Read("GetFlags").flags;
    }
    CAnimBlendHierarchySAInterface* CAnimBlendStaticAssociationDE::GetAnimHierarchyInterface()
    {
        // Source reads the raw +18 target; it does not dereference or validate
        // it. A later hierarchy consumer must re-admit its own backend tag.
        return reinterpret_cast<CAnimBlendHierarchySAInterface*>(Read("GetAnimHierarchyInterface").hierarchyAddress);
    }
    CAnimBlendStaticAssociationSAInterface* CAnimBlendStaticAssociationDE::GetInterface()
    {
        // This is only the current backend's opaque physical-row tag. It
        // grants neither a classic structure layout nor a native lifetime.
        return reinterpret_cast<CAnimBlendStaticAssociationSAInterface*>(Read("GetInterface").interfaceAddress);
    }
}
