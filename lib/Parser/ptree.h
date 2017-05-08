//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

struct Ident;
typedef Ident *IdentPtr;

class Scope;

/***************************************************************************
Flags for classifying node operators.
***************************************************************************/
const uint fnopNone     = 0x0000;
const uint fnopConst    = 0x0001; // constant
const uint fnopLeaf     = 0x0002; // leaf
const uint fnopUni      = 0x0004; // unary
const uint fnopBin      = 0x0008; // binary
const uint fnopRel      = 0x0010; // relational
const uint fnopAsg      = 0x0020; // assignment
const uint fnopBreak    = 0x0040; // break can be used within this statement
const uint fnopContinue = 0x0080; // continue can be used within this statement
const uint fnopCleanup  = 0x0100; // requires cleanup (eg, with or for-in).
const uint fnopJump     = 0x0200;
const uint fnopNotExprStmt = 0x0400;
const uint fnopBinList  = 0x0800;
const uint fnopExprMask = (fnopLeaf|fnopUni|fnopBin);

/***************************************************************************
Flags for classifying parse nodes.
***************************************************************************/
enum PNodeFlags : ushort
{
    fpnNone                                  = 0x0000,

    // knopFncDecl nodes.
    fpnArguments_overriddenByDecl            = 0x0001, // function has a let/const decl, class or nested function named 'arguments', which overrides the built-in arguments object in the body
    fpnArguments_overriddenInParam           = 0x0002, // function has a parameter named arguments
    fpnArguments_varDeclaration              = 0x0004, // function has a var declaration named 'arguments', which may change the way an 'arguments' identifier is resolved

    // knopVarDecl nodes.
    fpnArguments                             = 0x0008,
    fpnHidden                                = 0x0010,

    // Statement nodes.
    fpnExplicitSemicolon                     = 0x0020, // statement terminated by an explicit semicolon
    fpnAutomaticSemicolon                    = 0x0040, // statement terminated by an automatic semicolon
    fpnMissingSemicolon                      = 0x0080, // statement missing terminating semicolon, and is not applicable for automatic semicolon insertion
    fpnDclList                               = 0x0100, // statement is a declaration list
    fpnSyntheticNode                         = 0x0200, // node is added by the parser or does it represent user code
    fpnIndexOperator                         = 0x0400, // dot operator is an optimization of an index operator
    fpnJumbStatement                         = 0x0800, // break or continue that was removed by error recovery

    // Unary/Binary nodes
    fpnCanFlattenConcatExpr                  = 0x1000, // the result of the binary operation can participate in concat N

    // Potentially overlapping traversal flags
    // These flags are set and cleared during a single node traversal and their values can be used in other node traversals.
    fpnMemberReference                       = 0x2000, // The node is a member reference symbol
    fpnCapturesSyms                          = 0x4000, // The node is a statement (or contains a sub-statement)
                                                       // that captures symbols.
};

/***************************************************************************
Data structs for ParseNodes. ParseNode includes a union of these.
***************************************************************************/
struct PnUni
{
    ParseNodePtr pnode1;
};

struct PnBin
{
    ParseNodePtr pnodeNext;
    ParseNodePtr pnode1;
    ParseNodePtr pnode2;
};

struct PnTri
{
    ParseNodePtr pnodeNext;
    ParseNodePtr pnode1;
    ParseNodePtr pnode2;
    ParseNodePtr pnode3;
};

struct PnSlot
{
  uint slotIndex;
};

struct PnUniSlot : PnUni
{
  uint slotIndex;
  uint staticFuncId;
};

struct PnInt
{
    int32 lw;
};

struct PnFlt
{
    double dbl;
    bool maybeInt : 1;
};

class Symbol;
struct PidRefStack;
struct PnPid
{
    IdentPtr pid;
    Symbol **symRef;
    Symbol *sym;
    UnifiedRegex::RegexPattern* regexPattern;
    uint regexPatternIndex;

    void SetSymRef(PidRefStack *ref);
    Symbol **GetSymRef() const {TRACE_IT(33332); return symRef; }
    Js::PropertyId PropertyIdFromNameNode() const;
};

struct PnVar
{
    ParseNodePtr pnodeNext;
    IdentPtr pid;
    Symbol *sym;
    Symbol **symRef;
    ParseNodePtr pnodeInit;
    BOOLEAN isSwitchStmtDecl;
    BOOLEAN isBlockScopeFncDeclVar;

