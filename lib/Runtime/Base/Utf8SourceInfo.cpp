//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"
#include "Debug/DiagProbe.h"
#include "Debug/BreakpointProbe.h"
#include "Debug/DebugDocument.h"
#include "Debug/DebugManager.h"

namespace Js
{
    // if m_cchLength < 0 it came from an external source.
    // If m_cbLength > abs(m_cchLength) then m_utf8Source contains non-ASCII (multi-byte encoded) characters.

    Utf8SourceInfo::Utf8SourceInfo(ISourceHolder* mappableSource, int32 cchLength,
        SRCINFO const* srcInfo, DWORD_PTR secondaryHostSourceContext,
        ScriptContext* scriptContext, bool isLibraryCode, Js::Var scriptSource):
        sourceHolder(mappableSource),
        m_cchLength(cchLength),
        m_pHostBuffer(nullptr),
        m_srcInfo(srcInfo),
        m_secondaryHostSourceContext(secondaryHostSourceContext),
        m_debugDocument(nullptr),
        m_sourceInfoId(scriptContext->GetThreadContext()->NewSourceInfoNumber()),
        m_hasHostBuffer(false),
        m_isCesu8(false),
        m_isLibraryCode(isLibraryCode),
        m_isXDomain(false),
        m_isXDomainString(false),
        m_scriptContext(scriptContext),
        m_lineOffsetCache(nullptr),
        m_deferredFunctionsDictionary(nullptr),
        m_deferredFunctionsInitialized(false),
        topLevelFunctionInfoList(nullptr),
        debugModeSource(nullptr),
        debugModeSourceIsEmpty(false),
        debugModeSourceLength(0),
        m_isInDebugMode(false),
        callerUtf8SourceInfo(nullptr)
#ifndef NTBUILD
        ,sourceRef(scriptSource)
#endif
    {
        if (!sourceHolder->IsDeferrable())
        {TRACE_IT(37860);
            this->debugModeSource = this->sourceHolder->GetSource(_u("Entering Debug Mode"));
            this->debugModeSourceLength = this->sourceHolder->GetByteLength(_u("Entering Debug Mode"));
            this->debugModeSourceIsEmpty = !this->HasSource() || this->debugModeSource == nullptr;
        }
    }

    LPCUTF8 Utf8SourceInfo::GetSource(const char16 * reason) const
    {TRACE_IT(37861);
        AssertMsg(this->sourceHolder != nullptr, "We have no source mapper.");
        if (this->IsInDebugMode())
        {TRACE_IT(37862);
            AssertMsg(this->debugModeSource != nullptr || this->debugModeSourceIsEmpty, "Debug mode source should have been set by this point.");
            return debugModeSource;
        }
        else
        {TRACE_IT(37863);
            return sourceHolder->GetSource(reason == nullptr ? _u("Utf8SourceInfo::GetSource") : reason);
        }
    }

    size_t Utf8SourceInfo::GetCbLength(const char16 * reason) const
    {TRACE_IT(37864);
        AssertMsg(this->sourceHolder != nullptr, "We have no source mapper.");
        if (this->IsInDebugMode())
        {TRACE_IT(37865);
            AssertMsg(this->debugModeSource != nullptr || this->debugModeSourceIsEmpty, "Debug mode source should have been set by this point.");
            return debugModeSourceLength;
        }
        else
        {TRACE_IT(37866);
            return sourceHolder->GetByteLength(reason == nullptr ? _u("Utf8SourceInfo::GetSource") : reason);
        }
    }


    void
    Utf8SourceInfo::Dispose(bool isShutdown)
    {TRACE_IT(37867);
        ClearDebugDocument();
#ifndef NTBUILD
        this->sourceRef = nullptr;
#endif
        this->debugModeSource = nullptr;
        if (this->m_hasHostBuffer)
        {
            PERF_COUNTER_DEC(Basic, ScriptCodeBufferCount);
            HeapFree(GetProcessHeap(), 0 , m_pHostBuffer);
            m_pHostBuffer = nullptr;
        }
    };

    void
    Utf8SourceInfo::SetHostBuffer(BYTE * pcszCode)
    {TRACE_IT(37868);
        Assert(!this->m_hasHostBuffer);
        Assert(this->m_pHostBuffer == nullptr);
        this->m_hasHostBuffer = true;
        this->m_pHostBuffer = pcszCode;
    }
    enum
    {
        fsiHostManaged = 0x01,
        fsiScriptlet   = 0x02,
        fsiDeferredParse = 0x04
    };

