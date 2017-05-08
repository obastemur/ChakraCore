//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
#include "Library/JSONStack.h"
#include "Library/JSONParser.h"
#include "Library/JSON.h"

#define MAX_JSON_STRINGIFY_NAMES_ON_STACK 20
static const int JSONspaceSize = 10; //ES5 defined limit on the indentation space
using namespace Js;


namespace JSON
{
    Js::FunctionInfo EntryInfo::Stringify(FORCE_NO_WRITE_BARRIER_TAG(JSON::Stringify), Js::FunctionInfo::ErrorOnNew);
    Js::FunctionInfo EntryInfo::Parse(FORCE_NO_WRITE_BARRIER_TAG(JSON::Parse), Js::FunctionInfo::ErrorOnNew);

    Js::Var Parse(Js::JavascriptString* input, Js::RecyclableObject* reviver, Js::ScriptContext* scriptContext);

    Js::Var Parse(Js::RecyclableObject* function, Js::CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        //ES5:  parse(text [, reviver])
        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        Js::ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("JSON.parse"));
        Assert(!(callInfo.Flags & Js::CallFlags_New));

        if(args.Info.Count < 2)
        {TRACE_IT(55695);
            // if the text argument is missing it is assumed to be undefined.
            // ToString(undefined) returns "undefined" which is not a JSON grammar correct construct.  Shortcut and throw here
            Js::JavascriptError::ThrowSyntaxError(scriptContext, ERRsyntax);
        }

        Js::JavascriptString* input;
        Js::Var value = args[1];
        if (Js::JavascriptString::Is(value))
        {TRACE_IT(55696);
            input = Js::JavascriptString::FromVar(value);
        }
        else
        {TRACE_IT(55697);
            input = Js::JavascriptConversion::ToString(value, scriptContext);
        }
        Js::RecyclableObject* reviver = NULL;
        if (args.Info.Count > 2 && Js::JavascriptConversion::IsCallable(args[2]))
        {TRACE_IT(55698);
            reviver = Js::RecyclableObject::FromVar(args[2]);
        }

