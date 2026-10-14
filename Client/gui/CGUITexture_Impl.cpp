/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        gui/CGUITexture_Impl.cpp
 *  PURPOSE:     Texture handling class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"

CGUITexture_Impl::CGUITexture_Impl(CGUI_Impl* pGUI)
{
    m_Direct3D9 = pGUI->IsDirect3D9Renderer();
    // Save the renderer
    m_pRenderer = pGUI->GetRenderer();

    // Create the texture
    m_pTexture = m_pRenderer->createTexture();
}

CGUITexture_Impl::~CGUITexture_Impl()
{
    if (m_pTexture)
        m_pRenderer->destroyTexture(m_pTexture);
}

bool CGUITexture_Impl::LoadFromFile(const char* szFilename)
{
    // Try to load it
    try
    {
        m_pTexture->loadFromFile(szFilename, "");
    }
    catch (CEGUI::Exception)
    {
        return false;
    }
    return true;
}

void CGUITexture_Impl::LoadFromMemory(const void* pBuffer, unsigned int uiWidth, unsigned int uiHeight)
{
    m_pTexture->loadFromMemory(pBuffer, uiWidth, uiHeight);
}

void CGUITexture_Impl::Clear()
{
    // Destroy the previous texture and recreate it (empty)
    m_pRenderer->destroyTexture(m_pTexture);
    m_pTexture = m_pRenderer->createTexture();
}

CEGUI::Texture* CGUITexture_Impl::GetTexture()
{
    return m_pTexture;
}

void CGUITexture_Impl::SetTexture(CEGUI::Texture* pTexture)
{
    m_pTexture = pTexture;
}

LPDIRECT3DTEXTURE9 CGUITexture_Impl::GetD3DTexture()
{
    // Plain CEGUI textures work on either backend; a D3D12 texture cannot be
    // borrowed through this legacy D3D9-specific SDK entry point.
    if (!m_Direct3D9)
        return nullptr;
    return reinterpret_cast<CEGUI::DirectX9Texture*>(m_pTexture)->getD3DTexture();
}

void CGUITexture_Impl::CreateTexture(unsigned int width, unsigned int height)
{
    if (!m_Direct3D9)
        throw std::runtime_error("D3D9 GUI render-target textures are unavailable on D3D12");
    return reinterpret_cast<CEGUI::DirectX9Texture*>(m_pTexture)->createRenderTarget(width, height);
}
