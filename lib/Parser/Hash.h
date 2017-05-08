//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

// StaticSym contains a string literal at the end (flexible array) and is
// meant to be initialized statically. However, flexible array initialization
// is not allowed in standard C++. We declare each StaticSym with length
// instead and cast to common StaticSymLen<0>* (StaticSym*) to access.
template <uint32 N>
struct StaticSymLen
{
    uint32 luHash;
    uint32 cch;
    OLECHAR sz[N];
};

typedef StaticSymLen<0> StaticSym;

/***************************************************************************
Hashing functions. Definitions in core\hashfunc.cpp.
***************************************************************************/
ULONG CaseSensitiveComputeHash(LPCOLESTR prgch, LPCOLESTR end);
ULONG CaseSensitiveComputeHash(LPCUTF8 prgch, LPCUTF8 end);
ULONG CaseInsensitiveComputeHash(LPCOLESTR posz);

enum
{
    fidNil          = 0x0000,
    fidKwdRsvd      = 0x0001,     // the keyword is a reserved word
    fidKwdFutRsvd   = 0x0002,     // a future reserved word, but only in strict mode

    // Flags to identify tracked aliases of "eval"
    fidEval         = 0x0008,
    // Flags to identify tracked aliases of "let"
    fidLetOrConst   = 0x0010,     // ID has previously been used in a block-scoped declaration

    // This flag is used by the Parser CountDcls and FillDcls methods.
    // CountDcls sets the bit as it walks through the var decls so that
    // it can skip duplicates. FillDcls clears the bit as it walks through
    // again to skip duplicates.
    fidGlobalDcl    = 0x2000,

    fidUsed         = 0x4000,  // name referenced by source code

    fidModuleExport = 0x8000    // name is module export
};

struct BlockIdsStack
{
    int id;
    BlockIdsStack *prev;
};

class Span
{
    charcount_t m_ichMin;
    charcount_t m_ichLim;

public:
    Span(): m_ichMin((charcount_t)-1), m_ichLim((charcount_t)-1) {TRACE_IT(29195); }
    Span(charcount_t ichMin, charcount_t ichLim): m_ichMin(ichMin), m_ichLim(ichLim) {TRACE_IT(29196); }

    charcount_t GetIchMin() {TRACE_IT(29197); return m_ichMin; }
    charcount_t GetIchLim() {TRACE_IT(29198); Assert(m_ichMin != (charcount_t)-1); return m_ichLim; }
    void Set(charcount_t ichMin, charcount_t ichLim)
    {TRACE_IT(29199);
        m_ichMin = ichMin;
        m_ichLim = ichLim;
    }

    operator bool() {TRACE_IT(29200); return m_ichMin != -1; }
};

struct PidRefStack
{
    PidRefStack() : isAsg(false), isDynamic(false), id(0), funcId(0), sym(nullptr), prev(nullptr), isEscape(false), isModuleExport(false), isFuncAssignment(false) {TRACE_IT(29201);}
    PidRefStack(int id, Js::LocalFunctionId funcId) : isAsg(false), isDynamic(false), id(id), funcId(funcId), sym(nullptr), prev(nullptr), isEscape(false), isModuleExport(false), isFuncAssignment(false) {TRACE_IT(29202);}

    int GetScopeId() const    {TRACE_IT(29203); return id; }
    Js::LocalFunctionId GetFuncScopeId() const {TRACE_IT(29204); return funcId; }
    Symbol *GetSym() const    {TRACE_IT(29205); return sym; }
    void SetSym(Symbol *sym)  {TRACE_IT(29206); this->sym = sym; }
    bool IsAssignment() const {TRACE_IT(29207); return isAsg; }
    bool IsFuncAssignment() const {TRACE_IT(29208); return isFuncAssignment; }
    bool IsEscape() const {TRACE_IT(29209); return isEscape; }
    void SetIsEscape(bool is) {TRACE_IT(29210); isEscape = is; }
    bool IsDynamicBinding() const {TRACE_IT(29211); return isDynamic; }
    void SetDynamicBinding()  {TRACE_IT(29212); isDynamic = true; }

    Symbol **GetSymRef()
    {TRACE_IT(29213);
        return &sym;
    }

    bool           isAsg;
    bool           isDynamic;
    bool           isModuleExport;
    bool           isEscape;
    bool           isFuncAssignment;
    int            id;
    Js::LocalFunctionId funcId;
    Symbol        *sym;
    PidRefStack   *prev;
};

