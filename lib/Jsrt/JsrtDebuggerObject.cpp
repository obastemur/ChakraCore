//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "JsrtPch.h"
#include "JsrtDebuggerObject.h"
#include "JsrtDebugUtils.h"
#include "JsrtDebugManager.h"

JsrtDebuggerObjectBase::JsrtDebuggerObjectBase(JsrtDebuggerObjectType type, JsrtDebuggerObjectsManager* debuggerObjectsManager) :
    type(type),
    debuggerObjectsManager(debuggerObjectsManager)
{TRACE_IT(28190);
    Assert(debuggerObjectsManager != nullptr);
    this->handle = debuggerObjectsManager->GetNextHandle();
}

JsrtDebuggerObjectBase::~JsrtDebuggerObjectBase()
{TRACE_IT(28191);
    this->debuggerObjectsManager = nullptr;
}

JsrtDebuggerObjectsManager * JsrtDebuggerObjectBase::GetDebuggerObjectsManager()
{TRACE_IT(28192);
    return this->debuggerObjectsManager;
}

Js::DynamicObject * JsrtDebuggerObjectBase::GetChildren(Js::ScriptContext * scriptContext, uint fromCount, uint totalCount)
{
    AssertMsg(false, "Wrong type for GetChildren");
    return nullptr;
}

Js::DynamicObject * JsrtDebuggerObjectBase::GetChildren(WeakArenaReference<Js::IDiagObjectModelWalkerBase>* walkerRef, Js::ScriptContext * scriptContext, uint fromCount, uint totalCount)
{TRACE_IT(28193);
    Js::DynamicObject* childrensObject = scriptContext->GetLibrary()->CreateObject();

    uint propertiesArrayCount = 0;
    Js::JavascriptArray* propertiesArray = scriptContext->GetLibrary()->CreateArray();

    uint debuggerOnlyPropertiesArrayCount = 0;
    Js::JavascriptArray* debuggerOnlyPropertiesArray = scriptContext->GetLibrary()->CreateArray();

    Js::IDiagObjectModelWalkerBase* walker = walkerRef->GetStrongReference();

    uint32 childrensCount = 0;

    if (walker != nullptr)
    {TRACE_IT(28194);
        try
        {TRACE_IT(28195);
            childrensCount = walker->GetChildrenCount();
        }
        catch (const Js::JavascriptException& err)
        {TRACE_IT(28196);
            err.GetAndClear();  // discard exception object
        }

        if (fromCount < childrensCount)
        {TRACE_IT(28197);
            for (uint32 i = fromCount; i < childrensCount && (propertiesArrayCount + debuggerOnlyPropertiesArrayCount) < totalCount; ++i)
            {TRACE_IT(28198);
                Js::ResolvedObject resolvedObject;

                try
                {TRACE_IT(28199);
                    walker->Get(i, &resolvedObject);
                }
                catch (const Js::JavascriptException& err)
                {TRACE_IT(28200);
                    Js::JavascriptExceptionObject* exception = err.GetAndClear();
                    Js::Var error = exception->GetThrownObject(scriptContext);
                    resolvedObject.obj = error;
                    resolvedObject.address = nullptr;
                    resolvedObject.scriptContext = exception->GetScriptContext();
                    resolvedObject.typeId = Js::JavascriptOperators::GetTypeId(error);
                    resolvedObject.name = _u("{error}");
                    resolvedObject.propId = Js::Constants::NoProperty;
                }

                AutoPtr<WeakArenaReference<Js::IDiagObjectModelDisplay>> objectDisplayWeakRef(resolvedObject.GetObjectDisplay());
                Js::IDiagObjectModelDisplay* resolvedObjectDisplay = objectDisplayWeakRef->GetStrongReference();
                if (resolvedObjectDisplay != nullptr)
                {TRACE_IT(28201);
                    JsrtDebuggerObjectBase* debuggerObject = JsrtDebuggerObjectProperty::Make(this->GetDebuggerObjectsManager(), objectDisplayWeakRef);
                    Js::DynamicObject* object = debuggerObject->GetJSONObject(resolvedObject.scriptContext);
                    Js::Var marshaledObj = Js::CrossSite::MarshalVar(scriptContext, object);
                    if (resolvedObjectDisplay->IsFake())
                    {TRACE_IT(28202);
                        Js::JavascriptOperators::OP_SetElementI((Js::Var)debuggerOnlyPropertiesArray, Js::JavascriptNumber::ToVar(debuggerOnlyPropertiesArrayCount++, scriptContext), marshaledObj, scriptContext);
                    }
                    else
                    {TRACE_IT(28203);
                        Js::JavascriptOperators::OP_SetElementI((Js::Var)propertiesArray, Js::JavascriptNumber::ToVar(propertiesArrayCount++, scriptContext), marshaledObj, scriptContext);
                    }
                    objectDisplayWeakRef->ReleaseStrongReference();
                    objectDisplayWeakRef.Detach();
                }
            }
        }

        walkerRef->ReleaseStrongReference();
    }

    JsrtDebugUtils::AddPropertyToObject(childrensObject, JsrtDebugPropertyId::totalPropertiesOfObject, childrensCount, scriptContext);
    JsrtDebugUtils::AddPropertyToObject(childrensObject, JsrtDebugPropertyId::properties, propertiesArray, scriptContext);
    JsrtDebugUtils::AddPropertyToObject(childrensObject, JsrtDebugPropertyId::debuggerOnlyProperties, debuggerOnlyPropertiesArray, scriptContext);

    return childrensObject;
}

