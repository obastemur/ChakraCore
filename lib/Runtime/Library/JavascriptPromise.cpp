//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptPromise::JavascriptPromise(DynamicType * type)
        : DynamicObject(type)
    {TRACE_IT(60559);
        Assert(type->GetTypeId() == TypeIds_Promise);

        this->status = PromiseStatusCode_Undefined;
        this->result = nullptr;
        this->resolveReactions = nullptr;
        this->rejectReactions = nullptr;
    }

    // Promise() as defined by ES 2016 Sections 25.4.3.1
    Var JavascriptPromise::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();

        CHAKRATEL_LANGSTATS_INC_DATACOUNT(ES6_Promise);

        // SkipDefaultNewObject function flag should have prevented the default object from
        // being created, except when call true a host dispatch
        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
        bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && !JavascriptOperators::IsUndefined(newTarget);
        Assert(isCtorSuperCall || !(callInfo.Flags & CallFlags_New) || args[0] == nullptr
            || JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch);

        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("Promise"));

        // 1. If NewTarget is undefined, throw a TypeError exception.
        if ((callInfo.Flags & CallFlags_New) != CallFlags_New || (newTarget != nullptr && JavascriptOperators::IsUndefined(newTarget)))
        {TRACE_IT(60560);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_ClassConstructorCannotBeCalledWithoutNew, _u("Promise"));
        }

        // 2. If IsCallable(executor) is false, throw a TypeError exception.
        if (args.Info.Count < 2 || !JavascriptConversion::IsCallable(args[1]))
        {TRACE_IT(60561);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("Promise"));
        }
        RecyclableObject* executor = RecyclableObject::FromVar(args[1]);

        // 3. Let promise be ? OrdinaryCreateFromConstructor(NewTarget, "%PromisePrototype%", <<[[PromiseState]], [[PromiseResult]], [[PromiseFulfillReactions]], [[PromiseRejectReactions]], [[PromiseIsHandled]] >>).
        JavascriptPromise* promise = library->CreatePromise();
        if (isCtorSuperCall)
        {TRACE_IT(60562);
            JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), promise, library->GetPromisePrototype(), scriptContext);
        }

        JavascriptPromiseResolveOrRejectFunction* resolve;
        JavascriptPromiseResolveOrRejectFunction* reject;

        // 4. Set promise's [[PromiseState]] internal slot to "pending".
        // 5. Set promise's [[PromiseFulfillReactions]] internal slot to a new empty List.
        // 6. Set promise's [[PromiseRejectReactions]] internal slot to a new empty List.
        // 7. Set promise's [[PromiseIsHandled]] internal slot to false.
        // 8. Let resolvingFunctions be CreateResolvingFunctions(promise).
        InitializePromise(promise, &resolve, &reject, scriptContext);

        JavascriptExceptionObject* exception = nullptr;

        // 9. Let completion be Call(executor, undefined, << resolvingFunctions.[[Resolve]], resolvingFunctions.[[Reject]] >>).
        try
        {
            CALL_FUNCTION(executor, CallInfo(CallFlags_Value, 3),
                library->GetUndefined(),
                resolve,
                reject);
        }
        catch (const JavascriptException& err)
        {TRACE_IT(60563);
            exception = err.GetAndClear();
        }

        if (exception != nullptr)
        {
            // 10. If completion is an abrupt completion, then
            //    a. Perform ? Call(resolvingFunctions.[[Reject]], undefined, << completion.[[Value]] >>).
            TryRejectWithExceptionObject(exception, reject, scriptContext);
        }

        // 11. Return promise.
        return promise;
    }

    void JavascriptPromise::InitializePromise(JavascriptPromise* promise, JavascriptPromiseResolveOrRejectFunction** resolve, JavascriptPromiseResolveOrRejectFunction** reject, ScriptContext* scriptContext)
    {TRACE_IT(60564);
        Assert(promise->status == PromiseStatusCode_Undefined);
        Assert(resolve);
        Assert(reject);

        Recycler* recycler = scriptContext->GetRecycler();
        JavascriptLibrary* library = scriptContext->GetLibrary();

        promise->status = PromiseStatusCode_Unresolved;

        promise->resolveReactions = RecyclerNew(recycler, JavascriptPromiseReactionList, recycler);
        promise->rejectReactions = RecyclerNew(recycler, JavascriptPromiseReactionList, recycler);

        JavascriptPromiseResolveOrRejectFunctionAlreadyResolvedWrapper* alreadyResolvedRecord = RecyclerNewStructZ(scriptContext->GetRecycler(), JavascriptPromiseResolveOrRejectFunctionAlreadyResolvedWrapper);
        alreadyResolvedRecord->alreadyResolved = false;

        *resolve = library->CreatePromiseResolveOrRejectFunction(EntryResolveOrRejectFunction, promise, false, alreadyResolvedRecord);
        *reject = library->CreatePromiseResolveOrRejectFunction(EntryResolveOrRejectFunction, promise, true, alreadyResolvedRecord);
    }

    bool JavascriptPromise::Is(Var aValue)
    {TRACE_IT(60565);
        return Js::JavascriptOperators::GetTypeId(aValue) == TypeIds_Promise;
    }

    JavascriptPromise* JavascriptPromise::FromVar(Js::Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptPromise'");

        return static_cast<JavascriptPromise *>(RecyclableObject::FromVar(aValue));
    }

    BOOL JavascriptPromise::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {TRACE_IT(60566);
        stringBuilder->AppendCppLiteral(_u("[...]"));

        return TRUE;
    }

    BOOL JavascriptPromise::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {TRACE_IT(60567);
        stringBuilder->AppendCppLiteral(_u("Promise"));
        return TRUE;
    }

    JavascriptPromiseReactionList* JavascriptPromise::GetResolveReactions()
    {TRACE_IT(60568);
        return this->resolveReactions;
    }

    JavascriptPromiseReactionList* JavascriptPromise::GetRejectReactions()
    {TRACE_IT(60569);
        return this->rejectReactions;
    }

    // Promise.all as described in ES 2015 Section 25.4.4.1
    Var JavascriptPromise::EntryAll(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);
        Assert(!(callInfo.Flags & CallFlags_New));

        ScriptContext* scriptContext = function->GetScriptContext();

        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("Promise.all"));

        // 1. Let C be the this value.
        Var constructor = args[0];

        // 2. If Type(C) is not Object, throw a TypeError exception.
        if (!JavascriptOperators::IsObject(constructor))
        {TRACE_IT(60570);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedObject, _u("Promise.all"));
        }

        JavascriptLibrary* library = scriptContext->GetLibrary();
        Var iterable;

        if (args.Info.Count > 1)
        {TRACE_IT(60571);
            iterable = args[1];
        }
        else
        {TRACE_IT(60572);
            iterable = library->GetUndefined();
        }

        // 3. Let promiseCapability be NewPromiseCapability(C).
        JavascriptPromiseCapability* promiseCapability = NewPromiseCapability(constructor, scriptContext);

        // We know that constructor is an object at this point - further, we even know that it is a constructor - because NewPromiseCapability
        // would throw otherwise. That means we can safely cast constructor into a RecyclableObject* now and avoid having to perform ToObject
        // as part of the Invoke operation performed inside the loop below.
        RecyclableObject* constructorObject = RecyclableObject::FromVar(constructor);

        uint32 index = 0;
        JavascriptArray* values = nullptr;

        // We can't use a simple counter for the remaining element count since each Promise.all Resolve Element Function needs to know how many
        // elements are remaining when it runs and needs to update that counter for all other functions created by this call to Promise.all.
        // We can't just use a static variable, either, since this element count is only used for the Promise.all Resolve Element Functions created
        // by this call to Promise.all.
        JavascriptPromiseAllResolveElementFunctionRemainingElementsWrapper* remainingElementsWrapper = RecyclerNewStructZ(scriptContext->GetRecycler(), JavascriptPromiseAllResolveElementFunctionRemainingElementsWrapper);
        remainingElementsWrapper->remainingElements = 1;

        JavascriptExceptionObject* exception = nullptr;
        try
        {TRACE_IT(60573);
            // 4. Let iterator be GetIterator(iterable).
            RecyclableObject* iterator = JavascriptOperators::GetIterator(iterable, scriptContext);
            values = library->CreateArray(0);

            JavascriptOperators::DoIteratorStepAndValue(iterator, scriptContext, [&](Var next)
            {
                Var resolveVar = JavascriptOperators::GetProperty(constructorObject, Js::PropertyIds::resolve, scriptContext);

                if (!JavascriptConversion::IsCallable(resolveVar))
                {TRACE_IT(60574);
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedFunction);
                }

                RecyclableObject* resolveFunc = RecyclableObject::FromVar(resolveVar);

                Var nextPromise = CALL_FUNCTION(resolveFunc, Js::CallInfo(CallFlags_Value, 2),
                    constructorObject,
                    next);

                JavascriptPromiseAllResolveElementFunction* resolveElement = library->CreatePromiseAllResolveElementFunction(EntryAllResolveElementFunction, index, values, promiseCapability, remainingElementsWrapper);

                remainingElementsWrapper->remainingElements++;

                RecyclableObject* nextPromiseObject;

                if (!JavascriptConversion::ToObject(nextPromise, scriptContext, &nextPromiseObject))
                {TRACE_IT(60575);
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedObject);
                }

                Var thenVar = JavascriptOperators::GetProperty(nextPromiseObject, Js::PropertyIds::then, scriptContext);

                if (!JavascriptConversion::IsCallable(thenVar))
                {TRACE_IT(60576);
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedFunction);
                }

                RecyclableObject* thenFunc = RecyclableObject::FromVar(thenVar);

                CALL_FUNCTION(thenFunc, Js::CallInfo(CallFlags_Value, 3),
                    nextPromiseObject,
                    resolveElement,
                    promiseCapability->GetReject());

                index++;
            });
        }
        catch (const JavascriptException& err)
        {TRACE_IT(60577);
            exception = err.GetAndClear();
        }

        if (exception != nullptr)
        {
            TryRejectWithExceptionObject(exception, promiseCapability->GetReject(), scriptContext);

            // We need to explicitly return here to make sure we don't resolve in case index == 0 here.
            // That would happen if GetIterator or IteratorValue throws an exception in the first iteration.
            return promiseCapability->GetPromise();
        }

        remainingElementsWrapper->remainingElements--;

        // We want this call to happen outside the try statement because if it throws, we aren't supposed to reject the promise.
        if (remainingElementsWrapper->remainingElements == 0)
        {TRACE_IT(60578);
            Assert(values != nullptr);

            TryCallResolveOrRejectHandler(promiseCapability->GetResolve(), values, scriptContext);
        }

        return promiseCapability->GetPromise();
    }

    // Promise.prototype.catch as defined in ES 2015 Section 25.4.5.1
    Var JavascriptPromise::EntryCatch(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);
        Assert(!(callInfo.Flags & CallFlags_New));

        ScriptContext* scriptContext = function->GetScriptContext();

        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("Promise.prototype.catch"));

        RecyclableObject* promise;

        if (!JavascriptConversion::ToObject(args[0], scriptContext, &promise))
        {TRACE_IT(60579);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedObject, _u("Promise.prototype.catch"));
        }

        Var funcVar = JavascriptOperators::GetProperty(promise, Js::PropertyIds::then, scriptContext);

        if (!JavascriptConversion::IsCallable(funcVar))
        {TRACE_IT(60580);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("Promise.prototype.catch"));
        }

        Var onRejected;
        RecyclableObject* undefinedVar = scriptContext->GetLibrary()->GetUndefined();

        if (args.Info.Count > 1)
        {TRACE_IT(60581);
            onRejected = args[1];
        }
        else
        {TRACE_IT(60582);
            onRejected = undefinedVar;
        }

        RecyclableObject* func = RecyclableObject::FromVar(funcVar);

        return CALL_FUNCTION(func, Js::CallInfo(CallFlags_Value, 3),
            promise,
            undefinedVar,
            onRejected);
    }

    // Promise.race as described in ES 2015 Section 25.4.4.3
    Var JavascriptPromise::EntryRace(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);
        Assert(!(callInfo.Flags & CallFlags_New));

        ScriptContext* scriptContext = function->GetScriptContext();

        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("Promise.race"));

        // 1. Let C be the this value.
        Var constructor = args[0];

        // 2. If Type(C) is not Object, throw a TypeError exception.
        if (!JavascriptOperators::IsObject(constructor))
        {TRACE_IT(60583);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedObject, _u("Promise.race"));
        }

        Var undefinedVar = scriptContext->GetLibrary()->GetUndefined();
        Var iterable;

        if (args.Info.Count > 1)
        {TRACE_IT(60584);
            iterable = args[1];
        }
        else
        {TRACE_IT(60585);
            iterable = undefinedVar;
        }

        // 3. Let promiseCapability be NewPromiseCapability(C).
        JavascriptPromiseCapability* promiseCapability = NewPromiseCapability(constructor, scriptContext);

        // We know that constructor is an object at this point - further, we even know that it is a constructor - because NewPromiseCapability
        // would throw otherwise. That means we can safely cast constructor into a RecyclableObject* now and avoid having to perform ToObject
        // as part of the Invoke operation performed inside the loop below.
        RecyclableObject* constructorObject = RecyclableObject::FromVar(constructor);
        JavascriptExceptionObject* exception = nullptr;

        try
        {TRACE_IT(60586);
            // 4. Let iterator be GetIterator(iterable).
            RecyclableObject* iterator = JavascriptOperators::GetIterator(iterable, scriptContext);

            JavascriptOperators::DoIteratorStepAndValue(iterator, scriptContext, [&](Var next)
            {
                Var resolveVar = JavascriptOperators::GetProperty(constructorObject, Js::PropertyIds::resolve, scriptContext);

                if (!JavascriptConversion::IsCallable(resolveVar))
                {TRACE_IT(60587);
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedFunction);
                }

                RecyclableObject* resolveFunc = RecyclableObject::FromVar(resolveVar);

                Var nextPromise = CALL_FUNCTION(resolveFunc, Js::CallInfo(CallFlags_Value, 2),
                    constructorObject,
                    next);

                RecyclableObject* nextPromiseObject;

                if (!JavascriptConversion::ToObject(nextPromise, scriptContext, &nextPromiseObject))
                {TRACE_IT(60588);
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedObject);
                }

                Var thenVar = JavascriptOperators::GetProperty(nextPromiseObject, Js::PropertyIds::then, scriptContext);

                if (!JavascriptConversion::IsCallable(thenVar))
                {TRACE_IT(60589);
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedFunction);
                }

                RecyclableObject* thenFunc = RecyclableObject::FromVar(thenVar);

                CALL_FUNCTION(thenFunc, Js::CallInfo(CallFlags_Value, 3),
                    nextPromiseObject,
                    promiseCapability->GetResolve(),
                    promiseCapability->GetReject());

            });
        }
        catch (const JavascriptException& err)
        {TRACE_IT(60590);
            exception = err.GetAndClear();
        }

        if (exception != nullptr)
        {
            TryRejectWithExceptionObject(exception, promiseCapability->GetReject(), scriptContext);
        }

        return promiseCapability->GetPromise();
    }

    // Promise.reject as described in ES 2015 Section 25.4.4.4
    Var JavascriptPromise::EntryReject(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);
        Assert(!(callInfo.Flags & CallFlags_New));

        ScriptContext* scriptContext = function->GetScriptContext();

        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("Promise.reject"));

        // 1. Let C be the this value.
        Var constructor = args[0];

        // 2. If Type(C) is not Object, throw a TypeError exception.
        if (!JavascriptOperators::IsObject(constructor))
        {TRACE_IT(60591);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedObject, _u("Promise.reject"));
        }

        Var r;

        if (args.Info.Count > 1)
        {TRACE_IT(60592);
            r = args[1];
        }
        else
        {TRACE_IT(60593);
            r = scriptContext->GetLibrary()->GetUndefined();
        }

        // 3. Let promiseCapability be NewPromiseCapability(C).
        // 4. Perform ? Call(promiseCapability.[[Reject]], undefined, << r >>).
        // 5. Return promiseCapability.[[Promise]].
        return CreateRejectedPromise(r, scriptContext, constructor);
    }

    // Promise.resolve as described in ES 2015 Section 25.4.4.5
    Var JavascriptPromise::EntryResolve(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);
        Assert(!(callInfo.Flags & CallFlags_New));

        ScriptContext* scriptContext = function->GetScriptContext();

        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("Promise.resolve"));

        Var x;

        // 1. Let C be the this value.
        Var constructor = args[0];

        // 2. If Type(C) is not Object, throw a TypeError exception.
        if (!JavascriptOperators::IsObject(constructor))
        {TRACE_IT(60594);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedObject, _u("Promise.resolve"));
        }

        if (args.Info.Count > 1)
        {TRACE_IT(60595);
            x = args[1];
        }
        else
        {TRACE_IT(60596);
            x = scriptContext->GetLibrary()->GetUndefined();
        }

        // 3. If IsPromise(x) is true,
        if (JavascriptPromise::Is(x))
        {TRACE_IT(60597);
            // a. Let xConstructor be Get(x, "constructor").
            Var xConstructor = JavascriptOperators::GetProperty((RecyclableObject*)x, PropertyIds::constructor, scriptContext);

            // b. If SameValue(xConstructor, C) is true, return x.
            if (JavascriptConversion::SameValue(xConstructor, constructor))
            {TRACE_IT(60598);
                return x;
            }
        }

        // 4. Let promiseCapability be NewPromiseCapability(C).
        // 5. Perform ? Call(promiseCapability.[[Resolve]], undefined, << x >>).
        // 6. Return promiseCapability.[[Promise]].
        return CreateResolvedPromise(x, scriptContext, constructor);
    }

    // Promise.prototype.then as described in ES 2015 Section 25.4.5.3
    Var JavascriptPromise::EntryThen(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);
        Assert(!(callInfo.Flags & CallFlags_New));

        ScriptContext* scriptContext = function->GetScriptContext();

        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("Promise.prototype.then"));

        if (args.Info.Count < 1 || !JavascriptPromise::Is(args[0]))
        {TRACE_IT(60599);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedPromise, _u("Promise.prototype.then"));
        }

        JavascriptLibrary* library = scriptContext->GetLibrary();
        JavascriptPromise* promise = JavascriptPromise::FromVar(args[0]);
        RecyclableObject* rejectionHandler;
        RecyclableObject* fulfillmentHandler;

        if (args.Info.Count > 1 && JavascriptConversion::IsCallable(args[1]))
        {TRACE_IT(60600);
            fulfillmentHandler = RecyclableObject::FromVar(args[1]);
        }
        else
        {TRACE_IT(60601);
            fulfillmentHandler = library->GetIdentityFunction();
        }

        if (args.Info.Count > 2 && JavascriptConversion::IsCallable(args[2]))
        {TRACE_IT(60602);
            rejectionHandler = RecyclableObject::FromVar(args[2]);
        }
        else
        {TRACE_IT(60603);
            rejectionHandler = library->GetThrowerFunction();
        }

        return CreateThenPromise(promise, fulfillmentHandler, rejectionHandler, scriptContext);
    }

    // Promise Reject and Resolve Functions as described in ES 2015 Section 25.4.1.4.1 and 25.4.1.4.2
    Var JavascriptPromise::EntryResolveOrRejectFunction(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);
        Assert(!(callInfo.Flags & CallFlags_New));

        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();
        Var undefinedVar = library->GetUndefined();
        Var resolution;

        if (args.Info.Count > 1)
        {TRACE_IT(60604);
            resolution = args[1];
        }
        else
        {TRACE_IT(60605);
            resolution = undefinedVar;
        }

        JavascriptPromiseResolveOrRejectFunction* resolveOrRejectFunction = JavascriptPromiseResolveOrRejectFunction::FromVar(function);

        if (resolveOrRejectFunction->IsAlreadyResolved())
        {TRACE_IT(60606);
            return undefinedVar;
        }

        resolveOrRejectFunction->SetAlreadyResolved(true);

        bool rejecting = resolveOrRejectFunction->IsRejectFunction();

        JavascriptPromise* promise = resolveOrRejectFunction->GetPromise();

        return promise->ResolveHelper(resolution, rejecting, scriptContext);
    }

    Var JavascriptPromise::Resolve(Var resolution, ScriptContext* scriptContext)
    {TRACE_IT(60607);
        return this->ResolveHelper(resolution, false, scriptContext);
    }

    Var JavascriptPromise::Reject(Var resolution, ScriptContext* scriptContext)
    {TRACE_IT(60608);
        return this->ResolveHelper(resolution, true, scriptContext);
    }

    Var JavascriptPromise::ResolveHelper(Var resolution, bool isRejecting, ScriptContext* scriptContext)
    {TRACE_IT(60609);
        JavascriptLibrary* library = scriptContext->GetLibrary();
        Var undefinedVar = library->GetUndefined();

        // We only need to check SameValue and check for thenable resolution in the Resolve function case (not Reject)
        if (!isRejecting)
        {TRACE_IT(60610);
            if (JavascriptConversion::SameValue(resolution, this))
            {TRACE_IT(60611);
                JavascriptError* selfResolutionError = scriptContext->GetLibrary()->CreateTypeError();
                JavascriptError::SetErrorMessage(selfResolutionError, JSERR_PromiseSelfResolution, _u(""), scriptContext);

                resolution = selfResolutionError;
                isRejecting = true;
            }
            else if (RecyclableObject::Is(resolution))
            {TRACE_IT(60612);
                try
                {TRACE_IT(60613);
                    RecyclableObject* thenable = RecyclableObject::FromVar(resolution);
                    Var then = JavascriptOperators::GetProperty(thenable, Js::PropertyIds::then, scriptContext);

                    if (JavascriptConversion::IsCallable(then))
                    {TRACE_IT(60614);
                        JavascriptPromiseResolveThenableTaskFunction* resolveThenableTaskFunction = library->CreatePromiseResolveThenableTaskFunction(EntryResolveThenableTaskFunction, this, thenable, RecyclableObject::FromVar(then));

                        library->EnqueueTask(resolveThenableTaskFunction);

                        return undefinedVar;
                    }
                }
                catch (const JavascriptException& err)
                {TRACE_IT(60615);
                    resolution = err.GetAndClear()->GetThrownObject(scriptContext);

                    if (resolution == nullptr)
                    {TRACE_IT(60616);
                        resolution = undefinedVar;
                    }

                    isRejecting = true;
                }
            }
        }

        JavascriptPromiseReactionList* reactions;
        PromiseStatus newStatus;

        // Need to check rejecting state again as it might have changed due to failures
        if (isRejecting)
        {TRACE_IT(60617);
            reactions = this->GetRejectReactions();
            newStatus = PromiseStatusCode_HasRejection;
        }
        else
        {TRACE_IT(60618);
            reactions = this->GetResolveReactions();
            newStatus = PromiseStatusCode_HasResolution;
        }

        Assert(resolution != nullptr);

        this->result = resolution;
        this->resolveReactions = nullptr;
        this->rejectReactions = nullptr;
        this->status = newStatus;

        return TriggerPromiseReactions(reactions, resolution, scriptContext);
    }

    // Promise Capabilities Executor Function as described in ES 2015 Section 25.4.1.6.2
    Var JavascriptPromise::EntryCapabilitiesExecutorFunction(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);
        Assert(!(callInfo.Flags & CallFlags_New));

        ScriptContext* scriptContext = function->GetScriptContext();
        Var undefinedVar = scriptContext->GetLibrary()->GetUndefined();
        Var resolve = undefinedVar;
        Var reject = undefinedVar;

        if (args.Info.Count > 1)
        {TRACE_IT(60619);
            resolve = args[1];

            if (args.Info.Count > 2)
            {TRACE_IT(60620);
                reject = args[2];
            }
        }

        JavascriptPromiseCapabilitiesExecutorFunction* capabilitiesExecutorFunction = JavascriptPromiseCapabilitiesExecutorFunction::FromVar(function);
        JavascriptPromiseCapability* promiseCapability = capabilitiesExecutorFunction->GetCapability();

        if (!JavascriptOperators::IsUndefined(promiseCapability->GetResolve()) || !JavascriptOperators::IsUndefined(promiseCapability->GetReject()))
        {TRACE_IT(60621);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_UnexpectedMetadataFailure, _u("Promise"));
        }

        promiseCapability->SetResolve(resolve);
        promiseCapability->SetReject(reject);

        return undefinedVar;
    }

    // Promise Reaction Task Function as described in ES 2015 Section 25.4.2.1
    Var JavascriptPromise::EntryReactionTaskFunction(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);
        Assert(!(callInfo.Flags & CallFlags_New));

        ScriptContext* scriptContext = function->GetScriptContext();
        Var undefinedVar = scriptContext->GetLibrary()->GetUndefined();

        JavascriptPromiseReactionTaskFunction* reactionTaskFunction = JavascriptPromiseReactionTaskFunction::FromVar(function);
        JavascriptPromiseReaction* reaction = reactionTaskFunction->GetReaction();
        Var argument = reactionTaskFunction->GetArgument();
        JavascriptPromiseCapability* promiseCapability = reaction->GetCapabilities();
        RecyclableObject* handler = reaction->GetHandler();
        Var handlerResult = nullptr;
        JavascriptExceptionObject* exception = nullptr;

        {
            Js::JavascriptExceptionOperators::AutoCatchHandlerExists autoCatchHandlerExists(scriptContext);
            try
            {TRACE_IT(60622);
                handlerResult = CALL_FUNCTION(handler, Js::CallInfo(Js::CallFlags::CallFlags_Value, 2),
                    undefinedVar,
                    argument);
            }
            catch (const JavascriptException& err)
            {TRACE_IT(60623);
                exception = err.GetAndClear();
            }
        }

        if (exception != nullptr)
        {TRACE_IT(60624);
            return TryRejectWithExceptionObject(exception, promiseCapability->GetReject(), scriptContext);
        }

        Assert(handlerResult != nullptr);

        return TryCallResolveOrRejectHandler(promiseCapability->GetResolve(), handlerResult, scriptContext);
    }

    Var JavascriptPromise::TryCallResolveOrRejectHandler(Var handler, Var value, ScriptContext* scriptContext)
    {TRACE_IT(60625);
        Var undefinedVar = scriptContext->GetLibrary()->GetUndefined();

        if (!JavascriptConversion::IsCallable(handler))
        {TRACE_IT(60626);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedFunction);
        }

        RecyclableObject* handlerFunc = RecyclableObject::FromVar(handler);

        return CALL_FUNCTION(handlerFunc, CallInfo(CallFlags_Value, 2),
            undefinedVar,
            value);
    }

    Var JavascriptPromise::TryRejectWithExceptionObject(JavascriptExceptionObject* exceptionObject, Var handler, ScriptContext* scriptContext)
    {TRACE_IT(60627);
        Var thrownObject = exceptionObject->GetThrownObject(scriptContext);

        if (thrownObject == nullptr)
        {TRACE_IT(60628);
            thrownObject = scriptContext->GetLibrary()->GetUndefined();
        }

        return TryCallResolveOrRejectHandler(handler, thrownObject, scriptContext);
    }

    Var JavascriptPromise::CreateRejectedPromise(Var resolution, ScriptContext* scriptContext, Var promiseConstructor)
    {TRACE_IT(60629);
        if (promiseConstructor == nullptr)
        {TRACE_IT(60630);
            promiseConstructor = scriptContext->GetLibrary()->GetPromiseConstructor();
        }

        JavascriptPromiseCapability* promiseCapability = NewPromiseCapability(promiseConstructor, scriptContext);

        TryCallResolveOrRejectHandler(promiseCapability->GetReject(), resolution, scriptContext);

        return promiseCapability->GetPromise();
    }

    Var JavascriptPromise::CreateResolvedPromise(Var resolution, ScriptContext* scriptContext, Var promiseConstructor)
    {TRACE_IT(60631);
        if (promiseConstructor == nullptr)
        {TRACE_IT(60632);
            promiseConstructor = scriptContext->GetLibrary()->GetPromiseConstructor();
        }

        JavascriptPromiseCapability* promiseCapability = NewPromiseCapability(promiseConstructor, scriptContext);

        TryCallResolveOrRejectHandler(promiseCapability->GetResolve(), resolution, scriptContext);

        return promiseCapability->GetPromise();
    }

    Var JavascriptPromise::CreatePassThroughPromise(JavascriptPromise* sourcePromise, ScriptContext* scriptContext)
    {TRACE_IT(60633);
        JavascriptLibrary* library = scriptContext->GetLibrary();

        return CreateThenPromise(sourcePromise, library->GetIdentityFunction(), library->GetThrowerFunction(), scriptContext);
    }

    Var JavascriptPromise::CreateThenPromise(JavascriptPromise* sourcePromise, RecyclableObject* fulfillmentHandler, RecyclableObject* rejectionHandler, ScriptContext* scriptContext)
    {TRACE_IT(60634);
        Var constructor = JavascriptOperators::SpeciesConstructor(sourcePromise, scriptContext->GetLibrary()->GetPromiseConstructor(), scriptContext);
        JavascriptPromiseCapability* promiseCapability = NewPromiseCapability(constructor, scriptContext);

        JavascriptPromiseReaction* resolveReaction = JavascriptPromiseReaction::New(promiseCapability, fulfillmentHandler, scriptContext);
        JavascriptPromiseReaction* rejectReaction = JavascriptPromiseReaction::New(promiseCapability, rejectionHandler, scriptContext);

        switch (sourcePromise->status)
        {
        case PromiseStatusCode_Unresolved:
            sourcePromise->resolveReactions->Add(resolveReaction);
            sourcePromise->rejectReactions->Add(rejectReaction);
            break;
        case PromiseStatusCode_HasResolution:
            EnqueuePromiseReactionTask(resolveReaction, sourcePromise->result, scriptContext);
            break;
        case PromiseStatusCode_HasRejection:
            EnqueuePromiseReactionTask(rejectReaction, sourcePromise->result, scriptContext);
            break;
        default:
            AssertMsg(false, "Promise status is in an invalid state");
            break;
        }

        return promiseCapability->GetPromise();
    }

    // Promise Resolve Thenable Job as described in ES 2015 Section 25.4.2.2
    Var JavascriptPromise::EntryResolveThenableTaskFunction(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);
        Assert(!(callInfo.Flags & CallFlags_New));

        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();

        JavascriptPromiseResolveThenableTaskFunction* resolveThenableTaskFunction = JavascriptPromiseResolveThenableTaskFunction::FromVar(function);
        JavascriptPromise* promise = resolveThenableTaskFunction->GetPromise();
        RecyclableObject* thenable = resolveThenableTaskFunction->GetThenable();
        RecyclableObject* thenFunction = resolveThenableTaskFunction->GetThenFunction();

        JavascriptPromiseResolveOrRejectFunctionAlreadyResolvedWrapper* alreadyResolvedRecord = RecyclerNewStructZ(scriptContext->GetRecycler(), JavascriptPromiseResolveOrRejectFunctionAlreadyResolvedWrapper);
        alreadyResolvedRecord->alreadyResolved = false;

        JavascriptPromiseResolveOrRejectFunction* resolve = library->CreatePromiseResolveOrRejectFunction(EntryResolveOrRejectFunction, promise, false, alreadyResolvedRecord);
        JavascriptPromiseResolveOrRejectFunction* reject = library->CreatePromiseResolveOrRejectFunction(EntryResolveOrRejectFunction, promise, true, alreadyResolvedRecord);
        JavascriptExceptionObject* exception = nullptr;

        {
            Js::JavascriptExceptionOperators::AutoCatchHandlerExists autoCatchHandlerExists(scriptContext);
            try
            {TRACE_IT(60635);
                return CALL_FUNCTION(thenFunction, Js::CallInfo(Js::CallFlags::CallFlags_Value, 3),
                    thenable,
                    resolve,
                    reject);
            }
            catch (const JavascriptException& err)
            {TRACE_IT(60636);
                exception = err.GetAndClear();
            }
        }

        Assert(exception != nullptr);

        return TryRejectWithExceptionObject(exception, reject, scriptContext);
    }

    // Promise Identity Function as described in ES 2015Section 25.4.5.3.1
    Var JavascriptPromise::EntryIdentityFunction(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);
        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count > 1)
        {TRACE_IT(60637);
            Assert(args[1] != nullptr);

        return args[1];
    }
        else
        {TRACE_IT(60638);
            return function->GetScriptContext()->GetLibrary()->GetUndefined();
        }
    }

    // Promise Thrower Function as described in ES 2015Section 25.4.5.3.3
    Var JavascriptPromise::EntryThrowerFunction(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);
        Assert(!(callInfo.Flags & CallFlags_New));

        ScriptContext* scriptContext = function->GetScriptContext();
        Var arg;

        if (args.Info.Count > 1)
        {TRACE_IT(60639);
            Assert(args[1] != nullptr);

            arg = args[1];
        }
        else
        {TRACE_IT(60640);
            arg = scriptContext->GetLibrary()->GetUndefined();
        }

        JavascriptExceptionOperators::Throw(arg, scriptContext);
    }

    // Promise.all Resolve Element Function as described in ES6.0 (Release Candidate 3) Section 25.4.4.1.2
    Var JavascriptPromise::EntryAllResolveElementFunction(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);
        Assert(!(callInfo.Flags & CallFlags_New));

        ScriptContext* scriptContext = function->GetScriptContext();
        Var undefinedVar = scriptContext->GetLibrary()->GetUndefined();
        Var x;

        if (args.Info.Count > 1)
        {TRACE_IT(60641);
            x = args[1];
        }
        else
        {TRACE_IT(60642);
            x = undefinedVar;
        }

        JavascriptPromiseAllResolveElementFunction* allResolveElementFunction = JavascriptPromiseAllResolveElementFunction::FromVar(function);

        if (allResolveElementFunction->IsAlreadyCalled())
        {TRACE_IT(60643);
            return undefinedVar;
        }

        allResolveElementFunction->SetAlreadyCalled(true);

        uint32 index = allResolveElementFunction->GetIndex();
        JavascriptArray* values = allResolveElementFunction->GetValues();
        JavascriptPromiseCapability* promiseCapability = allResolveElementFunction->GetCapabilities();
        JavascriptExceptionObject* exception = nullptr;

        try
        {TRACE_IT(60644);
            values->SetItem(index, x, PropertyOperation_None);
        }
        catch (const JavascriptException& err)
        {TRACE_IT(60645);
            exception = err.GetAndClear();
        }

        if (exception != nullptr)
        {TRACE_IT(60646);
            return TryRejectWithExceptionObject(exception, promiseCapability->GetReject(), scriptContext);
        }

        if (allResolveElementFunction->DecrementRemainingElements() == 0)
        {TRACE_IT(60647);
            return TryCallResolveOrRejectHandler(promiseCapability->GetResolve(), values, scriptContext);
        }

        return undefinedVar;
    }

    Var JavascriptPromise::EntryJavascriptPromiseAsyncSpawnExecutorFunction(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);

        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();
        Var undefinedVar = library->GetUndefined();
        Var resolve = undefinedVar;
        Var reject = undefinedVar;

        Assert(args.Info.Count == 3);

        resolve = args[1];
        reject = args[2];

        Assert(JavascriptPromiseAsyncSpawnExecutorFunction::Is(function));
        JavascriptPromiseAsyncSpawnExecutorFunction* asyncSpawnExecutorFunction = JavascriptPromiseAsyncSpawnExecutorFunction::FromVar(function);
        Var self = asyncSpawnExecutorFunction->GetTarget();

        Var varCallArgs[] = { undefinedVar, self };
        JavascriptGenerator* gen = asyncSpawnExecutorFunction->GetGenerator();
        JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction* nextFunction = library->CreatePromiseAsyncSpawnStepArgumentExecutorFunction(EntryJavascriptPromiseAsyncSpawnStepNextExecutorFunction, gen, varCallArgs);

        Assert(JavascriptFunction::Is(resolve) && JavascriptFunction::Is(reject));
        AsyncSpawnStep(nextFunction, gen, JavascriptFunction::FromVar(resolve), JavascriptFunction::FromVar(reject));

        return undefinedVar;
    }

    Var JavascriptPromise::EntryJavascriptPromiseAsyncSpawnStepNextExecutorFunction(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction* asyncSpawnStepArgumentExecutorFunction = JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction::FromVar(function);
        Var argument = asyncSpawnStepArgumentExecutorFunction->GetArgument();

        JavascriptFunction* next = function->GetScriptContext()->GetLibrary()->EnsureGeneratorNextFunction();
        return CALL_FUNCTION(next, CallInfo(CallFlags_Value, 2), asyncSpawnStepArgumentExecutorFunction->GetGenerator(), argument);
    }

    Var JavascriptPromise::EntryJavascriptPromiseAsyncSpawnStepThrowExecutorFunction(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction* asyncSpawnStepArgumentExecutorFunction = JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction::FromVar(function);
        JavascriptFunction* throw_ = function->GetScriptContext()->GetLibrary()->EnsureGeneratorThrowFunction();
        return CALL_FUNCTION(throw_, CallInfo(CallFlags_Value, 2), asyncSpawnStepArgumentExecutorFunction->GetGenerator(), asyncSpawnStepArgumentExecutorFunction->GetArgument());
    }

    Var JavascriptPromise::EntryJavascriptPromiseAsyncSpawnCallStepExecutorFunction(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);

        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();
        Var undefinedVar = library->GetUndefined();

        Var argument = undefinedVar;

        if (args.Info.Count > 1)
        {TRACE_IT(60648);
            argument = args[1];
        }

        JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction* asyncSpawnStepExecutorFunction = JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction::FromVar(function);
        JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction* functionArg;
        JavascriptGenerator* gen = asyncSpawnStepExecutorFunction->GetGenerator();
        JavascriptFunction* reject = asyncSpawnStepExecutorFunction->GetReject();
        JavascriptFunction* resolve = asyncSpawnStepExecutorFunction->GetResolve();

        if (asyncSpawnStepExecutorFunction->GetIsReject())
        {TRACE_IT(60649);
            functionArg = library->CreatePromiseAsyncSpawnStepArgumentExecutorFunction(EntryJavascriptPromiseAsyncSpawnStepThrowExecutorFunction, gen, argument, NULL, NULL, false);
        }
        else
        {TRACE_IT(60650);
            functionArg = library->CreatePromiseAsyncSpawnStepArgumentExecutorFunction(EntryJavascriptPromiseAsyncSpawnStepNextExecutorFunction, gen, argument, NULL, NULL, false);
        }

        AsyncSpawnStep(functionArg, gen, resolve, reject);

        return undefinedVar;
    }

    void JavascriptPromise::AsyncSpawnStep(JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction* nextFunction, JavascriptGenerator* gen, JavascriptFunction* resolve, JavascriptFunction* reject)
    {TRACE_IT(60651);
        ScriptContext* scriptContext = resolve->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();
        Var undefinedVar = library->GetUndefined();

        JavascriptExceptionObject* exception = nullptr;
        Var value = nullptr;
        RecyclableObject* next = nullptr;
        bool done;

        try
        {TRACE_IT(60652);
            next = RecyclableObject::FromVar(CALL_FUNCTION(nextFunction, CallInfo(CallFlags_Value, 1), undefinedVar));
        }
        catch (const JavascriptException& err)
        {TRACE_IT(60653);
            exception = err.GetAndClear();
        }

        if (exception != nullptr)
        {
            // finished with failure, reject the promise
            TryRejectWithExceptionObject(exception, reject, scriptContext);
            return;
        }

        Assert(next != nullptr);
        done = JavascriptConversion::ToBool(JavascriptOperators::GetProperty(next, PropertyIds::done, scriptContext), scriptContext);
        if (done)
        {TRACE_IT(60654);
            // finished with success, resolve the promise
            value = JavascriptOperators::GetProperty(next, PropertyIds::value, scriptContext);
            CALL_FUNCTION(resolve, CallInfo(CallFlags_Value, 2), undefinedVar, value);
            return;
        }

        // not finished, chain off the yielded promise and `step` again
        JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction* successFunction = library->CreatePromiseAsyncSpawnStepArgumentExecutorFunction(EntryJavascriptPromiseAsyncSpawnCallStepExecutorFunction, gen, undefinedVar, resolve, reject);
        JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction* failFunction = library->CreatePromiseAsyncSpawnStepArgumentExecutorFunction(EntryJavascriptPromiseAsyncSpawnCallStepExecutorFunction, gen, undefinedVar, resolve, reject, true);

        JavascriptFunction* promiseResolve = library->EnsurePromiseResolveFunction();
        value = JavascriptOperators::GetProperty(next, PropertyIds::value, scriptContext);
        JavascriptPromise* promise = FromVar(CALL_FUNCTION(promiseResolve, CallInfo(CallFlags_Value, 2), library->GetPromiseConstructor(), value));

        JavascriptFunction* promiseThen = JavascriptFunction::FromVar(JavascriptOperators::GetProperty(promise, PropertyIds::then, scriptContext));
        CALL_FUNCTION(promiseThen, CallInfo(CallFlags_Value, 2), promise, successFunction);

        JavascriptFunction* promiseCatch = JavascriptFunction::FromVar(JavascriptOperators::GetProperty(promise, PropertyIds::catch_, scriptContext));
        CALL_FUNCTION(promiseCatch, CallInfo(CallFlags_Value, 2), promise, failFunction);
    }

