//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
#include "JSON.h"
#include "JSONParser.h"

using namespace Js;

namespace JSON
{
    // -------- Parser implementation ------------//
    void JSONParser::Finalizer()
    {TRACE_IT(55836);
        m_scanner.Finalizer();
        if(arenaAllocatorObject)
        {TRACE_IT(55837);
            this->scriptContext->ReleaseTemporaryGuestAllocator(arenaAllocatorObject);
        }
    }

    Js::Var JSONParser::Parse(LPCWSTR str, int length)
    {TRACE_IT(55838);
        if (length > MIN_CACHE_LENGTH)
        {TRACE_IT(55839);
            if (!this->arenaAllocatorObject)
            {TRACE_IT(55840);
                this->arenaAllocatorObject = scriptContext->GetTemporaryGuestAllocator(_u("JSONParse"));
                this->arenaAllocator = arenaAllocatorObject->GetAllocator();
            }
        }
        m_scanner.Init(str, length, &m_token, scriptContext, str, this->arenaAllocator);
        Scan();
        Js::Var ret = ParseObject();
        if (m_token.tk != tkEOF)
        {TRACE_IT(55841);
            m_scanner.ThrowSyntaxError(JSERR_JsonSyntax);
        }
        return ret;
    }

    Js::Var JSONParser::Parse(Js::JavascriptString* input)
    {TRACE_IT(55842);
        return Parse(input->GetSz(), input->GetLength());
    }