    void Utf8SourceInfo::RemoveFunctionBody(FunctionBody* functionBody)
    {TRACE_IT(37869);
        Assert(this->functionBodyDictionary);

        const LocalFunctionId functionId = functionBody->GetLocalFunctionId();
        Assert(functionBodyDictionary->Item(functionId) == functionBody);

        functionBodyDictionary->Remove(functionId);
        functionBody->SetIsFuncRegistered(false);
    }

    void Utf8SourceInfo::SetFunctionBody(FunctionBody * functionBody)
    {TRACE_IT(37870);
        Assert(this->m_scriptContext == functionBody->GetScriptContext());
        Assert(this->functionBodyDictionary);

        // Only register a function body when source info is ready. Note that m_pUtf8Source can still be null for lib script.
        Assert(functionBody->GetSourceIndex() != Js::Constants::InvalidSourceIndex);
        Assert(!functionBody->GetIsFuncRegistered());

        const LocalFunctionId functionId = functionBody->GetLocalFunctionId();
        FunctionBody* oldFunctionBody = nullptr;
        if (functionBodyDictionary->TryGetValue(functionId, &oldFunctionBody)) {TRACE_IT(37871);
            Assert(oldFunctionBody != functionBody);
            oldFunctionBody->SetIsFuncRegistered(false);
        }

        functionBodyDictionary->Item(functionId, functionBody);
        functionBody->SetIsFuncRegistered(true);
    }

    void Utf8SourceInfo::AddTopLevelFunctionInfo(FunctionInfo * functionInfo, Recycler * recycler)
    {TRACE_IT(37872);
        JsUtil::List<FunctionInfo *, Recycler> * list = EnsureTopLevelFunctionInfoList(recycler);
        Assert(!list->Contains(functionInfo));
        list->Add(functionInfo);
    }

    void Utf8SourceInfo::ClearTopLevelFunctionInfoList()
    {TRACE_IT(37873);
        if (this->topLevelFunctionInfoList)
        {TRACE_IT(37874);
            this->topLevelFunctionInfoList->Clear();
        }
    }

    JsUtil::List<FunctionInfo *, Recycler> *
    Utf8SourceInfo::EnsureTopLevelFunctionInfoList(Recycler * recycler)
    {TRACE_IT(37875);
        if (this->topLevelFunctionInfoList == nullptr)
        {TRACE_IT(37876);
            this->topLevelFunctionInfoList = JsUtil::List<FunctionInfo *, Recycler>::New(recycler);
        }
        return this->topLevelFunctionInfoList;
    }

    void Utf8SourceInfo::EnsureInitialized(int initialFunctionCount)
    {TRACE_IT(37877);
        ThreadContext* threadContext = ThreadContext::GetContextForCurrentThread();
        Recycler* recycler = threadContext->GetRecycler();

        if (this->functionBodyDictionary == nullptr)
        {TRACE_IT(37878);
            // This collection is allocated with leaf allocation policy. The references to the function body
            // here does not keep the function alive. However, the functions remove themselves at finalize
            // so if a function actually is in this map, it means that it is alive.
            this->functionBodyDictionary = RecyclerNew(recycler, FunctionBodyDictionary, recycler,
                initialFunctionCount, threadContext->GetEtwRundownCriticalSection());
        }

        if (CONFIG_FLAG(DeferTopLevelTillFirstCall) && !m_deferredFunctionsInitialized)
        {TRACE_IT(37879);
            Assert(this->m_deferredFunctionsDictionary == nullptr);
            this->m_deferredFunctionsDictionary = RecyclerNew(recycler, DeferredFunctionsDictionary, recycler,
                initialFunctionCount, threadContext->GetEtwRundownCriticalSection());
            m_deferredFunctionsInitialized = true;
        }
    }