#if ENABLE_TTD
    void JavascriptPromise::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {TRACE_IT(60655);
        if(this->result != nullptr)
        {TRACE_IT(60656);
            extractor->MarkVisitVar(this->result);
        }

        if(this->resolveReactions != nullptr)
        {TRACE_IT(60657);
            for(int32 i = 0; i < this->resolveReactions->Count(); ++i)
            {TRACE_IT(60658);
                this->resolveReactions->Item(i)->MarkVisitPtrs(extractor);
            }
        }

        if(this->rejectReactions != nullptr)
        {TRACE_IT(60659);
            for(int32 i = 0; i < this->rejectReactions->Count(); ++i)
            {TRACE_IT(60660);
                this->rejectReactions->Item(i)->MarkVisitPtrs(extractor);
            }
        }
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptPromise::GetSnapTag_TTD() const
    {TRACE_IT(60661);
        return TTD::NSSnapObjects::SnapObjectType::SnapPromiseObject;
    }

    void JavascriptPromise::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(60662);
        JsUtil::List<TTD_PTR_ID, HeapAllocator> depOnList(&HeapAllocator::Instance);

        TTD::NSSnapObjects::SnapPromiseInfo* spi = alloc.SlabAllocateStruct<TTD::NSSnapObjects::SnapPromiseInfo>();

        spi->Result = this->result;

        //Primitive kinds always inflated first so we only need to deal with complex kinds as depends on
        if(this->result != nullptr && TTD::JsSupport::IsVarComplexKind(this->result))
        {TRACE_IT(60663);
            depOnList.Add(TTD_CONVERT_VAR_TO_PTR_ID(this->result));
        }

        spi->Status = this->status;

        spi->ResolveReactionCount = (this->resolveReactions != nullptr) ? this->resolveReactions->Count() : 0;
        spi->ResolveReactions = nullptr;
        if(spi->ResolveReactionCount != 0)
        {TRACE_IT(60664);
            spi->ResolveReactions = alloc.SlabAllocateArray<TTD::NSSnapValues::SnapPromiseReactionInfo>(spi->ResolveReactionCount);

            for(uint32 i = 0; i < spi->ResolveReactionCount; ++i)
            {TRACE_IT(60665);
                this->resolveReactions->Item(i)->ExtractSnapPromiseReactionInto(spi->ResolveReactions + i, depOnList, alloc);
            }
        }

        spi->RejectReactionCount = (this->rejectReactions != nullptr) ? this->rejectReactions->Count() : 0;
        spi->RejectReactions = nullptr;
        if(spi->RejectReactionCount != 0)
        {TRACE_IT(60666);
            spi->RejectReactions = alloc.SlabAllocateArray<TTD::NSSnapValues::SnapPromiseReactionInfo>(spi->RejectReactionCount);

            for(uint32 i = 0; i < spi->RejectReactionCount; ++i)
            {TRACE_IT(60667);
                this->rejectReactions->Item(i)->ExtractSnapPromiseReactionInto(spi->RejectReactions+ i, depOnList, alloc);
            }
        }

        //see what we need to do wrt dependencies
        if(depOnList.Count() == 0)
        {TRACE_IT(60668);
            TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapPromiseInfo*, TTD::NSSnapObjects::SnapObjectType::SnapPromiseObject>(objData, spi);
        }
        else
        {TRACE_IT(60669);
            uint32 depOnCount = depOnList.Count();
            TTD_PTR_ID* depOnArray = alloc.SlabAllocateArray<TTD_PTR_ID>(depOnCount);

            for(uint32 i = 0; i < depOnCount; ++i)
            {TRACE_IT(60670);
                depOnArray[i] = depOnList.Item(i);
            }

            TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapPromiseInfo*, TTD::NSSnapObjects::SnapObjectType::SnapPromiseObject>(objData, spi, alloc, depOnCount, depOnArray);
        }
    }

    JavascriptPromise* JavascriptPromise::InitializePromise_TTD(ScriptContext* scriptContext, uint32 status, Var result, JsUtil::List<Js::JavascriptPromiseReaction*, HeapAllocator>& resolveReactions, JsUtil::List<Js::JavascriptPromiseReaction*, HeapAllocator>& rejectReactions)
    {TRACE_IT(60671);
        Recycler* recycler = scriptContext->GetRecycler();
        JavascriptLibrary* library = scriptContext->GetLibrary();

        JavascriptPromise* promise = library->CreatePromise();

        promise->status = (PromiseStatus)status;
        promise->result = result;

        promise->resolveReactions = RecyclerNew(recycler, JavascriptPromiseReactionList, recycler);
        promise->resolveReactions->Copy(&resolveReactions);

        promise->rejectReactions = RecyclerNew(recycler, JavascriptPromiseReactionList, recycler);
        promise->rejectReactions->Copy(&rejectReactions);

        return promise;
    }
