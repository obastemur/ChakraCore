//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

/**
 * The object that handles actions generated by a SwitchIRBuilder, which
 * will be either an IRBuilder or an IRBuilderAsmJs
 */
struct SwitchAdapter {

    virtual void AddBranchInstr(IR::BranchInstr * instr, uint32 offset, uint32 targetOffset, bool clearBackEdge = false) = 0;

    virtual void AddInstr(IR::Instr * instr, uint32 offset) = 0;

    virtual void CreateRelocRecord(IR::BranchInstr * branchInstr, uint32 offset, uint32 targetOffset, bool clearBackEdge = false) = 0;

    virtual void ConvertToBailOut(IR::Instr * instr, IR::BailOutKind kind) = 0;
};

/**
 * Handles delegating actions generated by a SwitchIRBuilder to an IRBuilder
 */
struct IRBuilderSwitchAdapter : public SwitchAdapter {
private:
    IRBuilder * m_builder;
public:
    IRBuilderSwitchAdapter(IRBuilder * builder)
        : m_builder(builder) {TRACE_IT(15543);}

    virtual void AddBranchInstr(IR::BranchInstr * instr, uint32 offset, uint32 targetOffset, bool clearBackEdge = false);

    virtual void AddInstr(IR::Instr * instr, uint32 offset);

    virtual void CreateRelocRecord(IR::BranchInstr * branchInstr, uint32 offset, uint32 targetOffset, bool clearBackEdge = false);

    virtual void ConvertToBailOut(IR::Instr * instr, IR::BailOutKind kind);
};

/**
 * Handles delegating actions generated by a SwitchIRBuilder to an IRBuilderAsmJs
 */
#ifdef ASMJS_PLAT
struct IRBuilderAsmJsSwitchAdapter : public SwitchAdapter {
private:
    IRBuilderAsmJs * m_builder;
public:
    IRBuilderAsmJsSwitchAdapter(IRBuilderAsmJs * builder)
        : m_builder(builder) {TRACE_IT(15544);}

    virtual void AddBranchInstr(IR::BranchInstr * instr, uint32 offset, uint32 targetOffset, bool clearBackEdge = false);

    virtual void AddInstr(IR::Instr * instr, uint32 offset);

    virtual void CreateRelocRecord(IR::BranchInstr * branchInstr, uint32 offset, uint32 targetOffset, bool clearBackEdge = false);

    virtual void ConvertToBailOut(IR::Instr * instr, IR::BailOutKind kind);
};
#endif

/**
 * Handles construction of switch statements, with appropriate optimizations
 */
class SwitchIRBuilder {
private:
    typedef JsUtil::List<CaseNode*, JitArenaAllocator>              CaseNodeList;
    typedef JsUtil::List<JITJavascriptString *, JitArenaAllocator> StrSwitchCaseList;

    SwitchAdapter*                  m_adapter;
    Func*                           m_func;
    JitArenaAllocator*              m_tempAlloc;
    CaseNodeList*                   m_caseNodes;
    bool                            m_seenOnlySingleCharStrCaseNodes;
    IR::Instr *                     m_profiledSwitchInstr;
    bool                            m_isAsmJs;
    bool                            m_switchOptBuildBail; //bool refers to whether the bail out has to be generated or not
    bool                            m_switchIntDynProfile; // bool refers to whether dynamic profile info says that the switch expression is an integer or not
    bool                            m_switchStrDynProfile; // bool refers to whether dynamic profile info says that the switch expression is a string or not
    BVSparse<JitArenaAllocator> *   m_intConstSwitchCases;
    StrSwitchCaseList *             m_strConstSwitchCases;

    Js::OpCode                      m_eqOp;
    Js::OpCode                      m_ltOp;
    Js::OpCode                      m_leOp;
    Js::OpCode                      m_gtOp;
    Js::OpCode                      m_geOp;
    Js::OpCode                      m_subOp;

public:

    SwitchIRBuilder(SwitchAdapter * adapter)
        : m_adapter(adapter)
        , m_profiledSwitchInstr(nullptr)
        , m_switchOptBuildBail(false)
        , m_switchIntDynProfile(false)
        , m_switchStrDynProfile(false)
        , m_isAsmJs(false)
        , m_seenOnlySingleCharStrCaseNodes(true) {TRACE_IT(15545);}

    void                Init(Func * func, JitArenaAllocator * tempAlloc, bool isAsmJs);
    void                BeginSwitch();
    void                EndSwitch(uint32 offset, uint32 targetOffset);
    void                SetProfiledInstruction(IR::Instr * instr, Js::ProfileId profileId);
    void                OnCase(IR::RegOpnd * src1Opnd, IR::Opnd * src2Opnd, uint32 offset, uint32 targetOffset);
    void                FlushCases(uint32 targetOffset);

    void                RefineCaseNodes();
    void                ResetCaseNodes();
    void                BuildCaseBrInstr(uint32 targetOffset);
    void                BuildBinaryTraverseInstr(int start, int end, uint32 defaultLeafBranch);
    void                BuildLinearTraverseInstr(int start, int end, uint32 defaultLeafBranch);
    void                BuildEmptyCasesInstr(CaseNode* currCaseNode, uint32 defaultLeafBranch);
    void                BuildOptimizedIntegerCaseInstrs(uint32 targetOffset);
    void                BuildMultiBrCaseInstrForStrings(uint32 targetOffset);
    void                FixUpMultiBrJumpTable(IR::MultiBranchInstr * multiBranchInstr, uint32 targetOffset);
    void                TryBuildBinaryTreeOrMultiBrForSwitchInts(IR::MultiBranchInstr * &multiBranchInstr, uint32 fallthrOffset,
        int startjmpTableIndex, int endjmpTableIndex, int startBinaryTravIndex, uint32 targetOffset);
    bool                TestAndAddStringCaseConst(JITJavascriptString * str);
    void                BuildBailOnNotInteger();
    void                BuildBailOnNotString();
    IR::MultiBranchInstr * BuildMultiBrCaseInstrForInts(uint32 start, uint32 end, uint32 targetOffset);
};
