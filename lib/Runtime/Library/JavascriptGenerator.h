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

        ResumeYieldData(Var data, JavascriptExceptionObject* exceptionObj) : data(data), exceptionObj(exceptionObj) {LOGMEIN("JavascriptGenerator.h] 15\n"); }
    };

    class JavascriptGenerator : public DynamicObject
    {
    public:
        enum class GeneratorState {
            Suspended,
            Executing,
            Completed
        };

        static uint32 GetFrameOffset() {LOGMEIN("JavascriptGenerator.h] 27\n"); return offsetof(JavascriptGenerator, frame); }
        static uint32 GetCallInfoOffset() {LOGMEIN("JavascriptGenerator.h] 28\n"); return offsetof(JavascriptGenerator, args) + Arguments::GetCallInfoOffset(); }
        static uint32 GetArgsPtrOffset() {LOGMEIN("JavascriptGenerator.h] 29\n"); return offsetof(JavascriptGenerator, args) + Arguments::GetValuesOffset(); }

    private:
        Field(InterpreterStackFrame*) frame;
        Field(GeneratorState) state;
        Field(Arguments) args;
        Field(ScriptFunction*) scriptFunction;

        DEFINE_VTABLE_CTOR_MEMBER_INIT(JavascriptGenerator, DynamicObject, args);
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(JavascriptGenerator);

        void SetState(GeneratorState state)
        {LOGMEIN("JavascriptGenerator.h] 41\n");
            this->state = state;
            if (state == GeneratorState::Completed)
            {LOGMEIN("JavascriptGenerator.h] 44\n");
                frame = nullptr;
                args.Values = nullptr;
                scriptFunction = nullptr;
            }
        }

        Var CallGenerator(ResumeYieldData* yieldData, const char16* apiNameForErrorMessage);
        JavascriptGenerator(DynamicType* type, Arguments& args, ScriptFunction* scriptFunction);

    public:
        static JavascriptGenerator* New(Recycler* recycler, DynamicType* generatorType, Arguments& args, ScriptFunction* scriptFunction);

        bool IsExecuting() const {LOGMEIN("JavascriptGenerator.h] 57\n"); return state == GeneratorState::Executing; }
        bool IsSuspended() const {LOGMEIN("JavascriptGenerator.h] 58\n"); return state == GeneratorState::Suspended; }
        bool IsCompleted() const {LOGMEIN("JavascriptGenerator.h] 59\n"); return state == GeneratorState::Completed; }
        bool IsSuspendedStart() const {LOGMEIN("JavascriptGenerator.h] 60\n"); return state == GeneratorState::Suspended && this->frame == nullptr; }

        void SetFrame(InterpreterStackFrame* frame, size_t bytes);
        InterpreterStackFrame* GetFrame() const {LOGMEIN("JavascriptGenerator.h] 63\n"); return frame; }

#if GLOBAL_ENABLE_WRITE_BARRIER
        virtual void Finalize(bool isShutdown) override;
#endif

        const Arguments& GetArguments() const {LOGMEIN("JavascriptGenerator.h] 69\n"); return args; }

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
