//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

class JsrtDebugManager;
class JsrtDebuggerObjectsManager;

// Type of objects we give to debugger
enum class JsrtDebuggerObjectType
{
    Function,
    Globals,
    Property,
    Scope
};

// Base class representing a debugger object
class JsrtDebuggerObjectBase
{
public:
    JsrtDebuggerObjectBase(JsrtDebuggerObjectType type, JsrtDebuggerObjectsManager* debuggerObjectsManager);
    virtual ~JsrtDebuggerObjectBase();

    JsrtDebuggerObjectType GetType() {TRACE_IT(28307); return type; }
    uint GetHandle() const {TRACE_IT(28308); return handle; }
    JsrtDebuggerObjectsManager* GetDebuggerObjectsManager();
    virtual Js::DynamicObject* GetJSONObject(Js::ScriptContext* scriptContext) = 0;
    virtual Js::DynamicObject* GetChildren(Js::ScriptContext* scriptContext, uint fromCount, uint totalCount);

    template<class JsrtDebuggerObjectType, class PostFunction>
    static void CreateDebuggerObject(JsrtDebuggerObjectsManager* debuggerObjectsManager, Js::ResolvedObject resolvedObject, Js::ScriptContext* scriptContext, PostFunction postFunction)
    {TRACE_IT(28309);
        AutoPtr<WeakArenaReference<Js::IDiagObjectModelDisplay>> objectDisplayWeakRef(resolvedObject.GetObjectDisplay());
        Js::IDiagObjectModelDisplay* objectDisplay = objectDisplayWeakRef->GetStrongReference();
        if (objectDisplay != nullptr)
        {TRACE_IT(28310);
            JsrtDebuggerObjectBase* debuggerObject = JsrtDebuggerObjectType::Make(debuggerObjectsManager, objectDisplayWeakRef);
            Js::DynamicObject* object = debuggerObject->GetJSONObject(resolvedObject.scriptContext);
            Assert(object != nullptr);
            Js::Var marshaledObj = Js::CrossSite::MarshalVar(scriptContext, object);
            postFunction(marshaledObj);
            objectDisplayWeakRef.Detach();
        }
    }

protected:
    Js::DynamicObject* GetChildren(WeakArenaReference<Js::IDiagObjectModelWalkerBase>* walkerRef, Js::ScriptContext * scriptContext, uint fromCount, uint totalCount);

private:
    JsrtDebuggerObjectType type;
    uint handle;
    JsrtDebuggerObjectsManager* debuggerObjectsManager;
};

class JsrtDebuggerObjectFunction : public JsrtDebuggerObjectBase
{
public:
    static JsrtDebuggerObjectBase* Make(JsrtDebuggerObjectsManager* debuggerObjectsManager, Js::FunctionBody* functionBody);
    Js::DynamicObject* GetJSONObject(Js::ScriptContext* scriptContext);

private:
    JsrtDebuggerObjectFunction(JsrtDebuggerObjectsManager* debuggerObjectsManager, Js::FunctionBody* functionBody);
    ~JsrtDebuggerObjectFunction();
    Js::FunctionBody* functionBody;
};

class JsrtDebuggerObjectProperty : public JsrtDebuggerObjectBase
{
public:
    static JsrtDebuggerObjectBase* Make(JsrtDebuggerObjectsManager* debuggerObjectsManager, WeakArenaReference<Js::IDiagObjectModelDisplay>* objectDisplay);

    Js::DynamicObject* GetJSONObject(Js::ScriptContext* scriptContext);
    Js::DynamicObject* GetChildren(Js::ScriptContext* scriptContext, uint fromCount, uint totalCount);

private:
    JsrtDebuggerObjectProperty(JsrtDebuggerObjectsManager* debuggerObjectsManager, WeakArenaReference<Js::IDiagObjectModelDisplay>* objectDisplay);
    ~JsrtDebuggerObjectProperty();
    WeakArenaReference<Js::IDiagObjectModelDisplay>* objectDisplay;
    WeakArenaReference<Js::IDiagObjectModelWalkerBase>* walkerRef;
};

class JsrtDebuggerObjectGlobalsNode : public JsrtDebuggerObjectBase
{
public:
    static JsrtDebuggerObjectBase* Make(JsrtDebuggerObjectsManager* debuggerObjectsManager, WeakArenaReference<Js::IDiagObjectModelDisplay>* objectDisplay);

