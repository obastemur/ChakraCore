//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#include "JsrtRuntime.h"

class JsrtContext : public FinalizableObject
{
public:
    static JsrtContext *New(JsrtRuntime * runtime);

    Js::ScriptContext * GetScriptContext() const {TRACE_IT(28027); return this->javascriptLibrary->scriptContext; }
    Js::JavascriptLibrary* GetJavascriptLibrary() const {TRACE_IT(28028); return this->javascriptLibrary; }
    JsrtRuntime * GetRuntime() const {TRACE_IT(28029); return this->runtime; }
    void* GetExternalData() const {TRACE_IT(28030); return this->externalData; }
    void SetExternalData(void * data) {TRACE_IT(28031); this->externalData = data; }

    static JsrtContext * GetCurrent();
    static bool TrySetCurrent(JsrtContext * context);
    static bool Is(void * ref);

    virtual void Mark(Recycler * recycler) override sealed;

#if ENABLE_TTD
    static void OnScriptLoad_TTDCallback(FinalizableObject* jsrtCtx, Js::JavascriptFunction * scriptFunction, Js::Utf8SourceInfo* utf8SourceInfo, CompileScriptException* compileException);
    static void OnReplayDisposeContext_TTDCallback(FinalizableObject* jsrtCtx);
#endif
    void OnScriptLoad(Js::JavascriptFunction * scriptFunction, Js::Utf8SourceInfo* utf8SourceInfo, CompileScriptException* compileException);
protected:
    DEFINE_VTABLE_CTOR_NOBASE(JsrtContext);
    JsrtContext(JsrtRuntime * runtime);
    void Link();
    void Unlink();
    void SetJavascriptLibrary(Js::JavascriptLibrary * library);
private:
    Field(Js::JavascriptLibrary *) javascriptLibrary;

    Field(JsrtRuntime *) runtime;
    Field(void*) externalData = nullptr;
    Field(TaggedPointer<JsrtContext>) previous;
    Field(TaggedPointer<JsrtContext>) next;
};