    Js::Var JSONParser::Walk(Js::JavascriptString* name, Js::PropertyId id, Js::Var holder, uint32 index)
    {
        AssertMsg(reviver, "JSON post parse walk with null reviver");
        Js::Var value;
        Js::Var values[3];
        Js::Arguments args(0, values);
        Js::RecyclableObject *undefined = scriptContext->GetLibrary()->GetUndefined();

        if (Js::DynamicObject::IsAnyArray(holder))
        {TRACE_IT(55843);
            // when called from an array the key is NULL and the keyId is the index.
            value = Js::JavascriptArray::FromAnyArray(holder)->DirectGetItem(id);
            name = scriptContext->GetIntegerString(id);

        }
        else
        {TRACE_IT(55844);
            AssertMsg(Js::JavascriptOperators::GetTypeId(holder) == Js::TypeIds_Object || Js::JavascriptOperators::GetTypeId(holder) == Js::TypeIds_Arguments,
                "The holder argument in a JSON::Walk function must be an object or an array");
            if (id == Constants::NoProperty)
            {TRACE_IT(55845);
                if (!Js::RecyclableObject::FromVar(holder)->GetItem(holder, index, &value, scriptContext))
                {TRACE_IT(55846);
                    value = undefined;
                }
            }
            else
            {TRACE_IT(55847);
                if (!Js::RecyclableObject::FromVar(holder)->GetProperty(holder, id, &value, NULL, scriptContext))
                {TRACE_IT(55848);
                    value = undefined;
                }
            }
        }

        // this is a post order walk. Visit the children before calling walk on this object
        if (Js::DynamicObject::IsAnyArray(value))
        {TRACE_IT(55849);
            Js::JavascriptArray* arrayVal = JavascriptArray::EnsureNonNativeArray(Js::JavascriptArray::FromAnyArray(value));
            Assert(!Js::JavascriptNativeIntArray::Is(arrayVal) && !Js::JavascriptNativeFloatArray::Is(arrayVal));
            uint length = arrayVal->GetLength();
            if (!arrayVal->IsCrossSiteObject())
            {TRACE_IT(55850);
                for(uint k = 0; k < length; k++)
                {TRACE_IT(55851);
                    Js::Var newElement = Walk(0, k, value);
                    if(Js::JavascriptOperators::IsUndefinedObject(newElement, undefined))
                    {TRACE_IT(55852);
                        arrayVal->DirectDeleteItemAt<Js::Var>(k);
                    }
                    else
                    {TRACE_IT(55853);
                        arrayVal->DirectSetItemAt(k, newElement);
                    }
                }
            }
            else
            {TRACE_IT(55854);
                for(uint k = 0; k < length; k++)
                {TRACE_IT(55855);
                    Js::Var newElement = Walk(0, k, value);
                    if(Js::JavascriptOperators::IsUndefinedObject(newElement, undefined))
                    {TRACE_IT(55856);
                        arrayVal->DirectDeleteItemAt<Js::Var>(k);
                    }
                    else
                    {TRACE_IT(55857);
                        arrayVal->SetItem(k, newElement, Js::PropertyOperation_None);
                    }
                }
            }
        }
        else
        {TRACE_IT(55858);
            Js::TypeId typeId = Js::JavascriptOperators::GetTypeId(value);
            if (typeId == Js::TypeIds_Object || typeId == Js::TypeIds_Arguments)
            {TRACE_IT(55859);
                Js::JavascriptStaticEnumerator enumerator;

                // normally we should have a JSON object here and the enumerator should be always be successful. However, the objects can be
                // modified by user code. It is better to skip a damaged object. ES5 spec doesn't specify an error here.
                if(Js::RecyclableObject::FromVar(value)->GetEnumerator(&enumerator, EnumeratorFlags::SnapShotSemantics, scriptContext))
                {TRACE_IT(55860);
                    Js::Var propertyNameVar;

                    while (true)
                    {TRACE_IT(55861);
                        Js::PropertyId idMember = Js::Constants::NoProperty;
                        propertyNameVar = enumerator.MoveAndGetNext(idMember);
                        if (propertyNameVar == nullptr)
                        {TRACE_IT(55862);
                            break;
                        }

                        //NOTE: If testing key value call enumerator->GetCurrentValue() to confirm value is correct;

                        AssertMsg(Js::JavascriptString::Is(propertyNameVar) , "bad enumeration on a JSON Object");

                        if (idMember != Js::Constants::NoProperty)
                        {TRACE_IT(55863);
                            Js::Var newElement = Walk(Js::JavascriptString::FromVar(propertyNameVar), idMember, value);
                            if (Js::JavascriptOperators::IsUndefinedObject(newElement, undefined))
                            {TRACE_IT(55864);
                                Js::JavascriptOperators::DeleteProperty(Js::RecyclableObject::FromVar(value), idMember);
                            }
                            else
                            {TRACE_IT(55865);
                                Js::JavascriptOperators::SetProperty(value, Js::RecyclableObject::FromVar(value), idMember, newElement, scriptContext);
                            }
                        }
                        // For the numeric cases the enumerator is set to a NullEnumerator (see class in ForInObjectEnumerator.h)
                        // Numerals do not have property Ids so we need to set and delete items
                        else
                        {TRACE_IT(55866);
                            uint32 propertyIndex = enumerator.GetCurrentItemIndex();
                            AssertMsg(Js::JavascriptArray::InvalidIndex != propertyIndex, "Not a numeric type");
                            Js::Var newElement = Walk(Js::JavascriptString::FromVar(propertyNameVar), idMember, value, propertyIndex);
                            if (Js::JavascriptOperators::IsUndefinedObject(newElement, undefined))
                            {TRACE_IT(55867);
                                Js::JavascriptOperators::DeleteItem(Js::RecyclableObject::FromVar(value), propertyIndex);

                            }
                            else
                            {TRACE_IT(55868);
                                Js::JavascriptOperators::SetItem(value, Js::RecyclableObject::FromVar(value), propertyIndex, newElement, scriptContext);
                            }

                        }
                    }
                }
            }
        }

        // apply reviver on this node now
        args.Info.Count = 3;
        args.Values[0] = holder;
        args.Values[1] = name;
        args.Values[2] = value;
        value = Js::JavascriptFunction::CallFunction<true>(reviver, reviver->GetEntryPoint(), args);
        return value;
    }