    Utf8SourceInfo*
    Utf8SourceInfo::NewWithHolder(ScriptContext* scriptContext, ISourceHolder* sourceHolder,
        int32 length, SRCINFO const* srcInfo, bool isLibraryCode, Js::Var scriptSource)
    {TRACE_IT(37880);
        // TODO: make this finalizable? Or have a finalizable version which would HeapDelete the string? Is this needed?
        DWORD_PTR secondaryHostSourceContext = Js::Constants::NoHostSourceContext;
        if (srcInfo->sourceContextInfo->IsDynamic())
        {TRACE_IT(37881);
            secondaryHostSourceContext = scriptContext->GetThreadContext()->GetDebugManager()->AllocateSecondaryHostSourceContext();
        }

        Recycler * recycler = scriptContext->GetRecycler();

        Utf8SourceInfo* toReturn = RecyclerNewFinalized(recycler,
            Utf8SourceInfo, sourceHolder, length, SRCINFO::Copy(recycler, srcInfo),
            secondaryHostSourceContext, scriptContext, isLibraryCode, scriptSource);

        if (!isLibraryCode && scriptContext->IsScriptContextInDebugMode())
        {TRACE_IT(37882);
            toReturn->debugModeSource = sourceHolder->GetSource(_u("Debug Mode Loading"));
            toReturn->debugModeSourceLength = sourceHolder->GetByteLength(_u("Debug Mode Loading"));
            toReturn->debugModeSourceIsEmpty = toReturn->debugModeSource == nullptr || sourceHolder->IsEmpty();
        }

        return toReturn;
    }

    Utf8SourceInfo*
    Utf8SourceInfo::New(ScriptContext* scriptContext, LPCUTF8 utf8String, int32 length,
        size_t numBytes, SRCINFO const* srcInfo, bool isLibraryCode)
    {TRACE_IT(37883);
        utf8char_t * newUtf8String = RecyclerNewArrayLeaf(scriptContext->GetRecycler(), utf8char_t, numBytes + 1);
        js_memcpy_s(newUtf8String, numBytes + 1, utf8String, numBytes + 1);
        return NewWithNoCopy(scriptContext, newUtf8String, length, numBytes,
            srcInfo, isLibraryCode);
    }

    Utf8SourceInfo*
    Utf8SourceInfo::NewWithNoCopy(ScriptContext* scriptContext, LPCUTF8 utf8String,
        int32 length, size_t numBytes, SRCINFO const * srcInfo, bool isLibraryCode, Js::Var scriptSource)
    {TRACE_IT(37884);
        ISourceHolder* sourceHolder = RecyclerNew(scriptContext->GetRecycler(), SimpleSourceHolder, utf8String, numBytes);

        return NewWithHolder(scriptContext, sourceHolder, length, srcInfo, isLibraryCode, scriptSource);
    }


    Utf8SourceInfo*
    Utf8SourceInfo::Clone(ScriptContext* scriptContext, const Utf8SourceInfo* sourceInfo)
    {TRACE_IT(37885);
        Utf8SourceInfo* newSourceInfo = Utf8SourceInfo::NewWithHolder(scriptContext,
            sourceInfo->GetSourceHolder()->Clone(scriptContext), sourceInfo->m_cchLength,
            SRCINFO::Copy(scriptContext->GetRecycler(), sourceInfo->GetSrcInfo()),
            sourceInfo->m_isLibraryCode);
        newSourceInfo->m_isXDomain = sourceInfo->m_isXDomain;
        newSourceInfo->m_isXDomainString = sourceInfo->m_isXDomainString;
        newSourceInfo->m_isLibraryCode = sourceInfo->m_isLibraryCode;
        newSourceInfo->SetIsCesu8(sourceInfo->GetIsCesu8());
        newSourceInfo->m_lineOffsetCache = sourceInfo->m_lineOffsetCache;

        if (scriptContext->IsScriptContextInDebugMode() && !newSourceInfo->GetIsLibraryCode())
        {TRACE_IT(37886);
            newSourceInfo->SetInDebugMode(true);
        }
        return newSourceInfo;
    }

    HRESULT Utf8SourceInfo::EnsureLineOffsetCacheNoThrow()
    {TRACE_IT(37887);
        HRESULT hr = S_OK;
        // This is a double check, otherwise we would have to have a private function, and add an assert.
        // Basically the outer check is for try/catch, inner check (inside EnsureLineOffsetCache) is for that method as its public.
        if (this->m_lineOffsetCache == nullptr)
        {TRACE_IT(37888);
            BEGIN_TRANSLATE_EXCEPTION_AND_ERROROBJECT_TO_HRESULT_NESTED
            {TRACE_IT(37889);
                this->EnsureLineOffsetCache();
            }
            END_TRANSLATE_EXCEPTION_AND_ERROROBJECT_TO_HRESULT_NOASSERT(hr);
        }
        return hr;
    }