enum AssignmentState : byte {
    NotAssigned,
    AssignedOnce,
    AssignedMultipleTimes
};

struct Ident
{
    friend class HashTbl;

private:
    Ident * m_pidNext;   // next identifier in this hash bucket
    PidRefStack *m_pidRefStack;
    ushort m_tk;         // token# if identifier is a keyword
    ushort m_grfid;      // see fidXXX above
    uint32 m_luHash;      // hash value

    uint32 m_cch;                   // length of the identifier spelling
    Js::PropertyId m_propertyId;

    AssignmentState assignmentState;

    OLECHAR m_sz[]; // the spelling follows (null terminated)

    void SetTk(tokens tk, ushort grfid);
public:
    LPCOLESTR Psz(void)
    {TRACE_IT(29214); return m_sz; }
    uint32 Cch(void)
    {TRACE_IT(29215); return m_cch; }
    tokens Tk(bool isStrictMode);
    uint32 Hash(void)
    {TRACE_IT(29216); return m_luHash; }

    PidRefStack *GetTopRef() const
    {TRACE_IT(29217);
        return m_pidRefStack;
    }

    void SetTopRef(PidRefStack *ref)
    {TRACE_IT(29218);
        m_pidRefStack = ref;
    }

    void PromoteAssignmentState()
    {TRACE_IT(29219);
        if (assignmentState == NotAssigned)
        {TRACE_IT(29220);
            assignmentState = AssignedOnce;
        }
        else if (assignmentState == AssignedOnce)
        {TRACE_IT(29221);
            assignmentState = AssignedMultipleTimes;
        }
    }

    bool IsSingleAssignment()
    {TRACE_IT(29222);
        return assignmentState == AssignedOnce;
    }

    PidRefStack *GetPidRefForScopeId(int scopeId)
    {TRACE_IT(29223);
        PidRefStack *ref;
        for (ref = m_pidRefStack; ref; ref = ref->prev)
        {TRACE_IT(29224);
            int refId = ref->GetScopeId();
            if (refId == scopeId)
            {TRACE_IT(29225);
                return ref;
            }
            if (refId < scopeId)
            {TRACE_IT(29226);
                break;
            }
        }
        return nullptr;
    }

    void PushPidRef(int blockId, Js::LocalFunctionId funcId, PidRefStack *newRef)
    {TRACE_IT(29227);
        AssertMsg(blockId >= 0, "Block Id's should be greater than 0");
        newRef->id = blockId;
        newRef->funcId = funcId;
        newRef->prev = m_pidRefStack;
        m_pidRefStack = newRef;
    }

    PidRefStack * RemovePrevPidRef(PidRefStack *ref)
    {TRACE_IT(29228);
        PidRefStack *prevRef;
        if (ref == nullptr)
        {TRACE_IT(29229);
            prevRef = m_pidRefStack;
            Assert(prevRef);
            m_pidRefStack = prevRef->prev;
        }
        else
        {TRACE_IT(29230);
            prevRef = ref->prev;
            Assert(prevRef);
            ref->prev = prevRef->prev;
        }
        return prevRef;
    }

    PidRefStack * TopDecl(int maxBlockId) const
    {TRACE_IT(29231);
        for (PidRefStack *pidRef = m_pidRefStack; pidRef; pidRef = pidRef->prev)
        {TRACE_IT(29232);
            if (pidRef->id > maxBlockId)
            {TRACE_IT(29233);
                continue;
            }
            if (pidRef->sym != nullptr)
            {TRACE_IT(29234);
                return pidRef;
            }
        }
        return nullptr;
    }