#endif

    // NewPromiseCapability as described in ES6.0 (draft 29) Section 25.4.1.6
    JavascriptPromiseCapability* JavascriptPromise::NewPromiseCapability(Var constructor, ScriptContext* scriptContext)
    {TRACE_IT(60672);
        if (!JavascriptOperators::IsConstructor(constructor))
        {TRACE_IT(60673);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedFunction);
        }

        RecyclableObject* constructorFunc = RecyclableObject::FromVar(constructor);

        return CreatePromiseCapabilityRecord(constructorFunc, scriptContext);
    }

    // CreatePromiseCapabilityRecord as described in ES6.0 (draft 29) Section 25.4.1.6.1
    JavascriptPromiseCapability* JavascriptPromise::CreatePromiseCapabilityRecord(RecyclableObject* constructor, ScriptContext* scriptContext)
    {TRACE_IT(60674);
        JavascriptLibrary* library = scriptContext->GetLibrary();
        Var undefinedVar = library->GetUndefined();
        JavascriptPromiseCapability* promiseCapability = JavascriptPromiseCapability::New(undefinedVar, undefinedVar, undefinedVar, scriptContext);

        JavascriptPromiseCapabilitiesExecutorFunction* executor = library->CreatePromiseCapabilitiesExecutorFunction(EntryCapabilitiesExecutorFunction, promiseCapability);

        CallInfo callinfo = Js::CallInfo((Js::CallFlags)(Js::CallFlags::CallFlags_Value | Js::CallFlags::CallFlags_New), 2);
        Var argVars[] = { constructor, executor };
        Arguments args(callinfo, argVars);
        Var promise = JavascriptFunction::CallAsConstructor(constructor, nullptr, args, scriptContext);

        if (!JavascriptConversion::IsCallable(promiseCapability->GetResolve()) || !JavascriptConversion::IsCallable(promiseCapability->GetReject()))
        {TRACE_IT(60675);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedFunction, _u("Promise"));
        }

        promiseCapability->SetPromise(promise);

        return promiseCapability;
    }

    // TriggerPromiseReactions as defined in ES 2015 Section 25.4.1.7
    Var JavascriptPromise::TriggerPromiseReactions(JavascriptPromiseReactionList* reactions, Var resolution, ScriptContext* scriptContext)
    {TRACE_IT(60676);
        JavascriptLibrary* library = scriptContext->GetLibrary();

        if (reactions != nullptr)
        {TRACE_IT(60677);
            for (int i = 0; i < reactions->Count(); i++)
            {TRACE_IT(60678);
                JavascriptPromiseReaction* reaction = reactions->Item(i);

                EnqueuePromiseReactionTask(reaction, resolution, scriptContext);
            }
        }

        return library->GetUndefined();
    }

    void JavascriptPromise::EnqueuePromiseReactionTask(JavascriptPromiseReaction* reaction, Var resolution, ScriptContext* scriptContext)
    {TRACE_IT(60679);
        Assert(resolution != nullptr);

        JavascriptLibrary* library = scriptContext->GetLibrary();
        JavascriptPromiseReactionTaskFunction* reactionTaskFunction = library->CreatePromiseReactionTaskFunction(EntryReactionTaskFunction, reaction, resolution);

        library->EnqueueTask(reactionTaskFunction);
    }

    JavascriptPromiseResolveOrRejectFunction::JavascriptPromiseResolveOrRejectFunction(DynamicType* type)
        : RuntimeFunction(type, &Js::JavascriptPromise::EntryInfo::ResolveOrRejectFunction), promise(nullptr), isReject(false), alreadyResolvedWrapper(nullptr)
    {TRACE_IT(60680); }

    JavascriptPromiseResolveOrRejectFunction::JavascriptPromiseResolveOrRejectFunction(DynamicType* type, FunctionInfo* functionInfo, JavascriptPromise* promise, bool isReject, JavascriptPromiseResolveOrRejectFunctionAlreadyResolvedWrapper* alreadyResolvedRecord)
        : RuntimeFunction(type, functionInfo), promise(promise), isReject(isReject), alreadyResolvedWrapper(alreadyResolvedRecord)
    {TRACE_IT(60681); }

    bool JavascriptPromiseResolveOrRejectFunction::Is(Var var)
    {TRACE_IT(60682);
        if (JavascriptFunction::Is(var))
        {TRACE_IT(60683);
            JavascriptFunction* obj = JavascriptFunction::FromVar(var);

            return VirtualTableInfo<JavascriptPromiseResolveOrRejectFunction>::HasVirtualTable(obj)
                || VirtualTableInfo<CrossSiteObject<JavascriptPromiseResolveOrRejectFunction>>::HasVirtualTable(obj);
        }

        return false;
    }

    JavascriptPromiseResolveOrRejectFunction* JavascriptPromiseResolveOrRejectFunction::FromVar(Var var)
    {TRACE_IT(60684);
        Assert(JavascriptPromiseResolveOrRejectFunction::Is(var));

        return static_cast<JavascriptPromiseResolveOrRejectFunction*>(var);
    }

    JavascriptPromise* JavascriptPromiseResolveOrRejectFunction::GetPromise()
    {TRACE_IT(60685);
        return this->promise;
    }

    bool JavascriptPromiseResolveOrRejectFunction::IsRejectFunction()
    {TRACE_IT(60686);
        return this->isReject;
    }

    bool JavascriptPromiseResolveOrRejectFunction::IsAlreadyResolved()
    {TRACE_IT(60687);
        Assert(this->alreadyResolvedWrapper);

        return this->alreadyResolvedWrapper->alreadyResolved;
    }

    void JavascriptPromiseResolveOrRejectFunction::SetAlreadyResolved(bool is)
    {TRACE_IT(60688);
        Assert(this->alreadyResolvedWrapper);

        this->alreadyResolvedWrapper->alreadyResolved = is;
    }