        return Parse(input, reviver, scriptContext);
    }

    Js::Var Parse(Js::JavascriptString* input, Js::RecyclableObject* reviver, Js::ScriptContext* scriptContext)
    {TRACE_IT(55699);
        // alignment required because of the union in JSONParser::m_token
        __declspec (align(8)) JSONParser parser(scriptContext, reviver);
        Js::Var result = NULL;

        TryFinally([&]()
        {
            result = parser.Parse(input);

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
            if (CONFIG_FLAG(ForceGCAfterJSONParse))
            {TRACE_IT(55700);
                Recycler* recycler = scriptContext->GetRecycler();
                recycler->CollectNow<CollectNowForceInThread>();
            }
#endif

            if(reviver)
            {TRACE_IT(55701);
                Js::DynamicObject* root = scriptContext->GetLibrary()->CreateObject();
                JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_OBJECT(root));
                Js::PropertyRecord const * propertyRecord;
                scriptContext->GetOrAddPropertyRecord(_u(""), 0, &propertyRecord);
                Js::PropertyId propertyId = propertyRecord->GetPropertyId();
                Js::JavascriptOperators::InitProperty(root, propertyId, result);
                result = parser.Walk(scriptContext->GetLibrary()->GetEmptyString(), propertyId, root);
            }
        },
        [&](bool/*hasException*/)
        {TRACE_IT(55702);
            parser.Finalizer();
        });

        return result;
    }

    inline bool IsValidReplacerType(Js::TypeId typeId)
    {TRACE_IT(55703);
        switch(typeId)
        {
            case Js::TypeIds_Integer:
            case Js::TypeIds_String:
            case Js::TypeIds_Number:
            case Js::TypeIds_NumberObject:
            case Js::TypeIds_Int64Number:
            case Js::TypeIds_UInt64Number:
            case Js::TypeIds_StringObject:
                return true;
        }
        return false;
    }

    uint32 AddToNameTable(StringifySession::StringTable nameTable[], uint32 tableLen, uint32 size, Js::Var item, Js::ScriptContext* scriptContext)
    {TRACE_IT(55704);
        Js::Var value = nullptr;
        switch (Js::JavascriptOperators::GetTypeId(item))
        {
        case Js::TypeIds_Integer:
            value = scriptContext->GetIntegerString(item);
            break;
        case Js::TypeIds_String:
            value = item;
            break;
        case Js::TypeIds_Number:
        case Js::TypeIds_NumberObject:
        case Js::TypeIds_Int64Number:
        case Js::TypeIds_UInt64Number:
        case Js::TypeIds_StringObject:
            value = Js::JavascriptConversion::ToString(item, scriptContext);
            break;
        }
        if (value && Js::JavascriptString::Is(value))
        {TRACE_IT(55705);
            // Only validate size when about to modify it. We skip over all other (non-valid) replacement elements.
            if (tableLen == size)
            {TRACE_IT(55706);
                Js::Throw::FatalInternalError(); // nameTable buffer calculation is wrong
            }
            Js::JavascriptString *propertyName = Js::JavascriptString::FromVar(value);
            nameTable[tableLen].propName = propertyName;
            Js::PropertyRecord const * propertyRecord;
            scriptContext->GetOrAddPropertyRecord(propertyName->GetString(), propertyName->GetLength(), &propertyRecord);
            nameTable[tableLen].propRecord = propertyRecord;        // Keep the property id alive.
            tableLen++;
        }
        return tableLen;
    }

    BVSparse<ArenaAllocator>* AllocateMap(ArenaAllocator *tempAlloc)
    {TRACE_IT(55707);
        //To escape error C2712: Cannot use __try in functions that require object unwinding
        return Anew(tempAlloc, BVSparse<ArenaAllocator>, tempAlloc);
    }

    Js::Var Stringify(Js::RecyclableObject* function, Js::CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        //ES5: Stringify(value, [replacer][, space]])
        ARGUMENTS(args, callInfo);
        Js::JavascriptLibrary* library = function->GetType()->GetLibrary();
        Js::ScriptContext* scriptContext = library->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("JSON.stringify"));

        Assert(!(callInfo.Flags & Js::CallFlags_New));

        if (args.Info.Count < 2)
        {TRACE_IT(55708);
            // if value is missing it is assumed to be 'undefined'.
            // shortcut: the stringify algorithm returns undefined in this case.
            return library->GetUndefined();
        }

        Js::Var value = args[1];
        Js::Var replacerArg = args.Info.Count > 2 ? args[2] : nullptr;
        Js::Var space = args.Info.Count > 3 ? args[3] : library->GetNull();

        Js::DynamicObject* remoteObject;
        if (Js::JavascriptOperators::GetTypeId(value) == Js::TypeIds_HostDispatch)
        {TRACE_IT(55709);
            remoteObject = Js::RecyclableObject::FromVar(value)->GetRemoteObject();
            if (remoteObject != nullptr)
            {TRACE_IT(55710);
                value = Js::DynamicObject::FromVar(remoteObject);
            }
            else
            {TRACE_IT(55711);
                Js::Var result;
                if (Js::RecyclableObject::FromVar(value)->InvokeBuiltInOperationRemotely(Stringify, args, &result))
                {TRACE_IT(55712);
                    return result;
                }
            }
        }
        Js::Var result = nullptr;
        StringifySession stringifySession(scriptContext);
        StringifySession::StringTable* nameTable = nullptr;            //stringifySession will point to the memory allocated by nameTable, so make sure lifespans are linked.

        DECLARE_TEMP_GUEST_ALLOCATOR(nameTableAlloc);

        if (replacerArg)
        {TRACE_IT(55713);
            if (Js::JavascriptOperators::IsArray(replacerArg))
            {TRACE_IT(55714);
                uint32 length;
                Js::JavascriptArray *reArray = nullptr;
                Js::RecyclableObject *reRemoteArray = Js::RecyclableObject::FromVar(replacerArg);
                bool isArray = false;

                if (Js::JavascriptArray::Is(replacerArg))
                {TRACE_IT(55715);
                    reArray = Js::JavascriptArray::FromVar(replacerArg);
                    length = reArray->GetLength();
                    isArray = true;
                }
                else
                {TRACE_IT(55716);
                    length = Js::JavascriptConversion::ToUInt32(Js::JavascriptOperators::OP_GetLength(replacerArg, scriptContext), scriptContext);
                }

                uint32 count = 0;
                Js::Var item = nullptr;

                if (isArray)
                {TRACE_IT(55717);
                    for (uint32 i = 0; i< length; i++)
                    {TRACE_IT(55718);
                        Js::TypeId idn = Js::JavascriptOperators::GetTypeId(reArray->DirectGetItem(i));
                        if(IsValidReplacerType(idn))
                        {TRACE_IT(55719);
                            count++;
                        }
                    }
                }
                else
                {TRACE_IT(55720);
                    for (uint32 i = 0; i< length; i++)
                    {TRACE_IT(55721);
                        if (Js::JavascriptOperators::GetItem(reRemoteArray, i, &item, scriptContext))
                        {TRACE_IT(55722);
                            Js::TypeId idn = Js::JavascriptOperators::GetTypeId(item);
                            if(IsValidReplacerType(idn))
                            {TRACE_IT(55723);
                                count++;
                            }
                        }
                    }
                }

                uint32 tableLen = 0;
                if (count)
                {TRACE_IT(55724);
                    // the name table goes away with stringify session.
                    if (count < MAX_JSON_STRINGIFY_NAMES_ON_STACK)
                    {
                         PROBE_STACK(scriptContext, (sizeof(StringifySession::StringTable) * count)) ;
                         nameTable = (StringifySession::StringTable*)_alloca(sizeof(StringifySession::StringTable) * count);
                    }
                    else
                    {
                         ACQUIRE_TEMP_GUEST_ALLOCATOR(nameTableAlloc, scriptContext, _u("JSON"));
                         nameTable = AnewArray(nameTableAlloc, StringifySession::StringTable, count);
                    }
                    if (isArray && !!reArray->IsCrossSiteObject())
                    {TRACE_IT(55725);
                        for (uint32 i = 0; i < length; i++)
                        {TRACE_IT(55726);
                            item = reArray->DirectGetItem(i);
                            tableLen = AddToNameTable(nameTable, tableLen, count, item, scriptContext);
                        }
                    }
                    else
                    {TRACE_IT(55727);
                        for (uint32 i = 0; i < length; i++)
                        {TRACE_IT(55728);
                            if (Js::JavascriptOperators::GetItem(reRemoteArray, i, &item, scriptContext))
                            {TRACE_IT(55729);
                                tableLen = AddToNameTable(nameTable, tableLen, count, item, scriptContext);
                            }
                        }
                    }

                    //Eliminate duplicates in replacer array.
                    BEGIN_TEMP_ALLOCATOR(tempAlloc, scriptContext, _u("JSON"))
                    {TRACE_IT(55730);
                        BVSparse<ArenaAllocator>* propIdMap = AllocateMap(tempAlloc); //Anew(tempAlloc, BVSparse<ArenaAllocator>, tempAlloc);

                        // TODO: Potential arithmetic overflow for table size/count/tableLen if large replacement args are specified.
                        // tableLen is ensured by AddToNameTable but this doesn't propagate as an annotation so we assume here to fix the OACR warning.
                        _Analysis_assume_(tableLen <= count);
                        Assert(tableLen <= count);

                        uint32 j = 0;
                        for (uint32 i=0; i < tableLen; i++)
                        {TRACE_IT(55731);
                            if(propIdMap->TestAndSet(nameTable[i].propRecord->GetPropertyId())) //Find & skip duplicate
                            {TRACE_IT(55732);
                                continue;
                            }
                            if (j != i)
                            {TRACE_IT(55733);
                                nameTable[j] = nameTable[i];
                            }
                            j++;
                        }
                        tableLen = j;
                    }
                    END_TEMP_ALLOCATOR(tempAlloc, scriptContext);
                }

                stringifySession.InitReplacer(nameTable, tableLen);
            }
            else if (Js::JavascriptConversion::IsCallable(replacerArg))
            {TRACE_IT(55734);
                stringifySession.InitReplacer(Js::RecyclableObject::FromVar(replacerArg));
            }
        }

        BEGIN_TEMP_ALLOCATOR(tempAlloc, scriptContext, _u("JSON"))
        {TRACE_IT(55735);
            stringifySession.CompleteInit(space, tempAlloc);

            Js::DynamicObject* wrapper = scriptContext->GetLibrary()->CreateObject();
            JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_OBJECT(wrapper));
            Js::PropertyRecord const * propertyRecord;
            scriptContext->GetOrAddPropertyRecord(_u(""), 0, &propertyRecord);
            Js::PropertyId propertyId = propertyRecord->GetPropertyId();
            Js::JavascriptOperators::InitProperty(wrapper, propertyId, value);
            result = stringifySession.Str(scriptContext->GetLibrary()->GetEmptyString(), propertyId, wrapper);
        }
        END_TEMP_ALLOCATOR(tempAlloc, scriptContext);

        RELEASE_TEMP_GUEST_ALLOCATOR(nameTableAlloc, scriptContext);
        return result;
    }

    // -------- StringifySession implementation ------------//

    void StringifySession::CompleteInit(Js::Var space, ArenaAllocator* tempAlloc)
    {TRACE_IT(55736);
        //set the stack, gap
        char16 buffer[JSONspaceSize];
        wmemset(buffer, _u(' '), JSONspaceSize);
        charcount_t len = 0;
        switch (Js::JavascriptOperators::GetTypeId(space))
        {
        case Js::TypeIds_Integer:
            {TRACE_IT(55737);
                len = max(0, min(JSONspaceSize, static_cast<int>(Js::TaggedInt::ToInt32(space))));
                break;
            }
        case Js::TypeIds_Number:
        case Js::TypeIds_NumberObject:
        case Js::TypeIds_Int64Number:
        case Js::TypeIds_UInt64Number:
            {TRACE_IT(55738);
                len = max(0, static_cast<int>(min(static_cast<double>(JSONspaceSize), Js::JavascriptConversion::ToInteger(space, scriptContext))));
                break;
            }
        case Js::TypeIds_String:
            {TRACE_IT(55739);
                len = min(static_cast<charcount_t>(JSONspaceSize), Js::JavascriptString::FromVar(space)->GetLength());
                if(len)
                {
                    js_wmemcpy_s(buffer, JSONspaceSize, Js::JavascriptString::FromVar(space)->GetString(), len);
                }
                break;
            }
        case Js::TypeIds_StringObject:
            {TRACE_IT(55740);
                Js::Var spaceString = Js::JavascriptConversion::ToString(space, scriptContext);
                if(Js::JavascriptString::Is(spaceString))
                {TRACE_IT(55741);
                    len = min(static_cast<charcount_t>(JSONspaceSize), Js::JavascriptString::FromVar(spaceString)->GetLength());
                    if(len)
                    {
                        js_wmemcpy_s(buffer, JSONspaceSize, Js::JavascriptString::FromVar(spaceString)->GetString(), len);
                    }
                }
                break;
            }
        }
        if (len)
        {TRACE_IT(55742);
            gap = Js::JavascriptString::NewCopyBuffer(buffer, len, scriptContext);
        }

        objectStack = Anew(tempAlloc, JSONStack, tempAlloc, scriptContext);
    }

    Js::Var StringifySession::Str(uint32 index, Js::Var holder)
    {TRACE_IT(55743);
        Js::Var value;
        Js::RecyclableObject *undefined = scriptContext->GetLibrary()->GetUndefined();

        if (Js::JavascriptArray::Is(holder) && !Js::JavascriptArray::FromVar(holder)->IsCrossSiteObject())
        {TRACE_IT(55744);
            if (Js::JavascriptOperators::IsUndefinedObject(value = Js::JavascriptArray::FromVar(holder)->DirectGetItem(index), undefined))
            {TRACE_IT(55745);
                return value;
            }
        }
        else
        {TRACE_IT(55746);
            Assert(Js::JavascriptOperators::IsArray(holder));
            Js::RecyclableObject *arr = RecyclableObject::FromVar(holder);
            if (!Js::JavascriptOperators::GetItem(arr, index, &value, scriptContext))
            {TRACE_IT(55747);
                return undefined;
            }
            if (Js::JavascriptOperators::IsUndefinedObject(value, undefined))
            {TRACE_IT(55748);
                return value;
            }
        }

        Js::JavascriptString *key = scriptContext->GetIntegerString(index);
        return StrHelper(key, value, holder);
    }

    Js::Var StringifySession::Str(Js::JavascriptString* key, Js::PropertyId keyId, Js::Var holder)
    {TRACE_IT(55749);
        Js::Var value;
        // We should look only into object's own properties here. When an object is serialized, only the own properties are considered,
        // the prototype chain is not considered. However, the property names can be selected via an array replacer. In this case
        // ES5 spec doesn't say the property has to own property or even to be enumerable. So, properties from the prototype, or non enum properties,
        // can end up being serialized. Well, that is the ES5 spec word.
        //if(!Js::RecyclableObject::FromVar(holder)->GetType()->GetProperty(holder, keyId, &value))

        if(!Js::JavascriptOperators::GetProperty(Js::RecyclableObject::FromVar(holder),keyId, &value, scriptContext))
        {TRACE_IT(55750);
            return scriptContext->GetLibrary()->GetUndefined();
        }
        return StrHelper(key, value, holder);
    }

    Js::Var StringifySession::StrHelper(Js::JavascriptString* key, Js::Var value, Js::Var holder)
    {
        PROBE_STACK(scriptContext, Js::Constants::MinStackDefault);
        AssertMsg(Js::RecyclableObject::Is(holder), "The holder argument in a JSON::Str function must be an object");

        Js::Var values[3];
        Js::Arguments args(0, values);
        Js::Var undefined = scriptContext->GetLibrary()->GetUndefined();

        //check and apply 'toJSON' filter
        if (Js::JavascriptOperators::IsJsNativeObject(value) || (Js::JavascriptOperators::IsObject(value)))
        {TRACE_IT(55751);
            Js::Var tojson;
            if (Js::JavascriptOperators::GetProperty(Js::RecyclableObject::FromVar(value), Js::PropertyIds::toJSON, &tojson, scriptContext) &&
                Js::JavascriptConversion::IsCallable(tojson))
            {TRACE_IT(55752);
                args.Info.Count = 2;
                args.Values[0] = value;
                args.Values[1] = key;

                Js::RecyclableObject* func = Js::RecyclableObject::FromVar(tojson);
                value = Js::JavascriptFunction::CallFunction<true>(func, func->GetEntryPoint(), args);
            }
        }

        //check and apply the user defined replacer filter
        if (ReplacerFunction == replacerType)
        {TRACE_IT(55753);
            args.Info.Count = 3;
            args.Values[0] = holder;
            args.Values[1] = key;
            args.Values[2] = value;

            Js::RecyclableObject* func = replacer.ReplacerFunction;
            value = Js::JavascriptFunction::CallFunction<true>(func, func->GetEntryPoint(), args);
        }

        Js::TypeId id = Js::JavascriptOperators::GetTypeId(value);
        if (Js::TypeIds_NumberObject == id)
        {TRACE_IT(55754);
            value = Js::JavascriptNumber::ToVarNoCheck(Js::JavascriptConversion::ToNumber(value, scriptContext),scriptContext);
        }
        else if (Js::TypeIds_StringObject == id)
        {TRACE_IT(55755);
            value = Js::JavascriptConversion::ToString(value, scriptContext);
        }
        else if (Js::TypeIds_BooleanObject == id)
        {TRACE_IT(55756);
            value = Js::JavascriptBooleanObject::FromVar(value)->GetValue() ? scriptContext->GetLibrary()->GetTrue() : scriptContext->GetLibrary()->GetFalse();
        }

        id = Js::JavascriptOperators::GetTypeId(value);
        switch (id)
        {
        case Js::TypeIds_Undefined:
        case Js::TypeIds_Symbol:
            return undefined;

        case Js::TypeIds_Null:
            return scriptContext->GetLibrary()->GetNullDisplayString();

        case Js::TypeIds_Integer:
            return scriptContext->GetIntegerString(value);

        case Js::TypeIds_Boolean:
            return (Js::JavascriptBoolean::FromVar(value)->GetValue()) ? scriptContext->GetLibrary()->GetTrueDisplayString() : scriptContext->GetLibrary()->GetFalseDisplayString();

        case Js::TypeIds_Int64Number:
            if (Js::NumberUtilities::IsFinite(static_cast<double>(Js::JavascriptInt64Number::FromVar(value)->GetValue())))
            {TRACE_IT(55757);
                return Js::JavascriptConversion::ToString(value, scriptContext);
            }
            else
            {TRACE_IT(55758);
                return scriptContext->GetLibrary()->GetNullDisplayString();
            }

        case Js::TypeIds_UInt64Number:
            if (Js::NumberUtilities::IsFinite(static_cast<double>(Js::JavascriptUInt64Number::FromVar(value)->GetValue())))
            {TRACE_IT(55759);
                return Js::JavascriptConversion::ToString(value, scriptContext);
            }
            else
            {TRACE_IT(55760);
                return scriptContext->GetLibrary()->GetNullDisplayString();
            }

        case Js::TypeIds_Number:
            if (Js::NumberUtilities::IsFinite(Js::JavascriptNumber::GetValue(value)))
            {TRACE_IT(55761);
                return Js::JavascriptConversion::ToString(value, scriptContext);
            }
            else
            {TRACE_IT(55762);
                return scriptContext->GetLibrary()->GetNullDisplayString();
            }

        case Js::TypeIds_String:
            return Quote(Js::JavascriptString::FromVar(value));

        default:
            Js::Var ret = undefined;
            if(Js::JavascriptOperators::IsJsNativeObject(value))
            {TRACE_IT(55763);
                if (!Js::JavascriptConversion::IsCallable(value))
                {TRACE_IT(55764);
                    if (objectStack->Has(value))
                    {TRACE_IT(55765);
                        Js::JavascriptError::ThrowTypeError(scriptContext, JSERR_JSONSerializeCircular);
                    }
                    objectStack->Push(value);

                    if(Js::JavascriptOperators::IsArray(value))
                    {TRACE_IT(55766);
                        ret = StringifyArray(value);
                    }
                    else
                    {TRACE_IT(55767);
                        ret = StringifyObject(value);
                    }
                    objectStack->Pop();
                }
            }
            else if (Js::JavascriptOperators::IsObject(value)) //every object which is not a native object gets stringified here
            {TRACE_IT(55768);
                if (objectStack->Has(value, false))
                {TRACE_IT(55769);
                    Js::JavascriptError::ThrowTypeError(scriptContext, JSERR_JSONSerializeCircular);
                }
                objectStack->Push(value, false);
                ret = StringifyObject(value);
                objectStack->Pop(false);
            }
            return ret;
        }
    }

    Js::Var StringifySession::StringifyObject(Js::Var value)
    {TRACE_IT(55770);
        Js::JavascriptString* propertyName;
        Js::PropertyId id;
        Js::PropertyRecord const * propRecord;

        bool isFirstMember = true;
        bool isEmpty = true;

        uint stepBackIndent = this->indent++;
        Js::JavascriptString* memberSeparator = NULL;       // comma  or comma+linefeed+indent
        Js::JavascriptString* indentString = NULL;          // gap*indent
        Js::RecyclableObject* object = Js::RecyclableObject::FromVar(value);
        Js::JavascriptString* result = NULL;

        if(ReplacerArray == this->replacerType)
        {TRACE_IT(55771);
            result = Js::ConcatStringBuilder::New(this->scriptContext, this->replacer.propertyList.length); // Reserve initial slots for properties.

            for (uint k = 0; k < this->replacer.propertyList.length;  k++)
            {TRACE_IT(55772);
                propertyName = replacer.propertyList.propertyNames[k].propName;
                id = replacer.propertyList.propertyNames[k].propRecord->GetPropertyId();

                StringifyMemberObject(propertyName, id, value, (Js::ConcatStringBuilder*)result, indentString, memberSeparator, isFirstMember,  isEmpty);
            }
        }
        else
        {TRACE_IT(55773);
            if (JavascriptProxy::Is(object))
            {TRACE_IT(55774);
                JavascriptProxy* proxyObject = JavascriptProxy::FromVar(object);
                JavascriptArray* proxyResult = proxyObject->PropertyKeysTrap(JavascriptProxy::KeysTrapKind::GetOwnPropertyNamesKind, this->scriptContext);

                // filter enumerable keys
                uint32 resultLength = proxyResult->GetLength();
                result = Js::ConcatStringBuilder::New(this->scriptContext, resultLength);    // Reserve initial slots for properties.
                Var element;
                for (uint32 i = 0; i < resultLength; i++)
                {TRACE_IT(55775);
                    element = proxyResult->DirectGetItem(i);

                    Assert(JavascriptString::Is(element));
                    propertyName = JavascriptString::FromVar(element);

                    PropertyDescriptor propertyDescriptor;
                    JavascriptConversion::ToPropertyKey(propertyName, scriptContext, &propRecord);
                    id = propRecord->GetPropertyId();
                    if (JavascriptOperators::GetOwnPropertyDescriptor(RecyclableObject::FromVar(proxyObject), id, scriptContext, &propertyDescriptor))
                    {TRACE_IT(55776);
                        if (propertyDescriptor.IsEnumerable())
                        {
                            StringifyMemberObject(propertyName, id, value, (Js::ConcatStringBuilder*)result, indentString, memberSeparator, isFirstMember, isEmpty);
                        }
                    }
                }
            }
            else
            {TRACE_IT(55777);
                uint32 precisePropertyCount = 0;
                Js::JavascriptStaticEnumerator enumerator;
                if (object->GetEnumerator(&enumerator, EnumeratorFlags::SnapShotSemantics, scriptContext))
                {TRACE_IT(55778);
                    Js::RecyclableObject *undefined = scriptContext->GetLibrary()->GetUndefined();

                    bool isPrecise;
                    uint32 propertyCount = GetPropertyCount(object, &enumerator, &isPrecise);
                    if (isPrecise)
                    {TRACE_IT(55779);
                        precisePropertyCount = propertyCount;
                    }

                    result = Js::ConcatStringBuilder::New(this->scriptContext, propertyCount);    // Reserve initial slots for properties.

                    if (ReplacerFunction != replacerType)
                    {TRACE_IT(55780);
                        Js::Var propertyNameVar;
                        enumerator.Reset();
                        while ((propertyNameVar = enumerator.MoveAndGetNext(id)) != NULL)
                        {TRACE_IT(55781);
                            if (!Js::JavascriptOperators::IsUndefinedObject(propertyNameVar, undefined))
                            {TRACE_IT(55782);
                                propertyName = Js::JavascriptString::FromVar(propertyNameVar);
                                if (id == Js::Constants::NoProperty)
                                {TRACE_IT(55783);
                                    //if unsuccessful get propertyId from the string
                                    scriptContext->GetOrAddPropertyRecord(propertyName->GetString(), propertyName->GetLength(), &propRecord);
                                    id = propRecord->GetPropertyId();
                                }
                                StringifyMemberObject(propertyName, id, value, (Js::ConcatStringBuilder*)result, indentString, memberSeparator, isFirstMember, isEmpty);
                            }
                        }
                    }
                    else // case: ES5 && ReplacerFunction == replacerType.
                    {TRACE_IT(55784);
                        Js::Var* nameTable = nullptr;
                        // ES5 requires that the new properties introduced by the replacer to not be stringified
                        // Get the actual count first.
                        if (precisePropertyCount == 0)  // Check if it was updated in earlier step.
                        {TRACE_IT(55785);
                            precisePropertyCount = this->GetPropertyCount(object, &enumerator);
                        }

                        // pick the property names before walking the object
                        DECLARE_TEMP_GUEST_ALLOCATOR(nameTableAlloc);
                        if (precisePropertyCount > 0)
                        {TRACE_IT(55786);
                            // allocate and fill a table with the property names
                            if (precisePropertyCount < MAX_JSON_STRINGIFY_NAMES_ON_STACK)
                            {
                                PROBE_STACK(scriptContext, (sizeof(Js::Var) * precisePropertyCount));
                                nameTable = (Js::Var*)_alloca(sizeof(Js::Var) * precisePropertyCount);
                            } else
                            {
                                ACQUIRE_TEMP_GUEST_ALLOCATOR(nameTableAlloc, scriptContext, _u("JSON"));
                                nameTable = AnewArray(nameTableAlloc, Js::Var, precisePropertyCount);
                            }
                            enumerator.Reset();
                            uint32 index = 0;
                            Js::Var propertyNameVar;
                            while ((propertyNameVar = enumerator.MoveAndGetNext(id)) != NULL && index < precisePropertyCount)
                            {TRACE_IT(55787);
                                if (!Js::JavascriptOperators::IsUndefinedObject(propertyNameVar, undefined))
                                {TRACE_IT(55788);
                                    nameTable[index++] = propertyNameVar;
                                }
                            }

                            // walk the property name list
                            for (uint k = 0; k < precisePropertyCount; k++)
                            {TRACE_IT(55789);
                                propertyName = Js::JavascriptString::FromVar(nameTable[k]);
                                scriptContext->GetOrAddPropertyRecord(propertyName->GetString(), propertyName->GetLength(), &propRecord);
                                id = propRecord->GetPropertyId();
                                StringifyMemberObject(propertyName, id, value, (Js::ConcatStringBuilder*)result, indentString, memberSeparator, isFirstMember, isEmpty);
                            }
                        }
                        RELEASE_TEMP_GUEST_ALLOCATOR(nameTableAlloc, scriptContext);
                    }
                }
            }
        }
        Assert(isEmpty || result);

        if(isEmpty)
        {TRACE_IT(55790);
            result = scriptContext->GetLibrary()->CreateStringFromCppLiteral(_u("{}"));
        }
        else
        {TRACE_IT(55791);
            if(this->gap)
            {TRACE_IT(55792);
                if(!indentString)
                {TRACE_IT(55793);
                    indentString = GetIndentString(this->indent);
                }
                // Note: it's better to use strings with length = 1 as the are cached/new instances are not created every time.
                Js::ConcatStringN<7>* retVal = Js::ConcatStringN<7>::New(this->scriptContext);
                retVal->SetItem(0, scriptContext->GetLibrary()->CreateStringFromCppLiteral(_u("{")));
                retVal->SetItem(1, scriptContext->GetLibrary()->CreateStringFromCppLiteral(_u("\n")));
                retVal->SetItem(2, indentString);
                retVal->SetItem(3, result);
                retVal->SetItem(4, scriptContext->GetLibrary()->CreateStringFromCppLiteral(_u("\n")));
                retVal->SetItem(5, GetIndentString(stepBackIndent));
                retVal->SetItem(6, scriptContext->GetLibrary()->CreateStringFromCppLiteral(_u("}")));
                result = retVal;
            }
            else
            {TRACE_IT(55794);
                result = Js::ConcatStringWrapping<_u('{'), _u('}')>::New(result);
            }
        }

        this->indent = stepBackIndent;
        return result;
    }

    Js::JavascriptString* StringifySession::GetArrayElementString(uint32 index, Js::Var arrayVar)
    {TRACE_IT(55795);
        Js::RecyclableObject *undefined = scriptContext->GetLibrary()->GetUndefined();

        Js::Var arrayElement = Str(index, arrayVar);
        if (Js::JavascriptOperators::IsUndefinedObject(arrayElement, undefined))
        {TRACE_IT(55796);
            return scriptContext->GetLibrary()->GetNullDisplayString();
        }
        return Js::JavascriptString::FromVar(arrayElement);
    }

    Js::Var StringifySession::StringifyArray(Js::Var value)
    {TRACE_IT(55797);
        uint stepBackIndent = this->indent++;
        Js::JavascriptString* memberSeparator = NULL;       // comma  or comma+linefeed+indent
        Js::JavascriptString* indentString = NULL;          // gap*indent

        uint32 length;

        if (Js::JavascriptArray::Is(value))
        {TRACE_IT(55798);
            length = Js::JavascriptArray::FromAnyArray(value)->GetLength();
        }
        else
        {TRACE_IT(55799);
            int64 len = Js::JavascriptConversion::ToLength(Js::JavascriptOperators::OP_GetLength(value, scriptContext), scriptContext);
            if (MaxCharCount <= len)
            {TRACE_IT(55800);
                // If the length goes more than MaxCharCount we will eventually fail (as OOM) in ConcatStringBuilder - so failing early.
                JavascriptError::ThrowRangeError(scriptContext, JSERR_OutOfBoundString);
            }
            length = (uint32)len;
        }

        Js::JavascriptString* result;
        if (length == 0)
        {TRACE_IT(55801);
            result = scriptContext->GetLibrary()->CreateStringFromCppLiteral(_u("[]"));
        }
        else
        {TRACE_IT(55802);
            if (length == 1)
            {TRACE_IT(55803);
                result = GetArrayElementString(0, value);
            }
            else
            {TRACE_IT(55804);
                Assert(length > 1);
                if (!indentString)
                {TRACE_IT(55805);
                    indentString = GetIndentString(this->indent);
                    memberSeparator = GetMemberSeparator(indentString);
                }
                bool isFirstMember = true;

                // Total node count: number of array elements (N = length) + indents [including member separators] (N = length - 1).
                result = Js::ConcatStringBuilder::New(this->scriptContext, length * 2 - 1);
                for (uint32 k = 0; k < length; k++)
                {TRACE_IT(55806);
                    if (!isFirstMember)
                    {TRACE_IT(55807);
                        ((Js::ConcatStringBuilder*)result)->Append(memberSeparator);
                    }
                    Js::JavascriptString* arrayElementString = GetArrayElementString(k, value);
                    ((Js::ConcatStringBuilder*)result)->Append(arrayElementString);
                    isFirstMember = false;
                }
            }

            if (this->gap)
            {TRACE_IT(55808);
                if (!indentString)
                {TRACE_IT(55809);
                    indentString = GetIndentString(this->indent);
                }
                Js::ConcatStringN<6>* retVal = Js::ConcatStringN<6>::New(this->scriptContext);
                retVal->SetItem(0, scriptContext->GetLibrary()->CreateStringFromCppLiteral(_u("[\n")));
                retVal->SetItem(1, indentString);
                retVal->SetItem(2, result);
                retVal->SetItem(3, scriptContext->GetLibrary()->CreateStringFromCppLiteral(_u("\n")));
                retVal->SetItem(4, GetIndentString(stepBackIndent));
                retVal->SetItem(5, scriptContext->GetLibrary()->CreateStringFromCppLiteral(_u("]")));
                result = retVal;
            }
            else
            {TRACE_IT(55810);
                result = Js::ConcatStringWrapping<_u('['), _u(']')>::New(result);
            }
        }

        this->indent = stepBackIndent;
        return result;
    }

    Js::JavascriptString* StringifySession::GetPropertySeparator()
    {TRACE_IT(55811);
        if(!propertySeparator)
        {TRACE_IT(55812);
            if(this->gap)
            {TRACE_IT(55813);
                propertySeparator = scriptContext->GetLibrary()->CreateStringFromCppLiteral(_u(": "));
            }
            else
            {TRACE_IT(55814);
                propertySeparator = scriptContext->GetLibrary()->CreateStringFromCppLiteral(_u(":"));
            }
        }
        return propertySeparator;
    }

    Js::JavascriptString* StringifySession::GetIndentString(uint count)
    {TRACE_IT(55815);
        // Note: this potentially can be improved by using a special ConcatString which has gap and count fields.
        //       Although this does not seem to be a critical path (using gap should not be often).
        Js::JavascriptString* res = scriptContext->GetLibrary()->CreateStringFromCppLiteral(_u(""));
        if(this->gap)
        {TRACE_IT(55816);
            for (uint i = 0 ; i < count; i++)
            {TRACE_IT(55817);
                res = Js::JavascriptString::Concat(res, this->gap);
            }
        }
        return res;
    }

    Js::JavascriptString* StringifySession::GetMemberSeparator(Js::JavascriptString* indentString)
    {TRACE_IT(55818);
        if(this->gap)
        {TRACE_IT(55819);
            return Js::JavascriptString::Concat(scriptContext->GetLibrary()->CreateStringFromCppLiteral(_u(",\n")), indentString);
        }
        else
        {TRACE_IT(55820);
            return scriptContext->GetLibrary()->GetCommaDisplayString();
        }
    }

    void StringifySession::StringifyMemberObject( Js::JavascriptString* propertyName, Js::PropertyId id, Js::Var value, Js::ConcatStringBuilder* result, Js::JavascriptString* &indentString, Js::JavascriptString* &memberSeparator, bool &isFirstMember, bool &isEmpty )
    {TRACE_IT(55821);
        Js::Var propertyObjectString = Str(propertyName, id, value);
        if(!Js::JavascriptOperators::IsUndefinedObject(propertyObjectString, scriptContext))
        {TRACE_IT(55822);
            int slotIndex = 0;
            Js::ConcatStringN<4>* tempResult = Js::ConcatStringN<4>::New(this->scriptContext);   // We may use 3 or 4 slots.
            if(!isFirstMember)
            {TRACE_IT(55823);
                if(!indentString)
                {TRACE_IT(55824);
                    indentString = GetIndentString(this->indent);
                    memberSeparator = GetMemberSeparator(indentString);
                }
                tempResult->SetItem(slotIndex++, memberSeparator);
            }
            tempResult->SetItem(slotIndex++, Quote(propertyName));
            tempResult->SetItem(slotIndex++, this->GetPropertySeparator());
            tempResult->SetItem(slotIndex++, Js::JavascriptString::FromVar(propertyObjectString));

            result->Append(tempResult);
            isFirstMember = false;
            isEmpty = false;
        }
    }

    // Returns precise property count for given object and enumerator, does not count properties that are undefined.
    inline uint32 StringifySession::GetPropertyCount(Js::RecyclableObject* object, Js::JavascriptStaticEnumerator* enumerator)
    {TRACE_IT(55825);
        uint32 count = 0;
        Js::Var propertyNameVar;
        Js::PropertyId id;
        enumerator->Reset();
        while ((propertyNameVar = enumerator->MoveAndGetNext(id)) != NULL)
        {TRACE_IT(55826);
            if (!Js::JavascriptOperators::IsUndefinedObject(propertyNameVar, this->scriptContext->GetLibrary()->GetUndefined()))
            {TRACE_IT(55827);
                ++count;
            }
        }
        return count;
    }

    // Returns property count (including array items) for given object and enumerator.
    // When object has objectArray, we do slow path return actual/precise count, in this case *pPrecise will receive true.
    // Otherwise we optimize for speed and try to guess the count, in this case *pPrecise will receive false.
    // Parameters:
    // - object: the object to get the number of properties for.
    // - enumerator: the enumerator to enumerate the object.
    // - [out] pIsPrecise: receives a boolean indicating whether the value returned is precise or just guessed.
    inline uint32 StringifySession::GetPropertyCount(Js::RecyclableObject* object, Js::JavascriptStaticEnumerator* enumerator, bool* pIsPrecise)
    {TRACE_IT(55828);
        Assert(pIsPrecise);
        *pIsPrecise = false;

        uint32 count = object->GetPropertyCount();
        if (Js::DynamicObject::Is(object) && Js::DynamicObject::FromVar(object)->HasObjectArray())
        {TRACE_IT(55829);
            // Can't use array->GetLength() as this can be sparse array for which we stringify only real/set properties.
            // Do one walk through the elements.
            // This would account for prototype property as well.
            count = this->GetPropertyCount(object, enumerator);
            *pIsPrecise = true;
        }
        if (!*pIsPrecise && count > sizeof(Js::JavascriptString*) * 8)
        {TRACE_IT(55830);
            // For large # of elements just one more for potential prototype wouldn't matter.
            ++count;
        }

        return count;
    }

    inline Js::JavascriptString* StringifySession::Quote(Js::JavascriptString* value)
    {TRACE_IT(55831);
        // By default, optimize for scenario when we don't need to change the inside of the string. That's majority of cases.
        return Js::JSONString::Escape<Js::EscapingOperation_NotEscape>(value);
    }
} // namespace JSON
