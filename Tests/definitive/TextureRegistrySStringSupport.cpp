/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Link the exact existing SString lowercase method for CPU fixtures
 ****************************************************************************/
// Use the actual public legacy component's production warning boundary;
// the exact selected method below remains ordinary /W4 /WX consumer code.
#include <game/CShaderTextureWatchTypes.h>

// Exact selected definition from Shared/sdk/SString.hpp, not a substitute.
SString SString::ToLower() const
{
    SString strResult = *this;
    std::transform(strResult.begin(), strResult.end(), strResult.begin(), SharedUtil::tolower<uchar>);
    return strResult;
}