#if ENABLE_TTD
    void JavascriptPromiseResolveOrRejectFunction::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {TRACE_IT(60689);
        TTDAssert(this->promise != nullptr, "Was not expecting that!!!");

        extractor->MarkVisitVar(this->promise);
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptPromiseResolveOrRejectFunction::GetSnapTag_TTD() const
    {TRACE_IT(60690);
        return TTD::NSSnapObjects::SnapObjectType::SnapPromiseResolveOrRejectFunctionObject;
    }

    void JavascriptPromiseResolveOrRejectFunction::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(60691);
        TTD::NSSnapObjects::SnapPromiseResolveOrRejectFunctionInfo* sprri = alloc.SlabAllocateStruct<TTD::NSSnapObjects::SnapPromiseResolveOrRejectFunctionInfo>();

        uint32 depOnCount = 1;
        TTD_PTR_ID* depOnArray = alloc.SlabAllocateArray<TTD_PTR_ID>(depOnCount);

        sprri->PromiseId = TTD_CONVERT_VAR_TO_PTR_ID(this->promise);
        depOnArray[0] = sprri->PromiseId;

        sprri->IsReject = this->isReject;

        sprri->AlreadyResolvedWrapperId = TTD_CONVERT_PROMISE_INFO_TO_PTR_ID(this->alreadyResolvedWrapper);
        sprri->AlreadyResolvedValue = this->alreadyResolvedWrapper->alreadyResolved;

        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapPromiseResolveOrRejectFunctionInfo*, TTD::NSSnapObjects::SnapObjectType::SnapPromiseResolveOrRejectFunctionObject>(objData, sprri, alloc, depOnCount, depOnArray);
    }