JsrtDebuggerObjectsManager::JsrtDebuggerObjectsManager(JsrtDebugManager* jsrtDebugManager) :
    handleId(0),
    jsrtDebugManager(jsrtDebugManager),
    handleToDebuggerObjectsDictionary(nullptr),
    dataToDebuggerObjectsDictionary(nullptr)
{TRACE_IT(28204);
    Assert(jsrtDebugManager != nullptr);
}

JsrtDebuggerObjectsManager::~JsrtDebuggerObjectsManager()
{TRACE_IT(28205);
    if (this->dataToDebuggerObjectsDictionary != nullptr)
    {TRACE_IT(28206);
        AssertMsg(this->dataToDebuggerObjectsDictionary->Count() == 0, "Should have cleared all debugger objects by now?");

        Adelete(this->GetDebugObjectArena(), this->dataToDebuggerObjectsDictionary);
        this->dataToDebuggerObjectsDictionary = nullptr;
    }

    if (this->handleToDebuggerObjectsDictionary != nullptr)
    {TRACE_IT(28207);
        AssertMsg(this->handleToDebuggerObjectsDictionary->Count() == 0, "Should have cleared all handle by now?");

        Adelete(this->GetDebugObjectArena(), this->handleToDebuggerObjectsDictionary);
        this->handleToDebuggerObjectsDictionary = nullptr;
    }
}

void JsrtDebuggerObjectsManager::ClearAll()
{TRACE_IT(28208);
    if (this->dataToDebuggerObjectsDictionary != nullptr)
    {TRACE_IT(28209);
        this->dataToDebuggerObjectsDictionary->Clear();
    }

    if (this->handleToDebuggerObjectsDictionary != nullptr)
    {TRACE_IT(28210);
        this->handleToDebuggerObjectsDictionary->Map([this](uint handle, JsrtDebuggerObjectBase* debuggerObject) {
            Adelete(this->GetDebugObjectArena(), debuggerObject);
        });
        this->handleToDebuggerObjectsDictionary->Clear();
    }

    this->handleId = 0;
}

ArenaAllocator * JsrtDebuggerObjectsManager::GetDebugObjectArena()
{TRACE_IT(28211);
    return this->GetJsrtDebugManager()->GetDebugObjectArena();
}

bool JsrtDebuggerObjectsManager::TryGetDebuggerObjectFromHandle(uint handle, JsrtDebuggerObjectBase ** debuggerObject)
{TRACE_IT(28212);
    if (this->handleToDebuggerObjectsDictionary == nullptr)
    {TRACE_IT(28213);
        return false;
    }

    return this->handleToDebuggerObjectsDictionary->TryGetValue(handle, debuggerObject);
}

void JsrtDebuggerObjectsManager::AddToDebuggerObjectsDictionary(JsrtDebuggerObjectBase * debuggerObject)
{TRACE_IT(28214);
    Assert(debuggerObject != nullptr);

    uint handle = debuggerObject->GetHandle();

    Assert(handle > 0);

    if (this->handleToDebuggerObjectsDictionary == nullptr)
    {TRACE_IT(28215);
        this->handleToDebuggerObjectsDictionary = Anew(this->GetDebugObjectArena(), DebuggerObjectsDictionary, this->GetDebugObjectArena(), 10);
    }

    Assert(!this->handleToDebuggerObjectsDictionary->ContainsKey(handle));

    int index = this->handleToDebuggerObjectsDictionary->Add(handle, debuggerObject);

    Assert(index != -1);
}

void JsrtDebuggerObjectsManager::AddToDataToDebuggerObjectsDictionary(void * data, JsrtDebuggerObjectBase * debuggerObject)
{TRACE_IT(28216);
    Assert(data != nullptr);
    Assert(debuggerObject != nullptr);

    if (this->dataToDebuggerObjectsDictionary == nullptr)
    {TRACE_IT(28217);
        this->dataToDebuggerObjectsDictionary = Anew(this->GetDebugObjectArena(), DataToDebuggerObjectsDictionary, this->GetDebugObjectArena(), 10);
    }

    Assert(!this->dataToDebuggerObjectsDictionary->ContainsKey(data));

    int index = this->dataToDebuggerObjectsDictionary->Add(data, debuggerObject);

    Assert(index != -1);

    this->AddToDebuggerObjectsDictionary(debuggerObject);
}

bool JsrtDebuggerObjectsManager::TryGetDataFromDataToDebuggerObjectsDictionary(void * data, JsrtDebuggerObjectBase ** debuggerObject)
{TRACE_IT(28218);
    if (this->dataToDebuggerObjectsDictionary == nullptr)
    {TRACE_IT(28219);
        return false;
    }

    return this->dataToDebuggerObjectsDictionary->TryGetValue(data, debuggerObject);
}

JsrtDebuggerStackFrame::JsrtDebuggerStackFrame(JsrtDebuggerObjectsManager * debuggerObjectsManager, Js::DiagStackFrame * stackFrame, uint frameIndex) :
    debuggerObjectsManager(debuggerObjectsManager),
    frameIndex(frameIndex),
    stackFrame(stackFrame)
{TRACE_IT(28220);
    Assert(this->stackFrame != nullptr);
}

JsrtDebuggerStackFrame::~JsrtDebuggerStackFrame()
{TRACE_IT(28221);
    this->debuggerObjectsManager = nullptr;
    this->stackFrame = nullptr;
}

