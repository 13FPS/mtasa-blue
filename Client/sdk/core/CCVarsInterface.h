/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        sdk/core/CCVarsInterface.h
 *  PURPOSE:     Core client variables interface
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

#include <cstddef>
#include <string>
#include <type_traits>

class CCVarsInterface
{
public:
    // Existence check
    virtual bool Exists(const std::string& strVariable) = 0;

    // Get queries
    virtual bool Get(const std::string& strVariable, bool& val) = 0;
    virtual bool Get(const std::string& strVariable, std::string& val) = 0;
    virtual bool Get(const std::string& strVariable, int& val) = 0;
    virtual bool Get(const std::string& strVariable, unsigned int& val) = 0;
    virtual bool Get(const std::string& strVariable, float& val) = 0;

    // Set queries
    virtual void Set(const std::string& strVariable, bool val) = 0;
    virtual void Set(const std::string& strVariable, const std::string& val) = 0;
    virtual void Set(const std::string& strVariable, int val) = 0;
    virtual void Set(const std::string& strVariable, unsigned int val) = 0;
    virtual void Set(const std::string& strVariable, float val) = 0;

    // Get without needing caller temp variable
    template <class T>
    T GetValue(const std::string& strVariable, const T& defaultValue = {})
    {
        if constexpr (std::is_same_v<T, std::size_t> && !std::is_same_v<T, unsigned int>)
        {
            // CVars retain their unsigned-int storage/ABI. A pointer-sized
            // consumer can widen that value without narrowing its default or
            // adding an architecture-dependent virtual overload.
            unsigned int value = 0;
            return Get(strVariable, value) ? static_cast<T>(value) : defaultValue;
        }
        else
        {
            T value = defaultValue;
            Get(strVariable, value);
            return value;
        }
    }
};
