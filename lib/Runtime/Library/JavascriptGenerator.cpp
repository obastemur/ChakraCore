//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
#include "Language/InterpreterStackFrame.h"

namespace Js
{
    JavascriptGenerator::JavascriptGenerator(DynamicType* type, Arguments &args, ScriptFunction* scriptFunction)
        : DynamicObject(type), frame(nullptr), state(GeneratorState::Suspended), args(args), scriptFunction(scriptFunction)
    {LOGMEIN("JavascriptGenerator.cpp] 11\n");
    }

    JavascriptGenerator* JavascriptGenerator::New(Recycler* recycler, DynamicType* generatorType, Arguments& args, ScriptFunction* scriptFunction)
    {LOGMEIN("JavascriptGenerator.cpp] 15\n");
#if GLOBAL_ENABLE_WRITE_BARRIER
        if (CONFIG_FLAG(ForceSoftwareWriteBarrier))
        {LOGMEIN("JavascriptGenerator.cpp] 18\n");
            JavascriptGenerator* obj = RecyclerNewFinalized(
                recycler, JavascriptGenerator, generatorType, args, scriptFunction);
            recycler->RegisterPendingWriteBarrierBlock(obj->args.Values, obj->args.Info.Count * sizeof(Var));
            recycler->RegisterPendingWriteBarrierBlock(&obj->args.Values, sizeof(Var*));
            return obj;
        }
        else
#endif
        {
            return RecyclerNew(recycler, JavascriptGenerator, generatorType, args, scriptFunction);
        }
    }

    bool JavascriptGenerator::Is(Var var)
    {LOGMEIN("JavascriptGenerator.cpp] 33\n");
        return JavascriptOperators::GetTypeId(var) == TypeIds_Generator;
    }

    JavascriptGenerator* JavascriptGenerator::FromVar(Var var)
    {
        AssertMsg(Is(var), "Ensure var is actually a 'JavascriptGenerator'");

        return static_cast<JavascriptGenerator*>(var);
    }

    void JavascriptGenerator::SetFrame(InterpreterStackFrame* frame, size_t bytes)
    {LOGMEIN("JavascriptGenerator.cpp] 45\n");
        Assert(this->frame == nullptr);
        this->frame = frame;
#if GLOBAL_ENABLE_WRITE_BARRIER
        if (CONFIG_FLAG(ForceSoftwareWriteBarrier))
        {LOGMEIN("JavascriptGenerator.cpp] 50\n");
            this->GetScriptContext()->GetRecycler()->RegisterPendingWriteBarrierBlock(frame, bytes);
        }
#endif
    }

#if GLOBAL_ENABLE_WRITE_BARRIER
    void JavascriptGenerator::Finalize(bool isShutdown)
    {LOGMEIN("JavascriptGenerator.cpp] 58\n");
        if (CONFIG_FLAG(ForceSoftwareWriteBarrier) && !isShutdown)
        {LOGMEIN("JavascriptGenerator.cpp] 60\n");
            if (this->frame)
            {LOGMEIN("JavascriptGenerator.cpp] 62\n");
                this->GetScriptContext()->GetRecycler()->UnRegisterPendingWriteBarrierBlock(this->frame);
            }
            if (this->args.Values)
            {LOGMEIN("JavascriptGenerator.cpp] 66\n");
                this->GetScriptContext()->GetRecycler()->UnRegisterPendingWriteBarrierBlock(this->args.Values);
            }
        }
    }
#endif

