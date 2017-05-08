//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "JsrtPch.h"
#include "JsrtDebugEventObject.h"
#include "RuntimeDebugPch.h"
#include "screrror.h"   // For CompileScriptException

JsrtDebugEventObject::JsrtDebugEventObject(Js::ScriptContext *scriptContext)
{TRACE_IT(28032);
    Assert(scriptContext != nullptr);
    this->scriptContext = scriptContext;
    this->eventDataObject = scriptContext->GetLibrary()->CreateObject();
    Assert(this->eventDataObject != nullptr);
}

JsrtDebugEventObject::~JsrtDebugEventObject()
{TRACE_IT(28033);
    this->eventDataObject = nullptr;
    this->scriptContext = nullptr;
}

Js::DynamicObject* JsrtDebugEventObject::GetEventDataObject()
{TRACE_IT(28034);
    return this->eventDataObject;
}

JsrtDebugDocumentManager::JsrtDebugDocumentManager(JsrtDebugManager* jsrtDebugManager) :
    breakpointDebugDocumentDictionary(nullptr)
{TRACE_IT(28035);
    Assert(jsrtDebugManager != nullptr);
    this->jsrtDebugManager = jsrtDebugManager;
}

JsrtDebugDocumentManager::~JsrtDebugDocumentManager()
{TRACE_IT(28036);
    if (this->breakpointDebugDocumentDictionary != nullptr)
    {TRACE_IT(28037);
        AssertMsg(this->breakpointDebugDocumentDictionary->Count() == 0, "Should have cleared all entries by now?");

        Adelete(this->jsrtDebugManager->GetDebugObjectArena(), this->breakpointDebugDocumentDictionary);

        this->breakpointDebugDocumentDictionary = nullptr;
    }
    this->jsrtDebugManager = nullptr;
}

void JsrtDebugDocumentManager::AddDocument(UINT bpId, Js::DebugDocument * debugDocument)
{TRACE_IT(28038);
    BreakpointDebugDocumentDictionary* breakpointDebugDocumentDictionary = this->GetBreakpointDictionary();

    Assert(!breakpointDebugDocumentDictionary->ContainsKey(bpId));

    breakpointDebugDocumentDictionary->Add(bpId, debugDocument);
}

void JsrtDebugDocumentManager::ClearDebugDocument(Js::ScriptContext * scriptContext)
{TRACE_IT(28039);
    if (scriptContext != nullptr)
    {TRACE_IT(28040);
        scriptContext->MapScript([&](Js::Utf8SourceInfo* sourceInfo)
        {
            if (sourceInfo->HasDebugDocument())
            {TRACE_IT(28041);
                Js::DebugDocument* debugDocument = sourceInfo->GetDebugDocument();

                // Remove the debugdocument from breakpoint dictionary
                if (this->breakpointDebugDocumentDictionary != nullptr)
                {TRACE_IT(28042);
                    this->breakpointDebugDocumentDictionary->MapAndRemoveIf([&](JsUtil::SimpleDictionaryEntry<UINT, Js::DebugDocument *> keyValue)
                    {
                        if (keyValue.Value() != nullptr && keyValue.Value() == debugDocument)
                        {TRACE_IT(28043);
                            return true;
                        }
                        return false;
                    });
                }

                debugDocument->GetUtf8SourceInfo()->ClearDebugDocument();
                HeapDelete(debugDocument);
                debugDocument = nullptr;
            }
        });
    }
}

void JsrtDebugDocumentManager::ClearBreakpointDebugDocumentDictionary()
{TRACE_IT(28044);
    if (this->breakpointDebugDocumentDictionary != nullptr)
    {TRACE_IT(28045);
        this->breakpointDebugDocumentDictionary->Clear();
    }
}

bool JsrtDebugDocumentManager::RemoveBreakpoint(UINT breakpointId)
{TRACE_IT(28046);
    if (this->breakpointDebugDocumentDictionary != nullptr)
    {TRACE_IT(28047);
        BreakpointDebugDocumentDictionary* breakpointDebugDocumentDictionary = this->GetBreakpointDictionary();
        Js::DebugDocument* debugDocument = nullptr;
        if (breakpointDebugDocumentDictionary->TryGetValue(breakpointId, &debugDocument))
        {TRACE_IT(28048);
            Js::StatementLocation statement;
            if (debugDocument->FindBPStatementLocation(breakpointId, &statement))
            {TRACE_IT(28049);
                debugDocument->SetBreakPoint(statement, BREAKPOINT_DELETED);
                return true;
            }
        }
    }

    return false;
}

JsrtDebugDocumentManager::BreakpointDebugDocumentDictionary * JsrtDebugDocumentManager::GetBreakpointDictionary()
{TRACE_IT(28050);
    if (this->breakpointDebugDocumentDictionary == nullptr)
    {TRACE_IT(28051);
        this->breakpointDebugDocumentDictionary = Anew(this->jsrtDebugManager->GetDebugObjectArena(), BreakpointDebugDocumentDictionary, this->jsrtDebugManager->GetDebugObjectArena(), 10);
    }
    return breakpointDebugDocumentDictionary;
}