Js::DynamicObject * JsrtDebuggerStackFrame::GetJSONObject(Js::ScriptContext* scriptContext)
{TRACE_IT(28222);
    Js::ScriptContext *frameScriptContext = stackFrame->GetScriptContext();
    Js::DynamicObject* stackTraceObject = frameScriptContext->GetLibrary()->CreateObject();

    Js::FunctionBody* functionBody = stackFrame->GetFunction();
    Js::Utf8SourceInfo* utf8SourceInfo = functionBody->GetUtf8SourceInfo();

    JsrtDebugUtils::AddPropertyToObject(stackTraceObject, JsrtDebugPropertyId::index, frameIndex, scriptContext);
    JsrtDebugUtils::AddScriptIdToObject(stackTraceObject, utf8SourceInfo);

    int currentByteCodeOffset = stackFrame->GetByteCodeOffset();

    if (stackFrame->IsInterpreterFrame() && frameIndex != 0)
    {TRACE_IT(28223);
        // For non-leaf interpreter frames back up 1 instruction so we see the caller
        // rather than the statement after the caller
        currentByteCodeOffset--;
    }

    JsrtDebugUtils::AddLineColumnToObject(stackTraceObject, functionBody, currentByteCodeOffset);
    JsrtDebugUtils::AddSourceLengthAndTextToObject(stackTraceObject, functionBody, currentByteCodeOffset);

    JsrtDebuggerObjectBase* functionObject = JsrtDebuggerObjectFunction::Make(this->debuggerObjectsManager, functionBody);
    JsrtDebugUtils::AddPropertyToObject(stackTraceObject, JsrtDebugPropertyId::functionHandle, functionObject->GetHandle(), frameScriptContext);

    return stackTraceObject;
}

