/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Check actual shared shader channel storage across translation units
 ****************************************************************************/
#include <game/CShaderTextureWatch.h>
unsigned NextActualSharedShaderChannelId()
{
    CMatchChannel channel;
    return channel.m_uiId;
}