    void InitDeclNode(IdentPtr name, ParseNodePtr initExpr)
    {TRACE_IT(33333);
        this->pid = name;
        this->pnodeInit = initExpr;
        this->pnodeNext = nullptr;
        this->sym = nullptr;
        this->symRef = nullptr;
        this->isSwitchStmtDecl = false;
        this->isBlockScopeFncDeclVar = false;
    }
};

struct PnLabel
{
    IdentPtr pid;
    ParseNodePtr pnodeNext;
};

struct PnArrLit : PnUni
{
    uint count;
    uint spreadCount;
    BYTE arrayOfTaggedInts:1;     // indicates that array initializer nodes are all tagged ints
    BYTE arrayOfInts:1;           // indicates that array initializer nodes are all ints
    BYTE arrayOfNumbers:1;        // indicates that array initializer nodes are all numbers
    BYTE hasMissingValues:1;
};

class FuncInfo;

enum PnodeBlockType : unsigned
{
    Global,
    Function,
    Regular,
    Parameter
};

enum FncFlags
{
    kFunctionNone                               = 0,
    kFunctionNested                             = 1 << 0, // True if function is nested in another.
    kFunctionDeclaration                        = 1 << 1, // is this a declaration or an expression?
    kFunctionCallsEval                          = 1 << 2, // function uses eval
    kFunctionUsesArguments                      = 1 << 3, // function uses arguments
    kFunctionHasHeapArguments                   = 1 << 4, // function's "arguments" escape the scope
    kFunctionHasReferenceableBuiltInArguments   = 1 << 5, // the built-in 'arguments' object is referenceable in the function
    kFunctionIsAccessor                         = 1 << 6, // function is a property getter or setter
    kFunctionHasNonThisStmt                     = 1 << 7,
    kFunctionStrictMode                         = 1 << 8,
    kFunctionDoesNotEscape                      = 1 << 9, // function is known not to escape its declaring scope
    kFunctionIsModule                           = 1 << 10, // function is a module body
    kFunctionHasThisStmt                        = 1 << 11, // function has at least one this.assignment and might be a constructor
    kFunctionHasWithStmt                        = 1 << 12, // function (or child) uses with
    kFunctionIsLambda                           = 1 << 13,
    kFunctionChildCallsEval                     = 1 << 14,
    kFunctionHasNonSimpleParameterList          = 1 << 15,
    kFunctionHasSuperReference                  = 1 << 16,
    kFunctionIsMethod                           = 1 << 17,
    kFunctionIsClassConstructor                 = 1 << 18, // function is a class constructor
    kFunctionIsBaseClassConstructor             = 1 << 19, // function is a base class constructor
    kFunctionIsClassMember                      = 1 << 20, // function is a class member
    kFunctionNameIsHidden                       = 1 << 21, // True if a named function expression has its name hidden from nested functions
    kFunctionIsGeneratedDefault                 = 1 << 22, // Is the function generated by us as a default (e.g. default class constructor)
    kFunctionHasDefaultArguments                = 1 << 23, // Function has one or more ES6 default arguments
    kFunctionIsStaticMember                     = 1 << 24,
    kFunctionIsGenerator                        = 1 << 25, // Function is an ES6 generator function
    kFunctionAsmjsMode                          = 1 << 26,
    kFunctionHasNewTargetReference              = 1 << 27, // function has a reference to new.target
    kFunctionIsAsync                            = 1 << 28, // function is async
    kFunctionHasDirectSuper                     = 1 << 29, // super()
    kFunctionIsDefaultModuleExport              = 1 << 30, // function is the default export of a module
    kFunctionHasAnyWriteToFormals               = 1 << 31  // To Track if there are any writes to formals.
};

struct RestorePoint;
struct DeferredFunctionStub;

struct PnFnc
{
    ParseNodePtr pnodeNext;
    ParseNodePtr pnodeName;
    IdentPtr pid;
    LPCOLESTR hint;
    uint32 hintLength;
    uint32 hintOffset;
    bool  isNameIdentifierRef;
    bool  nestedFuncEscapes;
    ParseNodePtr pnodeScopes;
    ParseNodePtr pnodeBodyScope;
    ParseNodePtr pnodeParams;
    ParseNodePtr pnodeVars;
    ParseNodePtr pnodeBody;
    ParseNodePtr pnodeRest;

    FuncInfo *funcInfo; // function information gathered during byte code generation
    Scope *scope;

