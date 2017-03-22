//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptPromise::JavascriptPromise(DynamicType * type)
        : DynamicObject(type)
    {LOGMEIN("JavascriptPromise.cpp] 10\n");
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
        {LOGMEIN("JavascriptPromise.cpp] 42\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_ClassConstructorCannotBeCalledWithoutNew, _u("Promise"));
        }

        // 2. If IsCallable(executor) is false, throw a TypeError exception.
        if (args.Info.Count < 2 || !JavascriptConversion::IsCallable(args[1]))
        {LOGMEIN("JavascriptPromise.cpp] 48\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("Promise"));
        }
        RecyclableObject* executor = RecyclableObject::FromVar(args[1]);

        // 3. Let promise be ? OrdinaryCreateFromConstructor(NewTarget, "%PromisePrototype%", <<[[PromiseState]], [[PromiseResult]], [[PromiseFulfillReactions]], [[PromiseRejectReactions]], [[PromiseIsHandled]] >>).
        JavascriptPromise* promise = library->CreatePromise();
        if (isCtorSuperCall)
        {LOGMEIN("JavascriptPromise.cpp] 56\n");
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
        {LOGMEIN("JavascriptPromise.cpp] 81\n");
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
    {LOGMEIN("JavascriptPromise.cpp] 97\n");
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
    {LOGMEIN("JavascriptPromise.cpp] 118\n");
        return Js::JavascriptOperators::GetTypeId(aValue) == TypeIds_Promise;
    }

    JavascriptPromise* JavascriptPromise::FromVar(Js::Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptPromise'");

        return static_cast<JavascriptPromise *>(RecyclableObject::FromVar(aValue));
    }

    BOOL JavascriptPromise::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("JavascriptPromise.cpp] 130\n");
        stringBuilder->AppendCppLiteral(_u("[...]"));

        return TRUE;
    }

    BOOL JavascriptPromise::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("JavascriptPromise.cpp] 137\n");
        stringBuilder->AppendCppLiteral(_u("Promise"));
        return TRUE;
    }

    JavascriptPromiseReactionList* JavascriptPromise::GetResolveReactions()
    {LOGMEIN("JavascriptPromise.cpp] 143\n");
        return this->resolveReactions;
    }

    JavascriptPromiseReactionList* JavascriptPromise::GetRejectReactions()
    {LOGMEIN("JavascriptPromise.cpp] 148\n");
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
        {LOGMEIN("JavascriptPromise.cpp] 168\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedObject, _u("Promise.all"));
        }

        JavascriptLibrary* library = scriptContext->GetLibrary();
        Var iterable;

        if (args.Info.Count > 1)
        {LOGMEIN("JavascriptPromise.cpp] 176\n");
            iterable = args[1];
        }
        else
        {
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
        {LOGMEIN("JavascriptPromise.cpp] 204\n");
            // 4. Let iterator be GetIterator(iterable).
            RecyclableObject* iterator = JavascriptOperators::GetIterator(iterable, scriptContext);
            values = library->CreateArray(0);

            JavascriptOperators::DoIteratorStepAndValue(iterator, scriptContext, [&](Var next)
            {
                Var resolveVar = JavascriptOperators::GetProperty(constructorObject, Js::PropertyIds::resolve, scriptContext);

                if (!JavascriptConversion::IsCallable(resolveVar))
                {LOGMEIN("JavascriptPromise.cpp] 214\n");
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
                {LOGMEIN("JavascriptPromise.cpp] 231\n");
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedObject);
                }

                Var thenVar = JavascriptOperators::GetProperty(nextPromiseObject, Js::PropertyIds::then, scriptContext);

                if (!JavascriptConversion::IsCallable(thenVar))
                {LOGMEIN("JavascriptPromise.cpp] 238\n");
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
        {LOGMEIN("JavascriptPromise.cpp] 253\n");
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
        {LOGMEIN("JavascriptPromise.cpp] 270\n");
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
        {LOGMEIN("JavascriptPromise.cpp] 293\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedObject, _u("Promise.prototype.catch"));
        }

        Var funcVar = JavascriptOperators::GetProperty(promise, Js::PropertyIds::then, scriptContext);

        if (!JavascriptConversion::IsCallable(funcVar))
        {LOGMEIN("JavascriptPromise.cpp] 300\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("Promise.prototype.catch"));
        }

        Var onRejected;
        RecyclableObject* undefinedVar = scriptContext->GetLibrary()->GetUndefined();

        if (args.Info.Count > 1)
        {LOGMEIN("JavascriptPromise.cpp] 308\n");
            onRejected = args[1];
        }
        else
        {
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
        {LOGMEIN("JavascriptPromise.cpp] 340\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedObject, _u("Promise.race"));
        }

        Var undefinedVar = scriptContext->GetLibrary()->GetUndefined();
        Var iterable;

        if (args.Info.Count > 1)
        {LOGMEIN("JavascriptPromise.cpp] 348\n");
            iterable = args[1];
        }
        else
        {
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
        {LOGMEIN("JavascriptPromise.cpp] 366\n");
            // 4. Let iterator be GetIterator(iterable).
            RecyclableObject* iterator = JavascriptOperators::GetIterator(iterable, scriptContext);

            JavascriptOperators::DoIteratorStepAndValue(iterator, scriptContext, [&](Var next)
            {
                Var resolveVar = JavascriptOperators::GetProperty(constructorObject, Js::PropertyIds::resolve, scriptContext);

                if (!JavascriptConversion::IsCallable(resolveVar))
                {LOGMEIN("JavascriptPromise.cpp] 375\n");
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedFunction);
                }

                RecyclableObject* resolveFunc = RecyclableObject::FromVar(resolveVar);

                Var nextPromise = CALL_FUNCTION(resolveFunc, Js::CallInfo(CallFlags_Value, 2),
                    constructorObject,
                    next);

                RecyclableObject* nextPromiseObject;

                if (!JavascriptConversion::ToObject(nextPromise, scriptContext, &nextPromiseObject))
                {LOGMEIN("JavascriptPromise.cpp] 388\n");
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedObject);
                }

                Var thenVar = JavascriptOperators::GetProperty(nextPromiseObject, Js::PropertyIds::then, scriptContext);

                if (!JavascriptConversion::IsCallable(thenVar))
                {LOGMEIN("JavascriptPromise.cpp] 395\n");
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
        {LOGMEIN("JavascriptPromise.cpp] 409\n");
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
        {LOGMEIN("JavascriptPromise.cpp] 437\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedObject, _u("Promise.reject"));
        }

        Var r;

        if (args.Info.Count > 1)
        {LOGMEIN("JavascriptPromise.cpp] 444\n");
            r = args[1];
        }
        else
        {
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
        {LOGMEIN("JavascriptPromise.cpp] 476\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedObject, _u("Promise.resolve"));
        }

        if (args.Info.Count > 1)
        {LOGMEIN("JavascriptPromise.cpp] 481\n");
            x = args[1];
        }
        else
        {
            x = scriptContext->GetLibrary()->GetUndefined();
        }

        // 3. If IsPromise(x) is true,
        if (JavascriptPromise::Is(x))
        {LOGMEIN("JavascriptPromise.cpp] 491\n");
            // a. Let xConstructor be Get(x, "constructor").
            Var xConstructor = JavascriptOperators::GetProperty((RecyclableObject*)x, PropertyIds::constructor, scriptContext);

            // b. If SameValue(xConstructor, C) is true, return x.
            if (JavascriptConversion::SameValue(xConstructor, constructor))
            {LOGMEIN("JavascriptPromise.cpp] 497\n");
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
        {LOGMEIN("JavascriptPromise.cpp] 520\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedPromise, _u("Promise.prototype.then"));
        }

        JavascriptLibrary* library = scriptContext->GetLibrary();
        JavascriptPromise* promise = JavascriptPromise::FromVar(args[0]);
        RecyclableObject* rejectionHandler;
        RecyclableObject* fulfillmentHandler;

        if (args.Info.Count > 1 && JavascriptConversion::IsCallable(args[1]))
        {LOGMEIN("JavascriptPromise.cpp] 530\n");
            fulfillmentHandler = RecyclableObject::FromVar(args[1]);
        }
        else
        {
            fulfillmentHandler = library->GetIdentityFunction();
        }

        if (args.Info.Count > 2 && JavascriptConversion::IsCallable(args[2]))
        {LOGMEIN("JavascriptPromise.cpp] 539\n");
            rejectionHandler = RecyclableObject::FromVar(args[2]);
        }
        else
        {
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
        {LOGMEIN("JavascriptPromise.cpp] 563\n");
            resolution = args[1];
        }
        else
        {
            resolution = undefinedVar;
        }

        JavascriptPromiseResolveOrRejectFunction* resolveOrRejectFunction = JavascriptPromiseResolveOrRejectFunction::FromVar(function);

        if (resolveOrRejectFunction->IsAlreadyResolved())
        {LOGMEIN("JavascriptPromise.cpp] 574\n");
            return undefinedVar;
        }

        resolveOrRejectFunction->SetAlreadyResolved(true);

        bool rejecting = resolveOrRejectFunction->IsRejectFunction();

        JavascriptPromise* promise = resolveOrRejectFunction->GetPromise();

        return promise->ResolveHelper(resolution, rejecting, scriptContext);
    }

    Var JavascriptPromise::Resolve(Var resolution, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptPromise.cpp] 588\n");
        return this->ResolveHelper(resolution, false, scriptContext);
    }

    Var JavascriptPromise::Reject(Var resolution, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptPromise.cpp] 593\n");
        return this->ResolveHelper(resolution, true, scriptContext);
    }

    Var JavascriptPromise::ResolveHelper(Var resolution, bool isRejecting, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptPromise.cpp] 598\n");
        JavascriptLibrary* library = scriptContext->GetLibrary();
        Var undefinedVar = library->GetUndefined();

        // We only need to check SameValue and check for thenable resolution in the Resolve function case (not Reject)
        if (!isRejecting)
        {LOGMEIN("JavascriptPromise.cpp] 604\n");
            if (JavascriptConversion::SameValue(resolution, this))
            {LOGMEIN("JavascriptPromise.cpp] 606\n");
                JavascriptError* selfResolutionError = scriptContext->GetLibrary()->CreateTypeError();
                JavascriptError::SetErrorMessage(selfResolutionError, JSERR_PromiseSelfResolution, _u(""), scriptContext);

                resolution = selfResolutionError;
                isRejecting = true;
            }
            else if (RecyclableObject::Is(resolution))
            {LOGMEIN("JavascriptPromise.cpp] 614\n");
                try
                {LOGMEIN("JavascriptPromise.cpp] 616\n");
                    RecyclableObject* thenable = RecyclableObject::FromVar(resolution);
                    Var then = JavascriptOperators::GetProperty(thenable, Js::PropertyIds::then, scriptContext);

                    if (JavascriptConversion::IsCallable(then))
                    {LOGMEIN("JavascriptPromise.cpp] 621\n");
                        JavascriptPromiseResolveThenableTaskFunction* resolveThenableTaskFunction = library->CreatePromiseResolveThenableTaskFunction(EntryResolveThenableTaskFunction, this, thenable, RecyclableObject::FromVar(then));

                        library->EnqueueTask(resolveThenableTaskFunction);

                        return undefinedVar;
                    }
                }
                catch (const JavascriptException& err)
                {LOGMEIN("JavascriptPromise.cpp] 630\n");
                    resolution = err.GetAndClear()->GetThrownObject(scriptContext);

                    if (resolution == nullptr)
                    {LOGMEIN("JavascriptPromise.cpp] 634\n");
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
        {LOGMEIN("JavascriptPromise.cpp] 648\n");
            reactions = this->GetRejectReactions();
            newStatus = PromiseStatusCode_HasRejection;
        }
        else
        {
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
        {LOGMEIN("JavascriptPromise.cpp] 681\n");
            resolve = args[1];

            if (args.Info.Count > 2)
            {LOGMEIN("JavascriptPromise.cpp] 685\n");
                reject = args[2];
            }
        }

        JavascriptPromiseCapabilitiesExecutorFunction* capabilitiesExecutorFunction = JavascriptPromiseCapabilitiesExecutorFunction::FromVar(function);
        JavascriptPromiseCapability* promiseCapability = capabilitiesExecutorFunction->GetCapability();

        if (!JavascriptOperators::IsUndefined(promiseCapability->GetResolve()) || !JavascriptOperators::IsUndefined(promiseCapability->GetReject()))
        {LOGMEIN("JavascriptPromise.cpp] 694\n");
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
            {LOGMEIN("JavascriptPromise.cpp] 725\n");
                handlerResult = CALL_FUNCTION(handler, Js::CallInfo(Js::CallFlags::CallFlags_Value, 2),
                    undefinedVar,
                    argument);
            }
            catch (const JavascriptException& err)
            {LOGMEIN("JavascriptPromise.cpp] 731\n");
                exception = err.GetAndClear();
            }
        }

        if (exception != nullptr)
        {LOGMEIN("JavascriptPromise.cpp] 737\n");
            return TryRejectWithExceptionObject(exception, promiseCapability->GetReject(), scriptContext);
        }

        Assert(handlerResult != nullptr);

        return TryCallResolveOrRejectHandler(promiseCapability->GetResolve(), handlerResult, scriptContext);
    }

    Var JavascriptPromise::TryCallResolveOrRejectHandler(Var handler, Var value, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptPromise.cpp] 747\n");
        Var undefinedVar = scriptContext->GetLibrary()->GetUndefined();

        if (!JavascriptConversion::IsCallable(handler))
        {LOGMEIN("JavascriptPromise.cpp] 751\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedFunction);
        }

        RecyclableObject* handlerFunc = RecyclableObject::FromVar(handler);

        return CALL_FUNCTION(handlerFunc, CallInfo(CallFlags_Value, 2),
            undefinedVar,
            value);
    }

    Var JavascriptPromise::TryRejectWithExceptionObject(JavascriptExceptionObject* exceptionObject, Var handler, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptPromise.cpp] 763\n");
        Var thrownObject = exceptionObject->GetThrownObject(scriptContext);

        if (thrownObject == nullptr)
        {LOGMEIN("JavascriptPromise.cpp] 767\n");
            thrownObject = scriptContext->GetLibrary()->GetUndefined();
        }

        return TryCallResolveOrRejectHandler(handler, thrownObject, scriptContext);
    }

    Var JavascriptPromise::CreateRejectedPromise(Var resolution, ScriptContext* scriptContext, Var promiseConstructor)
    {LOGMEIN("JavascriptPromise.cpp] 775\n");
        if (promiseConstructor == nullptr)
        {LOGMEIN("JavascriptPromise.cpp] 777\n");
            promiseConstructor = scriptContext->GetLibrary()->GetPromiseConstructor();
        }

        JavascriptPromiseCapability* promiseCapability = NewPromiseCapability(promiseConstructor, scriptContext);

        TryCallResolveOrRejectHandler(promiseCapability->GetReject(), resolution, scriptContext);

        return promiseCapability->GetPromise();
    }

    Var JavascriptPromise::CreateResolvedPromise(Var resolution, ScriptContext* scriptContext, Var promiseConstructor)
    {LOGMEIN("JavascriptPromise.cpp] 789\n");
        if (promiseConstructor == nullptr)
        {LOGMEIN("JavascriptPromise.cpp] 791\n");
            promiseConstructor = scriptContext->GetLibrary()->GetPromiseConstructor();
        }

        JavascriptPromiseCapability* promiseCapability = NewPromiseCapability(promiseConstructor, scriptContext);

        TryCallResolveOrRejectHandler(promiseCapability->GetResolve(), resolution, scriptContext);

        return promiseCapability->GetPromise();
    }

    Var JavascriptPromise::CreatePassThroughPromise(JavascriptPromise* sourcePromise, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptPromise.cpp] 803\n");
        JavascriptLibrary* library = scriptContext->GetLibrary();

        return CreateThenPromise(sourcePromise, library->GetIdentityFunction(), library->GetThrowerFunction(), scriptContext);
    }

    Var JavascriptPromise::CreateThenPromise(JavascriptPromise* sourcePromise, RecyclableObject* fulfillmentHandler, RecyclableObject* rejectionHandler, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptPromise.cpp] 810\n");
        Var constructor = JavascriptOperators::SpeciesConstructor(sourcePromise, scriptContext->GetLibrary()->GetPromiseConstructor(), scriptContext);
        JavascriptPromiseCapability* promiseCapability = NewPromiseCapability(constructor, scriptContext);

        JavascriptPromiseReaction* resolveReaction = JavascriptPromiseReaction::New(promiseCapability, fulfillmentHandler, scriptContext);
        JavascriptPromiseReaction* rejectReaction = JavascriptPromiseReaction::New(promiseCapability, rejectionHandler, scriptContext);

        switch (sourcePromise->status)
        {LOGMEIN("JavascriptPromise.cpp] 818\n");
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
            {LOGMEIN("JavascriptPromise.cpp] 862\n");
                return CALL_FUNCTION(thenFunction, Js::CallInfo(Js::CallFlags::CallFlags_Value, 3),
                    thenable,
                    resolve,
                    reject);
            }
            catch (const JavascriptException& err)
            {LOGMEIN("JavascriptPromise.cpp] 869\n");
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
        {LOGMEIN("JavascriptPromise.cpp] 887\n");
            Assert(args[1] != nullptr);

        return args[1];
    }
        else
        {
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
        {LOGMEIN("JavascriptPromise.cpp] 909\n");
            Assert(args[1] != nullptr);

            arg = args[1];
        }
        else
        {
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
        {LOGMEIN("JavascriptPromise.cpp] 934\n");
            x = args[1];
        }
        else
        {
            x = undefinedVar;
        }

        JavascriptPromiseAllResolveElementFunction* allResolveElementFunction = JavascriptPromiseAllResolveElementFunction::FromVar(function);

        if (allResolveElementFunction->IsAlreadyCalled())
        {LOGMEIN("JavascriptPromise.cpp] 945\n");
            return undefinedVar;
        }

        allResolveElementFunction->SetAlreadyCalled(true);

        uint32 index = allResolveElementFunction->GetIndex();
        JavascriptArray* values = allResolveElementFunction->GetValues();
        JavascriptPromiseCapability* promiseCapability = allResolveElementFunction->GetCapabilities();
        JavascriptExceptionObject* exception = nullptr;

        try
        {LOGMEIN("JavascriptPromise.cpp] 957\n");
            values->SetItem(index, x, PropertyOperation_None);
        }
        catch (const JavascriptException& err)
        {LOGMEIN("JavascriptPromise.cpp] 961\n");
            exception = err.GetAndClear();
        }

        if (exception != nullptr)
        {LOGMEIN("JavascriptPromise.cpp] 966\n");
            return TryRejectWithExceptionObject(exception, promiseCapability->GetReject(), scriptContext);
        }

        if (allResolveElementFunction->DecrementRemainingElements() == 0)
        {LOGMEIN("JavascriptPromise.cpp] 971\n");
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
        {LOGMEIN("JavascriptPromise.cpp] 1040\n");
            argument = args[1];
        }

        JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction* asyncSpawnStepExecutorFunction = JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction::FromVar(function);
        JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction* functionArg;
        JavascriptGenerator* gen = asyncSpawnStepExecutorFunction->GetGenerator();
        JavascriptFunction* reject = asyncSpawnStepExecutorFunction->GetReject();
        JavascriptFunction* resolve = asyncSpawnStepExecutorFunction->GetResolve();

        if (asyncSpawnStepExecutorFunction->GetIsReject())
        {LOGMEIN("JavascriptPromise.cpp] 1051\n");
            functionArg = library->CreatePromiseAsyncSpawnStepArgumentExecutorFunction(EntryJavascriptPromiseAsyncSpawnStepThrowExecutorFunction, gen, argument, NULL, NULL, false);
        }
        else
        {
            functionArg = library->CreatePromiseAsyncSpawnStepArgumentExecutorFunction(EntryJavascriptPromiseAsyncSpawnStepNextExecutorFunction, gen, argument, NULL, NULL, false);
        }

        AsyncSpawnStep(functionArg, gen, resolve, reject);

        return undefinedVar;
    }

    void JavascriptPromise::AsyncSpawnStep(JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction* nextFunction, JavascriptGenerator* gen, JavascriptFunction* resolve, JavascriptFunction* reject)
    {LOGMEIN("JavascriptPromise.cpp] 1065\n");
        ScriptContext* scriptContext = resolve->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();
        Var undefinedVar = library->GetUndefined();

        JavascriptExceptionObject* exception = nullptr;
        Var value = nullptr;
        RecyclableObject* next = nullptr;
        bool done;

        try
        {LOGMEIN("JavascriptPromise.cpp] 1076\n");
            next = RecyclableObject::FromVar(CALL_FUNCTION(nextFunction, CallInfo(CallFlags_Value, 1), undefinedVar));
        }
        catch (const JavascriptException& err)
        {LOGMEIN("JavascriptPromise.cpp] 1080\n");
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
        {LOGMEIN("JavascriptPromise.cpp] 1094\n");
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
    {LOGMEIN("JavascriptPromise.cpp] 1118\n");
        if(this->result != nullptr)
        {LOGMEIN("JavascriptPromise.cpp] 1120\n");
            extractor->MarkVisitVar(this->result);
        }

        if(this->resolveReactions != nullptr)
        {LOGMEIN("JavascriptPromise.cpp] 1125\n");
            for(int32 i = 0; i < this->resolveReactions->Count(); ++i)
            {LOGMEIN("JavascriptPromise.cpp] 1127\n");
                this->resolveReactions->Item(i)->MarkVisitPtrs(extractor);
            }
        }

        if(this->rejectReactions != nullptr)
        {LOGMEIN("JavascriptPromise.cpp] 1133\n");
            for(int32 i = 0; i < this->rejectReactions->Count(); ++i)
            {LOGMEIN("JavascriptPromise.cpp] 1135\n");
                this->rejectReactions->Item(i)->MarkVisitPtrs(extractor);
            }
        }
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptPromise::GetSnapTag_TTD() const
    {LOGMEIN("JavascriptPromise.cpp] 1142\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapPromiseObject;
    }

    void JavascriptPromise::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("JavascriptPromise.cpp] 1147\n");
        JsUtil::List<TTD_PTR_ID, HeapAllocator> depOnList(&HeapAllocator::Instance);

        TTD::NSSnapObjects::SnapPromiseInfo* spi = alloc.SlabAllocateStruct<TTD::NSSnapObjects::SnapPromiseInfo>();

        spi->Result = this->result;

        //Primitive kinds always inflated first so we only need to deal with complex kinds as depends on
        if(this->result != nullptr && TTD::JsSupport::IsVarComplexKind(this->result))
        {LOGMEIN("JavascriptPromise.cpp] 1156\n");
            depOnList.Add(TTD_CONVERT_VAR_TO_PTR_ID(this->result));
        }

        spi->Status = this->status;

        spi->ResolveReactionCount = (this->resolveReactions != nullptr) ? this->resolveReactions->Count() : 0;
        spi->ResolveReactions = nullptr;
        if(spi->ResolveReactionCount != 0)
        {LOGMEIN("JavascriptPromise.cpp] 1165\n");
            spi->ResolveReactions = alloc.SlabAllocateArray<TTD::NSSnapValues::SnapPromiseReactionInfo>(spi->ResolveReactionCount);

            for(uint32 i = 0; i < spi->ResolveReactionCount; ++i)
            {LOGMEIN("JavascriptPromise.cpp] 1169\n");
                this->resolveReactions->Item(i)->ExtractSnapPromiseReactionInto(spi->ResolveReactions + i, depOnList, alloc);
            }
        }

        spi->RejectReactionCount = (this->rejectReactions != nullptr) ? this->rejectReactions->Count() : 0;
        spi->RejectReactions = nullptr;
        if(spi->RejectReactionCount != 0)
        {LOGMEIN("JavascriptPromise.cpp] 1177\n");
            spi->RejectReactions = alloc.SlabAllocateArray<TTD::NSSnapValues::SnapPromiseReactionInfo>(spi->RejectReactionCount);

            for(uint32 i = 0; i < spi->RejectReactionCount; ++i)
            {LOGMEIN("JavascriptPromise.cpp] 1181\n");
                this->rejectReactions->Item(i)->ExtractSnapPromiseReactionInto(spi->RejectReactions+ i, depOnList, alloc);
            }
        }

        //see what we need to do wrt dependencies
        if(depOnList.Count() == 0)
        {LOGMEIN("JavascriptPromise.cpp] 1188\n");
            TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapPromiseInfo*, TTD::NSSnapObjects::SnapObjectType::SnapPromiseObject>(objData, spi);
        }
        else
        {
            uint32 depOnCount = depOnList.Count();
            TTD_PTR_ID* depOnArray = alloc.SlabAllocateArray<TTD_PTR_ID>(depOnCount);

            for(uint32 i = 0; i < depOnCount; ++i)
            {LOGMEIN("JavascriptPromise.cpp] 1197\n");
                depOnArray[i] = depOnList.Item(i);
            }

            TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapPromiseInfo*, TTD::NSSnapObjects::SnapObjectType::SnapPromiseObject>(objData, spi, alloc, depOnCount, depOnArray);
        }
    }

    JavascriptPromise* JavascriptPromise::InitializePromise_TTD(ScriptContext* scriptContext, uint32 status, Var result, JsUtil::List<Js::JavascriptPromiseReaction*, HeapAllocator>& resolveReactions, JsUtil::List<Js::JavascriptPromiseReaction*, HeapAllocator>& rejectReactions)
    {LOGMEIN("JavascriptPromise.cpp] 1206\n");
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
    {LOGMEIN("JavascriptPromise.cpp] 1227\n");
        if (!JavascriptOperators::IsConstructor(constructor))
        {LOGMEIN("JavascriptPromise.cpp] 1229\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedFunction);
        }

        RecyclableObject* constructorFunc = RecyclableObject::FromVar(constructor);

        return CreatePromiseCapabilityRecord(constructorFunc, scriptContext);
    }

    // CreatePromiseCapabilityRecord as described in ES6.0 (draft 29) Section 25.4.1.6.1
    JavascriptPromiseCapability* JavascriptPromise::CreatePromiseCapabilityRecord(RecyclableObject* constructor, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptPromise.cpp] 1240\n");
        JavascriptLibrary* library = scriptContext->GetLibrary();
        Var undefinedVar = library->GetUndefined();
        JavascriptPromiseCapability* promiseCapability = JavascriptPromiseCapability::New(undefinedVar, undefinedVar, undefinedVar, scriptContext);

        JavascriptPromiseCapabilitiesExecutorFunction* executor = library->CreatePromiseCapabilitiesExecutorFunction(EntryCapabilitiesExecutorFunction, promiseCapability);

        CallInfo callinfo = Js::CallInfo((Js::CallFlags)(Js::CallFlags::CallFlags_Value | Js::CallFlags::CallFlags_New), 2);
        Var argVars[] = { constructor, executor };
        Arguments args(callinfo, argVars);
        Var promise = JavascriptFunction::CallAsConstructor(constructor, nullptr, args, scriptContext);

        if (!JavascriptConversion::IsCallable(promiseCapability->GetResolve()) || !JavascriptConversion::IsCallable(promiseCapability->GetReject()))
        {LOGMEIN("JavascriptPromise.cpp] 1253\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedFunction, _u("Promise"));
        }

        promiseCapability->SetPromise(promise);

        return promiseCapability;
    }

    // TriggerPromiseReactions as defined in ES 2015 Section 25.4.1.7
    Var JavascriptPromise::TriggerPromiseReactions(JavascriptPromiseReactionList* reactions, Var resolution, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptPromise.cpp] 1264\n");
        JavascriptLibrary* library = scriptContext->GetLibrary();

        if (reactions != nullptr)
        {LOGMEIN("JavascriptPromise.cpp] 1268\n");
            for (int i = 0; i < reactions->Count(); i++)
            {LOGMEIN("JavascriptPromise.cpp] 1270\n");
                JavascriptPromiseReaction* reaction = reactions->Item(i);

                EnqueuePromiseReactionTask(reaction, resolution, scriptContext);
            }
        }

        return library->GetUndefined();
    }

    void JavascriptPromise::EnqueuePromiseReactionTask(JavascriptPromiseReaction* reaction, Var resolution, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptPromise.cpp] 1281\n");
        Assert(resolution != nullptr);

        JavascriptLibrary* library = scriptContext->GetLibrary();
        JavascriptPromiseReactionTaskFunction* reactionTaskFunction = library->CreatePromiseReactionTaskFunction(EntryReactionTaskFunction, reaction, resolution);

        library->EnqueueTask(reactionTaskFunction);
    }

    JavascriptPromiseResolveOrRejectFunction::JavascriptPromiseResolveOrRejectFunction(DynamicType* type)
        : RuntimeFunction(type, &Js::JavascriptPromise::EntryInfo::ResolveOrRejectFunction), promise(nullptr), isReject(false), alreadyResolvedWrapper(nullptr)
    {LOGMEIN("JavascriptPromise.cpp] 1292\n"); }

    JavascriptPromiseResolveOrRejectFunction::JavascriptPromiseResolveOrRejectFunction(DynamicType* type, FunctionInfo* functionInfo, JavascriptPromise* promise, bool isReject, JavascriptPromiseResolveOrRejectFunctionAlreadyResolvedWrapper* alreadyResolvedRecord)
        : RuntimeFunction(type, functionInfo), promise(promise), isReject(isReject), alreadyResolvedWrapper(alreadyResolvedRecord)
    {LOGMEIN("JavascriptPromise.cpp] 1296\n"); }

    bool JavascriptPromiseResolveOrRejectFunction::Is(Var var)
    {LOGMEIN("JavascriptPromise.cpp] 1299\n");
        if (JavascriptFunction::Is(var))
        {LOGMEIN("JavascriptPromise.cpp] 1301\n");
            JavascriptFunction* obj = JavascriptFunction::FromVar(var);

            return VirtualTableInfo<JavascriptPromiseResolveOrRejectFunction>::HasVirtualTable(obj)
                || VirtualTableInfo<CrossSiteObject<JavascriptPromiseResolveOrRejectFunction>>::HasVirtualTable(obj);
        }

        return false;
    }

    JavascriptPromiseResolveOrRejectFunction* JavascriptPromiseResolveOrRejectFunction::FromVar(Var var)
    {LOGMEIN("JavascriptPromise.cpp] 1312\n");
        Assert(JavascriptPromiseResolveOrRejectFunction::Is(var));

        return static_cast<JavascriptPromiseResolveOrRejectFunction*>(var);
    }

    JavascriptPromise* JavascriptPromiseResolveOrRejectFunction::GetPromise()
    {LOGMEIN("JavascriptPromise.cpp] 1319\n");
        return this->promise;
    }

    bool JavascriptPromiseResolveOrRejectFunction::IsRejectFunction()
    {LOGMEIN("JavascriptPromise.cpp] 1324\n");
        return this->isReject;
    }

    bool JavascriptPromiseResolveOrRejectFunction::IsAlreadyResolved()
    {LOGMEIN("JavascriptPromise.cpp] 1329\n");
        Assert(this->alreadyResolvedWrapper);

        return this->alreadyResolvedWrapper->alreadyResolved;
    }

    void JavascriptPromiseResolveOrRejectFunction::SetAlreadyResolved(bool is)
    {LOGMEIN("JavascriptPromise.cpp] 1336\n");
        Assert(this->alreadyResolvedWrapper);

        this->alreadyResolvedWrapper->alreadyResolved = is;
    }

#if ENABLE_TTD
    void JavascriptPromiseResolveOrRejectFunction::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {LOGMEIN("JavascriptPromise.cpp] 1344\n");
        TTDAssert(this->promise != nullptr, "Was not expecting that!!!");

        extractor->MarkVisitVar(this->promise);
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptPromiseResolveOrRejectFunction::GetSnapTag_TTD() const
    {LOGMEIN("JavascriptPromise.cpp] 1351\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapPromiseResolveOrRejectFunctionObject;
    }

    void JavascriptPromiseResolveOrRejectFunction::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("JavascriptPromise.cpp] 1356\n");
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
    {LOGMEIN("JavascriptPromise.cpp] 1376\n"); }

    bool JavascriptPromiseAsyncSpawnExecutorFunction::Is(Var var)
    {LOGMEIN("JavascriptPromise.cpp] 1379\n");
        if (JavascriptFunction::Is(var))
        {LOGMEIN("JavascriptPromise.cpp] 1381\n");
            JavascriptFunction* obj = JavascriptFunction::FromVar(var);

            return VirtualTableInfo<JavascriptPromiseAsyncSpawnExecutorFunction>::HasVirtualTable(obj)
                || VirtualTableInfo<CrossSiteObject<JavascriptPromiseAsyncSpawnExecutorFunction>>::HasVirtualTable(obj);
        }

        return false;
    }

    JavascriptPromiseAsyncSpawnExecutorFunction* JavascriptPromiseAsyncSpawnExecutorFunction::FromVar(Var var)
    {LOGMEIN("JavascriptPromise.cpp] 1392\n");
        Assert(JavascriptPromiseAsyncSpawnExecutorFunction::Is(var));

        return static_cast<JavascriptPromiseAsyncSpawnExecutorFunction*>(var);
    }

    JavascriptGenerator* JavascriptPromiseAsyncSpawnExecutorFunction::GetGenerator()
    {LOGMEIN("JavascriptPromise.cpp] 1399\n");
        return this->generator;
    }

    Var JavascriptPromiseAsyncSpawnExecutorFunction::GetTarget()
    {LOGMEIN("JavascriptPromise.cpp] 1404\n");
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
    {LOGMEIN("JavascriptPromise.cpp] 1428\n"); }

    bool JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction::Is(Var var)
    {LOGMEIN("JavascriptPromise.cpp] 1431\n");
        if (JavascriptFunction::Is(var))
        {LOGMEIN("JavascriptPromise.cpp] 1433\n");
            JavascriptFunction* obj = JavascriptFunction::FromVar(var);

            return VirtualTableInfo<JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction>::HasVirtualTable(obj)
                || VirtualTableInfo<CrossSiteObject<JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction>>::HasVirtualTable(obj);
        }

        return false;
    }

    JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction* JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction::FromVar(Var var)
    {LOGMEIN("JavascriptPromise.cpp] 1444\n");
        Assert(JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction::Is(var));

        return static_cast<JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction*>(var);
    }

    JavascriptGenerator* JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction::GetGenerator()
    {LOGMEIN("JavascriptPromise.cpp] 1451\n");
        return this->generator;
    }

    JavascriptFunction* JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction::GetResolve()
    {LOGMEIN("JavascriptPromise.cpp] 1456\n");
        return this->resolve;
    }

    JavascriptFunction* JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction::GetReject()
    {LOGMEIN("JavascriptPromise.cpp] 1461\n");
        return this->reject;
    }

    bool JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction::GetIsReject()
    {LOGMEIN("JavascriptPromise.cpp] 1466\n");
        return this->isReject;
    }

    Var JavascriptPromiseAsyncSpawnStepArgumentExecutorFunction::GetArgument()
    {LOGMEIN("JavascriptPromise.cpp] 1471\n");
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
    {LOGMEIN("JavascriptPromise.cpp] 1495\n"); }

    bool JavascriptPromiseCapabilitiesExecutorFunction::Is(Var var)
    {LOGMEIN("JavascriptPromise.cpp] 1498\n");
        if (JavascriptFunction::Is(var))
        {LOGMEIN("JavascriptPromise.cpp] 1500\n");
            JavascriptFunction* obj = JavascriptFunction::FromVar(var);

            return VirtualTableInfo<JavascriptPromiseCapabilitiesExecutorFunction>::HasVirtualTable(obj)
                || VirtualTableInfo<CrossSiteObject<JavascriptPromiseCapabilitiesExecutorFunction>>::HasVirtualTable(obj);
        }

        return false;
    }

    JavascriptPromiseCapabilitiesExecutorFunction* JavascriptPromiseCapabilitiesExecutorFunction::FromVar(Var var)
    {LOGMEIN("JavascriptPromise.cpp] 1511\n");
        Assert(JavascriptPromiseCapabilitiesExecutorFunction::Is(var));

        return static_cast<JavascriptPromiseCapabilitiesExecutorFunction*>(var);
    }

    JavascriptPromiseCapability* JavascriptPromiseCapabilitiesExecutorFunction::GetCapability()
    {LOGMEIN("JavascriptPromise.cpp] 1518\n");
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
    {LOGMEIN("JavascriptPromise.cpp] 1541\n");
        return RecyclerNew(scriptContext->GetRecycler(), JavascriptPromiseCapability, promise, resolve, reject);
    }

    Var JavascriptPromiseCapability::GetResolve()
    {LOGMEIN("JavascriptPromise.cpp] 1546\n");
        return this->resolve;
    }

    Var JavascriptPromiseCapability::GetReject()
    {LOGMEIN("JavascriptPromise.cpp] 1551\n");
        return this->reject;
    }

    Var JavascriptPromiseCapability::GetPromise()
    {LOGMEIN("JavascriptPromise.cpp] 1556\n");
        return this->promise;
    }

    void JavascriptPromiseCapability::SetPromise(Var promise)
    {LOGMEIN("JavascriptPromise.cpp] 1561\n");
        this->promise = promise;
    }

    void JavascriptPromiseCapability::SetResolve(Var resolve)
    {LOGMEIN("JavascriptPromise.cpp] 1566\n");
        this->resolve = resolve;
    }

    void JavascriptPromiseCapability::SetReject(Var reject)
    {LOGMEIN("JavascriptPromise.cpp] 1571\n");
        this->reject = reject;
    }

#if ENABLE_TTD
    void JavascriptPromiseCapability::MarkVisitPtrs(TTD::SnapshotExtractor* extractor)
    {LOGMEIN("JavascriptPromise.cpp] 1577\n");
        TTDAssert(this->promise != nullptr && this->resolve != nullptr && this->reject != nullptr, "Seems odd, I was not expecting this!!!");

        extractor->MarkVisitVar(this->promise);

        extractor->MarkVisitVar(this->resolve);
        extractor->MarkVisitVar(this->reject);
    }

    void JavascriptPromiseCapability::ExtractSnapPromiseCapabilityInto(TTD::NSSnapValues::SnapPromiseCapabilityInfo* snapPromiseCapability, JsUtil::List<TTD_PTR_ID, HeapAllocator>& depOnList, TTD::SlabAllocator& alloc)
    {LOGMEIN("JavascriptPromise.cpp] 1587\n");
        snapPromiseCapability->CapabilityId = TTD_CONVERT_PROMISE_INFO_TO_PTR_ID(this);

        snapPromiseCapability->PromiseVar = this->promise;
        if(TTD::JsSupport::IsVarComplexKind(this->promise))
        {LOGMEIN("JavascriptPromise.cpp] 1592\n");
            depOnList.Add(TTD_CONVERT_VAR_TO_PTR_ID(this->resolve));
        }

        snapPromiseCapability->ResolveVar = this->resolve;
        if(TTD::JsSupport::IsVarComplexKind(this->resolve))
        {LOGMEIN("JavascriptPromise.cpp] 1598\n");
            depOnList.Add(TTD_CONVERT_VAR_TO_PTR_ID(this->resolve));
        }

        snapPromiseCapability->RejectVar = this->reject;
        if(TTD::JsSupport::IsVarComplexKind(this->reject))
        {LOGMEIN("JavascriptPromise.cpp] 1604\n");
            depOnList.Add(TTD_CONVERT_VAR_TO_PTR_ID(this->reject));
        }
    }
#endif

    JavascriptPromiseReaction* JavascriptPromiseReaction::New(JavascriptPromiseCapability* capabilities, RecyclableObject* handler, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptPromise.cpp] 1611\n");
        return RecyclerNew(scriptContext->GetRecycler(), JavascriptPromiseReaction, capabilities, handler);
    }

    JavascriptPromiseCapability* JavascriptPromiseReaction::GetCapabilities()
    {LOGMEIN("JavascriptPromise.cpp] 1616\n");
        return this->capabilities;
    }

    RecyclableObject* JavascriptPromiseReaction::GetHandler()
    {LOGMEIN("JavascriptPromise.cpp] 1621\n");
        return this->handler;
    }

#if ENABLE_TTD
    void JavascriptPromiseReaction::MarkVisitPtrs(TTD::SnapshotExtractor* extractor)
    {LOGMEIN("JavascriptPromise.cpp] 1627\n");
        TTDAssert(this->handler != nullptr && this->capabilities != nullptr, "Seems odd, I was not expecting this!!!");

        extractor->MarkVisitVar(this->handler);

        this->capabilities->MarkVisitPtrs(extractor);
    }

    void JavascriptPromiseReaction::ExtractSnapPromiseReactionInto(TTD::NSSnapValues::SnapPromiseReactionInfo* snapPromiseReaction, JsUtil::List<TTD_PTR_ID, HeapAllocator>& depOnList, TTD::SlabAllocator& alloc)
    {LOGMEIN("JavascriptPromise.cpp] 1636\n");
        TTDAssert(this->handler != nullptr && this->capabilities != nullptr, "Seems odd, I was not expecting this!!!");

        snapPromiseReaction->PromiseReactionId = TTD_CONVERT_PROMISE_INFO_TO_PTR_ID(this);

        snapPromiseReaction->HandlerObjId = TTD_CONVERT_VAR_TO_PTR_ID(this->handler);
        depOnList.Add(snapPromiseReaction->HandlerObjId);

        this->capabilities->ExtractSnapPromiseCapabilityInto(&snapPromiseReaction->Capabilities, depOnList, alloc);
    }
#endif

    JavascriptPromiseReaction* JavascriptPromiseReactionTaskFunction::GetReaction()
    {LOGMEIN("JavascriptPromise.cpp] 1649\n");
        return this->reaction;
    }

    Var JavascriptPromiseReactionTaskFunction::GetArgument()
    {LOGMEIN("JavascriptPromise.cpp] 1654\n");
        return this->argument;
    }

#if ENABLE_TTD
    void JavascriptPromiseReactionTaskFunction::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {LOGMEIN("JavascriptPromise.cpp] 1660\n");
        TTDAssert(this->argument != nullptr && this->reaction != nullptr, "Was not expecting this!!!");

        extractor->MarkVisitVar(this->argument);

        this->reaction->MarkVisitPtrs(extractor);
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptPromiseReactionTaskFunction::GetSnapTag_TTD() const
    {LOGMEIN("JavascriptPromise.cpp] 1669\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapPromiseReactionTaskFunctionObject;
    }

    void JavascriptPromiseReactionTaskFunction::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("JavascriptPromise.cpp] 1674\n");
        TTD::NSSnapObjects::SnapPromiseReactionTaskFunctionInfo* sprtfi = alloc.SlabAllocateStruct<TTD::NSSnapObjects::SnapPromiseReactionTaskFunctionInfo>();

        JsUtil::List<TTD_PTR_ID, HeapAllocator> depOnList(&HeapAllocator::Instance);

        sprtfi->Argument = this->argument;

        if(this->argument != nullptr && TTD::JsSupport::IsVarComplexKind(this->argument))
        {LOGMEIN("JavascriptPromise.cpp] 1682\n");
            depOnList.Add(TTD_CONVERT_VAR_TO_PTR_ID(this->argument));
        }

        this->reaction->ExtractSnapPromiseReactionInto(&sprtfi->Reaction, depOnList, alloc);

        //see what we need to do wrt dependencies
        if(depOnList.Count() == 0)
        {LOGMEIN("JavascriptPromise.cpp] 1690\n");
            TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapPromiseReactionTaskFunctionInfo*, TTD::NSSnapObjects::SnapObjectType::SnapPromiseReactionTaskFunctionObject>(objData, sprtfi);
        }
        else
        {
            uint32 depOnCount = depOnList.Count();
            TTD_PTR_ID* depOnArray = alloc.SlabAllocateArray<TTD_PTR_ID>(depOnCount);

            for(uint32 i = 0; i < depOnCount; ++i)
            {LOGMEIN("JavascriptPromise.cpp] 1699\n");
                depOnArray[i] = depOnList.Item(i);
            }

            TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapPromiseReactionTaskFunctionInfo*, TTD::NSSnapObjects::SnapObjectType::SnapPromiseReactionTaskFunctionObject>(objData, sprtfi, alloc, depOnCount, depOnArray);
        }
    }
#endif

    JavascriptPromise* JavascriptPromiseResolveThenableTaskFunction::GetPromise()
    {LOGMEIN("JavascriptPromise.cpp] 1709\n");
        return this->promise;
    }

    RecyclableObject* JavascriptPromiseResolveThenableTaskFunction::GetThenable()
    {LOGMEIN("JavascriptPromise.cpp] 1714\n");
        return this->thenable;
    }

    RecyclableObject* JavascriptPromiseResolveThenableTaskFunction::GetThenFunction()
    {LOGMEIN("JavascriptPromise.cpp] 1719\n");
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
    {LOGMEIN("JavascriptPromise.cpp] 1743\n"); }

    JavascriptPromiseAllResolveElementFunction::JavascriptPromiseAllResolveElementFunction(DynamicType* type, FunctionInfo* functionInfo, uint32 index, JavascriptArray* values, JavascriptPromiseCapability* capabilities, JavascriptPromiseAllResolveElementFunctionRemainingElementsWrapper* remainingElementsWrapper)
        : RuntimeFunction(type, functionInfo), index(index), values(values), capabilities(capabilities), remainingElementsWrapper(remainingElementsWrapper), alreadyCalled(false)
    {LOGMEIN("JavascriptPromise.cpp] 1747\n"); }

    bool JavascriptPromiseAllResolveElementFunction::Is(Var var)
    {LOGMEIN("JavascriptPromise.cpp] 1750\n");
        if (JavascriptFunction::Is(var))
        {LOGMEIN("JavascriptPromise.cpp] 1752\n");
            JavascriptFunction* obj = JavascriptFunction::FromVar(var);

            return VirtualTableInfo<JavascriptPromiseAllResolveElementFunction>::HasVirtualTable(obj)
                || VirtualTableInfo<CrossSiteObject<JavascriptPromiseAllResolveElementFunction>>::HasVirtualTable(obj);
        }

        return false;
    }

    JavascriptPromiseAllResolveElementFunction* JavascriptPromiseAllResolveElementFunction::FromVar(Var var)
    {LOGMEIN("JavascriptPromise.cpp] 1763\n");
        Assert(JavascriptPromiseAllResolveElementFunction::Is(var));

        return static_cast<JavascriptPromiseAllResolveElementFunction*>(var);
    }

    JavascriptPromiseCapability* JavascriptPromiseAllResolveElementFunction::GetCapabilities()
    {LOGMEIN("JavascriptPromise.cpp] 1770\n");
        return this->capabilities;
    }

    uint32 JavascriptPromiseAllResolveElementFunction::GetIndex()
    {LOGMEIN("JavascriptPromise.cpp] 1775\n");
        return this->index;
    }

    uint32 JavascriptPromiseAllResolveElementFunction::GetRemainingElements()
    {LOGMEIN("JavascriptPromise.cpp] 1780\n");
        return this->remainingElementsWrapper->remainingElements;
    }

    JavascriptArray* JavascriptPromiseAllResolveElementFunction::GetValues()
    {LOGMEIN("JavascriptPromise.cpp] 1785\n");
        return this->values;
    }

    uint32 JavascriptPromiseAllResolveElementFunction::DecrementRemainingElements()
    {LOGMEIN("JavascriptPromise.cpp] 1790\n");
        return --(this->remainingElementsWrapper->remainingElements);
    }

    bool JavascriptPromiseAllResolveElementFunction::IsAlreadyCalled() const
    {LOGMEIN("JavascriptPromise.cpp] 1795\n");
        return this->alreadyCalled;
    }

    void JavascriptPromiseAllResolveElementFunction::SetAlreadyCalled(const bool is)
    {LOGMEIN("JavascriptPromise.cpp] 1800\n");
        this->alreadyCalled = is;
    }

#if ENABLE_TTD
    void JavascriptPromiseAllResolveElementFunction::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {LOGMEIN("JavascriptPromise.cpp] 1806\n");
        TTDAssert(this->capabilities != nullptr && this->remainingElementsWrapper != nullptr && this->values != nullptr, "Don't think these can be null");

        this->capabilities->MarkVisitPtrs(extractor);
        extractor->MarkVisitVar(this->values);
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptPromiseAllResolveElementFunction::GetSnapTag_TTD() const
    {LOGMEIN("JavascriptPromise.cpp] 1814\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapPromiseAllResolveElementFunctionObject;
    }

    void JavascriptPromiseAllResolveElementFunction::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("JavascriptPromise.cpp] 1819\n");
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
        {LOGMEIN("JavascriptPromise.cpp] 1838\n");
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