    void Utf8SourceInfo::EnsureLineOffsetCache()
    {TRACE_IT(37890);
        if (this->m_lineOffsetCache == nullptr)
        {TRACE_IT(37891);
            LPCUTF8 sourceStart = this->GetSource(_u("Utf8SourceInfo::AllocateLineOffsetCache"));
            LPCUTF8 sourceEnd = sourceStart + this->GetCbLength(_u("Utf8SourceInfo::AllocateLineOffsetCache"));

            LPCUTF8 sourceAfterBOM = sourceStart;
            charcount_t startChar = FunctionBody::SkipByteOrderMark(sourceAfterBOM /* byref */);
            int64 byteStartOffset = (sourceAfterBOM - sourceStart);

            Recycler* recycler = this->m_scriptContext->GetRecycler();
            this->m_lineOffsetCache = RecyclerNew(recycler, JsUtil::LineOffsetCache<Recycler>, recycler, sourceAfterBOM, sourceEnd, startChar, (int)byteStartOffset);
        }
    }

    Js::FunctionBody* Utf8SourceInfo::FindFunction(Js::LocalFunctionId id) const
    {TRACE_IT(37892);
        Js::FunctionBody *matchedFunctionBody = nullptr;

        if (this->functionBodyDictionary)
        {TRACE_IT(37893);
            // Ignore return value - OK if function is not found.
            this->functionBodyDictionary->TryGetValue(id, &matchedFunctionBody);

            if (matchedFunctionBody == nullptr || matchedFunctionBody->IsPartialDeserializedFunction())
            {TRACE_IT(37894);
                return nullptr;
            }
        }

        return matchedFunctionBody;
    }

    void Utf8SourceInfo::GetLineInfoForCharPosition(charcount_t charPosition, charcount_t *outLineNumber, charcount_t *outColumn, charcount_t *outLineByteOffset, bool allowSlowLookup)
    {TRACE_IT(37895);
        AssertMsg(this->m_lineOffsetCache != nullptr || allowSlowLookup, "LineOffsetCache wasn't created, EnsureLineOffsetCache should have been called.");
        AssertMsg(outLineNumber != nullptr && outColumn != nullptr && outLineByteOffset != nullptr, "Expected out parameter's can't be a nullptr.");

        charcount_t lineCharOffset = 0;
        int line = 0;
        if (this->m_lineOffsetCache == nullptr)
        {TRACE_IT(37896);
            LPCUTF8 sourceStart = this->GetSource(_u("Utf8SourceInfo::AllocateLineOffsetCache"));
            LPCUTF8 sourceEnd = sourceStart + this->GetCbLength(_u("Utf8SourceInfo::AllocateLineOffsetCache"));

            LPCUTF8 sourceAfterBOM = sourceStart;
            lineCharOffset = FunctionBody::SkipByteOrderMark(sourceAfterBOM /* byref */);
            Assert((sourceAfterBOM - sourceStart) < MAXUINT32);
            charcount_t byteStartOffset = (charcount_t)(sourceAfterBOM - sourceStart);

            line = JsUtil::LineOffsetCache<Recycler>::FindLineForCharacterOffset(sourceAfterBOM, sourceEnd, lineCharOffset, byteStartOffset, charPosition);

            *outLineByteOffset = byteStartOffset;
        }
        else
        {TRACE_IT(37897);
            line = this->m_lineOffsetCache->GetLineForCharacterOffset(charPosition, &lineCharOffset, outLineByteOffset);
        }

        Assert(charPosition >= lineCharOffset);

        *outLineNumber = line;
        *outColumn = charPosition - lineCharOffset;
    }

    void Utf8SourceInfo::CreateLineOffsetCache(const JsUtil::LineOffsetCache<Recycler>::LineOffsetCacheItem *items, charcount_t numberOfItems)
    {TRACE_IT(37898);
        AssertMsg(this->m_lineOffsetCache == nullptr, "LineOffsetCache is already initialized!");
        Recycler* recycler = this->m_scriptContext->GetRecycler();
        this->m_lineOffsetCache = RecyclerNew(recycler, JsUtil::LineOffsetCache<Recycler>, recycler, items, numberOfItems);
    }