    Js::Var JSONParser::ParseObject()
    {
        PROBE_STACK(scriptContext, Js::Constants::MinStackDefault);

        Js::Var retVal;

        switch (m_token.tk)
        {

        case tkFltCon:
            retVal = Js::JavascriptNumber::ToVarIntCheck(m_token.GetDouble(), scriptContext);
            Scan();
            return retVal;

        case tkStrCon:
            {TRACE_IT(55869);
                // will auto-null-terminate the string (as length=len+1)
                uint len = m_scanner.GetCurrentStringLen();
                retVal = Js::JavascriptString::NewCopyBuffer(m_scanner.GetCurrentString(), len, scriptContext);
                Scan();
                return retVal;
            }

        case tkTRUE:
            retVal = scriptContext->GetLibrary()->GetTrue();
            Scan();
            return retVal;

        case tkFALSE:
            retVal = scriptContext->GetLibrary()->GetFalse();
            Scan();
            return retVal;

        case tkNULL:
            retVal = scriptContext->GetLibrary()->GetNull();
            Scan();
            return retVal;

        case tkSub:  // unary minus

            if (Scan() == tkFltCon)
            {TRACE_IT(55870);
                retVal = Js::JavascriptNumber::ToVarIntCheck(-m_token.GetDouble(), scriptContext);
                Scan();
                return retVal;
            }
            else
            {TRACE_IT(55871);
                m_scanner.ThrowSyntaxError(JSERR_JsonBadNumber);
            }

        case tkLBrack:
            {TRACE_IT(55872);

                Js::JavascriptArray* arrayObj = scriptContext->GetLibrary()->CreateArray(0);

                //skip '['
                Scan();

                //iterate over the array members, get JSON objects and add them in the pArrayMemberList
                uint k = 0;
                while (true)
                {TRACE_IT(55873);
                    if(tkRBrack == m_token.tk)
                    {TRACE_IT(55874);
                        break;
                    }
                    Js::Var value = ParseObject();
                    arrayObj->SetItem(k++, value, Js::PropertyOperation_None);

                    // if next token is not a comma consider the end of the array member list.
                    if (tkComma != m_token.tk)
                        break;
                    Scan();
                    if(tkRBrack == m_token.tk)
                    {TRACE_IT(55875);
                        m_scanner.ThrowSyntaxError(JSERR_JsonIllegalChar);
                    }
                }
                //check and consume the ending ']'
                CheckCurrentToken(tkRBrack, JSERR_JsonNoRbrack);
                return arrayObj;

            }

        case tkLCurly:
            {TRACE_IT(55876);

                // Parse an object, "{"name1" : ObjMember1, "name2" : ObjMember2, ...} "
                if(IsCaching())
                {TRACE_IT(55877);
                    if(!typeCacheList)
                    {TRACE_IT(55878);
                        typeCacheList = Anew(this->arenaAllocator, JsonTypeCacheList, this->arenaAllocator, 8);
                    }
                }

                // first, create the object
                Js::DynamicObject* object = scriptContext->GetLibrary()->CreateObject();
                JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_OBJECT(object));
#if ENABLE_DEBUG_CONFIG_OPTIONS
                if (Js::Configuration::Global.flags.IsEnabled(Js::autoProxyFlag))
                {TRACE_IT(55879);
                    object = DynamicObject::FromVar(JavascriptProxy::AutoProxyWrapper(object));
                }
#endif

                //next token after '{'
                Scan();

                //if empty object "{}" return;
                if(tkRCurly == m_token.tk)
                {TRACE_IT(55880);
                    Scan();
                    return object;
                }
                JsonTypeCache* previousCache = nullptr;
                JsonTypeCache* currentCache = nullptr;
                //parse the list of members
                while(true)
                {TRACE_IT(55881);
                    // parse a list member:  "name" : ObjMember
                    // and add it to the object.

                    //pick "name"
                    if(tkStrCon != m_token.tk)
                    {TRACE_IT(55882);
                        m_scanner.ThrowSyntaxError(JSERR_JsonIllegalChar);
                    }

                    // currentStrLength = length w/o null-termination
                    WCHAR* currentStr = m_scanner.GetCurrentString();
                    uint currentStrLength = m_scanner.GetCurrentStringLen();

                    DynamicType* typeWithoutProperty = object->GetDynamicType();
                    if(IsCaching())
                    {TRACE_IT(55883);
                        if(!previousCache)
                        {TRACE_IT(55884);
                            // This is the first property in the list - see if we have an existing cache for it.
                            currentCache = typeCacheList->LookupWithKey(Js::HashedCharacterBuffer<WCHAR>(currentStr, currentStrLength), nullptr);
                        }
                        if(currentCache && currentCache->typeWithoutProperty == typeWithoutProperty &&
                            currentCache->propertyRecord->Equals(JsUtil::CharacterBuffer<WCHAR>(currentStr, currentStrLength)))
                        {TRACE_IT(55885);
                            //check and consume ":"
                            if(Scan() != tkColon )
                            {TRACE_IT(55886);
                                m_scanner.ThrowSyntaxError(JSERR_JsonNoColon);
                            }
                            Scan();

                            // Cache all values from currentCache as there is a chance that ParseObject might change the cache
                            DynamicType* typeWithProperty = currentCache->typeWithProperty;
                            PropertyId propertyId = currentCache->propertyRecord->GetPropertyId();
                            PropertyIndex propertyIndex = currentCache->propertyIndex;
                            previousCache = currentCache;
                            currentCache = currentCache->next;

                            // fast path for type transition and property set
                            object->EnsureSlots(typeWithoutProperty->GetTypeHandler()->GetSlotCapacity(),
                                typeWithProperty->GetTypeHandler()->GetSlotCapacity(), scriptContext, typeWithProperty->GetTypeHandler());
                            object->ReplaceType(typeWithProperty);
                            Js::Var value = ParseObject();
                            object->SetSlot(SetSlotArguments(propertyId, propertyIndex, value));

                            // if the next token is not a comma consider the list of members done.
                            if (tkComma != m_token.tk)
                                break;
                            Scan();
                            continue;
                        }
                    }

                    // slow path
                    Js::PropertyRecord const * propertyRecord;
                    scriptContext->GetOrAddPropertyRecord(currentStr, currentStrLength, &propertyRecord);

                    //check and consume ":"
                    if(Scan() != tkColon )
                    {TRACE_IT(55887);
                        m_scanner.ThrowSyntaxError(JSERR_JsonNoColon);
                    }
                    Scan();
                    Js::Var value = ParseObject();
                    PropertyValueInfo info;
                    object->SetProperty(propertyRecord->GetPropertyId(), value, PropertyOperation_None, &info);

                    DynamicType* typeWithProperty = object->GetDynamicType();
                    if(IsCaching() && !propertyRecord->IsNumeric() && !info.IsNoCache() && typeWithProperty->GetIsShared() && typeWithProperty->GetTypeHandler()->IsPathTypeHandler())
                    {TRACE_IT(55888);
                        PropertyIndex propertyIndex = info.GetPropertyIndex();

                        if(!previousCache)
                        {TRACE_IT(55889);
                            // This is the first property in the set add it to the dictionary.
                            currentCache = JsonTypeCache::New(this->arenaAllocator, propertyRecord, typeWithoutProperty, typeWithProperty, propertyIndex);
                            typeCacheList->AddNew(propertyRecord, currentCache);
                        }
                        else if(!currentCache)
                        {TRACE_IT(55890);
                            currentCache = JsonTypeCache::New(this->arenaAllocator, propertyRecord, typeWithoutProperty, typeWithProperty, propertyIndex);
                            previousCache->next = currentCache;
                        }
                        else
                        {TRACE_IT(55891);
                            // cache miss!!
                            currentCache->Update(propertyRecord, typeWithoutProperty, typeWithProperty, propertyIndex);
                        }
                        previousCache = currentCache;
                        currentCache = currentCache->next;
                    }

                    // if the next token is not a comma consider the list of members done.
                    if (tkComma != m_token.tk)
                        break;
                    Scan();
                }

                // check  and consume the ending '}"
                CheckCurrentToken(tkRCurly, JSERR_JsonNoRcurly);
                return object;
            }

        default:
            m_scanner.ThrowSyntaxError(JSERR_JsonSyntax);
        }
    }
} // namespace JSON
