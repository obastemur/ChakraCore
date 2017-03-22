//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    ExternalLibraryBase::ExternalLibraryBase() :
        scriptContext(nullptr),
        javascriptLibrary(nullptr),
        next(nullptr)
    {LOGMEIN("ExternalLibraryBase.cpp] 12\n");

    }

    void ExternalLibraryBase::Initialize(JavascriptLibrary* library)
    {LOGMEIN("ExternalLibraryBase.cpp] 17\n");
        Assert(this->javascriptLibrary == nullptr);
        this->javascriptLibrary = library;
        this->scriptContext = library->GetScriptContext();
#if DBG
        ExternalLibraryBase* current = library->externalLibraryList;
        while (current != nullptr)
        {LOGMEIN("ExternalLibraryBase.cpp] 24\n");
            Assert(current != this);
            current = current->next;
        }
#endif
        this->next = library->externalLibraryList;
        library->externalLibraryList = this;
    }

    void ExternalLibraryBase::Close()
    {LOGMEIN("ExternalLibraryBase.cpp] 34\n");
        ExternalLibraryBase* current = javascriptLibrary->externalLibraryList;
#if DBG
        bool found = false;
#endif
        if (current == this)
        {LOGMEIN("ExternalLibraryBase.cpp] 40\n");
            javascriptLibrary->externalLibraryList = this->next;
#if DBG
            found = true;
#endif
        }
        else
        {
            while (current != nullptr)
            {LOGMEIN("ExternalLibraryBase.cpp] 49\n");
                if (current->next == this)
                {LOGMEIN("ExternalLibraryBase.cpp] 51\n");
                    current->next = this->next;
#if DBG
                    found = true;
#endif
                    break;
                }
            }
        }
        Assert(found);
        this->javascriptLibrary = nullptr;
    }
}
