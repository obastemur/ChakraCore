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
            : parent(parent), funcExprScopeInfo(nullptr), paramScopeInfo(nullptr), symbolCount(symbolCount), scope(nullptr), areNamesCached(false), hasLocalInClosure(false), parentOnly(false)
        {TRACE_IT(41927);
        }

        bool IsParentInfoOnly() const {TRACE_IT(41928); return parentOnly; }

        void SetFuncExprScopeInfo(ScopeInfo* funcExprScopeInfo)
        {TRACE_IT(41929);
            this->funcExprScopeInfo = funcExprScopeInfo;
        }

        void SetParamScopeInfo(ScopeInfo* paramScopeInfo)
        {TRACE_IT(41930);
            this->paramScopeInfo = paramScopeInfo;
        }

        void SetSymbolId(int i, PropertyId propertyId)
        {TRACE_IT(41931);
            Assert(!areNamesCached);
            Assert(i >= 0 && i < symbolCount);
            symbols[i].propertyId = propertyId;
        }

        void SetSymbolType(int i, SymbolType symbolType)
        {TRACE_IT(41932);
            Assert(!areNamesCached);
            Assert(i >= 0 && i < symbolCount);
            symbols[i].symbolType = symbolType;
        }

        void SetHasFuncAssignment(int i, bool has)
        {TRACE_IT(41933);
            Assert(!areNamesCached);
            Assert(i >= 0 && i < symbolCount);
            symbols[i].hasFuncAssignment = has;
        }

        void SetIsBlockVariable(int i, bool is)
        {TRACE_IT(41934);
            Assert(!areNamesCached);
            Assert(i >= 0 && i < symbolCount);
            symbols[i].isBlockVariable = is;
        }

        void SetIsFuncExpr(int i, bool is)
        {TRACE_IT(41935);
            Assert(!areNamesCached);
            Assert(i >= 0 && i < symbolCount);
            symbols[i].isFuncExpr = is;
        }

        void SetIsModuleExportStorage(int i, bool is)
        {TRACE_IT(41936);
            Assert(!areNamesCached);
            Assert(i >= 0 && i < symbolCount);
            symbols[i].isModuleExportStorage = is;
        }

        void SetIsModuleImport(int i, bool is)
        {TRACE_IT(41937);
            Assert(!areNamesCached);
            Assert(i >= 0 && i < symbolCount);
            symbols[i].isModuleImport = is;
        }

        void SetPropertyName(int i, PropertyRecord const* name)
        {TRACE_IT(41938);
            Assert(!areNamesCached);
            Assert(i >= 0 && i < symbolCount);
            symbols[i].name = name;
        }

        PropertyId GetSymbolId(int i) const
        {TRACE_IT(41939);
            Assert(!areNamesCached);
            Assert(i >= 0 && i < symbolCount);
            return symbols[i].propertyId;
        }

        SymbolType GetSymbolType(int i) const
        {TRACE_IT(41940);
            Assert(i >= 0 && i < symbolCount);
            return symbols[i].symbolType;
        }

        bool GetHasFuncAssignment(int i)
        {TRACE_IT(41941);
            Assert(i >= 0 && i < symbolCount);
            return symbols[i].hasFuncAssignment;
        }

        bool GetIsModuleExportStorage(int i)
        {TRACE_IT(41942);
            Assert(i >= 0 && i < symbolCount);
            return symbols[i].isModuleExportStorage;
        }

        bool GetIsModuleImport(int i)
        {TRACE_IT(41943);
            Assert(i >= 0 && i < symbolCount);
            return symbols[i].isModuleImport;
        }

        bool GetIsBlockVariable(int i)
        {TRACE_IT(41944);
            Assert(i >= 0 && i < symbolCount);
            return symbols[i].isBlockVariable;
        }

        bool GetIsFuncExpr(int i)
        {TRACE_IT(41945);
            Assert(i >= 0 && i < symbolCount);
            return symbols[i].isFuncExpr;
        }

        PropertyRecord const* GetPropertyName(int i)
        {TRACE_IT(41946);
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
        {TRACE_IT(41947);
            return parent ? parent->GetParseableFunctionInfo() : nullptr;
        }

        ScopeInfo* GetParentScopeInfo() const
        {TRACE_IT(41948);
            return parent ? parent->GetParseableFunctionInfo()->GetScopeInfo() : nullptr;
        }

        ScopeInfo* GetFuncExprScopeInfo() const
        {TRACE_IT(41949);
            return funcExprScopeInfo;
        }

        ScopeInfo* GetParamScopeInfo() const
        {TRACE_IT(41950);
            return paramScopeInfo;
        }

        Scope * GetScope() const
        {TRACE_IT(41951);
            return scope;
        }

        void SetScopeId(int id)
        {TRACE_IT(41952);
            this->scopeId = id;
        }

        int GetScopeId() const
        {TRACE_IT(41953);
            return scopeId;
        }

        int GetSymbolCount() const
        {TRACE_IT(41954);
            return symbolCount;
        }

        bool IsGlobalEval() const
        {TRACE_IT(41955);
            return isGlobalEval;
        }

        void SetHasLocalInClosure(bool has)
        {TRACE_IT(41956);
            hasLocalInClosure = has;
        }

        bool GetHasOwnLocalInClosure() const
        {TRACE_IT(41957);
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
            {TRACE_IT(41958);
                return oldCapturesAll;
            }
        };
    };
}