    Var JavascriptGenerator::CallGenerator(ResumeYieldData* yieldData, const char16* apiNameForErrorMessage)
    {LOGMEIN("JavascriptGenerator.cpp] 74\n");
        ScriptContext* scriptContext = this->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();
        Var result = nullptr;

        if (this->IsExecuting())
        {LOGMEIN("JavascriptGenerator.cpp] 80\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_GeneratorAlreadyExecuting, apiNameForErrorMessage);
        }

        {LOGMEIN("JavascriptGenerator.cpp] 84\n");
            // RAII helper to set the state of the generator to completed if an exception is thrown
            // or if the save state InterpreterStackFrame is never created implying the generator
            // is JITed and returned without ever yielding.
            class GeneratorStateHelper
            {
                JavascriptGenerator* g;
                bool didThrow;
            public:
                GeneratorStateHelper(JavascriptGenerator* g) : g(g), didThrow(true) {LOGMEIN("JavascriptGenerator.cpp] 93\n"); g->SetState(GeneratorState::Executing); }
                ~GeneratorStateHelper() {LOGMEIN("JavascriptGenerator.cpp] 94\n"); g->SetState(didThrow || g->frame == nullptr ? GeneratorState::Completed : GeneratorState::Suspended); }
                void DidNotThrow() {LOGMEIN("JavascriptGenerator.cpp] 95\n"); didThrow = false; }
            } helper(this);

            Var thunkArgs[] = { this, yieldData };
            Arguments arguments(_countof(thunkArgs), thunkArgs);

            try
            {LOGMEIN("JavascriptGenerator.cpp] 102\n");
                result = JavascriptFunction::CallFunction<1>(this->scriptFunction, this->scriptFunction->GetEntryPoint(), arguments);
                helper.DidNotThrow();
            }
            catch (const JavascriptException& err)
            {LOGMEIN("JavascriptGenerator.cpp] 107\n");
                Js::JavascriptExceptionObject* exceptionObj = err.GetAndClear();
                if (!exceptionObj->IsGeneratorReturnException())
                {LOGMEIN("JavascriptGenerator.cpp] 110\n");
                    JavascriptExceptionOperators::DoThrow(exceptionObj, scriptContext);
                }
                result = exceptionObj->GetThrownObject(nullptr);
            }
        }

        if (!this->IsCompleted())
        {LOGMEIN("JavascriptGenerator.cpp] 118\n");
            int nextOffset = this->frame->GetReader()->GetCurrentOffset();
            int endOffset = this->frame->GetFunctionBody()->GetByteCode()->GetLength();

            if (nextOffset != endOffset - 1)
            {LOGMEIN("JavascriptGenerator.cpp] 123\n");
                // Yielded values are already wrapped in an IteratorResult object, so we don't need to wrap them.
                return result;
            }
        }

        result = library->CreateIteratorResultObject(result, library->GetTrue());
        this->SetState(GeneratorState::Completed);

        return result;
    }

    Var JavascriptGenerator::EntryNext(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();

        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("Generator.prototype.next"));

        if (!JavascriptGenerator::Is(args[0]))
        {LOGMEIN("JavascriptGenerator.cpp] 146\n");
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Generator.prototype.next"), _u("Generator"));
        }

        JavascriptGenerator* generator = JavascriptGenerator::FromVar(args[0]);
        Var input = args.Info.Count > 1 ? args[1] : library->GetUndefined();

        if (generator->IsCompleted())
        {LOGMEIN("JavascriptGenerator.cpp] 154\n");
            return library->CreateIteratorResultObjectUndefinedTrue();
        }

        ResumeYieldData yieldData(input, nullptr);
        return generator->CallGenerator(&yieldData, _u("Generator.prototype.next"));
    }

    Var JavascriptGenerator::EntryReturn(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();

        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("Generator.prototype.return"));

        if (!JavascriptGenerator::Is(args[0]))
        {LOGMEIN("JavascriptGenerator.cpp] 173\n");
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Generator.prototype.return"), _u("Generator"));
        }

        JavascriptGenerator* generator = JavascriptGenerator::FromVar(args[0]);
        Var input = args.Info.Count > 1 ? args[1] : library->GetUndefined();

        if (generator->IsSuspendedStart())
        {LOGMEIN("JavascriptGenerator.cpp] 181\n");
            generator->SetState(GeneratorState::Completed);
        }

        if (generator->IsCompleted())
        {LOGMEIN("JavascriptGenerator.cpp] 186\n");
            return library->CreateIteratorResultObject(input, library->GetTrue());
        }

        ResumeYieldData yieldData(input, RecyclerNew(scriptContext->GetRecycler(), GeneratorReturnExceptionObject, input, scriptContext));
        return generator->CallGenerator(&yieldData, _u("Generator.prototype.return"));
    }

    Var JavascriptGenerator::EntryThrow(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();

        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("Generator.prototype.throw"));

        if (!JavascriptGenerator::Is(args[0]))
        {LOGMEIN("JavascriptGenerator.cpp] 205\n");
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Generator.prototype.throw"), _u("Generator"));
        }

        JavascriptGenerator* generator = JavascriptGenerator::FromVar(args[0]);
        Var input = args.Info.Count > 1 ? args[1] : library->GetUndefined();

        if (generator->IsSuspendedStart())
        {LOGMEIN("JavascriptGenerator.cpp] 213\n");
            generator->SetState(GeneratorState::Completed);
        }

        if (generator->IsCompleted())
        {LOGMEIN("JavascriptGenerator.cpp] 218\n");
            JavascriptExceptionOperators::OP_Throw(input, scriptContext);
        }

        ResumeYieldData yieldData(input, RecyclerNew(scriptContext->GetRecycler(), JavascriptExceptionObject, input, scriptContext, nullptr));
        return generator->CallGenerator(&yieldData, _u("Generator.prototype.throw"));
    }

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType JavascriptGenerator::GetSnapTag_TTD() const
    {LOGMEIN("JavascriptGenerator.cpp] 228\n");
        return TTD::NSSnapObjects::SnapObjectType::Invalid;
    }

    void JavascriptGenerator::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {
        TTDAssert(false, "Invalid -- JavascriptGenerator");
    }
#endif
}