Js::DynamicObject * JsrtDebuggerStackFrame::GetLocalsObject(Js::ScriptContext* scriptContext)
{TRACE_IT(28224);
    /*
        {
            "thisObject" : {},
            "exception" : {},
            "arguments" : {},
            "returnValue" : {},
            "functionCallsReturn" : [{}, {}],
            "locals" : [],
            "scopes" : [{}, {}],
            "globals" : {}
        }
     */

    Js::DynamicObject* propertiesObject = scriptContext->GetLibrary()->CreateObject();

    Js::Var returnValueObject = nullptr;

    uint functionCallsReturnCount = 0;
    Js::JavascriptArray* functionCallsReturn = scriptContext->GetLibrary()->CreateArray();

    uint totalLocalsCount = 0;
    Js::JavascriptArray* localsArray = scriptContext->GetLibrary()->CreateArray();

    uint scopesCount = 0;
    Js::JavascriptArray* scopesArray = scriptContext->GetLibrary()->CreateArray();

    Js::DynamicObject* globalsObject = nullptr;

    ReferencedArenaAdapter* pRefArena = scriptContext->GetThreadContext()->GetDebugManager()->GetDiagnosticArena();
    Js::IDiagObjectModelDisplay* pLocalsDisplay = Anew(pRefArena->Arena(), Js::LocalsDisplay, this->stackFrame);
    WeakArenaReference<Js::IDiagObjectModelWalkerBase>* objectModelWalker = pLocalsDisplay->CreateWalker();

    if (objectModelWalker != nullptr)
    {TRACE_IT(28225);
        Js::LocalsWalker* localsWalker = (Js::LocalsWalker*)objectModelWalker->GetStrongReference();

        if (localsWalker != nullptr)
        {TRACE_IT(28226);
            // If 'this' is available add 'thisObject'
            Js::ResolvedObject thisResolvedObject;
            {TRACE_IT(28227);
                ENFORCE_ENTRYEXITRECORD_HASCALLER(scriptContext);
                thisResolvedObject.obj = this->stackFrame->GetThisFromFrame(&thisResolvedObject.address, localsWalker);
            }
            if (thisResolvedObject.obj != nullptr)
            {TRACE_IT(28228);
                thisResolvedObject.scriptContext = scriptContext;
                thisResolvedObject.name = _u("this");
                thisResolvedObject.typeId = Js::JavascriptOperators::GetTypeId(thisResolvedObject.obj);
                JsrtDebuggerObjectBase::CreateDebuggerObject<JsrtDebuggerObjectProperty>(this->debuggerObjectsManager, thisResolvedObject, this->stackFrame->GetScriptContext(), [&](Js::Var marshaledObj)
                {
                    JsrtDebugUtils::AddPropertyToObject(propertiesObject, JsrtDebugPropertyId::thisObject, marshaledObj, scriptContext);
                });
            }

            uint32 totalProperties = localsWalker->GetChildrenCount();
            if (totalProperties > 0)
            {TRACE_IT(28229);
                int index = 0;
                Js::ResolvedObject resolvedObject;
                resolvedObject.scriptContext = this->stackFrame->GetScriptContext();

                // If we have a exception add 'exception'
                if (Js::VariableWalkerBase::GetExceptionObject(index, this->stackFrame, &resolvedObject))
                {TRACE_IT(28230);
                    JsrtDebuggerObjectBase::CreateDebuggerObject<JsrtDebuggerObjectProperty>(this->debuggerObjectsManager, resolvedObject, scriptContext, [&](Js::Var marshaledObj)
                    {
                        JsrtDebugUtils::AddPropertyToObject(propertiesObject, JsrtDebugPropertyId::exception, marshaledObj, scriptContext);
                    });
                }

                // If user have not explicitly defined 'arguments' add 'arguments'
                if (localsWalker->HasUserNotDefinedArguments() && localsWalker->CreateArgumentsObject(&resolvedObject))
                {TRACE_IT(28231);
                    JsrtDebuggerObjectBase::CreateDebuggerObject<JsrtDebuggerObjectProperty>(this->debuggerObjectsManager, resolvedObject, scriptContext, [&](Js::Var marshaledObj)
                    {
                        JsrtDebugUtils::AddPropertyToObject(propertiesObject, JsrtDebugPropertyId::arguments, marshaledObj, scriptContext);
                    });
                }

                Js::ReturnedValueList *returnedValueList = this->stackFrame->GetScriptContext()->GetDebugContext()->GetProbeContainer()->GetReturnedValueList();

                // If we have return value(s) add them to 'returnValue' or 'functionCallsReturn'
                if (returnedValueList != nullptr && returnedValueList->Count() > 0 && this->stackFrame->IsTopFrame())
                {TRACE_IT(28232);
                    for (int i = 0; i < returnedValueList->Count(); ++i)
                    {TRACE_IT(28233);
                        Js::ReturnedValue * returnValue = returnedValueList->Item(i);
                        Js::VariableWalkerBase::GetReturnedValueResolvedObject(returnValue, this->stackFrame, &resolvedObject);

                        JsrtDebuggerObjectBase::CreateDebuggerObject<JsrtDebuggerObjectProperty>(debuggerObjectsManager, resolvedObject, scriptContext, [&](Js::Var marshaledObj)
                        {

                            if (returnValue->isValueOfReturnStatement)
                            {TRACE_IT(28234);
                                returnValueObject = marshaledObj;
                            }
                            else
                            {TRACE_IT(28235);
                                Js::JavascriptOperators::OP_SetElementI((Js::Var)functionCallsReturn, Js::JavascriptNumber::ToVar(functionCallsReturnCount, scriptContext), marshaledObj, scriptContext);
                                functionCallsReturnCount++;
                            }
                        });
                    }

                    if (returnValueObject != nullptr)
                    {TRACE_IT(28236);
                        JsrtDebugUtils::AddPropertyToObject(propertiesObject, JsrtDebugPropertyId::returnValue, returnValueObject, scriptContext);
                    }

                    if (functionCallsReturnCount > 0)
                    {TRACE_IT(28237);
                        JsrtDebugUtils::AddPropertyToObject(propertiesObject, JsrtDebugPropertyId::functionCallsReturn, functionCallsReturn, scriptContext);
                    }
                }

                // Add all locals variable(s) available under 'locals'
                uint32 localsCount = localsWalker->GetLocalVariablesCount();
                for (uint32 i = 0; i < localsCount; ++i)
                {TRACE_IT(28238);
                    if (!localsWalker->GetLocal(i, &resolvedObject))
                    {TRACE_IT(28239);
                        break;
                    }

                    JsrtDebuggerObjectBase::CreateDebuggerObject<JsrtDebuggerObjectProperty>(debuggerObjectsManager, resolvedObject, scriptContext, [&](Js::Var marshaledObj)
                    {
                        Js::JavascriptOperators::OP_SetElementI((Js::Var)localsArray, Js::JavascriptNumber::ToVar(totalLocalsCount, scriptContext), marshaledObj, scriptContext);
                        totalLocalsCount++;
                    });
                }

                // Add all variable(s) captured under 'scopes'
                index = 0;
                BOOL foundGroup = TRUE;
                while (foundGroup)
                {TRACE_IT(28240);
                    foundGroup = localsWalker->GetScopeObject(index++, &resolvedObject);
                    if (foundGroup == TRUE)
                    {TRACE_IT(28241);
                        AutoPtr<WeakArenaReference<Js::IDiagObjectModelDisplay>> objectDisplayWeakRef(resolvedObject.GetObjectDisplay());
                        JsrtDebuggerObjectBase* debuggerObject = JsrtDebuggerObjectScope::Make(debuggerObjectsManager, objectDisplayWeakRef, scopesCount);
                        Js::DynamicObject* object = debuggerObject->GetJSONObject(resolvedObject.scriptContext);
                        Assert(object != nullptr);
                        Js::Var marshaledObj = Js::CrossSite::MarshalVar(scriptContext, object);
                        Js::JavascriptOperators::OP_SetElementI((Js::Var)scopesArray, Js::JavascriptNumber::ToVar(scopesCount, scriptContext), marshaledObj, scriptContext);
                        scopesCount++;
                        objectDisplayWeakRef.Detach();
                    }
                }

                // Add globals handle
                if (localsWalker->GetGlobalsObject(&resolvedObject))
                {TRACE_IT(28242);
                    JsrtDebuggerObjectBase::CreateDebuggerObject<JsrtDebuggerObjectGlobalsNode>(this->debuggerObjectsManager, resolvedObject, scriptContext, [&](Js::Var marshaledObj)
                    {
                        globalsObject = (Js::DynamicObject*)marshaledObj;
                    });
                }
            }

            objectModelWalker->ReleaseStrongReference();
            HeapDelete(objectModelWalker);
        }

        Adelete(pRefArena->Arena(), pLocalsDisplay);
    }

    JsrtDebugUtils::AddPropertyToObject(propertiesObject, JsrtDebugPropertyId::locals, localsArray, scriptContext);
    JsrtDebugUtils::AddPropertyToObject(propertiesObject, JsrtDebugPropertyId::scopes, scopesArray, scriptContext);

    if (globalsObject == nullptr)
    {TRACE_IT(28243);
        globalsObject = scriptContext->GetLibrary()->CreateObject();
    }

    JsrtDebugUtils::AddPropertyToObject(propertiesObject, JsrtDebugPropertyId::globals, globalsObject, scriptContext);

    return propertiesObject;
}

