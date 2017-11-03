//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Common.h"
#include "Core/ConfigParser.h"

bool ConfigParserAPI::FillConsoleTitle(__ecount(cchBufferSize) LPCHAR_T buffer, size_t cchBufferSize, __in LPCHAR_T moduleName)
{
    return false;
}

void ConfigParserAPI::DisplayInitialOutput(__in LPCHAR_T moduleName)
{
}

LPCCHAR_T JsUtil::ExternalApi::GetFeatureKeyName()
{
    return _u("");
}

extern "C"
{
    // For now, ChakraCore runs only platform that has MessageBoxW API
    bool IsMessageBoxWPresent()
    {
        return true;
    }
}
