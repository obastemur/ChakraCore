//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class FileLoadHelpers
{
public :
    static HRESULT LoadScriptFromFile(LPCSTR filename, LPCCHAR_T& contents, bool* isUtf8Out = nullptr, LPCCHAR_T* contentsRawOut = nullptr, UINT* lengthBytesOut = nullptr, bool printFileOpenError = true);
};
