//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

enum RegionType : BYTE
{
    RegionTypeInvalid,
    RegionTypeRoot,
    RegionTypeTry,
    RegionTypeCatch,
    RegionTypeFinally
};

class Region
{
public:
    Region() : type(RegionTypeInvalid),
               parent(NULL), matchingTryRegion(nullptr), matchingCatchRegion(nullptr), matchingFinallyRegion(nullptr), selfOrFirstTryAncestor(nullptr),
               start(NULL), end(NULL),
               writeThroughSymbolsSet(nullptr),
               ehBailoutData(nullptr), bailoutReturnThunkLabel(nullptr), returnThunkEmitted(false),
               exceptionObjectSym(nullptr) {TRACE_IT(15132);}
    static Region * New(RegionType, Region *, Func *);

public:
    inline RegionType GetType() const                   {TRACE_IT(15133); return this->type; }
    inline void SetType(RegionType type)                {TRACE_IT(15134); this->type = type; }

    inline Region * GetParent() const                   {TRACE_IT(15135); return this->parent; }
    inline void SetParent(Region* parent)               {TRACE_IT(15136); this->parent = parent; }

    inline Region * GetMatchingTryRegion() const        {TRACE_IT(15137); return this->matchingTryRegion; }
    inline void SetMatchingTryRegion(Region* tryRegion) {TRACE_IT(15138); this->matchingTryRegion = tryRegion; }

    inline Region * GetMatchingCatchRegion() const      {TRACE_IT(15139); return this->matchingCatchRegion; }
    inline void SetMatchingCatchRegion(Region* catchRegion) {TRACE_IT(15140); this->matchingCatchRegion = catchRegion; }

    inline Region * GetMatchingFinallyRegion() const    {TRACE_IT(15141); return this->matchingFinallyRegion; }
    inline void SetMatchingFinallyRegion(Region* finallyRegion) {TRACE_IT(15142); this->matchingFinallyRegion = finallyRegion; }

    inline IR::Instr * GetStart() const                 {TRACE_IT(15143); return this->start; }
    inline void SetStart(IR::Instr * instr)             {TRACE_IT(15144); this->start = instr; }
    inline IR::Instr * GetEnd() const                   {TRACE_IT(15145); return this->end; }
    inline void SetEnd(IR::Instr * instr)               {TRACE_IT(15146); this->end = instr; }
    inline IR::LabelInstr * GetBailoutReturnThunkLabel() const {TRACE_IT(15147); return this->bailoutReturnThunkLabel; }
    inline StackSym * GetExceptionObjectSym() const     {TRACE_IT(15148); return this->exceptionObjectSym; }
    inline void SetExceptionObjectSym(StackSym * sym)   {TRACE_IT(15149); this->exceptionObjectSym = sym; }
    void   AllocateEHBailoutData(Func * func, IR::Instr * tryInstr);
    Region * GetSelfOrFirstTryAncestor();

private:
    RegionType                      type;
    Region *                        parent;
    Region *                        matchingTryRegion;
    Region *                        matchingCatchRegion;
    Region *                        matchingFinallyRegion;
    Region *                        selfOrFirstTryAncestor; // = self, if try region, otherwise
                                                            // = first try ancestor
    IR::Instr *                     start;
    IR::Instr *                     end;
    StackSym *                      exceptionObjectSym;
    IR::LabelInstr *                bailoutReturnThunkLabel;
    // bailoutReturnThunkLabel is the Label denoting start of return thunk for this region.

    // The JIT'ed code of a function having EH may have multiple frames on the stack, pertaining to the JIT'ed code and the TryCatch helper.
    // After a bailout in an EH region, we want to jump to the epilog of the function, but we have to do this via a series of returns (to clear out the frames on the stack).

    // To achieve this, post bailout, we jump to the return thunk of that region, which loads the appropriate return address into eax and executes a RET.
    // This has the effect of returning that address to the TryCatch helper, which, in turn, returns it to its caller JIT'ed code.
    // Non-top-level EH regions return the address of their parent's return thunk, and top level EH regions return the address
    // where the return value from a bailout is loaded into eax (restoreReturnValueFromBailoutLabel in EHBailoutPatchUp::Emit).
    // (Control should go to a return thunk only in case of a bailout from an EH region.)

public:
    BVSparse<JitArenaAllocator> *   writeThroughSymbolsSet;
    Js::EHBailoutData *             ehBailoutData;
    bool                            returnThunkEmitted;
};