bool JsrtDebuggerStackFrame::Evaluate(Js::ScriptContext* scriptContext, const char16 *source, int sourceLength, bool isLibraryCode, Js::DynamicObject** evalResult)
{TRACE_IT(28244);
    *evalResult = nullptr;
    bool success = false;
    if (this->stackFrame != nullptr)
    {TRACE_IT(28245);
        Js::ResolvedObject resolvedObject;
        HRESULT hr = S_OK;
        Js::ScriptContext* frameScriptContext = this->stackFrame->GetScriptContext();

        Js::JavascriptExceptionObject *exceptionObject = nullptr;
        {
            BEGIN_JS_RUNTIME_CALL_EX_AND_TRANSLATE_EXCEPTION_AND_ERROROBJECT_TO_HRESULT_NESTED(frameScriptContext, false)
            {TRACE_IT(28246);
                ENFORCE_ENTRYEXITRECORD_HASCALLER(frameScriptContext);
                this->stackFrame->EvaluateImmediate(source, sourceLength, isLibraryCode, &resolvedObject);
            }
            END_JS_RUNTIME_CALL_AND_TRANSLATE_AND_GET_EXCEPTION_AND_ERROROBJECT_TO_HRESULT(hr, frameScriptContext, exceptionObject);
        }

        if (resolvedObject.obj == nullptr)
        {TRACE_IT(28247);
            resolvedObject.name = _u("{exception}");
            resolvedObject.typeId = Js::TypeIds_Error;
            resolvedObject.address = nullptr;

            if (exceptionObject != nullptr)
            {TRACE_IT(28248);
                resolvedObject.obj = exceptionObject->GetThrownObject(scriptContext);
            }
            else
            {TRACE_IT(28249);
                resolvedObject.obj = scriptContext->GetLibrary()->GetUndefined();
            }
        }
        else
        {TRACE_IT(28250);
          success = true;
        }

        if (resolvedObject.obj != nullptr)
        {TRACE_IT(28251);
            resolvedObject.scriptContext = frameScriptContext;

            charcount_t len = Js::JavascriptString::GetBufferLength(source);
            resolvedObject.name = AnewNoThrowArray(this->debuggerObjectsManager->GetDebugObjectArena(), WCHAR, len + 1);

            if (resolvedObject.name != nullptr)
            {TRACE_IT(28252);
                wcscpy_s((WCHAR*)resolvedObject.name, len + 1, source);
            }
            else
            {TRACE_IT(28253);
                // len can be big, if we failed just have empty string
                resolvedObject.name = _u("");
            }

            resolvedObject.typeId = Js::JavascriptOperators::GetTypeId(resolvedObject.obj);
            JsrtDebuggerObjectBase::CreateDebuggerObject<JsrtDebuggerObjectProperty>(this->debuggerObjectsManager, resolvedObject, scriptContext, [&](Js::Var marshaledObj)
            {
                *evalResult = (Js::DynamicObject*)marshaledObj;
            });
        }
    }
    return success;
}

JsrtDebuggerObjectProperty::JsrtDebuggerObjectProperty(JsrtDebuggerObjectsManager* debuggerObjectsManager, WeakArenaReference<Js::IDiagObjectModelDisplay>* objectDisplay) :
    JsrtDebuggerObjectBase(JsrtDebuggerObjectType::Property, debuggerObjectsManager),
    objectDisplay(objectDisplay),
    walkerRef(nullptr)
{TRACE_IT(28254);
    Assert(objectDisplay != nullptr);
}

JsrtDebuggerObjectProperty::~JsrtDebuggerObjectProperty()
{TRACE_IT(28255);
    if (this->objectDisplay != nullptr)
    {TRACE_IT(28256);
        HeapDelete(this->objectDisplay);
        this->objectDisplay = nullptr;
    }

    if (this->walkerRef != nullptr)
    {TRACE_IT(28257);
        HeapDelete(this->walkerRef);
        this->walkerRef = nullptr;
    }
}

JsrtDebuggerObjectBase * JsrtDebuggerObjectProperty::Make(JsrtDebuggerObjectsManager* debuggerObjectsManager, WeakArenaReference<Js::IDiagObjectModelDisplay>* objectDisplay)
{TRACE_IT(28258);
    JsrtDebuggerObjectBase* debuggerObject = Anew(debuggerObjectsManager->GetDebugObjectArena(), JsrtDebuggerObjectProperty, debuggerObjectsManager, objectDisplay);

    debuggerObjectsManager->AddToDebuggerObjectsDictionary(debuggerObject);

    return debuggerObject;
}

Js::DynamicObject * JsrtDebuggerObjectProperty::GetJSONObject(Js::ScriptContext* scriptContext)
{TRACE_IT(28259);
    Js::IDiagObjectModelDisplay* objectDisplayRef = this->objectDisplay->GetStrongReference();

    Js::DynamicObject* propertyObject = nullptr;

    if (objectDisplayRef != nullptr)
    {TRACE_IT(28260);
        propertyObject = scriptContext->GetLibrary()->CreateObject();

        LPCWSTR name = objectDisplayRef->Name();

        JsrtDebugUtils::AddPropertyToObject(propertyObject, JsrtDebugPropertyId::name, name, wcslen(name), scriptContext);

        JsrtDebugUtils::AddPropertyType(propertyObject, objectDisplayRef, scriptContext); // Will add type, value, display, className, propertyAttributes

        JsrtDebugUtils::AddPropertyToObject(propertyObject, JsrtDebugPropertyId::handle, this->GetHandle(), scriptContext);

        this->objectDisplay->ReleaseStrongReference();
    }

    return propertyObject;
}

