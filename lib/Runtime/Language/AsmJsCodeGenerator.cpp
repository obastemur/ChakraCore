//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

#ifdef ASMJS_PLAT
#include "CodeGenAllocators.h"

namespace Js
{
    AsmJsCodeGenerator::AsmJsCodeGenerator( ScriptContext* scriptContext ) :
        mScriptContext( scriptContext )
        ,mPageAllocator(scriptContext->GetThreadContext()->GetPageAllocator())
    {TRACE_IT(46190);
        //use the same foreground allocator as NativeCodeGen
        mForegroundAllocators = GetForegroundAllocator(scriptContext->GetNativeCodeGenerator(),mPageAllocator);
        mEncoder.SetPageAllocator( mPageAllocator );
        mEncoder.SetCodeGenAllocator( mForegroundAllocators );
    }

    void AsmJsCodeGenerator::CodeGen( FunctionBody* functionBody )
    {TRACE_IT(46191);
        AsmJsFunctionInfo* asmInfo = functionBody->GetAsmJsFunctionInfo();
        Assert( asmInfo );

        void* address = mEncoder.Encode( functionBody );
        if( address )
        {TRACE_IT(46192);
            FunctionEntryPointInfo* funcEntrypointInfo = (FunctionEntryPointInfo*)functionBody->GetDefaultEntryPointInfo();
            EntryPointInfo* entrypointInfo = (EntryPointInfo*)funcEntrypointInfo;
            Assert(entrypointInfo->GetIsAsmJSFunction());
            //set entrypointinfo address and nativeAddress with TJ address
            Js::JavascriptMethod method = reinterpret_cast<Js::JavascriptMethod>(address);
            entrypointInfo->jsMethod = method;
            entrypointInfo->SetNativeAddress(method);
#if ENABLE_DEBUG_CONFIG_OPTIONS
            funcEntrypointInfo->SetIsTJMode(true);
#endif
            if (!PreReservedVirtualAllocWrapper::IsInRange((void*)mScriptContext->GetThreadContext()->GetPreReservedRegionAddr(), (void*)address))
            {TRACE_IT(46193);
                Assert(entrypointInfo->GetCodeSize() < (uint64)((uint64)1 << 32));
                mScriptContext->GetJitFuncRangeCache()->AddFuncRange((void*)address, (uint)entrypointInfo->GetCodeSize());
            }
        }
    }

}
#endif