    PidRefStack * FindOrAddPidRef(ArenaAllocator *alloc, int scopeId, Js::LocalFunctionId funcId)
    {TRACE_IT(29235);
        // If the stack is empty, or we are pushing to the innermost scope already,
        // we can go ahead and push a new PidRef on the stack.
        if (m_pidRefStack == nullptr)
        {TRACE_IT(29236);
            PidRefStack *newRef = Anew(alloc, PidRefStack, scopeId, funcId);
            if (newRef == nullptr)
            {TRACE_IT(29237);
                return nullptr;
            }
            newRef->prev = m_pidRefStack;
            m_pidRefStack = newRef;
            return newRef;
        }

        // Search for the corresponding PidRef, or the position to insert the new PidRef.
        PidRefStack *ref = m_pidRefStack;
        PidRefStack *prevRef = nullptr;
        while (1)
        {TRACE_IT(29238);
            // We may already have a ref for this scopeId.
            if (ref->id == scopeId)
            {TRACE_IT(29239);
                return ref;
            }

            if (ref->prev == nullptr || ref->id  < scopeId)
            {TRACE_IT(29240);
                // No existing PidRef for this scopeId, so create and insert one at this position.
                PidRefStack *newRef = Anew(alloc, PidRefStack, scopeId, funcId);
                if (newRef == nullptr)
                {TRACE_IT(29241);
                    return nullptr;
                }

                if (ref->id < scopeId)
                {TRACE_IT(29242);
                    if (prevRef != nullptr)
                    {TRACE_IT(29243);
                        // Param scope has a reference to the same pid as the one we are inserting into the body.
                        // There is a another reference (prevRef), probably from an inner block in the body.
                        // So we should insert the new reference between them.
                        newRef->prev = prevRef->prev;
                        prevRef->prev = newRef;
                    }
                    else
                    {TRACE_IT(29244);
                        // When we have code like below, prevRef will be null,
                        // function (a = x) { var x = 1; }
                        newRef->prev = m_pidRefStack;
                        m_pidRefStack = newRef;
                    }
                }
                else
                {TRACE_IT(29245);
                    newRef->prev = ref->prev;
                    ref->prev = newRef;
                }
                return newRef;
            }

            Assert(ref->prev->id <= ref->id);
            prevRef = ref;
            ref = ref->prev;
        }
    }

    Js::PropertyId GetPropertyId() const {TRACE_IT(29246); return m_propertyId; }
    void SetPropertyId(Js::PropertyId id) {TRACE_IT(29247); m_propertyId = id; }

    void SetIsEval() {TRACE_IT(29248); m_grfid |= fidEval; }
    BOOL GetIsEval() const {TRACE_IT(29249); return m_grfid & fidEval; }

    void SetIsLetOrConst() {TRACE_IT(29250); m_grfid |= fidLetOrConst; }
    BOOL GetIsLetOrConst() const {TRACE_IT(29251); return m_grfid & fidLetOrConst; }

    void SetIsModuleExport() {TRACE_IT(29252); m_grfid |= fidModuleExport; }
    BOOL GetIsModuleExport() const {TRACE_IT(29253); return m_grfid & fidModuleExport; }
};


/*****************************************************************************/

class HashTbl
{
public:
    static HashTbl * Create(uint cidHash, ErrHandler * perr);

    void Release(void)
    {TRACE_IT(29254);
        delete this;  // invokes overrided operator delete
    }


    BOOL TokIsBinop(tokens tk, int *popl, OpCode *pnop)
    {TRACE_IT(29255);
        const KWD *pkwd = KwdOfTok(tk);

        if (nullptr == pkwd)
            return FALSE;
        *popl = pkwd->prec2;
        *pnop = pkwd->nop2;
        return TRUE;
    }

    BOOL TokIsUnop(tokens tk, int *popl, OpCode *pnop)
    {TRACE_IT(29256);
        const KWD *pkwd = KwdOfTok(tk);

        if (nullptr == pkwd)
            return FALSE;
        *popl = pkwd->prec1;
        *pnop = pkwd->nop1;
        return TRUE;
    }

    IdentPtr PidFromTk(tokens tk);
    IdentPtr PidHashName(LPCOLESTR psz)
    {TRACE_IT(29257);
        size_t csz = wcslen(psz);
        Assert(csz <= ULONG_MAX);
        return PidHashNameLen(psz, static_cast<uint32>(csz));
    }

    template <typename CharType>
    IdentPtr PidHashNameLen(CharType const * psz, CharType const * end, uint32 cch);
    template <typename CharType>
    IdentPtr PidHashNameLen(CharType const * psz, uint32 cch);
    template <typename CharType>
    IdentPtr PidHashNameLenWithHash(_In_reads_(cch) CharType const * psz, CharType const * end, int32 cch, uint32 luHash);


    template <typename CharType>
    inline IdentPtr FindExistingPid(
        CharType const * prgch,
        CharType const * end,
        int32 cch,
        uint32 luHash,
        IdentPtr **pppInsert,
        int32 *pBucketCount
#if PROFILE_DICTIONARY
        , int& depth
#endif
        );