    uint nestedCount; // Nested function count (valid until children have been processed)
    uint nestedIndex; // Index within the parent function

    uint16 firstDefaultArg; // Position of the first default argument, if any

    unsigned int fncFlags;
    int32 astSize;
    size_t cbMin; // Min an Lim UTF8 offsets.
    size_t cbLim;
    ULONG lineNumber;   // Line number relative to the current source buffer of the function declaration.
    ULONG columnNumber; // Column number of the declaration.
    Js::LocalFunctionId functionId;
#if DBG
    Js::LocalFunctionId deferredParseNextFunctionId;
#endif
    RestorePoint *pRestorePoint;
    DeferredFunctionStub *deferredStub;
    bool canBeDeferred;
    bool fibPreventsDeferral;
    bool isBodyAndParamScopeMerged; // Indicates whether the param scope and the body scope of the function can be merged together or not.
                                    // We cannot merge both scopes together if there is any closure capture or eval is present in the param scope.

    static const int32 MaxStackClosureAST = 800000;

    static bool CanBeRedeferred(unsigned int flags) {TRACE_IT(33334); return !(flags & (kFunctionIsGenerator | kFunctionIsAsync)); }

private:
    void SetFlags(uint flags, bool set)
    {TRACE_IT(33335);
        if (set)
        {TRACE_IT(33336);
            fncFlags |= flags;
        }
        else
        {TRACE_IT(33337);
            fncFlags &= ~flags;
        }
    }

    bool HasFlags(uint flags) const
    {TRACE_IT(33338);
        return (fncFlags & flags) == flags;
    }

    bool HasAnyFlags(uint flags) const
    {TRACE_IT(33339);
        return (fncFlags & flags) != 0;
    }

    bool HasNoFlags(uint flags) const
    {TRACE_IT(33340);
        return (fncFlags & flags) == 0;
    }

public:
    void ClearFlags()
    {TRACE_IT(33341);
        fncFlags = kFunctionNone;
        canBeDeferred = false;
        fibPreventsDeferral = false;
    }

    void SetAsmjsMode(bool set = true) { SetFlags(kFunctionAsmjsMode, set); }
    void SetCallsEval(bool set = true) { SetFlags(kFunctionCallsEval, set); }
    void SetChildCallsEval(bool set = true) { SetFlags(kFunctionChildCallsEval, set); }
    void SetDeclaration(bool set = true) { SetFlags(kFunctionDeclaration, set); }
    void SetDoesNotEscape(bool set = true) { SetFlags(kFunctionDoesNotEscape, set); }
    void SetHasDefaultArguments(bool set = true) { SetFlags(kFunctionHasDefaultArguments, set); }
    void SetHasHeapArguments(bool set = true) { SetFlags(kFunctionHasHeapArguments, set); }
    void SetHasAnyWriteToFormals(bool set = true) {TRACE_IT(33342); SetFlags((uint)kFunctionHasAnyWriteToFormals, set); }
    void SetHasNonSimpleParameterList(bool set = true) { SetFlags(kFunctionHasNonSimpleParameterList, set); }
    void SetHasNonThisStmt(bool set = true) { SetFlags(kFunctionHasNonThisStmt, set); }
    void SetHasReferenceableBuiltInArguments(bool set = true) { SetFlags(kFunctionHasReferenceableBuiltInArguments, set); }
    void SetHasSuperReference(bool set = true) { SetFlags(kFunctionHasSuperReference, set); }
    void SetHasDirectSuper(bool set = true) { SetFlags(kFunctionHasDirectSuper, set); }
    void SetHasNewTargetReference(bool set = true) { SetFlags(kFunctionHasNewTargetReference, set); }
    void SetHasThisStmt(bool set = true) { SetFlags(kFunctionHasThisStmt, set); }
    void SetHasWithStmt(bool set = true) { SetFlags(kFunctionHasWithStmt, set); }
    void SetIsAccessor(bool set = true) { SetFlags(kFunctionIsAccessor, set); }
    void SetIsAsync(bool set = true) { SetFlags(kFunctionIsAsync, set); }
    void SetIsClassConstructor(bool set = true) { SetFlags(kFunctionIsClassConstructor, set); }
    void SetIsBaseClassConstructor(bool set = true) { SetFlags(kFunctionIsBaseClassConstructor, set); }
    void SetIsClassMember(bool set = true) { SetFlags(kFunctionIsClassMember, set); }
    void SetIsGeneratedDefault(bool set = true) { SetFlags(kFunctionIsGeneratedDefault, set); }
    void SetIsGenerator(bool set = true) { SetFlags(kFunctionIsGenerator, set); }
    void SetIsLambda(bool set = true) { SetFlags(kFunctionIsLambda, set); }
    void SetIsMethod(bool set = true) { SetFlags(kFunctionIsMethod, set); }
    void SetIsStaticMember(bool set = true) { SetFlags(kFunctionIsStaticMember, set); }
    void SetNameIsHidden(bool set = true) { SetFlags(kFunctionNameIsHidden, set); }
    void SetNested(bool set = true) { SetFlags(kFunctionNested, set); }
    void SetStrictMode(bool set = true) { SetFlags(kFunctionStrictMode, set); }
    void SetIsModule(bool set = true) { SetFlags(kFunctionIsModule, set); }
    void SetUsesArguments(bool set = true) { SetFlags(kFunctionUsesArguments, set); }
    void SetIsDefaultModuleExport(bool set = true) { SetFlags(kFunctionIsDefaultModuleExport, set); }
    void SetNestedFuncEscapes(bool set = true) {TRACE_IT(33343); nestedFuncEscapes = set; }
    void SetCanBeDeferred(bool set = true) {TRACE_IT(33344); canBeDeferred = set; }
    void SetFIBPreventsDeferral(bool set = true) {TRACE_IT(33345); fibPreventsDeferral = set; }
    void ResetBodyAndParamScopeMerged() {TRACE_IT(33346); isBodyAndParamScopeMerged = false; }