    Js::DynamicObject* GetJSONObject(Js::ScriptContext* scriptContext);
    Js::DynamicObject* GetChildren(Js::ScriptContext* scriptContext, uint fromCount, uint totalCount);

private:
    JsrtDebuggerObjectGlobalsNode(JsrtDebuggerObjectsManager* debuggerObjectsManager, WeakArenaReference<Js::IDiagObjectModelDisplay>* objectDisplay);
    ~JsrtDebuggerObjectGlobalsNode();
    WeakArenaReference<Js::IDiagObjectModelDisplay>* objectDisplay;
    WeakArenaReference<Js::IDiagObjectModelWalkerBase>* walkerRef;
};

class JsrtDebuggerObjectScope : public JsrtDebuggerObjectBase
{
public:
    static JsrtDebuggerObjectBase* Make(JsrtDebuggerObjectsManager* debuggerObjectsManager, WeakArenaReference<Js::IDiagObjectModelDisplay>* objectDisplay, uint index);

    Js::DynamicObject* GetJSONObject(Js::ScriptContext* scriptContext);
    Js::DynamicObject* GetChildren(Js::ScriptContext* scriptContext, uint fromCount, uint totalCount);

private:
    JsrtDebuggerObjectScope(JsrtDebuggerObjectsManager* debuggerObjectsManager, WeakArenaReference<Js::IDiagObjectModelDisplay>* objectDisplay, uint index);
    ~JsrtDebuggerObjectScope();
    WeakArenaReference<Js::IDiagObjectModelDisplay>* objectDisplay;
    WeakArenaReference<Js::IDiagObjectModelWalkerBase>* walkerRef;
    uint index;
};

class JsrtDebuggerStackFrame
{
public:
    JsrtDebuggerStackFrame(JsrtDebuggerObjectsManager * debuggerObjectsManager, Js::DiagStackFrame* stackFrame, uint frameIndex);
    ~JsrtDebuggerStackFrame();
    Js::DynamicObject* GetJSONObject(Js::ScriptContext* scriptContext);
    Js::DynamicObject* GetLocalsObject(Js::ScriptContext* scriptContext);
    bool Evaluate(Js::ScriptContext* scriptContext, const char16 *source, int sourceLength, bool isLibraryCode, Js::DynamicObject** evalResult);
    uint GetIndex() const {TRACE_IT(28311); return this->frameIndex; }

private:
    uint frameIndex;
    Js::DiagStackFrame* stackFrame;
    JsrtDebuggerObjectsManager * debuggerObjectsManager;
};

class JsrtDebugStackFrames
{
public:
    JsrtDebugStackFrames(JsrtDebugManager* jsrtDebugManager);
    ~JsrtDebugStackFrames();
    Js::JavascriptArray* StackFrames(Js::ScriptContext* scriptContext);
    bool TryGetFrameObjectFromFrameIndex(uint frameIndex, JsrtDebuggerStackFrame ** debuggerStackFrame);
private:
    Js::DynamicObject* GetStackFrame(Js::DiagStackFrame * stackFrame, uint frameIndex);
    JsrtDebugManager* jsrtDebugManager;

    typedef JsUtil::BaseDictionary<uint, JsrtDebuggerStackFrame*, ArenaAllocator> FramesDictionary;
    FramesDictionary* framesDictionary;
    void ClearFrameDictionary();
};

// Class managing objects we give to debugger, it maintains various mappings
class JsrtDebuggerObjectsManager
{
public:
    JsrtDebuggerObjectsManager(JsrtDebugManager* jsrtDebugManager);
    ~JsrtDebuggerObjectsManager();

    void ClearAll();
    JsrtDebugManager* GetJsrtDebugManager() {TRACE_IT(28312); return this->jsrtDebugManager; };
    ArenaAllocator* GetDebugObjectArena();
    uint GetNextHandle() {TRACE_IT(28313); return ++handleId; }

    void AddToDebuggerObjectsDictionary(JsrtDebuggerObjectBase* debuggerObject);
    bool TryGetDebuggerObjectFromHandle(uint handle, JsrtDebuggerObjectBase** debuggerObject);

    void AddToDataToDebuggerObjectsDictionary(void* data, JsrtDebuggerObjectBase* debuggerObject);
    bool TryGetDataFromDataToDebuggerObjectsDictionary(void* data, JsrtDebuggerObjectBase** debuggerObject);

private:
    uint handleId;
    JsrtDebugManager* jsrtDebugManager;

    typedef JsUtil::BaseDictionary<uint, JsrtDebuggerObjectBase*, ArenaAllocator> DebuggerObjectsDictionary;
    DebuggerObjectsDictionary* handleToDebuggerObjectsDictionary;

    typedef JsUtil::BaseDictionary<void*, JsrtDebuggerObjectBase*, ArenaAllocator> DataToDebuggerObjectsDictionary;
    DataToDebuggerObjectsDictionary* dataToDebuggerObjectsDictionary;
};