    tokens TkFromNameLen(_In_reads_(cch) LPCOLESTR prgch, uint32 cch, bool isStrictMode);
    tokens TkFromNameLenColor(_In_reads_(cch) LPCOLESTR prgch, uint32 cch);
    NoReleaseAllocator* GetAllocator() {TRACE_IT(29258);return &m_noReleaseAllocator;}

    bool Contains(_In_reads_(cch) LPCOLESTR prgch, int32 cch);

private:
    NoReleaseAllocator m_noReleaseAllocator;            // to allocate identifiers
    Ident ** m_prgpidName;        // hash table for names

    uint32 m_luMask;                // hash mask
    uint32 m_luCount;              // count of the number of entires in the hash table
    ErrHandler * m_perr;        // error handler to use
    IdentPtr m_rpid[tkLimKwd];

    HashTbl(ErrHandler * perr)
    {TRACE_IT(29259);
        m_prgpidName = nullptr;
        m_perr = perr;
        memset(&m_rpid, 0, sizeof(m_rpid));
    }
    ~HashTbl(void) {TRACE_IT(29260);}

    void operator delete(void* p, size_t size)
    {
        HeapFree(p, size);
    }

    // Called to grow the number of buckets in the table to reduce the table density.
    void Grow();

    // Automatically grow the table if a bucket's length grows beyond BucketLengthLimit and the table is densely populated.
    static const uint BucketLengthLimit = 5;

    // When growing the bucket size we'll grow by GrowFactor. GrowFactor MUST be a power of 2.
    static const uint GrowFactor = 4;

#if DEBUG
    uint CountAndVerifyItems(IdentPtr *buckets, uint bucketCount, uint mask);
#endif

    static bool CharsAreEqual(__in_z LPCOLESTR psz1, __in_ecount(psz2end - psz2) LPCOLESTR psz2, LPCOLESTR psz2end)
    {TRACE_IT(29261);
        return memcmp(psz1, psz2, (psz2end - psz2) * sizeof(OLECHAR)) == 0;
    }
    static bool CharsAreEqual(__in_z LPCOLESTR psz1, LPCUTF8 psz2, LPCUTF8 psz2end)
    {TRACE_IT(29262);
        return utf8::CharsAreEqual(psz1, psz2, psz2end, utf8::doAllowThreeByteSurrogates);
    }
    static bool CharsAreEqual(__in_z LPCOLESTR psz1, __in_ecount(psz2end - psz2) char const * psz2, char const * psz2end)
    {TRACE_IT(29263);
        while (psz2 < psz2end)
        {TRACE_IT(29264);
            if (*psz1++ != *psz2++)
            {TRACE_IT(29265);
                return false;
            }
        }
        return true;
    }
    static void CopyString(__in_ecount((psz2end - psz2) + 1) LPOLESTR psz1, __in_ecount(psz2end - psz2) LPCOLESTR psz2, LPCOLESTR psz2end)
    {TRACE_IT(29266);
        size_t cch = psz2end - psz2;
        js_memcpy_s(psz1, cch * sizeof(OLECHAR), psz2, cch * sizeof(OLECHAR));
        psz1[cch] = 0;
    }
    static void CopyString(LPOLESTR psz1, LPCUTF8 psz2, LPCUTF8 psz2end)
    {TRACE_IT(29267);
        utf8::DecodeUnitsIntoAndNullTerminate(psz1, psz2, psz2end);
    }
    static void CopyString(LPOLESTR psz1, char const * psz2, char const * psz2end)
    {TRACE_IT(29268);
        while (psz2 < psz2end)
        {TRACE_IT(29269);
            *(psz1++) = *psz2++;
        }
        *psz1 = 0;
    }

    // note: on failure this may throw or return FALSE, depending on
    // where the failure occurred.
    BOOL Init(uint cidHash);

    /*************************************************************************/
    /* The following members are related to the keyword descriptor tables    */
    /*************************************************************************/
    struct KWD
    {
        OpCode nop2;
        byte prec2;
        OpCode nop1;
        byte prec1;
    };
    struct ReservedWordInfo
    {
        StaticSym const * sym;
        ushort grfid;
    };
    static const ReservedWordInfo s_reservedWordInfo[tkID];
    static const KWD g_mptkkwd[tkLimKwd];
    static const KWD * KwdOfTok(tokens tk)
    {TRACE_IT(29270); return (unsigned int)tk < tkLimKwd ? g_mptkkwd + tk : nullptr; }

#if PROFILE_DICTIONARY
    DictionaryStats *stats;
#endif
};