    bool CallsEval() const {TRACE_IT(33347); return HasFlags(kFunctionCallsEval); }
    bool ChildCallsEval() const {TRACE_IT(33348); return HasFlags(kFunctionChildCallsEval); }
    bool DoesNotEscape() const {TRACE_IT(33349); return HasFlags(kFunctionDoesNotEscape); }
    bool GetArgumentsObjectEscapes() const {TRACE_IT(33350); return HasFlags(kFunctionHasHeapArguments); }
    bool GetAsmjsMode() const {TRACE_IT(33351); return HasFlags(kFunctionAsmjsMode); }
    bool GetStrictMode() const {TRACE_IT(33352); return HasFlags(kFunctionStrictMode); }
    bool HasDefaultArguments() const {TRACE_IT(33353); return HasFlags(kFunctionHasDefaultArguments); }
    bool HasHeapArguments() const {TRACE_IT(33354); return true; /* HasFlags(kFunctionHasHeapArguments); Disabling stack arguments. Always return HeapArguments as True */ }
    bool HasAnyWriteToFormals() const {TRACE_IT(33355); return HasFlags((uint)kFunctionHasAnyWriteToFormals); }
    bool HasOnlyThisStmts() const {TRACE_IT(33356); return !HasFlags(kFunctionHasNonThisStmt); }
    bool HasReferenceableBuiltInArguments() const {TRACE_IT(33357); return HasFlags(kFunctionHasReferenceableBuiltInArguments); }
    bool HasSuperReference() const {TRACE_IT(33358); return HasFlags(kFunctionHasSuperReference); }
    bool HasDirectSuper() const {TRACE_IT(33359); return HasFlags(kFunctionHasDirectSuper); }
    bool HasNewTargetReference() const {TRACE_IT(33360); return HasFlags(kFunctionHasNewTargetReference); }
    bool HasNonSimpleParameterList() {TRACE_IT(33361); return HasFlags(kFunctionHasNonSimpleParameterList); }
    bool HasThisStmt() const {TRACE_IT(33362); return HasFlags(kFunctionHasThisStmt); }
    bool HasWithStmt() const {TRACE_IT(33363); return HasFlags(kFunctionHasWithStmt); }
    bool IsAccessor() const {TRACE_IT(33364); return HasFlags(kFunctionIsAccessor); }
    bool IsAsync() const {TRACE_IT(33365); return HasFlags(kFunctionIsAsync); }
    bool IsConstructor() const {TRACE_IT(33366); return HasNoFlags(kFunctionIsAsync|kFunctionIsLambda|kFunctionIsAccessor);  }
    bool IsClassConstructor() const {TRACE_IT(33367); return HasFlags(kFunctionIsClassConstructor); }
    bool IsBaseClassConstructor() const {TRACE_IT(33368); return HasFlags(kFunctionIsBaseClassConstructor); }
    bool IsClassMember() const {TRACE_IT(33369); return HasFlags(kFunctionIsClassMember); }
    bool IsDeclaration() const {TRACE_IT(33370); return HasFlags(kFunctionDeclaration); }
    bool IsGeneratedDefault() const {TRACE_IT(33371); return HasFlags(kFunctionIsGeneratedDefault); }
    bool IsGenerator() const {TRACE_IT(33372); return HasFlags(kFunctionIsGenerator); }
    bool IsCoroutine() const {TRACE_IT(33373); return HasAnyFlags(kFunctionIsGenerator | kFunctionIsAsync); }
    bool IsLambda() const {TRACE_IT(33374); return HasFlags(kFunctionIsLambda); }
    bool IsMethod() const {TRACE_IT(33375); return HasFlags(kFunctionIsMethod); }
    bool IsNested() const {TRACE_IT(33376); return HasFlags(kFunctionNested); }
    bool IsStaticMember() const {TRACE_IT(33377); return HasFlags(kFunctionIsStaticMember); }
    bool IsModule() const {TRACE_IT(33378); return HasFlags(kFunctionIsModule); }
    bool NameIsHidden() const {TRACE_IT(33379); return HasFlags(kFunctionNameIsHidden); }
    bool UsesArguments() const {TRACE_IT(33380); return HasFlags(kFunctionUsesArguments); }
    bool IsDefaultModuleExport() const {TRACE_IT(33381); return HasFlags(kFunctionIsDefaultModuleExport); }
    bool NestedFuncEscapes() const {TRACE_IT(33382); return nestedFuncEscapes; }
    bool CanBeDeferred() const {TRACE_IT(33383); return canBeDeferred; }
    bool FIBPreventsDeferral() const {TRACE_IT(33384); return fibPreventsDeferral; }
    bool IsBodyAndParamScopeMerged() {TRACE_IT(33385); return isBodyAndParamScopeMerged; }

