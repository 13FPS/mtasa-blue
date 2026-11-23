/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Link the SString methods used by CPU texture matching and statistics
 ****************************************************************************/
#include <game/CShaderTextureWatchTypes.h>

// These fixtures link only the string operations reached by the matching
// registry. The test runner checks their definitions against SString.hpp.
SString SString::ToLower() const
{
    SString strResult = *this;
    std::transform(strResult.begin(), strResult.end(), strResult.begin(), SharedUtil::tolower<uchar>);
    return strResult;
}

// The production substring method narrows the string length to its SDK int.
#pragma warning(push)
#pragma warning(disable : 4267)
SString SString::SubStr(int iPos, int iCount) const
{
    if (iPos < 0)
    {
        iCount += iPos;
        iPos = 0;
    }
    iCount = std::max(0, iCount);
    if (iPos + iCount > (int)length())
    {
        iCount = length() - iPos;
    }
    if (iCount < 1)
        return "";
    return substr(iPos, iCount);
}
#pragma warning(pop)

SString SString::Left(int iCount) const
{
    return SubStr(0, iCount);
}
