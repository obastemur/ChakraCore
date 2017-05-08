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
    {TRACE_IT(55249);

    }

    void ExternalLibraryBase::Initialize(JavascriptLibrary* library)
    {TRACE_IT(55250);
        Assert(this->javascriptLibrary == nullptr);
        this->javascriptLibrary = library;
        this->scriptContext = library->GetScriptContext();
#if DBG
        ExternalLibraryBase* current = library->externalLibraryList;
        while (current != nullptr)
        {TRACE_IT(55251);
            Assert(current != this);
            current = current->next;
        }
#endif
        this->next = library->externalLibraryList;
        library->externalLibraryList = this;
    }

    void ExternalLibraryBase::Close()
    {TRACE_IT(55252);
        ExternalLibraryBase* current = javascriptLibrary->externalLibraryList;
#if DBG
        bool found = false;
#endif
        if (current == this)
        {TRACE_IT(55253);
            javascriptLibrary->externalLibraryList = this->next;
#if DBG
            found = true;
#endif
        }
        else
        {TRACE_IT(55254);
            while (current != nullptr)
            {TRACE_IT(55255);
                if (current->next == this)
                {TRACE_IT(55256);
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