    size_t LengthInBytes()
    {TRACE_IT(33386);
        return cbLim - cbMin;
    }

    Symbol *GetFuncSymbol();
    void SetFuncSymbol(Symbol *sym);

    ParseNodePtr GetParamScope() const;
    ParseNodePtr GetBodyScope() const;
    ParseNodePtr GetTopLevelScope() const
    {TRACE_IT(33387);
        // Top level scope will be the same for knopProg and knopFncDecl.
        return GetParamScope();
    }

    template<typename Fn>
    void MapContainerScopes(Fn fn)
    {TRACE_IT(33388);
        fn(this->pnodeScopes->sxBlock.pnodeScopes);
        if (this->pnodeBodyScope != nullptr)
        {TRACE_IT(33389);
            fn(this->pnodeBodyScope->sxBlock.pnodeScopes);
        }
    }
};

struct PnClass
{
    ParseNodePtr pnodeName;
    ParseNodePtr pnodeDeclName;
    ParseNodePtr pnodeBlock;
    ParseNodePtr pnodeConstructor;
    ParseNodePtr pnodeMembers;
    ParseNodePtr pnodeStaticMembers;
    ParseNodePtr pnodeExtends;

    bool isDefaultModuleExport;

    void SetIsDefaultModuleExport(bool set) {TRACE_IT(33390); isDefaultModuleExport = set; }
    bool IsDefaultModuleExport() const {TRACE_IT(33391); return isDefaultModuleExport; }
};

struct PnExportDefault
{
    ParseNodePtr pnodeExpr;
};

struct PnStrTemplate
{
    ParseNodePtr pnodeStringLiterals;
    ParseNodePtr pnodeStringRawLiterals;
    ParseNodePtr pnodeSubstitutionExpressions;
    uint16 countStringLiterals;
    BYTE isTaggedTemplate:1;
};

struct PnProg : PnFnc
{
    ParseNodePtr pnodeLastValStmt;
    bool m_UsesArgumentsAtGlobal;
};

struct PnModule : PnProg
{
    ModuleImportOrExportEntryList* localExportEntries;
    ModuleImportOrExportEntryList* indirectExportEntries;
    ModuleImportOrExportEntryList* starExportEntries;
    ModuleImportOrExportEntryList* importEntries;
    IdentPtrList* requestedModules;
};

struct PnCall
{
    ParseNodePtr pnodeNext;
    ParseNodePtr pnodeTarget;
    ParseNodePtr pnodeArgs;
    uint16 argCount;
    uint16 spreadArgCount;
    BYTE callOfConstants : 1;
    BYTE isApplyCall : 1;
    BYTE isEvalCall : 1;
};

