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
               exceptionObjectSym(nullptr) {LOGMEIN("Region.h] 23\n");}
    static Region * New(RegionType, Region *, Func *);

public:
    inline RegionType GetType() const                   {LOGMEIN("Region.h] 27\n"); return this->type; }
    inline void SetType(RegionType type)                {LOGMEIN("Region.h] 28\n"); this->type = type; }

    inline Region * GetParent() const                   {LOGMEIN("Region.h] 30\n"); return this->parent; }
    inline void SetParent(Region* parent)               {LOGMEIN("Region.h] 31\n"); this->parent = parent; }

    inline Region * GetMatchingTryRegion() const        {LOGMEIN("Region.h] 33\n"); return this->matchingTryRegion; }
    inline void SetMatchingTryRegion(Region* tryRegion) {LOGMEIN("Region.h] 34\n"); this->matchingTryRegion = tryRegion; }

    inline Region * GetMatchingCatchRegion() const      {LOGMEIN("Region.h] 36\n"); return this->matchingCatchRegion; }
    inline void SetMatchingCatchRegion(Region* catchRegion) {LOGMEIN("Region.h] 37\n"); this->matchingCatchRegion = catchRegion; }

    inline Region * GetMatchingFinallyRegion() const    {LOGMEIN("Region.h] 39\n"); return this->matchingFinallyRegion; }
    inline void SetMatchingFinallyRegion(Region* finallyRegion) {LOGMEIN("Region.h] 40\n"); this->matchingFinallyRegion = finallyRegion; }

    inline IR::Instr * GetStart() const                 {LOGMEIN("Region.h] 42\n"); return this->start; }
    inline void SetStart(IR::Instr * instr)             {LOGMEIN("Region.h] 43\n"); this->start = instr; }
    inline IR::Instr * GetEnd() const                   {LOGMEIN("Region.h] 44\n"); return this->end; }
    inline void SetEnd(IR::Instr * instr)               {LOGMEIN("Region.h] 45\n"); this->end = instr; }
    inline IR::LabelInstr * GetBailoutReturnThunkLabel() const {LOGMEIN("Region.h] 46\n"); return this->bailoutReturnThunkLabel; }
    inline StackSym * GetExceptionObjectSym() const     {LOGMEIN("Region.h] 47\n"); return this->exceptionObjectSym; }
    inline void SetExceptionObjectSym(StackSym * sym)   {LOGMEIN("Region.h] 48\n"); this->exceptionObjectSym = sym; }
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
