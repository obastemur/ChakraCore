//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    // Helper struct used to communicate to a yield point whether it was resumed via next(), return(), or throw()
    // and provide the data necessary for the corresponding action taken (see OP_ResumeYield)
    struct ResumeYieldData
    {
        Var data;
        JavascriptExceptionObject* exceptionObj;

        ResumeYieldData(Var data, JavascriptExceptionObject* exceptionObj) : data(data), exceptionObj(exceptionObj) {TRACE_IT(58995); }
    };

    class JavascriptGenerator : public DynamicObject
    {
    public:
        enum class GeneratorState {
            Suspended,
            Executing,
            Completed
        };

        static uint32 GetFrameOffset() {TRACE_IT(58996); return offsetof(JavascriptGenerator, frame); }
        static uint32 GetCallInfoOffset() {TRACE_IT(58997); return offsetof(JavascriptGenerator, args) + Arguments::GetCallInfoOffset(); }
        static uint32 GetArgsPtrOffset() {TRACE_IT(58998); return offsetof(JavascriptGenerator, args) + Arguments::GetValuesOffset(); }

    private:
        Field(InterpreterStackFrame*) frame;
        Field(GeneratorState) state;
        Field(Arguments) args;
        Field(ScriptFunction*) scriptFunction;

        DEFINE_VTABLE_CTOR_MEMBER_INIT(JavascriptGenerator, DynamicObject, args);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(JavascriptGenerator);

        void SetState(GeneratorState state)
        {TRACE_IT(58999);
            this->state = state;
            if (state == GeneratorState::Completed)
            {TRACE_IT(59000);
                frame = nullptr;
                args.Values = nullptr;
                scriptFunction = nullptr;
            }
        }

        Var CallGenerator(ResumeYieldData* yieldData, const char16* apiNameForErrorMessage);
        JavascriptGenerator(DynamicType* type, Arguments& args, ScriptFunction* scriptFunction);

    public:
        static JavascriptGenerator* New(Recycler* recycler, DynamicType* generatorType, Arguments& args, ScriptFunction* scriptFunction);

        bool IsExecuting() const {TRACE_IT(59001); return state == GeneratorState::Executing; }
        bool IsSuspended() const {TRACE_IT(59002); return state == GeneratorState::Suspended; }
        bool IsCompleted() const {TRACE_IT(59003); return state == GeneratorState::Completed; }
        bool IsSuspendedStart() const {TRACE_IT(59004); return state == GeneratorState::Suspended && this->frame == nullptr; }

        void SetFrame(InterpreterStackFrame* frame, size_t bytes);
        InterpreterStackFrame* GetFrame() const {TRACE_IT(59005); return frame; }

#if GLOBAL_ENABLE_WRITE_BARRIER
        virtual void Finalize(bool isShutdown) override;
#endif

        const Arguments& GetArguments() const {TRACE_IT(59006); return args; }

        static bool Is(Var var);
        static JavascriptGenerator* FromVar(Var var);

        class EntryInfo
        {
        public:
            static FunctionInfo Next;
            static FunctionInfo Return;
            static FunctionInfo Throw;
        };
        static Var EntryNext(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryReturn(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryThrow(RecyclableObject* function, CallInfo callInfo, ...);

#if ENABLE_TTD
        virtual TTD::NSSnapObjects::SnapObjectType GetSnapTag_TTD() const override;
        virtual void ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc) override;
#endif
    };
}
