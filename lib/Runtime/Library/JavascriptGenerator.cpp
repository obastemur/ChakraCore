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
    {TRACE_IT(58964);
    }

    JavascriptGenerator* JavascriptGenerator::New(Recycler* recycler, DynamicType* generatorType, Arguments& args, ScriptFunction* scriptFunction)
    {TRACE_IT(58965);
#if GLOBAL_ENABLE_WRITE_BARRIER
        if (CONFIG_FLAG(ForceSoftwareWriteBarrier))
        {TRACE_IT(58966);
            JavascriptGenerator* obj = RecyclerNewFinalized(
                recycler, JavascriptGenerator, generatorType, args, scriptFunction);
            recycler->RegisterPendingWriteBarrierBlock(obj->args.Values, obj->args.Info.Count * sizeof(Var));
            recycler->RegisterPendingWriteBarrierBlock(&obj->args.Values, sizeof(Var*));
            return obj;
        }
        else
#endif
        {TRACE_IT(58967);
            return RecyclerNew(recycler, JavascriptGenerator, generatorType, args, scriptFunction);
        }
    }

    bool JavascriptGenerator::Is(Var var)
    {TRACE_IT(58968);
        return JavascriptOperators::GetTypeId(var) == TypeIds_Generator;
    }

    JavascriptGenerator* JavascriptGenerator::FromVar(Var var)
    {
        AssertMsg(Is(var), "Ensure var is actually a 'JavascriptGenerator'");

        return static_cast<JavascriptGenerator*>(var);
    }

    void JavascriptGenerator::SetFrame(InterpreterStackFrame* frame, size_t bytes)
    {TRACE_IT(58969);
        Assert(this->frame == nullptr);
        this->frame = frame;
#if GLOBAL_ENABLE_WRITE_BARRIER
        if (CONFIG_FLAG(ForceSoftwareWriteBarrier))
        {TRACE_IT(58970);
            this->GetScriptContext()->GetRecycler()->RegisterPendingWriteBarrierBlock(frame, bytes);
        }
#endif
    }

#if GLOBAL_ENABLE_WRITE_BARRIER
    void JavascriptGenerator::Finalize(bool isShutdown)
    {TRACE_IT(58971);
        if (CONFIG_FLAG(ForceSoftwareWriteBarrier) && !isShutdown)
        {TRACE_IT(58972);
            if (this->frame)
            {TRACE_IT(58973);
                this->GetScriptContext()->GetRecycler()->UnRegisterPendingWriteBarrierBlock(this->frame);
            }
            if (this->args.Values)
            {TRACE_IT(58974);
                this->GetScriptContext()->GetRecycler()->UnRegisterPendingWriteBarrierBlock(this->args.Values);
            }
        }
    }
#endif

    Var JavascriptGenerator::CallGenerator(ResumeYieldData* yieldData, const char16* apiNameForErrorMessage)
    {TRACE_IT(58975);
        ScriptContext* scriptContext = this->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();
        Var result = nullptr;

        if (this->IsExecuting())
        {TRACE_IT(58976);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_GeneratorAlreadyExecuting, apiNameForErrorMessage);
        }

        {TRACE_IT(58977);
            // RAII helper to set the state of the generator to completed if an exception is thrown
            // or if the save state InterpreterStackFrame is never created implying the generator
            // is JITed and returned without ever yielding.
            class GeneratorStateHelper
            {
                JavascriptGenerator* g;
                bool didThrow;
            public:
                GeneratorStateHelper(JavascriptGenerator* g) : g(g), didThrow(true) {TRACE_IT(58978); g->SetState(GeneratorState::Executing); }
                ~GeneratorStateHelper() {TRACE_IT(58979); g->SetState(didThrow || g->frame == nullptr ? GeneratorState::Completed : GeneratorState::Suspended); }
                void DidNotThrow() {TRACE_IT(58980); didThrow = false; }
            } helper(this);

            Var thunkArgs[] = { this, yieldData };
            Arguments arguments(_countof(thunkArgs), thunkArgs);

            try
            {TRACE_IT(58981);
                result = JavascriptFunction::CallFunction<1>(this->scriptFunction, this->scriptFunction->GetEntryPoint(), arguments);
                helper.DidNotThrow();
            }
            catch (const JavascriptException& err)
            {TRACE_IT(58982);
                Js::JavascriptExceptionObject* exceptionObj = err.GetAndClear();
                if (!exceptionObj->IsGeneratorReturnException())
                {TRACE_IT(58983);
                    JavascriptExceptionOperators::DoThrow(exceptionObj, scriptContext);
                }
                result = exceptionObj->GetThrownObject(nullptr);
            }
        }

        if (!this->IsCompleted())
        {TRACE_IT(58984);
            int nextOffset = this->frame->GetReader()->GetCurrentOffset();
            int endOffset = this->frame->GetFunctionBody()->GetByteCode()->GetLength();

            if (nextOffset != endOffset - 1)
            {TRACE_IT(58985);
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
        {TRACE_IT(58986);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Generator.prototype.next"), _u("Generator"));
        }

        JavascriptGenerator* generator = JavascriptGenerator::FromVar(args[0]);
        Var input = args.Info.Count > 1 ? args[1] : library->GetUndefined();

        if (generator->IsCompleted())
        {TRACE_IT(58987);
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
        {TRACE_IT(58988);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Generator.prototype.return"), _u("Generator"));
        }

        JavascriptGenerator* generator = JavascriptGenerator::FromVar(args[0]);
        Var input = args.Info.Count > 1 ? args[1] : library->GetUndefined();

        if (generator->IsSuspendedStart())
        {TRACE_IT(58989);
            generator->SetState(GeneratorState::Completed);
        }

        if (generator->IsCompleted())
        {TRACE_IT(58990);
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
        {TRACE_IT(58991);
            JavascriptError::ThrowTypeErrorVar(scriptContext, JSERR_NeedObjectOfType, _u("Generator.prototype.throw"), _u("Generator"));
        }

        JavascriptGenerator* generator = JavascriptGenerator::FromVar(args[0]);
        Var input = args.Info.Count > 1 ? args[1] : library->GetUndefined();

        if (generator->IsSuspendedStart())
        {TRACE_IT(58992);
            generator->SetState(GeneratorState::Completed);
        }

        if (generator->IsCompleted())
        {TRACE_IT(58993);
            JavascriptExceptionOperators::OP_Throw(input, scriptContext);
        }

        ResumeYieldData yieldData(input, RecyclerNew(scriptContext->GetRecycler(), JavascriptExceptionObject, input, scriptContext, nullptr));
        return generator->CallGenerator(&yieldData, _u("Generator.prototype.throw"));
    }

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType JavascriptGenerator::GetSnapTag_TTD() const
    {TRACE_IT(58994);
        return TTD::NSSnapObjects::SnapObjectType::Invalid;
    }

    void JavascriptGenerator::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {
        TTDAssert(false, "Invalid -- JavascriptGenerator");
    }
#endif
}
