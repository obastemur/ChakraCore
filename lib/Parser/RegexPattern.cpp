//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "ParserPch.h"

namespace UnifiedRegex
{
    RegexPattern::RegexPattern(Js::JavascriptLibrary *const library, Program* program, bool isLiteral)
        : library(library), isLiteral(isLiteral), isShallowClone(false)
    {TRACE_IT(31877);
        rep.unified.program = program;
        rep.unified.matcher = nullptr;
        rep.unified.trigramInfo = nullptr;
    }

    RegexPattern *RegexPattern::New(Js::ScriptContext *scriptContext, Program* program, bool isLiteral)
    {TRACE_IT(31878);
        return
            RecyclerNewFinalized(
                scriptContext->GetRecycler(),
                RegexPattern,
                scriptContext->GetLibrary(),
                program,
                isLiteral);
    }
    void RegexPattern::Finalize(bool isShutdown)
    {TRACE_IT(31879);
        if(isShutdown)
            return;

        const auto scriptContext = GetScriptContext();
        if(!scriptContext)
            return;

#if DBG
        // In JSRT, we might not have a chance to close at finalize time.
        if(!isLiteral && !scriptContext->IsClosed() && !scriptContext->GetThreadContext()->IsJSRT())
        {TRACE_IT(31880);
            const auto source = GetSource();
            RegexPattern *p;
            Assert(
                !GetScriptContext()->GetDynamicRegexMap()->TryGetValue(
                    RegexKey(source.GetBuffer(), source.GetLength(), GetFlags()),
                    &p) || ( source.GetLength() == 0 ) ||
                p != this);
        }
#endif

        if(isShallowClone)
            return;

        rep.unified.program->FreeBody(scriptContext->RegexAllocator());
    }

    void RegexPattern::Dispose(bool isShutdown)
    {TRACE_IT(31881);
    }

    Js::ScriptContext *RegexPattern::GetScriptContext() const
    {TRACE_IT(31882);
        return library->GetScriptContext();
    }

    Js::InternalString RegexPattern::GetSource() const
    {TRACE_IT(31883);
        return Js::InternalString(rep.unified.program->source, rep.unified.program->sourceLen);
    }

    RegexFlags RegexPattern::GetFlags() const
    {TRACE_IT(31884);
        return rep.unified.program->flags;
    }

    int RegexPattern::NumGroups() const
    {TRACE_IT(31885);
        return rep.unified.program->numGroups;
    }

    bool RegexPattern::IsIgnoreCase() const
    {TRACE_IT(31886);
        return (rep.unified.program->flags & IgnoreCaseRegexFlag) != 0;
    }

    bool RegexPattern::IsGlobal() const
    {TRACE_IT(31887);
        return (rep.unified.program->flags & GlobalRegexFlag) != 0;
    }

    bool RegexPattern::IsMultiline() const
    {TRACE_IT(31888);
        return (rep.unified.program->flags & MultilineRegexFlag) != 0;
    }

    bool RegexPattern::IsUnicode() const
    {TRACE_IT(31889);
        return GetScriptContext()->GetConfig()->IsES6UnicodeExtensionsEnabled() && (rep.unified.program->flags & UnicodeRegexFlag) != 0;
    }

    bool RegexPattern::IsSticky() const
    {TRACE_IT(31890);
        return GetScriptContext()->GetConfig()->IsES6RegExStickyEnabled() && (rep.unified.program->flags & StickyRegexFlag) != 0;
    }

    bool RegexPattern::WasLastMatchSuccessful() const
    {TRACE_IT(31891);
        return rep.unified.matcher != 0 && rep.unified.matcher->WasLastMatchSuccessful();
    }

    GroupInfo RegexPattern::GetGroup(int groupId) const
    {TRACE_IT(31892);
        Assert(groupId == 0 || WasLastMatchSuccessful());
        Assert(groupId >= 0 && groupId < NumGroups());
        return rep.unified.matcher->GetGroup(groupId);
    }

    RegexPattern *RegexPattern::CopyToScriptContext(Js::ScriptContext *scriptContext)
    {TRACE_IT(31893);
        // This routine assumes that this instance will outlive the copy, which is the case for copy-on-write,
        // and therefore doesn't copy the immutable parts of the pattern. This should not be confused with a
        // would be CloneToScriptContext which will would clone the immutable parts as well because the lifetime
        // of a clone might be longer than the original.

        RegexPattern *result = UnifiedRegex::RegexPattern::New(scriptContext, rep.unified.program, isLiteral);
        Matcher *matcherClone = rep.unified.matcher ? rep.unified.matcher->CloneToScriptContext(scriptContext, result) : nullptr;
        result->rep.unified.matcher = matcherClone;
        result->isShallowClone = true;
        return result;
    }

#if ENABLE_REGEX_CONFIG_OPTIONS
    void RegexPattern::Print(DebugWriter* w)
    {TRACE_IT(31894);
        w->Print(_u("/"));

        Js::InternalString str = GetSource();
        if (str.GetLength() == 0)
            w->Print(_u("(?:)"));
        else
        {TRACE_IT(31895);
            for (charcount_t i = 0; i < str.GetLength(); ++i)
            {TRACE_IT(31896);
                const char16 c = str.GetBuffer()[i];
                switch(c)
                {
                case _u('/'):
                    w->Print(_u("\\%lc"), c);
                    break;
                case _u('\n'):
                case _u('\r'):
                case _u('\x2028'):
                case _u('\x2029'):
                    w->PrintEscapedChar(c);
                    break;
                case _u('\\'):
                    Assert(i + 1 < str.GetLength()); // cannot end in a '\'
                    w->Print(_u("\\%lc"), str.GetBuffer()[++i]);
                    break;
                default:
                    w->PrintEscapedChar(c);
                    break;
                }
            }
        }
        w->Print(_u("/"));
        if (IsIgnoreCase())
            w->Print(_u("i"));
        if (IsGlobal())
            w->Print(_u("g"));
        if (IsMultiline())
            w->Print(_u("m"));
        if (IsUnicode())
            w->Print(_u("u"));
        if (IsSticky())
            w->Print(_u("y"));
        w->Print(_u(" /* "));
        w->Print(_u(", "));
        w->Print(isLiteral ? _u("literal") : _u("dynamic"));
        w->Print(_u(" */"));
    }
#endif
}