    DWORD_PTR Utf8SourceInfo::GetHostSourceContext() const
    {TRACE_IT(37899);
        return m_srcInfo->sourceContextInfo->dwHostSourceContext;
    }

    bool Utf8SourceInfo::IsDynamic() const
    {TRACE_IT(37900);
        return m_srcInfo->sourceContextInfo->IsDynamic();
    }

    SourceContextInfo* Utf8SourceInfo::GetSourceContextInfo() const
    {TRACE_IT(37901);
        return this->m_srcInfo->sourceContextInfo;
    }

    // Get's the first function in the function body dictionary
    // Used if the caller want's any function in this source info
    Js::FunctionBody* Utf8SourceInfo::GetAnyParsedFunction()
    {TRACE_IT(37902);
        if (this->functionBodyDictionary != nullptr && this->functionBodyDictionary->Count() > 0)
        {TRACE_IT(37903);
            FunctionBody* functionBody = nullptr;
            int i = 0;
            do
            {TRACE_IT(37904);
                functionBody = this->functionBodyDictionary->GetValueAt(i);
                if (functionBody != nullptr && functionBody->GetByteCode() == nullptr && !functionBody->GetIsFromNativeCodeModule()) functionBody = nullptr;
                i++;
            }
            while (functionBody == nullptr && i < this->functionBodyDictionary->Count());

            return functionBody;
        }

        return nullptr;
    }


    bool Utf8SourceInfo::IsHostManagedSource() const
    {TRACE_IT(37905);
        return ((this->m_srcInfo->grfsi & fsiHostManaged) == fsiHostManaged);
    }

    void Utf8SourceInfo::SetCallerUtf8SourceInfo(Utf8SourceInfo* callerUtf8SourceInfo)
    {TRACE_IT(37906);
        this->callerUtf8SourceInfo = callerUtf8SourceInfo;
    }

    Utf8SourceInfo* Utf8SourceInfo::GetCallerUtf8SourceInfo() const
    {TRACE_IT(37907);
        return this->callerUtf8SourceInfo;
    }

    void Utf8SourceInfo::TrackDeferredFunction(Js::LocalFunctionId functionID, Js::ParseableFunctionInfo *function)
    {TRACE_IT(37908);
        if (this->m_scriptContext->DoUndeferGlobalFunctions())
        {TRACE_IT(37909);
            Assert(m_deferredFunctionsInitialized);
            if (this->m_deferredFunctionsDictionary != nullptr)
            {TRACE_IT(37910);
                this->m_deferredFunctionsDictionary->Add(functionID, function);
            }
        }
    }

    void Utf8SourceInfo::StopTrackingDeferredFunction(Js::LocalFunctionId functionID)
    {TRACE_IT(37911);
        if (this->m_scriptContext->DoUndeferGlobalFunctions())
        {TRACE_IT(37912);
            Assert(m_deferredFunctionsInitialized);
            if (this->m_deferredFunctionsDictionary != nullptr)
            {TRACE_IT(37913);
                this->m_deferredFunctionsDictionary->Remove(functionID);
            }
        }
    }

    void Utf8SourceInfo::ClearDebugDocument(bool close)
    {TRACE_IT(37914);
        if (this->m_debugDocument != nullptr)
        {TRACE_IT(37915);
            if (close)
            {TRACE_IT(37916);
                m_debugDocument->CloseDocument();
            }

            this->m_debugDocument = nullptr;
        }
    }

    bool Utf8SourceInfo::GetDebugDocumentName(BSTR * sourceName)
    {TRACE_IT(37917);
#if defined(ENABLE_SCRIPT_DEBUGGING) && defined(_WIN32)
        if (this->HasDebugDocument() && this->GetDebugDocument()->HasDocumentText())
        {TRACE_IT(37918);
            // ToDo (SaAgarwa): Fix for JsRT debugging
            IDebugDocumentText *documentText = static_cast<IDebugDocumentText *>(this->GetDebugDocument()->GetDocumentText());
            if (documentText->GetName(DOCUMENTNAMETYPE_URL, sourceName) == S_OK)
            {TRACE_IT(37919);
                return true;
            }
        }
#endif
        return false;
    }
}