struct PnStmt
{
    ParseNodePtr pnodeOuter;

    // Set by parsing code, used by code gen.
    uint grfnop;

    // Needed for byte code gen.
    Js::ByteCodeLabel breakLabel;
    Js::ByteCodeLabel continueLabel;
};

struct PnBlock : PnStmt
{
    ParseNodePtr pnodeStmt;
    ParseNodePtr pnodeLastValStmt;
    ParseNodePtr pnodeLexVars;
    ParseNodePtr pnodeScopes;
    ParseNodePtr pnodeNext;
    Scope        *scope;

    ParseNodePtr enclosingBlock;
    int blockId;
    PnodeBlockType blockType:2;
    BYTE         callsEval:1;
    BYTE         childCallsEval:1;

    void SetCallsEval(bool does) {TRACE_IT(33392); callsEval = does; }
    bool GetCallsEval() const {TRACE_IT(33393); return callsEval; }

    void SetChildCallsEval(bool does) {TRACE_IT(33394); childCallsEval = does; }
    bool GetChildCallsEval() const {TRACE_IT(33395); return childCallsEval; }

    void SetEnclosingBlock(ParseNodePtr pnode) {TRACE_IT(33396); enclosingBlock = pnode; }
    ParseNodePtr GetEnclosingBlock() const {TRACE_IT(33397); return enclosingBlock; }

    bool HasBlockScopedContent() const;
};

struct PnJump : PnStmt
{
    ParseNodePtr pnodeTarget;
    bool hasExplicitTarget;
};

struct PnLoop : PnStmt
{
    // Needed for byte code gen
    uint loopId;
};

struct PnWhile : PnLoop
{
    ParseNodePtr pnodeCond;
    ParseNodePtr pnodeBody;
};

struct PnWith : PnStmt
{
    ParseNodePtr pnodeObj;
    ParseNodePtr pnodeBody;
    ParseNodePtr pnodeScopes;
    ParseNodePtr pnodeNext;
    Scope        *scope;
};

struct PnParamPattern
{
    ParseNodePtr pnodeNext;
    Js::RegSlot location;
    ParseNodePtr pnode1;
};

struct PnIf : PnStmt
{
    ParseNodePtr pnodeCond;
    ParseNodePtr pnodeTrue;
    ParseNodePtr pnodeFalse;
};

struct PnHelperCall2 {
  ParseNodePtr pnodeArg1;
  ParseNodePtr pnodeArg2;
  int helperId;
};

struct PnForInOrForOf : PnLoop
{
    ParseNodePtr pnodeObj;
    ParseNodePtr pnodeBody;
    ParseNodePtr pnodeLval;
    ParseNodePtr pnodeBlock;
    Js::RegSlot itemLocation;
};

struct PnFor : PnLoop
{
    ParseNodePtr pnodeCond;
    ParseNodePtr pnodeBody;
    ParseNodePtr pnodeInit;
    ParseNodePtr pnodeIncr;
    ParseNodePtr pnodeBlock;
    ParseNodePtr pnodeInverted;
};

struct PnSwitch : PnStmt
{
    ParseNodePtr pnodeVal;
    ParseNodePtr pnodeCases;
    ParseNodePtr pnodeDefault;
    ParseNodePtr pnodeBlock;
};

struct PnCase : PnStmt
{
    ParseNodePtr pnodeNext;
    ParseNodePtr pnodeExpr; // nullptr for default
    ParseNodePtr pnodeBody;
    Js::ByteCodeLabel labelCase;
};

struct PnReturn : PnStmt
{
    ParseNodePtr pnodeExpr;
};

struct PnTryFinally : PnStmt
{
    ParseNodePtr pnodeTry;
    ParseNodePtr pnodeFinally;
};

struct PnTryCatch : PnStmt
{
    ParseNodePtr pnodeTry;
    ParseNodePtr pnodeCatch;
};

struct PnTry : PnStmt
{
    ParseNodePtr pnodeBody;
};

struct PnCatch : PnStmt
{
    ParseNodePtr pnodeNext;
    ParseNodePtr pnodeParam;
    ParseNodePtr pnodeBody;
    ParseNodePtr pnodeScopes;
    Scope        *scope;
};

struct PnFinally : PnStmt
{
    ParseNodePtr pnodeBody;
};

