/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        core/CClientVariables.h
 *  PURPOSE:     Header file for client variable class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

#include <string>
#include <cmath>
#include <cstdlib>
#include "CSingleton.h"
#include <core/CCVarsInterface.h>
#include <xml/CXMLNode.h>
#include <xml/CXMLFile.h>
#include <CVector.h>
#include <CVector2D.h>
#include "CChat.h"

// Macros
#define CVARS_GET       CClientVariables::GetSingleton().Get
#define CVARS_SET       CClientVariables::GetSingleton().Set
#define CVARS_GET_VALUE CClientVariables::GetSingleton().GetValue

class CClientVariables : public CCVarsInterface, public CSingleton<CClientVariables>
{
// Sanity macros   << Who ever did this is idiot
#define SAN \
    if (!m_pStorage) \
    return
#define SANGET \
    if (!Node(strVariable)) \
    return false

public:
    CClientVariables();
    ~CClientVariables();

    // Get queries
    bool Get(const std::string& strVariable, bool& val) override { return ReadScalar(strVariable, val); }
    bool Get(const std::string& strVariable, std::string& val)
    {
        SANGET;
        val = Node(strVariable)->GetTagContent();
        return !val.empty();
    };
    bool Get(const std::string& strVariable, int& val) override { return ReadScalar(strVariable, val); }
    bool Get(const std::string& strVariable, unsigned int& val) override { return ReadScalar(strVariable, val); }
    bool Get(const std::string& strVariable, float& val) override
    {
        CXMLNode* node = m_pStorage ? m_pStorage->FindSubNode(strVariable.c_str()) : nullptr;
        if (!node)
            return false;

        // The legacy XML float accessor accepts numeric prefixes and returns zero
        // for arbitrary text. Leave the caller's default intact for a malformed setting.
        const std::string text = node->GetTagContent();
        char*             end = nullptr;
        const float       value = std::strtof(text.c_str(), &end);
        if (end == text.c_str() || !std::isfinite(value) || text.find_first_not_of(" \t\r\n\f\v", end - text.c_str()) != std::string::npos)
            return false;
        val = value;
        return true;
    }
    bool Get(const std::string& strVariable, CVector& val);
    bool Get(const std::string& strVariable, CVector2D& val);
    bool Get(const std::string& strVariable, CColor& val);

    // Set queries
    void Set(const std::string& strVariable, bool val)
    {
        SAN;
        m_iRevision++;
        Node(strVariable)->SetTagContent(val);
    };
    void Set(const std::string& strVariable, const char* val)
    {
        SAN;
        m_iRevision++;
        Node(strVariable)->SetTagContent(val);
    };
    void Set(const std::string& strVariable, const std::string& val)
    {
        SAN;
        m_iRevision++;
        Node(strVariable)->SetTagContent(val.c_str());
    };
    void Set(const std::string& strVariable, int val)
    {
        SAN;
        m_iRevision++;
        Node(strVariable)->SetTagContent(val);
    };
    void Set(const std::string& strVariable, unsigned int val)
    {
        SAN;
        m_iRevision++;
        Node(strVariable)->SetTagContent(val);
    };
    void Set(const std::string& strVariable, float val)
    {
        SAN;
        m_iRevision++;
        Node(strVariable)->SetTagContent(val);
    };
    void Set(const std::string& strVariable, CVector val);
    void Set(const std::string& strVariable, CVector2D val);
    void Set(const std::string& strVariable, CColor val);

    void ClampValue(const std::string& strVariable, int iMinValue, int iMaxValue);
    void ClampValue(const std::string& strVariable, float fMinValue, float fMaxValue);
    void ClampValue(const std::string& strVariable, CColor minValue, CColor maxValue);
    void ClampValue(const std::string& strVariable, CVector2D minValue, CVector2D maxValue);

    bool Exists(const std::string& strVariable);

    bool Load();
    void Unload();
    bool IsLoaded() { return m_bLoaded; }
    int  GetRevision() { return m_iRevision; }
    void ValidateValues();

private:
    template <class T>
    bool ReadScalar(const std::string& name, T& value) const
    {
        // Reading a missing setting must leave it available for one-time migration from the game.
        CXMLNode* node = m_pStorage ? m_pStorage->FindSubNode(name.c_str()) : nullptr;
        return node && node->GetTagContent(value);
    }
    CXMLNode* Node(const std::string& strVariable);
    void      LoadDefaults();

    bool      m_bLoaded;
    CXMLNode* m_pStorage;
    int       m_iRevision;
};
