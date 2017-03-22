//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js {
    //
    // ScopeInfo is used to persist Scope info of outer functions. When reparsing deferred nested
    // functions, use persisted ScopeInfo to restore outer closures.
    //
    class ScopeInfo
    {
        struct MapSymbolData
        {
            ByteCodeGenerator* byteCodeGenerator;
            FuncInfo* func;
            int nonScopeSymbolCount;
        };

        struct SymbolInfo
        {
            union
            {
                Field(PropertyId) propertyId;
                Field(PropertyRecord const*) name;
            };
            Field(SymbolType) symbolType;
            Field(bool) hasFuncAssignment;
            Field(bool) isBlockVariable;
            Field(bool) isFuncExpr;
            Field(bool) isModuleExportStorage;
            Field(bool) isModuleImport;
        };

    private:
        Field(FunctionInfo * const) parent;    // link to parent function
        Field(ScopeInfo*) funcExprScopeInfo;   // optional func expr scope info
        Field(ScopeInfo*) paramScopeInfo;      // optional param scope info

        Field(BYTE) isDynamic : 1;             // isDynamic bit affects how deferredChild access global ref
        Field(BYTE) isObject : 1;              // isObject bit affects how deferredChild access closure symbols
        Field(BYTE) mustInstantiate : 1;       // the scope must be instantiated as an object/array
        Field(BYTE) isCached : 1;              // indicates that local vars and functions are cached across invocations
        Field(BYTE) isGlobalEval : 1;
        Field(BYTE) areNamesCached : 1;
        Field(BYTE) canMergeWithBodyScope : 1;
        Field(BYTE) hasLocalInClosure : 1;
        Field(BYTE) parentOnly : 1;

        FieldNoBarrier(Scope *) scope;
        Field(int) scopeId;
        Field(int) symbolCount;                // symbol count in this scope
        Field(SymbolInfo) symbols[];           // symbol PropertyIDs, index == sym.scopeSlot

    private:
        ScopeInfo(FunctionInfo * parent, int symbolCount)
            : parent(parent), funcExprScopeInfo(nullptr), paramScopeInfo(nullptr), symbolCount(symbolCount), scope(nullptr), areNamesCached(false), canMergeWithBodyScope(true), hasLocalInClosure(false), parentOnly(false)
        {LOGMEIN("ScopeInfo.h] 58\n");
        }

        bool IsParentInfoOnly() const {LOGMEIN("ScopeInfo.h] 61\n"); return parentOnly; }

        void SetFuncExprScopeInfo(ScopeInfo* funcExprScopeInfo)
        {LOGMEIN("ScopeInfo.h] 64\n");
            this->funcExprScopeInfo = funcExprScopeInfo;
        }

        void SetParamScopeInfo(ScopeInfo* paramScopeInfo)
        {LOGMEIN("ScopeInfo.h] 69\n");
            this->paramScopeInfo = paramScopeInfo;
        }

        void SetSymbolId(int i, PropertyId propertyId)
        {LOGMEIN("ScopeInfo.h] 74\n");
            Assert(!areNamesCached);
            Assert(i >= 0 && i < symbolCount);
            symbols[i].propertyId = propertyId;
        }

        void SetSymbolType(int i, SymbolType symbolType)
        {LOGMEIN("ScopeInfo.h] 81\n");
            Assert(!areNamesCached);
            Assert(i >= 0 && i < symbolCount);
            symbols[i].symbolType = symbolType;
        }

        void SetHasFuncAssignment(int i, bool has)
        {LOGMEIN("ScopeInfo.h] 88\n");
            Assert(!areNamesCached);
            Assert(i >= 0 && i < symbolCount);
            symbols[i].hasFuncAssignment = has;
        }

        void SetIsBlockVariable(int i, bool is)
        {LOGMEIN("ScopeInfo.h] 95\n");
            Assert(!areNamesCached);
            Assert(i >= 0 && i < symbolCount);
            symbols[i].isBlockVariable = is;
        }

        void SetIsFuncExpr(int i, bool is)
        {LOGMEIN("ScopeInfo.h] 102\n");
            Assert(!areNamesCached);
            Assert(i >= 0 && i < symbolCount);
            symbols[i].isFuncExpr = is;
        }

        void SetIsModuleExportStorage(int i, bool is)
        {LOGMEIN("ScopeInfo.h] 109\n");
            Assert(!areNamesCached);
            Assert(i >= 0 && i < symbolCount);
            symbols[i].isModuleExportStorage = is;
        }

        void SetIsModuleImport(int i, bool is)
        {LOGMEIN("ScopeInfo.h] 116\n");
            Assert(!areNamesCached);
            Assert(i >= 0 && i < symbolCount);
            symbols[i].isModuleImport = is;
        }

        void SetPropertyName(int i, PropertyRecord const* name)
        {LOGMEIN("ScopeInfo.h] 123\n");
            Assert(!areNamesCached);
            Assert(i >= 0 && i < symbolCount);
            symbols[i].name = name;
        }

        PropertyId GetSymbolId(int i) const
        {LOGMEIN("ScopeInfo.h] 130\n");
            Assert(!areNamesCached);
            Assert(i >= 0 && i < symbolCount);
            return symbols[i].propertyId;
        }

        SymbolType GetSymbolType(int i) const
        {LOGMEIN("ScopeInfo.h] 137\n");
            Assert(i >= 0 && i < symbolCount);
            return symbols[i].symbolType;
        }

        bool GetHasFuncAssignment(int i)
        {LOGMEIN("ScopeInfo.h] 143\n");
            Assert(i >= 0 && i < symbolCount);
            return symbols[i].hasFuncAssignment;
        }

        bool GetIsModuleExportStorage(int i)
        {LOGMEIN("ScopeInfo.h] 149\n");
            Assert(i >= 0 && i < symbolCount);
            return symbols[i].isModuleExportStorage;
        }

        bool GetIsModuleImport(int i)
        {LOGMEIN("ScopeInfo.h] 155\n");
            Assert(i >= 0 && i < symbolCount);
            return symbols[i].isModuleImport;
        }

        bool GetIsBlockVariable(int i)
        {LOGMEIN("ScopeInfo.h] 161\n");
            Assert(i >= 0 && i < symbolCount);
            return symbols[i].isBlockVariable;
        }

        bool GetIsFuncExpr(int i)
        {LOGMEIN("ScopeInfo.h] 167\n");
            Assert(i >= 0 && i < symbolCount);
            return symbols[i].isFuncExpr;
        }

        PropertyRecord const* GetPropertyName(int i)
        {LOGMEIN("ScopeInfo.h] 173\n");
            Assert(areNamesCached);
            Assert(i >= 0 && i < symbolCount);
            return symbols[i].name;
        }

        void SaveSymbolInfo(Symbol* sym, MapSymbolData* mapSymbolData);

        static ScopeInfo* FromParent(FunctionBody* parent);
        static ScopeInfo* FromScope(ByteCodeGenerator* byteCodeGenerator, ParseableFunctionInfo* parent, Scope* scope, ScriptContext *scriptContext);
        static void SaveParentScopeInfo(FuncInfo* parentFunc, FuncInfo* func);
        static void SaveScopeInfo(ByteCodeGenerator* byteCodeGenerator, FuncInfo* parentFunc, FuncInfo* func);

    public:
        ParseableFunctionInfo * GetParent() const
        {LOGMEIN("ScopeInfo.h] 188\n");
            return parent ? parent->GetParseableFunctionInfo() : nullptr;
        }

        ScopeInfo* GetParentScopeInfo() const
        {LOGMEIN("ScopeInfo.h] 193\n");
            return parent ? parent->GetParseableFunctionInfo()->GetScopeInfo() : nullptr;
        }

        ScopeInfo* GetFuncExprScopeInfo() const
        {LOGMEIN("ScopeInfo.h] 198\n");
            return funcExprScopeInfo;
        }

        ScopeInfo* GetParamScopeInfo() const
        {LOGMEIN("ScopeInfo.h] 203\n");
            return paramScopeInfo;
        }

        Scope * GetScope() const
        {LOGMEIN("ScopeInfo.h] 208\n");
            return scope;
        }

        void SetScopeId(int id)
        {LOGMEIN("ScopeInfo.h] 213\n");
            this->scopeId = id;
        }

        int GetScopeId() const
        {LOGMEIN("ScopeInfo.h] 218\n");
            return scopeId;
        }

        int GetSymbolCount() const
        {LOGMEIN("ScopeInfo.h] 223\n");
            return symbolCount;
        }

        bool IsGlobalEval() const
        {LOGMEIN("ScopeInfo.h] 228\n");
            return isGlobalEval;
        }

        bool GetCanMergeWithBodyScope() const
        {LOGMEIN("ScopeInfo.h] 233\n");
            return canMergeWithBodyScope;
        }

        void SetHasLocalInClosure(bool has)
        {LOGMEIN("ScopeInfo.h] 238\n");
            hasLocalInClosure = has;
        }

        bool GetHasOwnLocalInClosure() const
        {LOGMEIN("ScopeInfo.h] 243\n");
            return hasLocalInClosure;
        }

        static void SaveScopeInfoForDeferParse(ByteCodeGenerator* byteCodeGenerator, FuncInfo* parentFunc, FuncInfo* func);

        void EnsurePidTracking(ScriptContext* scriptContext);

        void GetScopeInfo(Parser *parser, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo, Scope* scope);

        //
        // Turn on capturesAll for a Scope temporarily. Restore old capturesAll when this object
        // goes out of scope.
        //
        class AutoCapturesAllScope
        {
        private:
            Scope* scope;
            bool oldCapturesAll;

        public:
            AutoCapturesAllScope(Scope* scope, bool turnOn);
            ~AutoCapturesAllScope();
            bool OldCapturesAll() const
            {LOGMEIN("ScopeInfo.h] 267\n");
                return oldCapturesAll;
            }
        };
    };
}