struct ParseNode
{
    OpCode nop;
    ushort grfpn;
    charcount_t ichMin;         // start offset into the original source buffer
    charcount_t ichLim;         // end offset into the original source buffer
    Js::RegSlot location;
    bool isUsed;                // indicates whether an expression such as x++ is used
    bool emitLabels;
    bool notEscapedUse;         // Use by byte code generator.  Currently, only used by child of knopComma
    bool isInList;
    bool isCallApplyTargetLoad;
#ifdef EDIT_AND_CONTINUE
    ParseNodePtr parent;
#endif

    union
    {
        PnArrLit        sxArrLit;       // Array literal
        PnBin           sxBin;          // binary operators
        PnBlock         sxBlock;        // block { }
        PnCall          sxCall;         // function call
        PnCase          sxCase;         // switch case
        PnCatch         sxCatch;        // { catch(e : expr) {body} }
        PnClass         sxClass;        // class declaration
        PnFinally       sxFinally;      // finally
        PnExportDefault sxExportDefault;// export default expr;
        PnFlt           sxFlt;          // double constant
        PnFnc           sxFnc;          // function declaration
        PnFor           sxFor;          // for loop
        PnForInOrForOf  sxForInOrForOf; // for-in loop
        PnHelperCall2   sxHelperCall2;  // call to helper
        PnIf            sxIf;           // if
        PnInt           sxInt;          // integer constant
        PnJump          sxJump;         // break and continue
        PnLabel         sxLabel;        // label nodes
        PnLoop          sxLoop;         // base for loop nodes
        PnModule        sxModule;       // global module
        PnPid           sxPid;          // identifier or string
        PnProg          sxProg;         // global program
        PnReturn        sxReturn;       // return [expr]
        PnStmt          sxStmt;         // base for statement nodes
        PnStrTemplate   sxStrTemplate;  // string template declaration
        PnSwitch        sxSwitch;       // switch
        PnTri           sxTri;          // ternary operator
        PnTry           sxTry;          // try-catch
        PnTryCatch      sxTryCatch;     // try-catch
        PnTryFinally    sxTryFinally;   // try-catch-finally
        PnUni           sxUni;          // unary operators
        PnVar           sxVar;          // variable declaration
        PnWhile         sxWhile;        // while and do-while loops
        PnWith          sxWith;         // with
        PnParamPattern  sxParamPattern; // Destructure pattern for function/catch parameter
    };

    IdentPtr name()
    {TRACE_IT(33398);
        if (this->nop == knopName || this->nop == knopStr)
        {TRACE_IT(33399);
            return this->sxPid.pid;
        }
        else if (this->nop == knopVarDecl)
        {TRACE_IT(33400);
            return this->sxVar.pid;
        }
        else if (this->nop == knopConstDecl)
        {TRACE_IT(33401);
            return this->sxVar.pid;
        }
        return nullptr;
    }

    static const uint mpnopgrfnop[knopLim];

    static uint Grfnop(int nop)
    {TRACE_IT(33402);
        Assert(nop < knopLim);
        return nop < knopLim ? mpnopgrfnop[nop] : fnopNone;
    }

    BOOL IsStatement()
    {TRACE_IT(33403);
        return (nop >= knopList && nop != knopLabel) || ((Grfnop(nop) & fnopAsg) != 0);
    }

    uint Grfnop(void)
    {TRACE_IT(33404);
        Assert(nop < knopLim);
        return nop < knopLim ? mpnopgrfnop[nop] : fnopNone;
    }

    charcount_t LengthInCodepoints() const
    {TRACE_IT(33405);
        return (this->ichLim - this->ichMin);
    }

    // This node is a function decl node and function has a var declaration named 'arguments',
    bool HasVarArguments() const
    {TRACE_IT(33406);
        return ((nop == knopFncDecl) && (grfpn & PNodeFlags::fpnArguments_varDeclaration));
    }

    bool CapturesSyms() const
    {TRACE_IT(33407);
        return (grfpn & PNodeFlags::fpnCapturesSyms) != 0;
    }

    void SetCapturesSyms()
    {TRACE_IT(33408);
        grfpn |= PNodeFlags::fpnCapturesSyms;
    }

    bool IsInList() const {TRACE_IT(33409); return this->isInList; }
    void SetIsInList() {TRACE_IT(33410); this->isInList = true; }

    bool IsNotEscapedUse() const {TRACE_IT(33411); return this->notEscapedUse; }
    void SetNotEscapedUse() {TRACE_IT(33412); this->notEscapedUse = true; }