Js::DynamicObject* JsrtDebuggerObjectProperty::GetChildren(Js::ScriptContext* scriptContext, uint fromCount, uint totalCount)
{TRACE_IT(28261);
    Js::IDiagObjectModelDisplay* objectDisplayRef = objectDisplay->GetStrongReference();
    if (objectDisplayRef == nullptr)
    {TRACE_IT(28262);
        return nullptr;
    }

    if (this->walkerRef == nullptr)
    {TRACE_IT(28263);
        this->walkerRef = objectDisplayRef->CreateWalker();
    }

    Js::DynamicObject* childrens = __super::GetChildren(this->walkerRef, scriptContext, fromCount, totalCount);

    objectDisplay->ReleaseStrongReference();

    return childrens;
}

JsrtDebuggerObjectScope::JsrtDebuggerObjectScope(JsrtDebuggerObjectsManager * debuggerObjectsManager, WeakArenaReference<Js::IDiagObjectModelDisplay>* objectDisplay, uint index) :
    JsrtDebuggerObjectBase(JsrtDebuggerObjectType::Scope, debuggerObjectsManager),
    objectDisplay(objectDisplay),
    index(index),
    walkerRef(nullptr)
{TRACE_IT(28264);
    Assert(this->objectDisplay != nullptr);
}

JsrtDebuggerObjectScope::~JsrtDebuggerObjectScope()
{TRACE_IT(28265);
    if (this->objectDisplay != nullptr)
    {TRACE_IT(28266);
        HeapDelete(this->objectDisplay);
        this->objectDisplay = nullptr;
    }

    if (this->walkerRef != nullptr)
    {TRACE_IT(28267);
        HeapDelete(this->walkerRef);
        this->walkerRef = nullptr;
    }
}

JsrtDebuggerObjectBase * JsrtDebuggerObjectScope::Make(JsrtDebuggerObjectsManager * debuggerObjectsManager, WeakArenaReference<Js::IDiagObjectModelDisplay>* objectDisplay, uint index)
{TRACE_IT(28268);
    JsrtDebuggerObjectBase* debuggerObject = Anew(debuggerObjectsManager->GetDebugObjectArena(), JsrtDebuggerObjectScope, debuggerObjectsManager, objectDisplay, index);

    debuggerObjectsManager->AddToDebuggerObjectsDictionary(debuggerObject);

    return debuggerObject;
}

Js::DynamicObject * JsrtDebuggerObjectScope::GetJSONObject(Js::ScriptContext* scriptContext)
{TRACE_IT(28269);
    Js::IDiagObjectModelDisplay* modelDisplay = this->objectDisplay->GetStrongReference();

    Js::DynamicObject* scopeObject = nullptr;

    if (modelDisplay != nullptr)
    {TRACE_IT(28270);
        scopeObject = scriptContext->GetLibrary()->CreateObject();
        JsrtDebugUtils::AddPropertyToObject(scopeObject, JsrtDebugPropertyId::index, this->index, scriptContext);
        JsrtDebugUtils::AddPropertyToObject(scopeObject, JsrtDebugPropertyId::handle, this->GetHandle(), scriptContext);

        this->objectDisplay->ReleaseStrongReference();
    }

    return scopeObject;
}

Js::DynamicObject * JsrtDebuggerObjectScope::GetChildren(Js::ScriptContext * scriptContext, uint fromCount, uint totalCount)
{TRACE_IT(28271);
    Js::IDiagObjectModelDisplay* objectDisplayRef = objectDisplay->GetStrongReference();
    if (objectDisplayRef == nullptr)
    {TRACE_IT(28272);
        return nullptr;
    }

    if (this->walkerRef == nullptr)
    {TRACE_IT(28273);
        this->walkerRef = objectDisplayRef->CreateWalker();
    }

    Js::DynamicObject* childrens = __super::GetChildren(this->walkerRef, scriptContext, fromCount, totalCount);

    objectDisplay->ReleaseStrongReference();

    return childrens;
}

JsrtDebuggerObjectFunction::JsrtDebuggerObjectFunction(JsrtDebuggerObjectsManager* debuggerObjectsManager, Js::FunctionBody* functionBody) :
    JsrtDebuggerObjectBase(JsrtDebuggerObjectType::Function, debuggerObjectsManager),
    functionBody(functionBody)
{TRACE_IT(28274);
}

JsrtDebuggerObjectFunction::~JsrtDebuggerObjectFunction()
{TRACE_IT(28275);
    this->functionBody = nullptr;
}

JsrtDebuggerObjectBase * JsrtDebuggerObjectFunction::Make(JsrtDebuggerObjectsManager * debuggerObjectsManager, Js::FunctionBody * functionBody)
{TRACE_IT(28276);
    JsrtDebuggerObjectBase* debuggerObject = nullptr;

    if (debuggerObjectsManager->TryGetDataFromDataToDebuggerObjectsDictionary(functionBody, &debuggerObject))
    {TRACE_IT(28277);
        Assert(debuggerObject != nullptr);
        return debuggerObject;
    }

    debuggerObject = Anew(debuggerObjectsManager->GetDebugObjectArena(), JsrtDebuggerObjectFunction, debuggerObjectsManager, functionBody);

    debuggerObjectsManager->AddToDataToDebuggerObjectsDictionary(functionBody, debuggerObject);

    return debuggerObject;
}

