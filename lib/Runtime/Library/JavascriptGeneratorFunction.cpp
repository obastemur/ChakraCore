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
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 18\n");
        // Constructor used during copy on write.
        DebugOnly(VerifyEntryPoint());
    }

    JavascriptGeneratorFunction::JavascriptGeneratorFunction(DynamicType* type, GeneratorVirtualScriptFunction* scriptFunction)
        : ScriptFunctionBase(type, &functionInfo),
        scriptFunction(scriptFunction)
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 26\n");
        DebugOnly(VerifyEntryPoint());
    }

    JavascriptGeneratorFunction::JavascriptGeneratorFunction(DynamicType* type, FunctionInfo* functionInfo, GeneratorVirtualScriptFunction* scriptFunction)
        : ScriptFunctionBase(type, functionInfo),
        scriptFunction(scriptFunction)
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 33\n");
        DebugOnly(VerifyEntryPoint());
    }

    JavascriptAsyncFunction::JavascriptAsyncFunction(DynamicType* type, GeneratorVirtualScriptFunction* scriptFunction)
        : JavascriptGeneratorFunction(type, &functionInfo, scriptFunction)
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 39\n");
        DebugOnly(VerifyEntryPoint());
    }

    JavascriptAsyncFunction* JavascriptAsyncFunction::New(ScriptContext* scriptContext, GeneratorVirtualScriptFunction* scriptFunction)
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 44\n");
        return scriptContext->GetLibrary()->CreateAsyncFunction(functionInfo.GetOriginalEntryPoint(), scriptFunction);
    }

    bool JavascriptGeneratorFunction::Is(Var var)
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 49\n");
        if (JavascriptFunction::Is(var))
        {LOGMEIN("JavascriptGeneratorFunction.cpp] 51\n");
            JavascriptFunction* obj = JavascriptFunction::FromVar(var);

            return VirtualTableInfo<JavascriptGeneratorFunction>::HasVirtualTable(obj)
                || VirtualTableInfo<CrossSiteObject<JavascriptGeneratorFunction>>::HasVirtualTable(obj);
        }

        return false;
    }

    JavascriptGeneratorFunction* JavascriptGeneratorFunction::FromVar(Var var)
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 62\n");
        Assert(JavascriptGeneratorFunction::Is(var) || JavascriptAsyncFunction::Is(var));

        return static_cast<JavascriptGeneratorFunction*>(var);
    }

    bool JavascriptAsyncFunction::Is(Var var)
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 69\n");
        if (JavascriptFunction::Is(var))
        {LOGMEIN("JavascriptGeneratorFunction.cpp] 71\n");
            JavascriptFunction* obj = JavascriptFunction::FromVar(var);

            return VirtualTableInfo<JavascriptAsyncFunction>::HasVirtualTable(obj)
                || VirtualTableInfo<CrossSiteObject<JavascriptAsyncFunction>>::HasVirtualTable(obj);
        }

        return false;
    }

    JavascriptAsyncFunction* JavascriptAsyncFunction::FromVar(Var var)
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 82\n");
        Assert(JavascriptAsyncFunction::Is(var));

        return static_cast<JavascriptAsyncFunction*>(var);
    }

    JavascriptGeneratorFunction* JavascriptGeneratorFunction::OP_NewScGenFunc(FrameDisplay *environment, FunctionInfoPtrPtr infoRef)
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 89\n");
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
        {LOGMEIN("JavascriptGeneratorFunction.cpp] 167\n");
            e = err.GetAndClear();
        }

        if (e != nullptr)
        {LOGMEIN("JavascriptGeneratorFunction.cpp] 172\n");
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
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 203\n");
        return scriptFunction->GetDisplayNameImpl();
    }

    Var JavascriptGeneratorFunction::GetHomeObj() const
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 208\n");
        return scriptFunction->GetHomeObj();
    }

    void JavascriptGeneratorFunction::SetHomeObj(Var homeObj)
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 213\n");
        scriptFunction->SetHomeObj(homeObj);
    }

    void JavascriptGeneratorFunction::SetComputedNameVar(Var computedNameVar)
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 218\n");
        scriptFunction->SetComputedNameVar(computedNameVar);
    }

    Var JavascriptGeneratorFunction::GetComputedNameVar() const
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 223\n");
        return scriptFunction->GetComputedNameVar();
    }

    bool JavascriptGeneratorFunction::IsAnonymousFunction() const
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 228\n");
        return scriptFunction->IsAnonymousFunction();
    }

    Var JavascriptGeneratorFunction::GetSourceString() const
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 233\n");
        return scriptFunction->GetSourceString();
    }

    Var JavascriptGeneratorFunction::EnsureSourceString()
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 238\n");
        return scriptFunction->EnsureSourceString();
    }

    BOOL JavascriptGeneratorFunction::HasProperty(PropertyId propertyId)
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 243\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("JavascriptGeneratorFunction.cpp] 245\n");
            return true;
        }

        if (propertyId == PropertyIds::caller || propertyId == PropertyIds::arguments)
        {LOGMEIN("JavascriptGeneratorFunction.cpp] 250\n");
            // JavascriptFunction has special case for caller and arguments; call DynamicObject:: virtual directly to skip that.
            return DynamicObject::HasProperty(propertyId);
        }

        return JavascriptFunction::HasProperty(propertyId);
    }

    BOOL JavascriptGeneratorFunction::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 259\n");
        BOOL result;
        if (GetPropertyBuiltIns(originalInstance, propertyId, value, info, requestContext, &result))
        {LOGMEIN("JavascriptGeneratorFunction.cpp] 262\n");
            return result;
        }

        if (propertyId == PropertyIds::caller || propertyId == PropertyIds::arguments)
        {LOGMEIN("JavascriptGeneratorFunction.cpp] 267\n");
            // JavascriptFunction has special case for caller and arguments; call DynamicObject:: virtual directly to skip that.
            return DynamicObject::GetProperty(originalInstance, propertyId, value, info, requestContext);
        }

        return JavascriptFunction::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    BOOL JavascriptGeneratorFunction::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 276\n");
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr)
        {LOGMEIN("JavascriptGeneratorFunction.cpp] 281\n");
            BOOL result;
            if (GetPropertyBuiltIns(originalInstance, propertyRecord->GetPropertyId(), value, info, requestContext, &result))
            {LOGMEIN("JavascriptGeneratorFunction.cpp] 284\n");
                return result;
            }

            if (propertyRecord->GetPropertyId() == PropertyIds::caller || propertyRecord->GetPropertyId() == PropertyIds::arguments)
            {LOGMEIN("JavascriptGeneratorFunction.cpp] 289\n");
                // JavascriptFunction has special case for caller and arguments; call DynamicObject:: virtual directly to skip that.
                return DynamicObject::GetProperty(originalInstance, propertyNameString, value, info, requestContext);
            }
        }

        return JavascriptFunction::GetProperty(originalInstance, propertyNameString, value, info, requestContext);
    }

    bool JavascriptGeneratorFunction::GetPropertyBuiltIns(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext, BOOL* result)
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 299\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("JavascriptGeneratorFunction.cpp] 301\n");
            // Cannot just call the base GetProperty for `length` because we need
            // to get the length from our private ScriptFunction instead of ourself.
            int len = 0;
            Var varLength;
            if (scriptFunction->GetProperty(scriptFunction, PropertyIds::length, &varLength, NULL, requestContext))
            {LOGMEIN("JavascriptGeneratorFunction.cpp] 307\n");
                len = JavascriptConversion::ToInt32(varLength, requestContext);
            }

            *value = JavascriptNumber::ToVar(len, requestContext);
            *result = true;
            return true;
        }

        return false;
    }

    BOOL JavascriptGeneratorFunction::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 320\n");
        return JavascriptGeneratorFunction::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    BOOL JavascriptGeneratorFunction::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 325\n");
        BOOL result;
        if (SetPropertyBuiltIns(propertyId, value, flags, info, &result))
        {LOGMEIN("JavascriptGeneratorFunction.cpp] 328\n");
            return result;
        }

        if (propertyId == PropertyIds::caller || propertyId == PropertyIds::arguments)
        {LOGMEIN("JavascriptGeneratorFunction.cpp] 333\n");
            // JavascriptFunction has special case for caller and arguments; call DynamicObject:: virtual directly to skip that.
            return DynamicObject::SetProperty(propertyId, value, flags, info);
        }

        return JavascriptFunction::SetProperty(propertyId, value, flags, info);
    }

    BOOL JavascriptGeneratorFunction::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 342\n");
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr)
        {LOGMEIN("JavascriptGeneratorFunction.cpp] 347\n");
            BOOL result;
            if (SetPropertyBuiltIns(propertyRecord->GetPropertyId(), value, flags, info, &result))
            {LOGMEIN("JavascriptGeneratorFunction.cpp] 350\n");
                return result;
            }

            if (propertyRecord->GetPropertyId() == PropertyIds::caller || propertyRecord->GetPropertyId() == PropertyIds::arguments)
            {LOGMEIN("JavascriptGeneratorFunction.cpp] 355\n");
                // JavascriptFunction has special case for caller and arguments; call DynamicObject:: virtual directly to skip that.
                return DynamicObject::SetProperty(propertyNameString, value, flags, info);
            }
        }

        return JavascriptFunction::SetProperty(propertyNameString, value, flags, info);
    }

    bool JavascriptGeneratorFunction::SetPropertyBuiltIns(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info, BOOL* result)
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 365\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("JavascriptGeneratorFunction.cpp] 367\n");
            JavascriptError::ThrowCantAssignIfStrictMode(flags, this->GetScriptContext());

            *result = false;
            return true;
        }

        return false;
    }

    BOOL JavascriptGeneratorFunction::GetAccessors(PropertyId propertyId, Var *getter, Var *setter, ScriptContext * requestContext)
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 378\n");
        if (propertyId == PropertyIds::caller || propertyId == PropertyIds::arguments)
        {LOGMEIN("JavascriptGeneratorFunction.cpp] 380\n");
            // JavascriptFunction has special case for caller and arguments; call DynamicObject:: virtual directly to skip that.
            return DynamicObject::GetAccessors(propertyId, getter, setter, requestContext);
        }

        return JavascriptFunction::GetAccessors(propertyId, getter, setter, requestContext);
    }

    DescriptorFlags JavascriptGeneratorFunction::GetSetter(PropertyId propertyId, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 389\n");
        if (propertyId == PropertyIds::caller || propertyId == PropertyIds::arguments)
        {LOGMEIN("JavascriptGeneratorFunction.cpp] 391\n");
            // JavascriptFunction has special case for caller and arguments; call DynamicObject:: virtual directly to skip that.
            return DynamicObject::GetSetter(propertyId, setterValue, info, requestContext);
        }

        return JavascriptFunction::GetSetter(propertyId, setterValue, info, requestContext);
    }

    DescriptorFlags JavascriptGeneratorFunction::GetSetter(JavascriptString* propertyNameString, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 400\n");
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && (propertyRecord->GetPropertyId() == PropertyIds::caller || propertyRecord->GetPropertyId() == PropertyIds::arguments))
        {LOGMEIN("JavascriptGeneratorFunction.cpp] 405\n");
            // JavascriptFunction has special case for caller and arguments; call DynamicObject:: virtual directly to skip that.
            return DynamicObject::GetSetter(propertyNameString, setterValue, info, requestContext);
        }

        return JavascriptFunction::GetSetter(propertyNameString, setterValue, info, requestContext);
    }

    BOOL JavascriptGeneratorFunction::InitProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 414\n");
        return SetProperty(propertyId, value, PropertyOperation_None, info);
    }

    BOOL JavascriptGeneratorFunction::DeleteProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 419\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("JavascriptGeneratorFunction.cpp] 421\n");
            return false;
        }

        if (propertyId == PropertyIds::caller || propertyId == PropertyIds::arguments)
        {LOGMEIN("JavascriptGeneratorFunction.cpp] 426\n");
            // JavascriptFunction has special case for caller and arguments; call DynamicObject:: virtual directly to skip that.
            return DynamicObject::DeleteProperty(propertyId, flags);
        }

        return JavascriptFunction::DeleteProperty(propertyId, flags);
    }

    BOOL JavascriptGeneratorFunction::DeleteProperty(JavascriptString *propertyNameString, PropertyOperationFlags flags)
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 435\n");
        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());
        if (BuiltInPropertyRecords::length.Equals(propertyName))
        {LOGMEIN("JavascriptGeneratorFunction.cpp] 438\n");
            return false;
        }

        if (BuiltInPropertyRecords::caller.Equals(propertyName) || BuiltInPropertyRecords::arguments.Equals(propertyName))
        {LOGMEIN("JavascriptGeneratorFunction.cpp] 443\n");
            // JavascriptFunction has special case for caller and arguments; call DynamicObject:: virtual directly to skip that.
            return DynamicObject::DeleteProperty(propertyNameString, flags);
        }

        return JavascriptFunction::DeleteProperty(propertyNameString, flags);
    }

    BOOL JavascriptGeneratorFunction::IsWritable(PropertyId propertyId)
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 452\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("JavascriptGeneratorFunction.cpp] 454\n");
            return false;
        }

        if (propertyId == PropertyIds::caller || propertyId == PropertyIds::arguments)
        {LOGMEIN("JavascriptGeneratorFunction.cpp] 459\n");
            // JavascriptFunction has special case for caller and arguments; call DynamicObject:: virtual directly to skip that.
            return DynamicObject::IsWritable(propertyId);
        }

        return JavascriptFunction::IsWritable(propertyId);
    }

    BOOL JavascriptGeneratorFunction::IsEnumerable(PropertyId propertyId)
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 468\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("JavascriptGeneratorFunction.cpp] 470\n");
            return false;
        }

        if (propertyId == PropertyIds::caller || propertyId == PropertyIds::arguments)
        {LOGMEIN("JavascriptGeneratorFunction.cpp] 475\n");
            // JavascriptFunction has special case for caller and arguments; call DynamicObject:: virtual directly to skip that.
            return DynamicObject::IsEnumerable(propertyId);
        }

        return JavascriptFunction::IsEnumerable(propertyId);
    }

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType JavascriptGeneratorFunction::GetSnapTag_TTD() const
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 485\n");
        //we override this with invalid to make sure it isn't unexpectedly handled by the parent class
        return TTD::NSSnapObjects::SnapObjectType::Invalid;
    }

    void JavascriptGeneratorFunction::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {
        TTDAssert(false, "Invalid -- JavascriptGeneratorFunction");
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptAsyncFunction::GetSnapTag_TTD() const
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 496\n");
        //we override this with invalid to make sure it isn't unexpectedly handled by the parent class
        return TTD::NSSnapObjects::SnapObjectType::Invalid;
    }

    void JavascriptAsyncFunction::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {
        TTDAssert(false, "Invalid -- JavascriptGeneratorFunction");
    }

    TTD::NSSnapObjects::SnapObjectType GeneratorVirtualScriptFunction::GetSnapTag_TTD() const
    {LOGMEIN("JavascriptGeneratorFunction.cpp] 507\n");
        //we override this with invalid to make sure it isn't unexpectedly handled by the parent class
        return TTD::NSSnapObjects::SnapObjectType::Invalid;
    }

    void GeneratorVirtualScriptFunction::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {
        TTDAssert(false, "Invalid -- GeneratorVirtualScriptFunction");
    }
#endif
}
