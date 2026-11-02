/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep lazy row products owned until explicit typed producer drain
 ****************************************************************************/
#include "CCameraCamsDE.h"
#include "CPoolsDE.h"
#include "NativeGameTimingAccess.h"
#include <exception>
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    void CCameraCamsDE::Require(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE camera owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result)));
    }
    CCameraCamsDE::CCameraCamsDE(CPoolsDE& pools, NativeGameTimingAccess& owner) : CCameraSphereVisibleDE(owner), camPools(pools), camOwner(owner)
    {
        // Construction is CPU-only. No row hold can unwind on a foreign
        // callback if a later facade or another row fails to construct.
        Require(pools.HasNativeOwner(owner) ? ClockResult::Ok : ClockResult::OwnerConflict, "CCameraCamsDE constructor");
    }
    CCameraCamsDE::~CCameraCamsDE() noexcept
    {
        // Implicit unique_ptr cleanup could run on an engine callback even
        // though row release belongs to the producer. Require the explicit
        // owner drain, as the controlled pool requires guard retirement.
        for (const auto& product : products)
            if (product)
                std::terminate();
    }
    CCam* CCameraCamsDE::GetCam(BYTE index)
    {
        // Classic bounds are a CPU policy, including after publication closes.
        if (index >= products.size())
            return nullptr;
        Require(publicationClosed.load() ? ClockResult::Stopped : ClockResult::Ok, "GetCam");
        auto& product = products[index];
        if (!product)
        {
            // Allocation precedes the protected constructor's one acquisition.
            // reset is noexcept and the old slot is empty: no fallible work
            // can discard a successful hold before it reaches this owner.
            product.reset(new CCamTargetSetterDE(index, camPools, camOwner));
        }
        // Cache ownership survives refusal here, so Stop/source drift cannot
        // publish a stale product or trigger typed destruction on this callback.
        Require(product->ValidatePublication(), "GetCam");
        Require(publicationClosed.load() ? ClockResult::Stopped : ClockResult::Ok, "GetCam");
        return product.get();
    }
    ClockResult CCameraCamsDE::DrainCamsOnProducer() noexcept
    {
        if (!publicationClosed.load())
            return ClockResult::Busy;
        // Preflight every owned token before resetting any slot. A later
        // refusal must preserve earlier products and all native row holds.
        for (const auto& product : products)
            if (product)
            {
                const auto result = product->CheckProducerDrain();
                if (result != ClockResult::Ok)
                    return result;
            }
        for (auto& product : products)
            product.reset();
        return ClockResult::Ok;
    }
}