    bool CanFlattenConcatExpr() const {TRACE_IT(33413); return !!(this->grfpn & PNodeFlags::fpnCanFlattenConcatExpr); }

    bool IsCallApplyTargetLoad() {TRACE_IT(33414); return isCallApplyTargetLoad; }
    void SetIsCallApplyTargetLoad() {TRACE_IT(33415); isCallApplyTargetLoad = true; }

    bool IsVarLetOrConst() const
    {TRACE_IT(33416);
        return this->nop == knopVarDecl || this->nop == knopLetDecl || this->nop == knopConstDecl;
    }

    ParseNodePtr GetFormalNext()
    {TRACE_IT(33417);
        ParseNodePtr pnodeNext = nullptr;

        if (nop == knopParamPattern)
        {TRACE_IT(33418);
            pnodeNext = this->sxParamPattern.pnodeNext;
        }
        else
        {TRACE_IT(33419);
            Assert(IsVarLetOrConst());
            pnodeNext = this->sxVar.pnodeNext;
        }
        return pnodeNext;
    }

    bool IsPattern() const
    {TRACE_IT(33420);
        return nop == knopObjectPattern || nop == knopArrayPattern;
    }

#if DBG_DUMP
    void Dump();
#endif
};

const int kcbPnNone         = offsetof(ParseNode, sxUni);
const int kcbPnArrLit       = kcbPnNone + sizeof(PnArrLit);
const int kcbPnBin          = kcbPnNone + sizeof(PnBin);
const int kcbPnBlock        = kcbPnNone + sizeof(PnBlock);
const int kcbPnCall         = kcbPnNone + sizeof(PnCall);
const int kcbPnCase         = kcbPnNone + sizeof(PnCase);
const int kcbPnCatch        = kcbPnNone + sizeof(PnCatch);
const int kcbPnClass        = kcbPnNone + sizeof(PnClass);
const int kcbPnExportDefault= kcbPnNone + sizeof(PnExportDefault);
const int kcbPnFinally      = kcbPnNone + sizeof(PnFinally);
const int kcbPnFlt          = kcbPnNone + sizeof(PnFlt);
const int kcbPnFnc          = kcbPnNone + sizeof(PnFnc);
const int kcbPnFor          = kcbPnNone + sizeof(PnFor);
const int kcbPnForIn        = kcbPnNone + sizeof(PnForInOrForOf);
const int kcbPnForOf        = kcbPnNone + sizeof(PnForInOrForOf);
const int kcbPnHelperCall3  = kcbPnNone + sizeof(PnHelperCall2);
const int kcbPnIf           = kcbPnNone + sizeof(PnIf);
const int kcbPnInt          = kcbPnNone + sizeof(PnInt);
const int kcbPnJump         = kcbPnNone + sizeof(PnJump);
const int kcbPnLabel        = kcbPnNone + sizeof(PnLabel);
const int kcbPnModule       = kcbPnNone + sizeof(PnModule);
const int kcbPnPid          = kcbPnNone + sizeof(PnPid);
const int kcbPnProg         = kcbPnNone + sizeof(PnProg);
const int kcbPnReturn       = kcbPnNone + sizeof(PnReturn);
const int kcbPnSlot         = kcbPnNone + sizeof(PnSlot);
const int kcbPnStrTemplate  = kcbPnNone + sizeof(PnStrTemplate);
const int kcbPnSwitch       = kcbPnNone + sizeof(PnSwitch);
const int kcbPnTri          = kcbPnNone + sizeof(PnTri);
const int kcbPnTry          = kcbPnNone + sizeof(PnTry);
const int kcbPnTryCatch     = kcbPnNone + sizeof(PnTryCatch);
const int kcbPnTryFinally   = kcbPnNone + sizeof(PnTryFinally);
const int kcbPnUni          = kcbPnNone + sizeof(PnUni);
const int kcbPnUniSlot      = kcbPnNone + sizeof(PnUniSlot);
const int kcbPnVar          = kcbPnNone + sizeof(PnVar);
const int kcbPnWhile        = kcbPnNone + sizeof(PnWhile);
const int kcbPnWith         = kcbPnNone + sizeof(PnWith);
const int kcbPnParamPattern = kcbPnNone + sizeof(PnParamPattern);

#define AssertNodeMem(pnode) AssertPvCb(pnode, kcbPnNone)
#define AssertNodeMemN(pnode) AssertPvCbN(pnode, kcbPnNone)
