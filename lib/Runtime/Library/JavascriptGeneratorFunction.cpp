//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    FunctionInfo JavascriptGeneratorFunction::functionInfo(
        FORCE_NO_WRITE_BARRIER_TAG(JavascriptGeneratorFunction::EntryGeneratorFunctionImplementation),
        (FunctionInfo::Attributes)(FunctionInfo::DoNotProfile | FunctionInfo::ErrorOnNew));
    FunctionInfo JavascriptAsyncFunction::functionInfo(
        FORCE_NO_WRITE_BARRIER_TAG(JavascriptGeneratorFunction::EntryAsyncFunctionImplementation),
        (FunctionInfo::Attributes)(FunctionInfo::DoNotProfile | FunctionInfo::ErrorOnNew));

    JavascriptGeneratorFunction::JavascriptGeneratorFunction(DynamicType* type)
        : ScriptFunctionBase(type, &functionInfo),
        scriptFunction(nullptr)
    {TRACE_IT(59007);
        // Constructor used during copy on write.
        DebugOnly(VerifyEntryPoint());
    }

    JavascriptGeneratorFunction::JavascriptGeneratorFunction(DynamicType* type, GeneratorVirtualScriptFunction* scriptFunction)
        : ScriptFunctionBase(type, &functionInfo),
        scriptFunction(scriptFunction)
    {TRACE_IT(59008);
        DebugOnly(VerifyEntryPoint());
    }

    JavascriptGeneratorFunction::JavascriptGeneratorFunction(DynamicType* type, FunctionInfo* functionInfo, GeneratorVirtualScriptFunction* scriptFunction)
        : ScriptFunctionBase(type, functionInfo),
        scriptFunction(scriptFunction)
    {TRACE_IT(59009);
        DebugOnly(VerifyEntryPoint());
    }

    JavascriptAsyncFunction::JavascriptAsyncFunction(DynamicType* type, GeneratorVirtualScriptFunction* scriptFunction)
        : JavascriptGeneratorFunction(type, &functionInfo, scriptFunction)
    {TRACE_IT(59010);
        DebugOnly(VerifyEntryPoint());
    }

    JavascriptAsyncFunction* JavascriptAsyncFunction::New(ScriptContext* scriptContext, GeneratorVirtualScriptFunction* scriptFunction)
    {TRACE_IT(59011);
        return scriptContext->GetLibrary()->CreateAsyncFunction(functionInfo.GetOriginalEntryPoint(), scriptFunction);
    }

    bool JavascriptGeneratorFunction::Is(Var var)
    {TRACE_IT(59012);
        if (JavascriptFunction::Is(var))
        {TRACE_IT(59013);
            JavascriptFunction* obj = JavascriptFunction::FromVar(var);

            return VirtualTableInfo<JavascriptGeneratorFunction>::HasVirtualTable(obj)
                || VirtualTableInfo<CrossSiteObject<JavascriptGeneratorFunction>>::HasVirtualTable(obj);
        }

        return false;
    }

    JavascriptGeneratorFunction* JavascriptGeneratorFunction::FromVar(Var var)
    {TRACE_IT(59014);
        Assert(JavascriptGeneratorFunction::Is(var) || JavascriptAsyncFunction::Is(var));

        return static_cast<JavascriptGeneratorFunction*>(var);
    }

    bool JavascriptAsyncFunction::Is(Var var)
    {TRACE_IT(59015);
        if (JavascriptFunction::Is(var))
        {TRACE_IT(59016);
            JavascriptFunction* obj = JavascriptFunction::FromVar(var);

            return VirtualTableInfo<JavascriptAsyncFunction>::HasVirtualTable(obj)
                || VirtualTableInfo<CrossSiteObject<JavascriptAsyncFunction>>::HasVirtualTable(obj);
        }

        return false;
    }

    JavascriptAsyncFunction* JavascriptAsyncFunction::FromVar(Var var)
    {TRACE_IT(59017);
        Assert(JavascriptAsyncFunction::Is(var));

        return static_cast<JavascriptAsyncFunction*>(var);
    }

    JavascriptGeneratorFunction* JavascriptGeneratorFunction::OP_NewScGenFunc(FrameDisplay *environment, FunctionInfoPtrPtr infoRef)
    {TRACE_IT(59018);
        FunctionProxy* functionProxy = (*infoRef)->GetFunctionProxy();
        ScriptContext* scriptContext = functionProxy->GetScriptContext();

        bool hasSuperReference = functionProxy->HasSuperReference();

        GeneratorVirtualScriptFunction* scriptFunction = scriptContext->GetLibrary()->CreateGeneratorVirtualScriptFunction(functionProxy);
        scriptFunction->SetEnvironment(environment);
        scriptFunction->SetHasSuperReference(hasSuperReference);

        JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_FUNCTION(scriptFunction, EtwTrace::GetFunctionId(functionProxy)));

        JavascriptGeneratorFunction* genFunc =
            functionProxy->IsAsync()
            ? JavascriptAsyncFunction::New(scriptContext, scriptFunction)
            : scriptContext->GetLibrary()->CreateGeneratorFunction(functionInfo.GetOriginalEntryPoint(), scriptFunction);

        scriptFunction->SetRealGeneratorFunction(genFunc);

        return genFunc;
    }

    Var JavascriptGeneratorFunction::EntryGeneratorFunctionImplementation(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(stackArgs, callInfo);

        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptGeneratorFunction* generatorFunction = JavascriptGeneratorFunction::FromVar(function);

        // InterpreterStackFrame takes a pointer to the args, so copy them to the recycler heap
        // and use that buffer for this InterpreterStackFrame.
        Field(Var)* argsHeapCopy = RecyclerNewArray(scriptContext->GetRecycler(), Field(Var), stackArgs.Info.Count);
        CopyArray(argsHeapCopy, stackArgs.Info.Count, stackArgs.Values, stackArgs.Info.Count);
        Arguments heapArgs(callInfo, (Var*)argsHeapCopy);

        DynamicObject* prototype = scriptContext->GetLibrary()->CreateGeneratorConstructorPrototypeObject();
        JavascriptGenerator* generator = scriptContext->GetLibrary()->CreateGenerator(heapArgs, generatorFunction->scriptFunction, prototype);
        // Set the prototype from constructor
        JavascriptOperators::OrdinaryCreateFromConstructor(function, generator, prototype, scriptContext);

        Assert(!(callInfo.Flags & CallFlags_New));

        return generator;
    }

    Var JavascriptGeneratorFunction::EntryAsyncFunctionImplementation(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(stackArgs, callInfo);

        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();
        RecyclableObject* prototype = scriptContext->GetLibrary()->GetNull();

        // InterpreterStackFrame takes a pointer to the args, so copy them to the recycler heap
        // and use that buffer for this InterpreterStackFrame.
        Field(Var)* argsHeapCopy = RecyclerNewArray(scriptContext->GetRecycler(), Field(Var), stackArgs.Info.Count);
        CopyArray(argsHeapCopy, stackArgs.Info.Count, stackArgs.Values, stackArgs.Info.Count);
        Arguments heapArgs(callInfo, (Var*)argsHeapCopy);

        JavascriptExceptionObject* e = nullptr;
        JavascriptPromiseResolveOrRejectFunction* resolve;
        JavascriptPromiseResolveOrRejectFunction* reject;
        JavascriptPromiseAsyncSpawnExecutorFunction* executor =
            library->CreatePromiseAsyncSpawnExecutorFunction(
                JavascriptPromise::EntryJavascriptPromiseAsyncSpawnExecutorFunction,
                scriptContext->GetLibrary()->CreateGenerator(heapArgs, JavascriptAsyncFunction::FromVar(function)->GetGeneratorVirtualScriptFunction(), prototype),
                stackArgs[0]);

        JavascriptPromise* promise = library->CreatePromise();
        JavascriptPromise::InitializePromise(promise, &resolve, &reject, scriptContext);

        try
        {
            CALL_FUNCTION(executor, CallInfo(CallFlags_Value, 3), library->GetUndefined(), resolve, reject);
        }
        catch (const JavascriptException& err)
        {TRACE_IT(59019);
            e = err.GetAndClear();
        }

        if (e != nullptr)
        {TRACE_IT(59020);
            JavascriptPromise::TryRejectWithExceptionObject(e, reject, scriptContext);
        }

        return promise;
    }

    Var JavascriptGeneratorFunction::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        // Get called when creating a new generator function through the constructor (e.g. gf.__proto__.constructor) and sets EntryGeneratorFunctionImplementation as the entrypoint
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);

        return JavascriptFunction::NewInstanceHelper(function->GetScriptContext(), function, callInfo, args, FunctionKind::Generator);
    }

    Var JavascriptGeneratorFunction::NewInstanceRestrictedMode(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ScriptContext* scriptContext = function->GetScriptContext();

        scriptContext->CheckEvalRestriction();

        PROBE_STACK(scriptContext, Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);

        return JavascriptFunction::NewInstanceHelper(scriptContext, function, callInfo, args, FunctionKind::Generator);
    }

    JavascriptString* JavascriptGeneratorFunction::GetDisplayNameImpl() const
    {TRACE_IT(59021);
        return scriptFunction->GetDisplayNameImpl();
    }

    Var JavascriptGeneratorFunction::GetHomeObj() const
    {TRACE_IT(59022);
        return scriptFunction->GetHomeObj();
    }

    void JavascriptGeneratorFunction::SetHomeObj(Var homeObj)
    {TRACE_IT(59023);
        scriptFunction->SetHomeObj(homeObj);
    }

    void JavascriptGeneratorFunction::SetComputedNameVar(Var computedNameVar)
    {TRACE_IT(59024);
        scriptFunction->SetComputedNameVar(computedNameVar);
    }

    Var JavascriptGeneratorFunction::GetComputedNameVar() const
    {TRACE_IT(59025);
        return scriptFunction->GetComputedNameVar();
    }

    bool JavascriptGeneratorFunction::IsAnonymousFunction() const
    {TRACE_IT(59026);
        return scriptFunction->IsAnonymousFunction();
    }

    Var JavascriptGeneratorFunction::GetSourceString() const
    {TRACE_IT(59027);
        return scriptFunction->GetSourceString();
    }

    Var JavascriptGeneratorFunction::EnsureSourceString()
    {TRACE_IT(59028);
        return scriptFunction->EnsureSourceString();
    }

    BOOL JavascriptGeneratorFunction::HasProperty(PropertyId propertyId)
    {TRACE_IT(59029);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(59030);
            return true;
        }

        if (propertyId == PropertyIds::caller || propertyId == PropertyIds::arguments)
        {TRACE_IT(59031);
            // JavascriptFunction has special case for caller and arguments; call DynamicObject:: virtual directly to skip that.
            return DynamicObject::HasProperty(propertyId);
        }

        return JavascriptFunction::HasProperty(propertyId);
    }

    BOOL JavascriptGeneratorFunction::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(59032);
        BOOL result;
        if (GetPropertyBuiltIns(originalInstance, propertyId, value, info, requestContext, &result))
        {TRACE_IT(59033);
            return result;
        }

        if (propertyId == PropertyIds::caller || propertyId == PropertyIds::arguments)
        {TRACE_IT(59034);
            // JavascriptFunction has special case for caller and arguments; call DynamicObject:: virtual directly to skip that.
            return DynamicObject::GetProperty(originalInstance, propertyId, value, info, requestContext);
        }

        return JavascriptFunction::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    BOOL JavascriptGeneratorFunction::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(59035);
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr)
        {TRACE_IT(59036);
            BOOL result;
            if (GetPropertyBuiltIns(originalInstance, propertyRecord->GetPropertyId(), value, info, requestContext, &result))
            {TRACE_IT(59037);
                return result;
            }

            if (propertyRecord->GetPropertyId() == PropertyIds::caller || propertyRecord->GetPropertyId() == PropertyIds::arguments)
            {TRACE_IT(59038);
                // JavascriptFunction has special case for caller and arguments; call DynamicObject:: virtual directly to skip that.
                return DynamicObject::GetProperty(originalInstance, propertyNameString, value, info, requestContext);
            }
        }

        return JavascriptFunction::GetProperty(originalInstance, propertyNameString, value, info, requestContext);
    }

    bool JavascriptGeneratorFunction::GetPropertyBuiltIns(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext, BOOL* result)
    {TRACE_IT(59039);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(59040);
            // Cannot just call the base GetProperty for `length` because we need
            // to get the length from our private ScriptFunction instead of ourself.
            int len = 0;
            Var varLength;
            if (scriptFunction->GetProperty(scriptFunction, PropertyIds::length, &varLength, NULL, requestContext))
            {TRACE_IT(59041);
                len = JavascriptConversion::ToInt32(varLength, requestContext);
            }

            *value = JavascriptNumber::ToVar(len, requestContext);
            *result = true;
            return true;
        }

        return false;
    }

    BOOL JavascriptGeneratorFunction::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(59042);
        return JavascriptGeneratorFunction::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    BOOL JavascriptGeneratorFunction::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(59043);
        BOOL result;
        if (SetPropertyBuiltIns(propertyId, value, flags, info, &result))
        {TRACE_IT(59044);
            return result;
        }

        if (propertyId == PropertyIds::caller || propertyId == PropertyIds::arguments)
        {TRACE_IT(59045);
            // JavascriptFunction has special case for caller and arguments; call DynamicObject:: virtual directly to skip that.
            return DynamicObject::SetProperty(propertyId, value, flags, info);
        }

        return JavascriptFunction::SetProperty(propertyId, value, flags, info);
    }

    BOOL JavascriptGeneratorFunction::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(59046);
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr)
        {TRACE_IT(59047);
            BOOL result;
            if (SetPropertyBuiltIns(propertyRecord->GetPropertyId(), value, flags, info, &result))
            {TRACE_IT(59048);
                return result;
            }

            if (propertyRecord->GetPropertyId() == PropertyIds::caller || propertyRecord->GetPropertyId() == PropertyIds::arguments)
            {TRACE_IT(59049);
                // JavascriptFunction has special case for caller and arguments; call DynamicObject:: virtual directly to skip that.
                return DynamicObject::SetProperty(propertyNameString, value, flags, info);
            }
        }

        return JavascriptFunction::SetProperty(propertyNameString, value, flags, info);
    }

    bool JavascriptGeneratorFunction::SetPropertyBuiltIns(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info, BOOL* result)
    {TRACE_IT(59050);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(59051);
            JavascriptError::ThrowCantAssignIfStrictMode(flags, this->GetScriptContext());

            *result = false;
            return true;
        }

        return false;
    }

    BOOL JavascriptGeneratorFunction::GetAccessors(PropertyId propertyId, Var *getter, Var *setter, ScriptContext * requestContext)
    {TRACE_IT(59052);
        if (propertyId == PropertyIds::caller || propertyId == PropertyIds::arguments)
        {TRACE_IT(59053);
            // JavascriptFunction has special case for caller and arguments; call DynamicObject:: virtual directly to skip that.
            return DynamicObject::GetAccessors(propertyId, getter, setter, requestContext);
        }

        return JavascriptFunction::GetAccessors(propertyId, getter, setter, requestContext);
    }

    DescriptorFlags JavascriptGeneratorFunction::GetSetter(PropertyId propertyId, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(59054);
        if (propertyId == PropertyIds::caller || propertyId == PropertyIds::arguments)
        {TRACE_IT(59055);
            // JavascriptFunction has special case for caller and arguments; call DynamicObject:: virtual directly to skip that.
            return DynamicObject::GetSetter(propertyId, setterValue, info, requestContext);
        }

        return JavascriptFunction::GetSetter(propertyId, setterValue, info, requestContext);
    }

    DescriptorFlags JavascriptGeneratorFunction::GetSetter(JavascriptString* propertyNameString, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(59056);
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && (propertyRecord->GetPropertyId() == PropertyIds::caller || propertyRecord->GetPropertyId() == PropertyIds::arguments))
        {TRACE_IT(59057);
            // JavascriptFunction has special case for caller and arguments; call DynamicObject:: virtual directly to skip that.
            return DynamicObject::GetSetter(propertyNameString, setterValue, info, requestContext);
        }

        return JavascriptFunction::GetSetter(propertyNameString, setterValue, info, requestContext);
    }

    BOOL JavascriptGeneratorFunction::InitProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(59058);
        return SetProperty(propertyId, value, PropertyOperation_None, info);
    }

    BOOL JavascriptGeneratorFunction::DeleteProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {TRACE_IT(59059);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(59060);
            return false;
        }

        if (propertyId == PropertyIds::caller || propertyId == PropertyIds::arguments)
        {TRACE_IT(59061);
            // JavascriptFunction has special case for caller and arguments; call DynamicObject:: virtual directly to skip that.
            return DynamicObject::DeleteProperty(propertyId, flags);
        }

        return JavascriptFunction::DeleteProperty(propertyId, flags);
    }

    BOOL JavascriptGeneratorFunction::DeleteProperty(JavascriptString *propertyNameString, PropertyOperationFlags flags)
    {TRACE_IT(59062);
        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());
        if (BuiltInPropertyRecords::length.Equals(propertyName))
        {TRACE_IT(59063);
            return false;
        }

        if (BuiltInPropertyRecords::caller.Equals(propertyName) || BuiltInPropertyRecords::arguments.Equals(propertyName))
        {TRACE_IT(59064);
            // JavascriptFunction has special case for caller and arguments; call DynamicObject:: virtual directly to skip that.
            return DynamicObject::DeleteProperty(propertyNameString, flags);
        }

        return JavascriptFunction::DeleteProperty(propertyNameString, flags);
    }

    BOOL JavascriptGeneratorFunction::IsWritable(PropertyId propertyId)
    {TRACE_IT(59065);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(59066);
            return false;
        }

        if (propertyId == PropertyIds::caller || propertyId == PropertyIds::arguments)
        {TRACE_IT(59067);
            // JavascriptFunction has special case for caller and arguments; call DynamicObject:: virtual directly to skip that.
            return DynamicObject::IsWritable(propertyId);
        }

        return JavascriptFunction::IsWritable(propertyId);
    }

    BOOL JavascriptGeneratorFunction::IsEnumerable(PropertyId propertyId)
    {TRACE_IT(59068);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(59069);
            return false;
        }

        if (propertyId == PropertyIds::caller || propertyId == PropertyIds::arguments)
        {TRACE_IT(59070);
            // JavascriptFunction has special case for caller and arguments; call DynamicObject:: virtual directly to skip that.
            return DynamicObject::IsEnumerable(propertyId);
        }

        return JavascriptFunction::IsEnumerable(propertyId);
    }

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType JavascriptGeneratorFunction::GetSnapTag_TTD() const
    {TRACE_IT(59071);
        //we override this with invalid to make sure it isn't unexpectedly handled by the parent class
        return TTD::NSSnapObjects::SnapObjectType::Invalid;
    }

    void JavascriptGeneratorFunction::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {
        TTDAssert(false, "Invalid -- JavascriptGeneratorFunction");
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptAsyncFunction::GetSnapTag_TTD() const
    {TRACE_IT(59072);
        //we override this with invalid to make sure it isn't unexpectedly handled by the parent class
        return TTD::NSSnapObjects::SnapObjectType::Invalid;
    }

    void JavascriptAsyncFunction::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {
        TTDAssert(false, "Invalid -- JavascriptGeneratorFunction");
    }

    TTD::NSSnapObjects::SnapObjectType GeneratorVirtualScriptFunction::GetSnapTag_TTD() const
    {TRACE_IT(59073);
        //we override this with invalid to make sure it isn't unexpectedly handled by the parent class
        return TTD::NSSnapObjects::SnapObjectType::Invalid;
    }

    void GeneratorVirtualScriptFunction::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {
        TTDAssert(false, "Invalid -- GeneratorVirtualScriptFunction");
    }
#endif
}