#endif

    JavascriptPromiseAsyncSpawnExecutorFunction::JavascriptPromiseAsyncSpawnExecutorFunction(DynamicType* type, FunctionInfo* functionInfo, JavascriptGenerator* generator, Var target)
        : RuntimeFunction(type, functionInfo), generator(generator), target(target)
    {TRACE_IT(60692); }

    bool JavascriptPromiseAsyncSpawnExecutorFunction::Is(Var var)
    {TRACE_IT(60693);
        if (JavascriptFunction::Is(var))
        {TRACE_IT(60694);
            JavascriptFunction* obj = JavascriptFunction::FromVar(var);

            return VirtualTableInfo<JavascriptPromiseAsyncSpawnExecutorFunction>::HasVirtualTable(obj)
                || VirtualTableInfo<CrossSiteObject<JavascriptPromiseAsyncSpawnExecutorFunction>>::HasVirtualTable(obj);
        }

        return false;
    }

    JavascriptPromiseAsyncSpawnExecutorFunction* JavascriptPromiseAsyncSpawnExecutorFunction::FromVar(Var var)
    {TRACE_IT(60695);
        Assert(JavascriptPromiseAsyncSpawnExecutorFunction::Is(var));

        return static_cast<JavascriptPromiseAsyncSpawnExecutorFunction*>(var);
    }

    JavascriptGenerator* JavascriptPromiseAsyncSpawnExecutorFunction::GetGenerator()
    {TRACE_IT(60696);
        return this->generator;
    }

    Var JavascriptPromiseAsyncSpawnExecutorFunction::GetTarget()
    {TRACE_IT(60697);
        return this->target;
    }

