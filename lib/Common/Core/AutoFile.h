//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
class AutoFILE : public BasePtr<FILE>
{
public:
    AutoFILE(FILE * file = nullptr) : BasePtr<FILE>(file) {TRACE_IT(19618);};
    ~AutoFILE()
    {TRACE_IT(19619);
        Close();
    }
    AutoFILE& operator=(FILE * file)
    {TRACE_IT(19620);
        Close();
        this->ptr = file;
        return *this;
    }
    void Close()
    {TRACE_IT(19621);
        if (ptr != nullptr)
        {TRACE_IT(19622);
            fclose(ptr);
            ptr = nullptr;
        }
    }
};