Js::DynamicObject * JsrtDebuggerObjectFunction::GetJSONObject(Js::ScriptContext * scriptContext)
{TRACE_IT(28278);
    Js::DynamicObject* functionObject = scriptContext->GetLibrary()->CreateObject();

    JsrtDebugUtils::AddScriptIdToObject(functionObject, this->functionBody->GetUtf8SourceInfo());
    JsrtDebugUtils::AddPropertyToObject(functionObject, JsrtDebugPropertyId::line, (uint32) this->functionBody->GetLineNumber(), scriptContext);
    JsrtDebugUtils::AddPropertyToObject(functionObject, JsrtDebugPropertyId::column, (uint32) this->functionBody->GetColumnNumber(), scriptContext);
    JsrtDebugUtils::AddPropertyToObject(functionObject, JsrtDebugPropertyId::name, this->functionBody->GetDisplayName(), this->functionBody->GetDisplayNameLength(), scriptContext);
    JsrtDebugUtils::AddPropertyToObject(functionObject, JsrtDebugPropertyId::type, scriptContext->GetLibrary()->GetFunctionTypeDisplayString(), scriptContext);
    JsrtDebugUtils::AddPropertyToObject(functionObject, JsrtDebugPropertyId::handle, this->GetHandle(), scriptContext);

    return functionObject;
}

JsrtDebuggerObjectGlobalsNode::JsrtDebuggerObjectGlobalsNode(JsrtDebuggerObjectsManager* debuggerObjectsManager, WeakArenaReference<Js::IDiagObjectModelDisplay>* objectDisplay) :
    JsrtDebuggerObjectBase(JsrtDebuggerObjectType::Globals, debuggerObjectsManager),
    objectDisplay(objectDisplay),
    walkerRef(nullptr)
{TRACE_IT(28279);
    Assert(objectDisplay != nullptr);
}

JsrtDebuggerObjectGlobalsNode::~JsrtDebuggerObjectGlobalsNode()
{TRACE_IT(28280);
    if (this->objectDisplay != nullptr)
    {TRACE_IT(28281);
        HeapDelete(this->objectDisplay);
        this->objectDisplay = nullptr;
    }

    if (this->walkerRef != nullptr)
    {TRACE_IT(28282);
        HeapDelete(this->walkerRef);
        this->walkerRef = nullptr;
    }
}

JsrtDebuggerObjectBase * JsrtDebuggerObjectGlobalsNode::Make(JsrtDebuggerObjectsManager * debuggerObjectsManager, WeakArenaReference<Js::IDiagObjectModelDisplay>* objectDisplay)
{TRACE_IT(28283);
    JsrtDebuggerObjectBase* debuggerObject = Anew(debuggerObjectsManager->GetDebugObjectArena(), JsrtDebuggerObjectGlobalsNode, debuggerObjectsManager, objectDisplay);

    debuggerObjectsManager->AddToDebuggerObjectsDictionary(debuggerObject);

    return debuggerObject;
}

Js::DynamicObject * JsrtDebuggerObjectGlobalsNode::GetJSONObject(Js::ScriptContext * scriptContext)
{TRACE_IT(28284);
    Js::IDiagObjectModelDisplay* objectDisplayRef = this->objectDisplay->GetStrongReference();

    Js::DynamicObject* globalsNode = nullptr;

    if (objectDisplayRef != nullptr)
    {TRACE_IT(28285);
        globalsNode = scriptContext->GetLibrary()->CreateObject();
        JsrtDebugUtils::AddPropertyToObject(globalsNode, JsrtDebugPropertyId::handle, this->GetHandle(), scriptContext);
        this->objectDisplay->ReleaseStrongReference();
    }

    return globalsNode;
}

Js::DynamicObject * JsrtDebuggerObjectGlobalsNode::GetChildren(Js::ScriptContext * scriptContext, uint fromCount, uint totalCount)
{TRACE_IT(28286);
    Js::IDiagObjectModelDisplay* objectDisplayRef = objectDisplay->GetStrongReference();
    if (objectDisplayRef == nullptr)
    {TRACE_IT(28287);
        return nullptr;
    }

    if (this->walkerRef == nullptr)
    {TRACE_IT(28288);
        this->walkerRef = objectDisplayRef->CreateWalker();
    }

    Js::DynamicObject* childrens = __super::GetChildren(this->walkerRef, scriptContext, fromCount, totalCount);

    objectDisplay->ReleaseStrongReference();

    return childrens;
}

JsrtDebugStackFrames::JsrtDebugStackFrames(JsrtDebugManager* jsrtDebugManager):
    framesDictionary(nullptr)
{TRACE_IT(28289);
    Assert(jsrtDebugManager != nullptr);
    this->jsrtDebugManager = jsrtDebugManager;
}

JsrtDebugStackFrames::~JsrtDebugStackFrames()
{TRACE_IT(28290);
    if (this->framesDictionary != nullptr)
    {TRACE_IT(28291);
        this->ClearFrameDictionary();
        Adelete(this->jsrtDebugManager->GetDebugObjectArena(), this->framesDictionary);
        this->framesDictionary = nullptr;
    }
}