#if ENABLE_TTD
    void JavascriptPromiseAsyncSpawnExecutorFunction::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {
        TTDAssert(false, "Not Implemented Yet");
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptPromiseAsyncSpawnExecutorFunction::GetSnapTag_TTD() const
    {
        TTDAssert(false, "Not Implemented Yet");
        return TTD::NSSnapObjects::SnapObjectType::Invalid;
    }

    void JavascriptPromiseAsyncSpawnExecutorFunction::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {
        TTDAssert(false, "Not Implemented Yet");
    }
#endif

    JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction::JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction(DynamicType* type, FunctionInfo* functionInfo, JavascriptGenerator* generator, Var argument, JavascriptFunction* resolve, JavascriptFunction* reject, bool isReject)
        : RuntimeFunction(type, functionInfo), generator(generator), argument(argument), resolve(resolve), reject(reject), isReject(isReject)
    {TRACE_IT(60698); }

    bool JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction::Is(Var var)
    {TRACE_IT(60699);
        if (JavascriptFunction::Is(var))
        {TRACE_IT(60700);
            JavascriptFunction* obj = JavascriptFunction::FromVar(var);

            return VirtualTableInfo<JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction>::HasVirtualTable(obj)
                || VirtualTableInfo<CrossSiteObject<JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction>>::HasVirtualTable(obj);
        }

        return false;
    }

    JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction* JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction::FromVar(Var var)
    {TRACE_IT(60701);
        Assert(JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction::Is(var));

        return static_cast<JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction*>(var);
    }

    JavascriptGenerator* JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction::GetGenerator()
    {TRACE_IT(60702);
        return this->generator;
    }

    JavascriptFunction* JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction::GetResolve()
    {TRACE_IT(60703);
        return this->resolve;
    }

    JavascriptFunction* JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction::GetReject()
    {TRACE_IT(60704);
        return this->reject;
    }

    bool JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction::GetIsReject()
    {TRACE_IT(60705);
        return this->isReject;
    }

    Var JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction::GetArgument()
    {TRACE_IT(60706);
        return this->argument;
    }

#if ENABLE_TTD
    void JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {
        TTDAssert(false, "Not Implemented Yet");
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction::GetSnapTag_TTD() const
    {
        TTDAssert(false, "Not Implemented Yet");
        return TTD::NSSnapObjects::SnapObjectType::Invalid;
    }

    void JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {
        TTDAssert(false, "Not Implemented Yet");
    }
#endif

    JavascriptPromiseCapabilitiesExecutorFunction::JavascriptPromiseCapabilitiesExecutorFunction(DynamicType* type, FunctionInfo* functionInfo, JavascriptPromiseCapability* capability)
        : RuntimeFunction(type, functionInfo), capability(capability)
    {TRACE_IT(60707); }

    bool JavascriptPromiseCapabilitiesExecutorFunction::Is(Var var)
    {TRACE_IT(60708);
        if (JavascriptFunction::Is(var))
        {TRACE_IT(60709);
            JavascriptFunction* obj = JavascriptFunction::FromVar(var);

            return VirtualTableInfo<JavascriptPromiseCapabilitiesExecutorFunction>::HasVirtualTable(obj)
                || VirtualTableInfo<CrossSiteObject<JavascriptPromiseCapabilitiesExecutorFunction>>::HasVirtualTable(obj);
        }

        return false;
    }

    JavascriptPromiseCapabilitiesExecutorFunction* JavascriptPromiseCapabilitiesExecutorFunction::FromVar(Var var)
    {TRACE_IT(60710);
        Assert(JavascriptPromiseCapabilitiesExecutorFunction::Is(var));

        return static_cast<JavascriptPromiseCapabilitiesExecutorFunction*>(var);
    }

    JavascriptPromiseCapability* JavascriptPromiseCapabilitiesExecutorFunction::GetCapability()
    {TRACE_IT(60711);
        return this->capability;
    }

#if ENABLE_TTD
    void JavascriptPromiseCapabilitiesExecutorFunction::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {
        TTDAssert(false, "Not Implemented Yet");
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptPromiseCapabilitiesExecutorFunction::GetSnapTag_TTD() const
    {
        TTDAssert(false, "Not Implemented Yet");
        return TTD::NSSnapObjects::SnapObjectType::Invalid;
    }

    void JavascriptPromiseCapabilitiesExecutorFunction::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {
        TTDAssert(false, "Not Implemented Yet");
    }
#endif

    JavascriptPromiseCapability* JavascriptPromiseCapability::New(Var promise, Var resolve, Var reject, ScriptContext* scriptContext)
    {TRACE_IT(60712);
        return RecyclerNew(scriptContext->GetRecycler(), JavascriptPromiseCapability, promise, resolve, reject);
    }

    Var JavascriptPromiseCapability::GetResolve()
    {TRACE_IT(60713);
        return this->resolve;
    }

    Var JavascriptPromiseCapability::GetReject()
    {TRACE_IT(60714);
        return this->reject;
    }

    Var JavascriptPromiseCapability::GetPromise()
    {TRACE_IT(60715);
        return this->promise;
    }

    void JavascriptPromiseCapability::SetPromise(Var promise)
    {TRACE_IT(60716);
        this->promise = promise;
    }

    void JavascriptPromiseCapability::SetResolve(Var resolve)
    {TRACE_IT(60717);
        this->resolve = resolve;
    }

    void JavascriptPromiseCapability::SetReject(Var reject)
    {TRACE_IT(60718);
        this->reject = reject;
    }

#if ENABLE_TTD
    void JavascriptPromiseCapability::MarkVisitPtrs(TTD::SnapshotExtractor* extractor)
    {TRACE_IT(60719);
        TTDAssert(this->promise != nullptr && this->resolve != nullptr && this->reject != nullptr, "Seems odd, I was not expecting this!!!");

        extractor->MarkVisitVar(this->promise);

        extractor->MarkVisitVar(this->resolve);
        extractor->MarkVisitVar(this->reject);
    }

    void JavascriptPromiseCapability::ExtractSnapPromiseCapabilityInto(TTD::NSSnapValues::SnapPromiseCapabilityInfo* snapPromiseCapability, JsUtil::List<TTD_PTR_ID, HeapAllocator>& depOnList, TTD::SlabAllocator& alloc)
    {TRACE_IT(60720);
        snapPromiseCapability->CapabilityId = TTD_CONVERT_PROMISE_INFO_TO_PTR_ID(this);

        snapPromiseCapability->PromiseVar = this->promise;
        if(TTD::JsSupport::IsVarComplexKind(this->promise))
        {TRACE_IT(60721);
            depOnList.Add(TTD_CONVERT_VAR_TO_PTR_ID(this->resolve));
        }

        snapPromiseCapability->ResolveVar = this->resolve;
        if(TTD::JsSupport::IsVarComplexKind(this->resolve))
        {TRACE_IT(60722);
            depOnList.Add(TTD_CONVERT_VAR_TO_PTR_ID(this->resolve));
        }

        snapPromiseCapability->RejectVar = this->reject;
        if(TTD::JsSupport::IsVarComplexKind(this->reject))
        {TRACE_IT(60723);
            depOnList.Add(TTD_CONVERT_VAR_TO_PTR_ID(this->reject));
        }
    }
#endif

    JavascriptPromiseReaction* JavascriptPromiseReaction::New(JavascriptPromiseCapability* capabilities, RecyclableObject* handler, ScriptContext* scriptContext)
    {TRACE_IT(60724);
        return RecyclerNew(scriptContext->GetRecycler(), JavascriptPromiseReaction, capabilities, handler);
    }

    JavascriptPromiseCapability* JavascriptPromiseReaction::GetCapabilities()
    {TRACE_IT(60725);
        return this->capabilities;
    }

    RecyclableObject* JavascriptPromiseReaction::GetHandler()
    {TRACE_IT(60726);
        return this->handler;
    }

#if ENABLE_TTD
    void JavascriptPromiseReaction::MarkVisitPtrs(TTD::SnapshotExtractor* extractor)
    {TRACE_IT(60727);
        TTDAssert(this->handler != nullptr && this->capabilities != nullptr, "Seems odd, I was not expecting this!!!");

        extractor->MarkVisitVar(this->handler);

        this->capabilities->MarkVisitPtrs(extractor);
    }

    void JavascriptPromiseReaction::ExtractSnapPromiseReactionInto(TTD::NSSnapValues::SnapPromiseReactionInfo* snapPromiseReaction, JsUtil::List<TTD_PTR_ID, HeapAllocator>& depOnList, TTD::SlabAllocator& alloc)
    {TRACE_IT(60728);
        TTDAssert(this->handler != nullptr && this->capabilities != nullptr, "Seems odd, I was not expecting this!!!");

        snapPromiseReaction->PromiseReactionId = TTD_CONVERT_PROMISE_INFO_TO_PTR_ID(this);

        snapPromiseReaction->HandlerObjId = TTD_CONVERT_VAR_TO_PTR_ID(this->handler);
        depOnList.Add(snapPromiseReaction->HandlerObjId);

        this->capabilities->ExtractSnapPromiseCapabilityInto(&snapPromiseReaction->Capabilities, depOnList, alloc);
    }
#endif

    JavascriptPromiseReaction* JavascriptPromiseReactionTaskFunction::GetReaction()
    {TRACE_IT(60729);
        return this->reaction;
    }

    Var JavascriptPromiseReactionTaskFunction::GetArgument()
    {TRACE_IT(60730);
        return this->argument;
    }

#if ENABLE_TTD
    void JavascriptPromiseReactionTaskFunction::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {TRACE_IT(60731);
        TTDAssert(this->argument != nullptr && this->reaction != nullptr, "Was not expecting this!!!");

        extractor->MarkVisitVar(this->argument);

        this->reaction->MarkVisitPtrs(extractor);
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptPromiseReactionTaskFunction::GetSnapTag_TTD() const
    {TRACE_IT(60732);
        return TTD::NSSnapObjects::SnapObjectType::SnapPromiseReactionTaskFunctionObject;
    }

    void JavascriptPromiseReactionTaskFunction::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(60733);
        TTD::NSSnapObjects::SnapPromiseReactionTaskFunctionInfo* sprtfi = alloc.SlabAllocateStruct<TTD::NSSnapObjects::SnapPromiseReactionTaskFunctionInfo>();

        JsUtil::List<TTD_PTR_ID, HeapAllocator> depOnList(&HeapAllocator::Instance);

        sprtfi->Argument = this->argument;

        if(this->argument != nullptr && TTD::JsSupport::IsVarComplexKind(this->argument))
        {TRACE_IT(60734);
            depOnList.Add(TTD_CONVERT_VAR_TO_PTR_ID(this->argument));
        }

        this->reaction->ExtractSnapPromiseReactionInto(&sprtfi->Reaction, depOnList, alloc);

        //see what we need to do wrt dependencies
        if(depOnList.Count() == 0)
        {TRACE_IT(60735);
            TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapPromiseReactionTaskFunctionInfo*, TTD::NSSnapObjects::SnapObjectType::SnapPromiseReactionTaskFunctionObject>(objData, sprtfi);
        }
        else
        {TRACE_IT(60736);
            uint32 depOnCount = depOnList.Count();
            TTD_PTR_ID* depOnArray = alloc.SlabAllocateArray<TTD_PTR_ID>(depOnCount);

            for(uint32 i = 0; i < depOnCount; ++i)
            {TRACE_IT(60737);
                depOnArray[i] = depOnList.Item(i);
            }

            TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapPromiseReactionTaskFunctionInfo*, TTD::NSSnapObjects::SnapObjectType::SnapPromiseReactionTaskFunctionObject>(objData, sprtfi, alloc, depOnCount, depOnArray);
        }
    }
#endif

    JavascriptPromise* JavascriptPromiseResolveThenableTaskFunction::GetPromise()
    {TRACE_IT(60738);
        return this->promise;
    }

    RecyclableObject* JavascriptPromiseResolveThenableTaskFunction::GetThenable()
    {TRACE_IT(60739);
        return this->thenable;
    }

    RecyclableObject* JavascriptPromiseResolveThenableTaskFunction::GetThenFunction()
    {TRACE_IT(60740);
        return this->thenFunction;
    }

#if ENABLE_TTD
    void JavascriptPromiseResolveThenableTaskFunction::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {
        TTDAssert(false, "Not Implemented Yet");
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptPromiseResolveThenableTaskFunction::GetSnapTag_TTD() const
    {
        TTDAssert(false, "Not Implemented Yet");
        return TTD::NSSnapObjects::SnapObjectType::Invalid;
    }

    void JavascriptPromiseResolveThenableTaskFunction::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {
        TTDAssert(false, "Not Implemented Yet");
    }
#endif

    JavascriptPromiseAllResolveElementFunction::JavascriptPromiseAllResolveElementFunction(DynamicType* type)
        : RuntimeFunction(type, &Js::JavascriptPromise::EntryInfo::AllResolveElementFunction), index(0), values(nullptr), capabilities(nullptr), remainingElementsWrapper(nullptr), alreadyCalled(false)
    {TRACE_IT(60741); }

    JavascriptPromiseAllResolveElementFunction::JavascriptPromiseAllResolveElementFunction(DynamicType* type, FunctionInfo* functionInfo, uint32 index, JavascriptArray* values, JavascriptPromiseCapability* capabilities, JavascriptPromiseAllResolveElementFunctionRemainingElementsWrapper* remainingElementsWrapper)
        : RuntimeFunction(type, functionInfo), index(index), values(values), capabilities(capabilities), remainingElementsWrapper(remainingElementsWrapper), alreadyCalled(false)
    {TRACE_IT(60742); }

    bool JavascriptPromiseAllResolveElementFunction::Is(Var var)
    {TRACE_IT(60743);
        if (JavascriptFunction::Is(var))
        {TRACE_IT(60744);
            JavascriptFunction* obj = JavascriptFunction::FromVar(var);

            return VirtualTableInfo<JavascriptPromiseAllResolveElementFunction>::HasVirtualTable(obj)
                || VirtualTableInfo<CrossSiteObject<JavascriptPromiseAllResolveElementFunction>>::HasVirtualTable(obj);
        }

        return false;
    }

    JavascriptPromiseAllResolveElementFunction* JavascriptPromiseAllResolveElementFunction::FromVar(Var var)
    {TRACE_IT(60745);
        Assert(JavascriptPromiseAllResolveElementFunction::Is(var));

        return static_cast<JavascriptPromiseAllResolveElementFunction*>(var);
    }

    JavascriptPromiseCapability* JavascriptPromiseAllResolveElementFunction::GetCapabilities()
    {TRACE_IT(60746);
        return this->capabilities;
    }

    uint32 JavascriptPromiseAllResolveElementFunction::GetIndex()
    {TRACE_IT(60747);
        return this->index;
    }

    uint32 JavascriptPromiseAllResolveElementFunction::GetRemainingElements()
    {TRACE_IT(60748);
        return this->remainingElementsWrapper->remainingElements;
    }

    JavascriptArray* JavascriptPromiseAllResolveElementFunction::GetValues()
    {TRACE_IT(60749);
        return this->values;
    }

    uint32 JavascriptPromiseAllResolveElementFunction::DecrementRemainingElements()
    {TRACE_IT(60750);
        return --(this->remainingElementsWrapper->remainingElements);
    }

    bool JavascriptPromiseAllResolveElementFunction::IsAlreadyCalled() const
    {TRACE_IT(60751);
        return this->alreadyCalled;
    }

    void JavascriptPromiseAllResolveElementFunction::SetAlreadyCalled(const bool is)
    {TRACE_IT(60752);
        this->alreadyCalled = is;
    }

#if ENABLE_TTD
    void JavascriptPromiseAllResolveElementFunction::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {TRACE_IT(60753);
        TTDAssert(this->capabilities != nullptr && this->remainingElementsWrapper != nullptr && this->values != nullptr, "Don't think these can be null");

        this->capabilities->MarkVisitPtrs(extractor);
        extractor->MarkVisitVar(this->values);
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptPromiseAllResolveElementFunction::GetSnapTag_TTD() const
    {TRACE_IT(60754);
        return TTD::NSSnapObjects::SnapObjectType::SnapPromiseAllResolveElementFunctionObject;
    }

    void JavascriptPromiseAllResolveElementFunction::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(60755);
        TTD::NSSnapObjects::SnapPromiseAllResolveElementFunctionInfo* sprai = alloc.SlabAllocateStruct<TTD::NSSnapObjects::SnapPromiseAllResolveElementFunctionInfo>();

        JsUtil::List<TTD_PTR_ID, HeapAllocator> depOnList(&HeapAllocator::Instance);
        this->capabilities->ExtractSnapPromiseCapabilityInto(&sprai->Capabilities, depOnList, alloc);

        sprai->Index = this->index;
        sprai->RemainingElementsWrapperId = TTD_CONVERT_PROMISE_INFO_TO_PTR_ID(this->remainingElementsWrapper);
        sprai->RemainingElementsValue = this->remainingElementsWrapper->remainingElements;

        sprai->Values = TTD_CONVERT_VAR_TO_PTR_ID(this->values);
        depOnList.Add(sprai->Values);

        sprai->AlreadyCalled = this->alreadyCalled;

        uint32 depOnCount = depOnList.Count();
        TTD_PTR_ID* depOnArray = alloc.SlabAllocateArray<TTD_PTR_ID>(depOnCount);

        for(uint32 i = 0; i < depOnCount; ++i)
        {TRACE_IT(60756);
            depOnArray[i] = depOnList.Item(i);
        }

        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapPromiseAllResolveElementFunctionInfo*, TTD::NSSnapObjects::SnapObjectType::SnapPromiseAllResolveElementFunctionObject>(objData, sprai, alloc, depOnCount, depOnArray);
    }
#endif

    Var JavascriptPromise::EntryGetterSymbolSpecies(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ARGUMENTS(args, callInfo);

        Assert(args.Info.Count > 0);

        return args[0];
    }
} // namespace Js