static int __cdecl DiagStackFrameSorter(void * dispatchHaltFrameAddress, const void * diagStackFrame1, const void * diagStackFrame2)
{TRACE_IT(28292);
    const DWORD_PTR *p1 = reinterpret_cast<const DWORD_PTR*>(diagStackFrame1);
    const DWORD_PTR *p2 = reinterpret_cast<const DWORD_PTR*>(diagStackFrame2);

    Js::DiagStackFrame * pStackFrame1 = (Js::DiagStackFrame *)(*p1);
    Js::DiagStackFrame * pStackFrame2 = (Js::DiagStackFrame *)(*p2);

    DWORD_PTR stackAddress1 = pStackFrame1->GetStackAddress();
    DWORD_PTR stackAddress2 = pStackFrame2->GetStackAddress();

    return stackAddress1 > stackAddress2 ? 1 : -1;
}

Js::JavascriptArray * JsrtDebugStackFrames::StackFrames(Js::ScriptContext * scriptContext)
{TRACE_IT(28293);
    Js::JavascriptArray* stackTraceArray = nullptr;

    ThreadContext* threadContext = scriptContext->GetThreadContext();

    DWORD_PTR dispatchHaltFrameAddress = threadContext->GetDebugManager()->GetDispatchHaltFrameAddress();
    AssertMsg(dispatchHaltFrameAddress > 0, "Didn't set the dispatchHaltFrameAddress at time of break?");

    if (dispatchHaltFrameAddress != 0)
    {TRACE_IT(28294);
        if (this->framesDictionary == nullptr)
        {TRACE_IT(28295);
            this->framesDictionary = Anew(this->jsrtDebugManager->GetDebugObjectArena(), FramesDictionary, this->jsrtDebugManager->GetDebugObjectArena(), 10);
        }
        else
        {TRACE_IT(28296);
            this->ClearFrameDictionary();
        }

        typedef JsUtil::List<Js::DiagStackFrame*, ArenaAllocator> DiagStackFrameList;
        DiagStackFrameList* stackList = Anew(this->jsrtDebugManager->GetDebugObjectArena(), DiagStackFrameList, this->jsrtDebugManager->GetDebugObjectArena(), 10);

        // Walk all the script contexts and collect the frames which are below the address when break was reported.
        for (Js::ScriptContext *tempScriptContext = threadContext->GetScriptContextList();
        tempScriptContext != nullptr && tempScriptContext->IsScriptContextInDebugMode();
            tempScriptContext = tempScriptContext->next)
        {TRACE_IT(28297);
            Js::WeakDiagStack * framePointers = tempScriptContext->GetDebugContext()->GetProbeContainer()->GetFramePointers(dispatchHaltFrameAddress);
            if (framePointers != nullptr)
            {TRACE_IT(28298);
                Js::DiagStack* stackFrames = framePointers->GetStrongReference();
                if (stackFrames != nullptr)
                {TRACE_IT(28299);
                    int count = stackFrames->Count();
                    for (int frameIndex = 0; frameIndex < count; ++frameIndex)
                    {TRACE_IT(28300);
                        Js::DiagStackFrame* stackFrame = stackFrames->Peek(frameIndex);
                        stackList->Add(stackFrame);
                    }
                }

                framePointers->ReleaseStrongReference();
                HeapDelete(framePointers);
            }
        }

        // Frames can be from multiple contexts, sort them based on stack address
        stackList->Sort(DiagStackFrameSorter, (void*)dispatchHaltFrameAddress);

        stackTraceArray = scriptContext->GetLibrary()->CreateArray(stackList->Count(), stackList->Count());

        stackList->Map([&](int index, Js::DiagStackFrame* stackFrame)
        {
            AssertMsg(index != 0 || stackFrame->IsTopFrame(), "Index 0 frame is not marked as top frame");
            Js::DynamicObject* stackTraceObject = this->GetStackFrame(stackFrame, index);
            Js::Var marshaledObj = Js::CrossSite::MarshalVar(scriptContext, stackTraceObject);
            stackTraceArray->DirectSetItemAt(index, marshaledObj);
        });

        Adelete(this->jsrtDebugManager->GetDebugObjectArena(), stackList);
    }
    else
    {TRACE_IT(28301);
        // Empty array
        stackTraceArray = scriptContext->GetLibrary()->CreateArray(0, 0);
    }

    return stackTraceArray;
}

bool JsrtDebugStackFrames::TryGetFrameObjectFromFrameIndex(uint frameIndex, JsrtDebuggerStackFrame ** debuggerStackFrame)
{TRACE_IT(28302);
    if (this->framesDictionary != nullptr)
    {TRACE_IT(28303);
        return this->framesDictionary->TryGetValue(frameIndex, debuggerStackFrame);
    }

    return false;
}

Js::DynamicObject * JsrtDebugStackFrames::GetStackFrame(Js::DiagStackFrame * stackFrame, uint frameIndex)
{TRACE_IT(28304);
    JsrtDebuggerStackFrame* debuggerStackFrame = Anew(this->jsrtDebugManager->GetDebugObjectArena(), JsrtDebuggerStackFrame, this->jsrtDebugManager->GetDebuggerObjectsManager(), stackFrame, frameIndex);

    Assert(this->framesDictionary != nullptr);

    this->framesDictionary->Add(frameIndex, debuggerStackFrame);

    return debuggerStackFrame->GetJSONObject(stackFrame->GetScriptContext());
}

void JsrtDebugStackFrames::ClearFrameDictionary()
{TRACE_IT(28305);
    if (this->framesDictionary != nullptr)
    {TRACE_IT(28306);
        this->framesDictionary->Map([this](uint handle, JsrtDebuggerStackFrame* debuggerStackFrame) {
            Adelete(this->jsrtDebugManager->GetDebugObjectArena(), debuggerStackFrame);
        });
        this->framesDictionary->Clear();
    }
}
