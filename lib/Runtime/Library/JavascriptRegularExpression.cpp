//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

// Parser Includes
#include "DebugWriter.h"
#include "RegexPattern.h"

namespace Js
{
    JavascriptRegExp::JavascriptRegExp(UnifiedRegex::RegexPattern* pattern, DynamicType* type) :
        DynamicObject(type),
        pattern(pattern),
        splitPattern(nullptr),
        lastIndexVar(nullptr),
        lastIndexOrFlag(0)
    {LOGMEIN("JavascriptRegularExpression.cpp] 18\n");
        Assert(type->GetTypeId() == TypeIds_RegEx);
        Assert(!this->GetType()->AreThisAndPrototypesEnsuredToHaveOnlyWritableDataProperties());

#if ENABLE_REGEX_CONFIG_OPTIONS
        if (REGEX_CONFIG_FLAG(RegexTracing))
        {LOGMEIN("JavascriptRegularExpression.cpp] 24\n");
            UnifiedRegex::DebugWriter* w = type->GetScriptContext()->GetRegexDebugWriter();
            if (pattern == 0)
                w->PrintEOL(_u("// REGEX CREATE"));
            else
            {LOGMEIN("JavascriptRegularExpression.cpp] 29\n");
                w->Print(_u("// REGEX CREATE "));
                pattern->Print(w);
                w->EOL();
            }
        }
#endif
    }

    JavascriptRegExp::JavascriptRegExp(DynamicType * type) :
        DynamicObject(type),
        pattern(nullptr),
        splitPattern(nullptr),
        lastIndexVar(nullptr),
        lastIndexOrFlag(0)
    {LOGMEIN("JavascriptRegularExpression.cpp] 44\n");
        Assert(type->GetTypeId() == TypeIds_RegEx);

#if DBG
        if (REGEX_CONFIG_FLAG(RegexTracing))
        {LOGMEIN("JavascriptRegularExpression.cpp] 49\n");
            UnifiedRegex::DebugWriter* w = type->GetScriptContext()->GetRegexDebugWriter();
            w->PrintEOL(_u("REGEX CREATE"));
        }
#endif
    }

     JavascriptRegExp::JavascriptRegExp(JavascriptRegExp * instance) :
        DynamicObject(instance),
        pattern(instance->GetPattern()),
        splitPattern(instance->GetSplitPattern()),
        lastIndexVar(instance->lastIndexVar),
        lastIndexOrFlag(instance->lastIndexOrFlag)
    {LOGMEIN("JavascriptRegularExpression.cpp] 62\n");
        // For boxing stack instance
        Assert(ThreadContext::IsOnStack(instance));
    }

    bool JavascriptRegExp::Is(Var aValue)
    {LOGMEIN("JavascriptRegularExpression.cpp] 68\n");
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_RegEx;
    }

    // IsRegExp in the spec.
    bool JavascriptRegExp::IsRegExpLike(Var aValue, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptRegularExpression.cpp] 74\n");
        if (scriptContext->GetConfig()->IsES6RegExSymbolsEnabled())
        {LOGMEIN("JavascriptRegularExpression.cpp] 76\n");
            if (!JavascriptOperators::IsObject(aValue))
            {LOGMEIN("JavascriptRegularExpression.cpp] 78\n");
                return false;
            }

            Var symbolMatchProperty = JavascriptOperators::GetProperty(
                RecyclableObject::FromVar(aValue),
                PropertyIds::_symbolMatch,
                scriptContext);
            if (!JavascriptOperators::IsUndefined(symbolMatchProperty))
            {LOGMEIN("JavascriptRegularExpression.cpp] 87\n");
                return JavascriptConversion::ToBool(symbolMatchProperty, scriptContext);
            }
        }

        return JavascriptRegExp::Is(aValue);
    }

    JavascriptRegExp* JavascriptRegExp::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptRegExp'");

        return static_cast<JavascriptRegExp *>(RecyclableObject::FromVar(aValue));
    }

    CharCount JavascriptRegExp::GetLastIndexProperty(RecyclableObject* instance, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptRegularExpression.cpp] 103\n");
        int64 lastIndex = JavascriptConversion::ToLength(
            JavascriptOperators::GetProperty(instance, PropertyIds::lastIndex, scriptContext),
            scriptContext);
        return GetIndexOrMax(lastIndex);
    }

    void JavascriptRegExp::SetLastIndexProperty(Var instance, CharCount lastIndex, ScriptContext* scriptContext)
    {
        SetLastIndexProperty(
            instance,
            JavascriptNumber::ToVar(lastIndex, scriptContext),
            scriptContext);
    }

    void JavascriptRegExp::SetLastIndexProperty(Var instance, Var lastIndex, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptRegularExpression.cpp] 119\n");
        JavascriptOperators::SetProperty(
            instance,
            RecyclableObject::FromVar(instance),
            PropertyIds::lastIndex,
            lastIndex,
            scriptContext,
            static_cast<PropertyOperationFlags>(PropertyOperation_ThrowIfNotExtensible | PropertyOperation_ThrowIfNonWritable));
    }

    bool JavascriptRegExp::GetGlobalProperty(RecyclableObject* instance, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptRegularExpression.cpp] 130\n");
        return JavascriptConversion::ToBool(
            JavascriptOperators::GetProperty(instance, PropertyIds::global, scriptContext),
            scriptContext);
    }

    bool JavascriptRegExp::GetUnicodeProperty(RecyclableObject* instance, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptRegularExpression.cpp] 137\n");
        return JavascriptConversion::ToBool(
            JavascriptOperators::GetProperty(instance, PropertyIds::unicode, scriptContext),
            scriptContext);
    }

    CharCount JavascriptRegExp::AddIndex(CharCount base, CharCount offset)
    {LOGMEIN("JavascriptRegularExpression.cpp] 144\n");
        return (base + offset < base) // Overflow?
            ? MaxCharCount
            : base + offset;
    }

    CharCount JavascriptRegExp::GetIndexOrMax(int64 index)
    {LOGMEIN("JavascriptRegularExpression.cpp] 151\n");
        return (index > SIZE_MAX || IsValidCharCount((size_t) index))
            ? (CharCount) index
            : MaxCharCount;
    }

    InternalString JavascriptRegExp::GetSource() const
    {LOGMEIN("JavascriptRegularExpression.cpp] 158\n");
        return GetPattern()->GetSource();
    }

    UnifiedRegex::RegexFlags JavascriptRegExp::GetFlags() const
    {LOGMEIN("JavascriptRegularExpression.cpp] 163\n");
        return GetPattern()->GetFlags();
    }

    JavascriptRegExp* JavascriptRegExp::GetJavascriptRegExp(Arguments& args, PCWSTR propertyName, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptRegularExpression.cpp] 168\n");
        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptRegularExpression.cpp] 170\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedRegExp, propertyName);
        }

        return ToRegExp(args[0], propertyName, scriptContext);
    }

    JavascriptRegExp* JavascriptRegExp::ToRegExp(Var var, PCWSTR varName, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptRegularExpression.cpp] 178\n");
        if (JavascriptRegExp::Is(var))
        {LOGMEIN("JavascriptRegularExpression.cpp] 180\n");
            return JavascriptRegExp::FromVar(var);
        }

        if (JavascriptOperators::GetTypeId(var) == TypeIds_HostDispatch)
        {LOGMEIN("JavascriptRegularExpression.cpp] 185\n");
            TypeId remoteTypeId;
            RecyclableObject* reclObj = RecyclableObject::FromVar(var);
            reclObj->GetRemoteTypeId(&remoteTypeId);
            if (remoteTypeId == TypeIds_RegEx)
            {LOGMEIN("JavascriptRegularExpression.cpp] 190\n");
                return static_cast<JavascriptRegExp *>(reclObj->GetRemoteObject());
            }
        }

        JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedRegExp, varName);
    }


    RecyclableObject* JavascriptRegExp::GetThisObject(Arguments& args, PCWSTR varName, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptRegularExpression.cpp] 200\n");
        if (args.Info.Count == 0 || !JavascriptOperators::IsObject(args[0]))
        {LOGMEIN("JavascriptRegularExpression.cpp] 202\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedObject, varName);
        }

        return RecyclableObject::FromVar(args[0]);
    }

    JavascriptString* JavascriptRegExp::GetFirstStringArg(Arguments& args, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptRegularExpression.cpp] 210\n");
        if (args.Info.Count == 1)
        {LOGMEIN("JavascriptRegularExpression.cpp] 212\n");
            return scriptContext->GetLibrary()->GetUndefinedDisplayString();
        }
        else if (JavascriptString::Is(args[1]))
        {LOGMEIN("JavascriptRegularExpression.cpp] 216\n");
            return JavascriptString::FromVar(args[1]);
        }
        else
        {
            return JavascriptConversion::ToString(args[1], scriptContext);
        }
    }

    bool JavascriptRegExp::ShouldApplyPrototypeWebWorkaround(Arguments& args, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptRegularExpression.cpp] 226\n");
        return scriptContext->GetConfig()->IsES6PrototypeChain() && \
               args.Info.Count >= 1 && args[0] == scriptContext->GetLibrary()->GetRegExpPrototype();
    }

    Var JavascriptRegExp::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);

        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        // SkipDefaultNewObject function flag should have prevented the default object from
        // being created, except when call true a host dispatch.
        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : function;
        bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && !JavascriptOperators::IsUndefined(newTarget);
        Assert(isCtorSuperCall || !(callInfo.Flags & CallFlags_New) || args[0] == nullptr
            || JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch);

        UnifiedRegex::RegexPattern* pattern = nullptr;
        UnifiedRegex::RegexPattern* splitPattern = nullptr;
        JavascriptRegExp* regex = nullptr;

        if (callInfo.Count < 2)
        {LOGMEIN("JavascriptRegularExpression.cpp] 253\n");
            pattern = scriptContext->GetLibrary()->GetEmptyRegexPattern();
        }
        else if (JavascriptRegExp::IsRegExpLike(args[1], scriptContext))
        {LOGMEIN("JavascriptRegularExpression.cpp] 257\n");
            // JavascriptRegExp::IsRegExpLike() makes sure that args[1] is an Object.
            RecyclableObject* regexLikeObj = RecyclableObject::FromVar(args[1]);

            if (!(callInfo.Flags & CallFlags_New) &&
                (callInfo.Count == 2 || JavascriptOperators::IsUndefinedObject(args[2], scriptContext)) &&
                newTarget == JavascriptOperators::GetProperty(regexLikeObj, PropertyIds::constructor, scriptContext))
            {LOGMEIN("JavascriptRegularExpression.cpp] 264\n");
                // ES5 15.10.3.1 Called as a function: If pattern R is a regexp object and flags is undefined, then return R unchanged.
                // As per ES6 21.2.3.1: We should only return pattern when the this argument is not an uninitialized RegExp object.
                //                      If regex is null, we can be sure the this argument is not initialized.
                return regexLikeObj;
            }

            if (JavascriptRegExp::Is(regexLikeObj))
            {LOGMEIN("JavascriptRegularExpression.cpp] 272\n");
                JavascriptRegExp* source = JavascriptRegExp::FromVar(regexLikeObj);

                if (callInfo.Count > 2)
                {LOGMEIN("JavascriptRegularExpression.cpp] 276\n");
                    // As per ES 2015 21.2.3.1: If 1st argument is RegExp and 2nd argument is flag then return regexp with same pattern as 1st
                    // argument and flags supplied by the 2nd argument.
                    if (!JavascriptOperators::IsUndefinedObject(args[2], scriptContext))
                    {LOGMEIN("JavascriptRegularExpression.cpp] 280\n");
                        InternalString str = source->GetSource();
                        pattern = CreatePattern(JavascriptString::NewCopyBuffer(str.GetBuffer(), str.GetLength(), scriptContext),
                            args[2], scriptContext);

                        // "splitPattern" is a version of "pattern" without the sticky flag. If other flags are the same, we can safely
                        // reuse "splitPattern".
                        UnifiedRegex::RegexFlags currentSplitFlags =
                            static_cast<UnifiedRegex::RegexFlags>(source->GetPattern()->GetFlags() & ~UnifiedRegex::StickyRegexFlag);
                        UnifiedRegex::RegexFlags newSplitFlags =
                            static_cast<UnifiedRegex::RegexFlags>(pattern->GetFlags() & ~UnifiedRegex::StickyRegexFlag);
                        if (newSplitFlags == currentSplitFlags)
                        {LOGMEIN("JavascriptRegularExpression.cpp] 292\n");
                            splitPattern = source->GetSplitPattern();
                        }
                    }
                }
                if (!pattern)
                {LOGMEIN("JavascriptRegularExpression.cpp] 298\n");
                    pattern = source->GetPattern();
                    splitPattern = source->GetSplitPattern();
                }
            }
            else // RegExp-like
            {
                Var source = JavascriptOperators::GetProperty(regexLikeObj, PropertyIds::source, scriptContext);
                Var flags = args.Info.Count < 3 || JavascriptOperators::IsUndefinedObject(args[2])
                    ? JavascriptOperators::GetProperty(regexLikeObj, PropertyIds::flags, scriptContext)
                    : args[2];
                pattern = CreatePattern(source, flags, scriptContext);
            }
        }
        else
        {
            pattern = CreatePattern(args[1], (callInfo.Count > 2) ? args[2] : nullptr, scriptContext);
        }

        if (regex == nullptr)
        {LOGMEIN("JavascriptRegularExpression.cpp] 318\n");
            regex = scriptContext->GetLibrary()->CreateRegExp(nullptr);
        }

        regex->SetPattern(pattern);
        regex->SetSplitPattern(splitPattern);

        return isCtorSuperCall ?
            JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), regex, nullptr, scriptContext) :
            regex;
    }

    UnifiedRegex::RegexPattern* JavascriptRegExp::CreatePattern(Var aValue, Var options, ScriptContext *scriptContext)
    {LOGMEIN("JavascriptRegularExpression.cpp] 331\n");
        JavascriptString * strBody;

        if (JavascriptString::Is(aValue))
        {LOGMEIN("JavascriptRegularExpression.cpp] 335\n");
            strBody = JavascriptString::FromVar(aValue);
        }
        else if (JavascriptOperators::GetTypeId(aValue) == TypeIds_Undefined)
        {LOGMEIN("JavascriptRegularExpression.cpp] 339\n");
            strBody = scriptContext->GetLibrary()->GetEmptyString();
        }
        else
        {
            strBody = JavascriptConversion::ToString(aValue, scriptContext); // must be null terminated!
        }

        int cBody = strBody->GetLength();
        const char16 *szRegex = strBody->GetSz();
        int cOpts = 0;
        const char16 *szOptions = nullptr;

        JavascriptString * strOptions = nullptr;
        if (options != nullptr && !JavascriptOperators::IsUndefinedObject(options, scriptContext))
        {LOGMEIN("JavascriptRegularExpression.cpp] 354\n");
            if (JavascriptString::Is(options))
            {LOGMEIN("JavascriptRegularExpression.cpp] 356\n");
                strOptions = JavascriptString::FromVar(options);
            }
            else
            {
                strOptions = JavascriptConversion::ToString(options, scriptContext);
            }

            szOptions = strOptions->GetSz(); // must be null terminated!
            cOpts = strOptions->GetLength();
        }

        UnifiedRegex::RegexPattern* pattern = RegexHelper::CompileDynamic(scriptContext, szRegex, cBody, szOptions, cOpts, false);

        return pattern;
    }

    JavascriptRegExp* JavascriptRegExp::CreateRegEx(const char16* pSource, CharCount sourceLen, UnifiedRegex::RegexFlags flags, ScriptContext *scriptContext)
    {LOGMEIN("JavascriptRegularExpression.cpp] 374\n");
        UnifiedRegex::RegexPattern* pattern = RegexHelper::CompileDynamic(scriptContext, pSource, sourceLen, flags, false);

        return scriptContext->GetLibrary()->CreateRegExp(pattern);
    }

    JavascriptRegExp* JavascriptRegExp::CreateRegEx(Var aValue, Var options, ScriptContext *scriptContext)
    {LOGMEIN("JavascriptRegularExpression.cpp] 381\n");
        // This is called as helper from OpCode::CoerseRegEx. If aValue is regex pattern /a/, CreatePattern converts
        // it to pattern "/a/" instead of "a". So if we know that aValue is regex, then just return the same object
        if (JavascriptRegExp::Is(aValue))
        {LOGMEIN("JavascriptRegularExpression.cpp] 385\n");
            return JavascriptRegExp::FromVar(aValue);
        }
        else
        {
            return CreateRegExNoCoerce(aValue, options, scriptContext);
        }
    }

    JavascriptRegExp* JavascriptRegExp::CreateRegExNoCoerce(Var aValue, Var options, ScriptContext *scriptContext)
    {LOGMEIN("JavascriptRegularExpression.cpp] 395\n");
        UnifiedRegex::RegexPattern* pattern = CreatePattern(aValue, options, scriptContext);

        return scriptContext->GetLibrary()->CreateRegExp(pattern);
    }

    void JavascriptRegExp::CacheLastIndex()
    {LOGMEIN("JavascriptRegularExpression.cpp] 402\n");
        if (lastIndexVar == nullptr)
            lastIndexOrFlag = 0;
        else
        {LOGMEIN("JavascriptRegularExpression.cpp] 406\n");
            // Does ToInteger(lastIndex) yield an integer in [0, MaxCharCount]?
            double v = JavascriptConversion::ToInteger(lastIndexVar, GetScriptContext());
            if (JavascriptNumber::IsNan(v))
                lastIndexOrFlag = 0;
            else if (JavascriptNumber::IsPosInf(v) ||
                JavascriptNumber::IsNegInf(v) ||
                v < 0.0 ||
                v > (double)MaxCharCount)
                lastIndexOrFlag = InvalidValue;
            else
                lastIndexOrFlag = (CharCount)v;
        }
    }

    JavascriptString *JavascriptRegExp::ToString(bool sourceOnly)
    {LOGMEIN("JavascriptRegularExpression.cpp] 422\n");
        Js::InternalString str = pattern->GetSource();
        CompoundString *const builder = CompoundString::NewWithCharCapacity(str.GetLength() + 5, GetLibrary());

        if (!sourceOnly)
        {LOGMEIN("JavascriptRegularExpression.cpp] 427\n");
            builder->AppendChars(_u('/'));
        }
        if (pattern->IsLiteral())
        {LOGMEIN("JavascriptRegularExpression.cpp] 431\n");
            builder->AppendChars(str.GetBuffer(), str.GetLength());
        }
        else
        {
            // Need to ensure that the resulting static regex is functionally equivalent (as written) to 'this' regex. This
            // involves the following:
            //   - Empty regex should result in /(?:)/ rather than //, which is a comment
            //   - Unescaped '/' needs to be escaped so that it doesn't end the static regex prematurely
            //   - Line terminators need to be escaped since they're not allowed in a static regex
            if (str.GetLength() == 0)
            {LOGMEIN("JavascriptRegularExpression.cpp] 442\n");
                builder->AppendChars(_u("(?:)"));
            }
            else
            {
                bool escape = false;
                for (charcount_t i = 0; i < str.GetLength(); ++i)
                {LOGMEIN("JavascriptRegularExpression.cpp] 449\n");
                    const char16 c = str.GetBuffer()[i];

                    if(!escape)
                    {LOGMEIN("JavascriptRegularExpression.cpp] 453\n");
                        switch(c)
                        {LOGMEIN("JavascriptRegularExpression.cpp] 455\n");
                            case _u('/'):
                            case _u('\n'):
                            case _u('\r'):
                            case _u('\x2028'):
                            case _u('\x2029'):
                                // Unescaped '/' or line terminator needs to be escaped
                                break;

                            case _u('\\'):
                                // Escape sequence; the next character is escaped and shouldn't be escaped further
                                escape = true;
                                Assert(i + 1 < str.GetLength()); // cannot end in a '\'
                                // '\' is appended on the next iteration as 'escape' is true. This handles the case where we
                                // have an escaped line terminator (\<lineTerminator>), where \\n has a different meaning and we
                                // need to use \n instead.
                                continue;

                            default:
                                builder->AppendChars(c);
                                continue;
                        }
                    }
                    else
                    {
                        escape = false;
                    }

                    builder->AppendChars(_u('\\'));
                    switch(c)
                    {LOGMEIN("JavascriptRegularExpression.cpp] 485\n");
                        // Line terminators need to be escaped. \<lineTerminator> is a special case, where \\n doesn't work
                        // since that means a '\' followed by an 'n'. We need to use \n instead.
                        case _u('\n'):
                            builder->AppendChars(_u('n'));
                            break;
                        case _u('\r'):
                            builder->AppendChars(_u('r'));
                            break;
                        case _u('\x2028'):
                            builder->AppendChars(_u("u2028"));
                            break;
                        case _u('\x2029'):
                            builder->AppendChars(_u("u2029"));
                            break;

                        default:
                            builder->AppendChars(c);
                    }
                }
            }
        }

        if (!sourceOnly)
        {LOGMEIN("JavascriptRegularExpression.cpp] 509\n");
            builder->AppendChars(_u('/'));

            // Cross-browser compatibility - flags are listed in alphabetical order in the spec and by other browsers
            // If you change the order of the flags, don't forget to change it in EntryGetterFlags() and GetOptions() too.
            if (pattern->IsGlobal())
            {LOGMEIN("JavascriptRegularExpression.cpp] 515\n");
                builder->AppendChars(_u('g'));
            }
            if (pattern->IsIgnoreCase())
            {LOGMEIN("JavascriptRegularExpression.cpp] 519\n");
                builder->AppendChars(_u('i'));
            }
            if (pattern->IsMultiline())
            {LOGMEIN("JavascriptRegularExpression.cpp] 523\n");
                builder->AppendChars(_u('m'));
            }
            if (pattern->IsUnicode())
            {LOGMEIN("JavascriptRegularExpression.cpp] 527\n");
                builder->AppendChars(_u('u'));
            }
            if (pattern->IsSticky())
            {LOGMEIN("JavascriptRegularExpression.cpp] 531\n");
                builder->AppendChars(_u('y'));
            }
        }

        return builder;
    }

    Var JavascriptRegExp::EntryCompile(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        JavascriptRegExp* thisRegularExpression = GetJavascriptRegExp(args, _u("RegExp.prototype.compile"), scriptContext);

        UnifiedRegex::RegexPattern* pattern;
        UnifiedRegex::RegexPattern* splitPattern = nullptr;

        if (callInfo.Count == 1 )
        {LOGMEIN("JavascriptRegularExpression.cpp] 552\n");
            pattern = scriptContext->GetLibrary()->GetEmptyRegexPattern();
        }
        else if (JavascriptRegExp::Is(args[1]))
        {LOGMEIN("JavascriptRegularExpression.cpp] 556\n");
            JavascriptRegExp* source = JavascriptRegExp::FromVar(args[1]);
            //compile with a regular expression
            pattern = source->GetPattern();
            splitPattern = source->GetSplitPattern();
            // second arg must be undefined if a reg expression is passed
            if(callInfo.Count > 2 &&  JavascriptOperators::GetTypeId(args[2]) != TypeIds_Undefined)
            {LOGMEIN("JavascriptRegularExpression.cpp] 563\n");
                JavascriptError::ThrowSyntaxError(scriptContext, JSERR_RegExpSyntax);
            }
        }
        else
        {
            //compile with a string
            JavascriptString * strBody;
            if (JavascriptString::Is(args[1]))
            {LOGMEIN("JavascriptRegularExpression.cpp] 572\n");
                strBody = JavascriptString::FromVar(args[1]);
            }
            else if(JavascriptOperators::GetTypeId(args[1]) == TypeIds_Undefined)
            {LOGMEIN("JavascriptRegularExpression.cpp] 576\n");
                strBody = scriptContext->GetLibrary()->GetEmptyString();
            }
            else
            {
                strBody = JavascriptConversion::ToString(args[1], scriptContext);
            }

            int cBody = strBody->GetLength();
            const char16 *szRegex = strBody->GetSz(); // must be null terminated!
            int cOpts = 0;
            const char16 *szOptions = nullptr;

            JavascriptString * strOptions = nullptr;
            if (callInfo.Count > 2 && !JavascriptOperators::IsUndefinedObject(args[2], scriptContext))
            {LOGMEIN("JavascriptRegularExpression.cpp] 591\n");
                if (JavascriptString::Is(args[2]))
                {LOGMEIN("JavascriptRegularExpression.cpp] 593\n");
                    strOptions = JavascriptString::FromVar(args[2]);
                }
                else
                {
                    strOptions = JavascriptConversion::ToString(args[2], scriptContext);
                }

                szOptions = strOptions->GetSz(); // must be null terminated!
                cOpts = strOptions->GetLength();
            }
            pattern = RegexHelper::CompileDynamic(scriptContext, szRegex, cBody, szOptions, cOpts, false);
        }

        thisRegularExpression->SetPattern(pattern);
        thisRegularExpression->SetSplitPattern(splitPattern);
        thisRegularExpression->SetLastIndex(0);
        return thisRegularExpression;
    }

    Var JavascriptRegExp::OP_NewRegEx(Var aCompiledRegex, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptRegularExpression.cpp] 614\n");
        JavascriptRegExp * pNewInstance =
            RecyclerNew(scriptContext->GetRecycler(),JavascriptRegExp,((UnifiedRegex::RegexPattern*)aCompiledRegex),
            scriptContext->GetLibrary()->GetRegexType());
        return pNewInstance;
    }

    Var JavascriptRegExp::EntryExec(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        JavascriptRegExp * pRegEx = GetJavascriptRegExp(args, _u("RegExp.prototype.exec"), scriptContext);
        JavascriptString * pStr = GetFirstStringArg(args, scriptContext);
        return RegexHelper::RegexExec(scriptContext, pRegEx, pStr, RegexHelper::IsResultNotUsed(callInfo.Flags));
    }

    Var JavascriptRegExp::EntryTest(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        Assert(!(callInfo.Flags & CallFlags_New));

        RecyclableObject *thisObj = GetThisObject(args, _u("RegExp.prototype.test"), scriptContext);
        JavascriptString* string = GetFirstStringArg(args, scriptContext);
        return RegexHelper::RegexTest(scriptContext, thisObj, string);
    }

    Var JavascriptRegExp::EntryToString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        Assert(args.Info.Count > 0);

        PCWSTR const varName = _u("RegExp.prototype.toString");

        const ScriptConfiguration* scriptConfig = scriptContext->GetConfig();

        if (scriptConfig->IsES6RegExPrototypePropertiesEnabled())
        {LOGMEIN("JavascriptRegularExpression.cpp] 663\n");
            RecyclableObject *thisObj = GetThisObject(args, varName, scriptContext);
            JavascriptString* source = JavascriptConversion::ToString(
                JavascriptOperators::GetProperty(thisObj, PropertyIds::source, scriptContext),
                scriptContext);
            JavascriptString* flags = JavascriptConversion::ToString(
                JavascriptOperators::GetProperty(thisObj, PropertyIds::flags, scriptContext),
                scriptContext);

            CharCount length = source->GetLength() + flags->GetLength() + 2; // 2 for the two '/'s
            CompoundString *const builder =
                CompoundString::NewWithCharCapacity(length, scriptContext->GetLibrary());
            builder->Append(_u('/'));
            builder->Append(source);
            builder->Append(_u('/'));
            builder->Append(flags);
            return builder;
        }
        else
        {
            JavascriptRegExp* obj = GetJavascriptRegExp(args, varName, scriptContext);
            bool sourceOnly = false;
            return obj->ToString(sourceOnly);
        }
    }

    Var JavascriptRegExp::EntrySymbolMatch(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);
        Assert(!(callInfo.Flags & CallFlags_New));

        ScriptContext* scriptContext = function->GetScriptContext();

        CHAKRATEL_LANGSTATS_INC_DATACOUNT(ES6_RegexSymbolMatch);

        PCWSTR const varName = _u("RegExp.prototype[Symbol.match]");

        RecyclableObject *thisObj = GetThisObject(args, varName, scriptContext);
        JavascriptString* string = GetFirstStringArg(args, scriptContext);
        return RegexHelper::RegexMatch(
            scriptContext,
            thisObj,
            string,
            RegexHelper::IsResultNotUsed(callInfo.Flags));
    }

    bool JavascriptRegExp::HasOriginalRegExType(RecyclableObject* instance)
    {LOGMEIN("JavascriptRegularExpression.cpp] 711\n");
        JavascriptLibrary* library = instance->GetLibrary();

        if (instance->GetType() != library->GetRegexType())
        {LOGMEIN("JavascriptRegularExpression.cpp] 715\n");
            return false;
        }

        DynamicObject* regexPrototype = library->GetRegExpPrototype();
        return JavascriptOperators::GetPrototype(instance) == regexPrototype
            && regexPrototype->GetType() == library->GetRegexPrototypeType();
    }

    bool JavascriptRegExp::HasObservableConstructor(DynamicObject* regexPrototype)
    {LOGMEIN("JavascriptRegularExpression.cpp] 725\n");
        JavascriptLibrary* library = regexPrototype->GetLibrary();
        return regexPrototype->GetSlot(library->GetRegexConstructorSlotIndex()) != library->GetRegExpConstructor();
    }

    bool JavascriptRegExp::HasObservableExec(DynamicObject* regexPrototype)
    {LOGMEIN("JavascriptRegularExpression.cpp] 731\n");
        JavascriptLibrary* library = regexPrototype->GetLibrary();
        return regexPrototype->GetSlot(library->GetRegexExecSlotIndex()) != library->GetRegexExecFunction();
    }

    bool JavascriptRegExp::HasObservableFlags(DynamicObject* regexPrototype)
    {LOGMEIN("JavascriptRegularExpression.cpp] 737\n");
        JavascriptLibrary* library = regexPrototype->GetLibrary();
        return regexPrototype->GetScriptContext()->GetConfig()->IsES6RegExPrototypePropertiesEnabled()
            && regexPrototype->GetSlot(library->GetRegexFlagsGetterSlotIndex()) != library->GetRegexFlagsGetterFunction();
    }

    bool JavascriptRegExp::HasObservableGlobalFlag(DynamicObject* regexPrototype)
    {LOGMEIN("JavascriptRegularExpression.cpp] 744\n");
        JavascriptLibrary* library = regexPrototype->GetLibrary();
        return regexPrototype->GetScriptContext()->GetConfig()->IsES6RegExPrototypePropertiesEnabled()
            && regexPrototype->GetSlot(library->GetRegexGlobalGetterSlotIndex()) != library->GetRegexGlobalGetterFunction();
    }

    bool JavascriptRegExp::HasObservableUnicodeFlag(DynamicObject* regexPrototype)
    {LOGMEIN("JavascriptRegularExpression.cpp] 751\n");
        const ScriptConfiguration* scriptConfig = regexPrototype->GetScriptContext()->GetConfig();
        JavascriptLibrary* library = regexPrototype->GetLibrary();
        return scriptConfig->IsES6UnicodeExtensionsEnabled()
            && scriptConfig->IsES6RegExPrototypePropertiesEnabled()
            && regexPrototype->GetSlot(library->GetRegexUnicodeGetterSlotIndex()) != library->GetRegexUnicodeGetterFunction();
    }

    Var JavascriptRegExp::EntrySymbolReplace(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);
        Assert(!(callInfo.Flags & CallFlags_New));

        ScriptContext* scriptContext = function->GetScriptContext();

        CHAKRATEL_LANGSTATS_INC_DATACOUNT(ES6_RegexSymbolReplace);

        PCWSTR varName = _u("RegExp.prototype[Symbol.replace]");

        RecyclableObject* thisObj = GetThisObject(args, varName, scriptContext);
        JavascriptString* string = GetFirstStringArg(args, scriptContext);

        Var replaceValue = (args.Info.Count > 2) ? args[2] : scriptContext->GetLibrary()->GetUndefined();

        if (JavascriptFunction::Is(replaceValue))
        {LOGMEIN("JavascriptRegularExpression.cpp] 777\n");
            JavascriptFunction* replaceFunction = JavascriptFunction::FromVar(replaceValue);
            return RegexHelper::RegexReplaceFunction(scriptContext, thisObj, string, replaceFunction);
        }
        else
        {
            JavascriptString* replaceString = JavascriptConversion::ToString(replaceValue, scriptContext);
            return RegexHelper::RegexReplace(
                scriptContext,
                thisObj,
                string,
                replaceString,
                RegexHelper::IsResultNotUsed(callInfo.Flags));
        }
    }

    Var JavascriptRegExp::EntrySymbolSearch(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);
        Assert(!(callInfo.Flags & CallFlags_New));

        ScriptContext* scriptContext = function->GetScriptContext();

        CHAKRATEL_LANGSTATS_INC_DATACOUNT(ES6_RegexSymbolSearch);

        PCWSTR const varName = _u("RegExp.prototype[Symbol.search]");

        RecyclableObject *thisObj = GetThisObject(args, varName, scriptContext);
        Var regEx = args[0];

        JavascriptString* string = GetFirstStringArg(args, scriptContext);

        Var previousLastIndex = JavascriptOperators::GetProperty(thisObj, PropertyIds::lastIndex, scriptContext);
        SetLastIndexProperty(regEx, TaggedInt::ToVarUnchecked(0), scriptContext);

        Var result = CallExec(thisObj, string, varName, scriptContext);

        SetLastIndexProperty(regEx, previousLastIndex, scriptContext);

        return JavascriptOperators::IsNull(result)
            ? TaggedInt::ToVarUnchecked(-1)
            : JavascriptOperators::GetProperty(RecyclableObject::FromVar(result), PropertyIds::index, scriptContext);
    }

    Var JavascriptRegExp::EntrySymbolSplit(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);
        Assert(!(callInfo.Flags & CallFlags_New));

        ScriptContext* scriptContext = function->GetScriptContext();

        CHAKRATEL_LANGSTATS_INC_DATACOUNT(ES6_RegexSymbolSplit);

        RecyclableObject *thisObj = GetThisObject(args, _u("RegExp.prototype[Symbol.match]"), scriptContext);
        JavascriptString* string = GetFirstStringArg(args, scriptContext);

        // TODO: SPEC DEVIATION
        //
        // In RegexHelper::RegexSplit, we check if RegExp properties are overridden in order to determine
        // if the algorithm is observable. If it is, we go through the new ES6 algorithm, but otherwise, we
        // run the faster ES5 version.
        //
        // According to the spec, we're supposed to process "limit" after we use some of the RegExp properties.
        // However, there doesn't seem to be any reason why "limit" processing can't be pulled above the rest
        // in the spec. Therefore, we should see if such a spec update is OK. If not, this would have to be
        // moved to its correct place in the code.
        uint32 limit = (args.Info.Count < 3 || JavascriptOperators::IsUndefinedObject(args[2], scriptContext))
            ? UINT_MAX
            : JavascriptConversion::ToUInt32(args[2], scriptContext);

        return RegexHelper::RegexSplit(
            scriptContext,
            thisObj,
            string,
            limit,
            RegexHelper::IsResultNotUsed(callInfo.Flags));
    }

    Var JavascriptRegExp::CallExec(RecyclableObject* thisObj, JavascriptString* string, PCWSTR varName, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptRegularExpression.cpp] 858\n");
        Var exec = JavascriptOperators::GetProperty(thisObj, PropertyIds::exec, scriptContext);
        if (JavascriptConversion::IsCallable(exec))
        {LOGMEIN("JavascriptRegularExpression.cpp] 861\n");
            RecyclableObject* execFn = RecyclableObject::FromVar(exec);
            Var result = CALL_FUNCTION(execFn, CallInfo(CallFlags_Value, 2), thisObj, string);

            if (!JavascriptOperators::IsObjectOrNull(result))
            {LOGMEIN("JavascriptRegularExpression.cpp] 866\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_RegExpExecInvalidReturnType, varName);
            }

            return result;
        }

        JavascriptRegExp* regExObj = ToRegExp(thisObj, varName, scriptContext);
        return RegexHelper::RegexExec(scriptContext, regExObj, string, false);
    }

    UnifiedRegex::RegexFlags JavascriptRegExp::SetRegexFlag(
        PropertyId propertyId,
        UnifiedRegex::RegexFlags flags,
        UnifiedRegex::RegexFlags flag,
        ScriptContext* scriptContext)
    {LOGMEIN("JavascriptRegularExpression.cpp] 882\n");
        bool isEnabled = JavascriptConversion::ToBool(
            JavascriptOperators::GetProperty(this, propertyId, scriptContext),
            scriptContext);
        return isEnabled
            ? static_cast<UnifiedRegex::RegexFlags>(flags | flag)
            : static_cast<UnifiedRegex::RegexFlags>(flags & ~flag);
    }

    Var JavascriptRegExp::EntryGetterSymbolSpecies(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ARGUMENTS(args, callInfo);

        Assert(args.Info.Count > 0);

        return args[0];
    }

    Var JavascriptRegExp::EntryGetterFlags(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);
        Assert(!(callInfo.Flags & CallFlags_New));

        ScriptContext* scriptContext = function->GetScriptContext();

        RecyclableObject *thisObj = GetThisObject(args, _u("RegExp.prototype.flags"), scriptContext);
        Var flags;

        BEGIN_TEMP_ALLOCATOR(tempAlloc, scriptContext, _u("JavascriptRegExp"))
        {LOGMEIN("JavascriptRegularExpression.cpp] 912\n");
            StringBuilder<ArenaAllocator> bs(tempAlloc, 5);

#define APPEND_FLAG(propertyId, flag) AppendFlagForFlagsProperty(&bs, thisObj, propertyId, flag, scriptContext)
            // If you change the order of the flags, don't forget to change it in GetOptions() and ToString() too.
            APPEND_FLAG(PropertyIds::global, _u('g'));
            APPEND_FLAG(PropertyIds::ignoreCase, _u('i'));
            APPEND_FLAG(PropertyIds::multiline, _u('m'));

            ScriptConfiguration const * scriptConfig = scriptContext->GetConfig();

            if (scriptConfig->IsES6UnicodeExtensionsEnabled())
            {LOGMEIN("JavascriptRegularExpression.cpp] 924\n");
                APPEND_FLAG(PropertyIds::unicode, _u('u'));
            }

            if (scriptConfig->IsES6RegExStickyEnabled())
            {LOGMEIN("JavascriptRegularExpression.cpp] 929\n");
                APPEND_FLAG(PropertyIds::sticky, _u('y'));
            }
#undef APPEND_FLAG

            flags = Js::JavascriptString::NewCopyBuffer(bs.Detach(), bs.Count(), scriptContext);
        }
        END_TEMP_ALLOCATOR(tempAlloc, scriptContext);

        return flags;
    }

    void JavascriptRegExp::AppendFlagForFlagsProperty(
        StringBuilder<ArenaAllocator>* builder,
        RecyclableObject* thisObj,
        PropertyId propertyId,
        char16 flag,
        ScriptContext* scriptContext)
    {LOGMEIN("JavascriptRegularExpression.cpp] 947\n");
        Var propertyValue = JavascriptOperators::GetProperty(thisObj, propertyId, scriptContext);
        if (JavascriptConversion::ToBoolean(propertyValue, scriptContext))
        {LOGMEIN("JavascriptRegularExpression.cpp] 950\n");
            builder->Append(flag);
        }
    }

    Var JavascriptRegExp::EntryGetterOptions(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);
        Assert(!(callInfo.Flags & CallFlags_New));

        ScriptContext* scriptContext = function->GetScriptContext();
        if (ShouldApplyPrototypeWebWorkaround(args, scriptContext))
        {LOGMEIN("JavascriptRegularExpression.cpp] 963\n");
            return scriptContext->GetLibrary()->GetUndefined();
        }

        return GetJavascriptRegExp(args, _u("RegExp.prototype.options"), scriptContext)->GetOptions();
    }

    Var JavascriptRegExp::GetOptions()
    {LOGMEIN("JavascriptRegularExpression.cpp] 971\n");
        Var options;

        ScriptContext* scriptContext = this->GetLibrary()->GetScriptContext();
        BEGIN_TEMP_ALLOCATOR(tempAlloc, scriptContext, _u("JavascriptRegExp"))
        {LOGMEIN("JavascriptRegularExpression.cpp] 976\n");
            StringBuilder<ArenaAllocator> bs(tempAlloc, 4);

            // If you change the order of the flags, don't forget to change it in EntryGetterFlags() and ToString() too.
            if(GetPattern()->IsGlobal())
            {LOGMEIN("JavascriptRegularExpression.cpp] 981\n");
                bs.Append(_u('g'));
            }
            if(GetPattern()->IsIgnoreCase())
            {LOGMEIN("JavascriptRegularExpression.cpp] 985\n");
                bs.Append(_u('i'));
            }
            if(GetPattern()->IsMultiline())
            {LOGMEIN("JavascriptRegularExpression.cpp] 989\n");
                bs.Append(_u('m'));
            }
            if (GetPattern()->IsUnicode())
            {LOGMEIN("JavascriptRegularExpression.cpp] 993\n");
                bs.Append(_u('u'));
            }
            if (GetPattern()->IsSticky())
            {LOGMEIN("JavascriptRegularExpression.cpp] 997\n");
                bs.Append(_u('y'));
            }
            options = Js::JavascriptString::NewCopyBuffer(bs.Detach(), bs.Count(), scriptContext);
        }
        END_TEMP_ALLOCATOR(tempAlloc, scriptContext);

        return options;
    }

    Var JavascriptRegExp::EntryGetterSource(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);
        Assert(!(callInfo.Flags & CallFlags_New));

        ScriptContext* scriptContext = function->GetScriptContext();
        if (ShouldApplyPrototypeWebWorkaround(args, scriptContext))
        {LOGMEIN("JavascriptRegularExpression.cpp] 1015\n");
            return JavascriptString::NewCopyBuffer(_u("(?:)"), 4, scriptContext);
        }

        return GetJavascriptRegExp(args, _u("RegExp.prototype.source"), scriptContext)->ToString(true);
    }

#define DEFINE_FLAG_GETTER(methodName, propertyName, patternMethodName) \
    Var JavascriptRegExp::##methodName##(RecyclableObject* function, CallInfo callInfo, ...) \
    { \
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault); \
        ARGUMENTS(args, callInfo); \
        Assert(!(callInfo.Flags & CallFlags_New)); \
        \
        ScriptContext* scriptContext = function->GetScriptContext(); \
        if (ShouldApplyPrototypeWebWorkaround(args, scriptContext)) \
        {LOGMEIN("JavascriptRegularExpression.cpp] 1031\n");\
            return scriptContext->GetLibrary()->GetUndefined(); \
        }\
        \
        JavascriptRegExp* pRegEx = GetJavascriptRegExp(args, _u("RegExp.prototype.") _u(#propertyName), scriptContext); \
        return pRegEx->GetLibrary()->CreateBoolean(pRegEx->GetPattern()->##patternMethodName##()); \
    }

    DEFINE_FLAG_GETTER(EntryGetterGlobal, global, IsGlobal)
    DEFINE_FLAG_GETTER(EntryGetterIgnoreCase, ignoreCase, IsIgnoreCase)
    DEFINE_FLAG_GETTER(EntryGetterMultiline, multiline, IsMultiline)
    DEFINE_FLAG_GETTER(EntryGetterSticky, sticky, IsSticky)
    DEFINE_FLAG_GETTER(EntryGetterUnicode, unicode, IsUnicode)

    JavascriptRegExp * JavascriptRegExp::BoxStackInstance(JavascriptRegExp * instance)
    {LOGMEIN("JavascriptRegularExpression.cpp] 1046\n");
        Assert(ThreadContext::IsOnStack(instance));
        // On the stack, the we reserved a pointer before the object as to store the boxed value
        JavascriptRegExp ** boxedInstanceRef = ((JavascriptRegExp **)instance) - 1;
        JavascriptRegExp * boxedInstance = *boxedInstanceRef;
        if (boxedInstance)
        {LOGMEIN("JavascriptRegularExpression.cpp] 1052\n");
            return boxedInstance;
        }
        Assert(instance->GetTypeHandler()->GetInlineSlotsSize() == 0);
        boxedInstance = RecyclerNew(instance->GetRecycler(), JavascriptRegExp, instance);
        *boxedInstanceRef = boxedInstance;
        return boxedInstance;
    }

    // Both 'unicode' and 'sticky' special properties could be controlled via config
    // flags. Below, 'sticky' is listed after 'unicode' (for no special reason).
    // When the 'unicode' config flag is disabled, we want 'unicode' to be excluded
    // from the list, but we still want 'sticky' to be included depending on its
    // config flag. That's the reason why we have two lists for the special property
    // IDs.
    #define DEFAULT_SPECIAL_PROPERTY_IDS \
        PropertyIds::lastIndex, \
        PropertyIds::global, \
        PropertyIds::multiline, \
        PropertyIds::ignoreCase, \
        PropertyIds::source, \
        PropertyIds::options
    PropertyId const JavascriptRegExp::specialPropertyIdsAll[] =
    {
        DEFAULT_SPECIAL_PROPERTY_IDS,
        PropertyIds::unicode,
        PropertyIds::sticky
    };
    PropertyId const JavascriptRegExp::specialPropertyIdsWithoutUnicode[] =
    {
        DEFAULT_SPECIAL_PROPERTY_IDS,
        PropertyIds::sticky
    };

    BOOL JavascriptRegExp::HasProperty(PropertyId propertyId)
    {LOGMEIN("JavascriptRegularExpression.cpp] 1087\n");
        const ScriptConfiguration* scriptConfig = this->GetScriptContext()->GetConfig();

#define HAS_PROPERTY(ownProperty) \
        return (ownProperty ? true : DynamicObject::HasProperty(propertyId));

        switch (propertyId)
        {LOGMEIN("JavascriptRegularExpression.cpp] 1094\n");
        case PropertyIds::lastIndex:
            return true;
        case PropertyIds::global:
        case PropertyIds::multiline:
        case PropertyIds::ignoreCase:
        case PropertyIds::source:
        case PropertyIds::options:
            HAS_PROPERTY(!scriptConfig->IsES6RegExPrototypePropertiesEnabled());
        case PropertyIds::unicode:
            HAS_PROPERTY(scriptConfig->IsES6UnicodeExtensionsEnabled() && !scriptConfig->IsES6RegExPrototypePropertiesEnabled())
        case PropertyIds::sticky:
            HAS_PROPERTY(scriptConfig->IsES6RegExStickyEnabled() && !scriptConfig->IsES6RegExPrototypePropertiesEnabled())
        default:
            return DynamicObject::HasProperty(propertyId);
        }

#undef HAS_PROPERTY
    }

    BOOL JavascriptRegExp::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("JavascriptRegularExpression.cpp] 1115\n");
        return JavascriptRegExp::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    BOOL JavascriptRegExp::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("JavascriptRegularExpression.cpp] 1120\n");
        BOOL result;
        if (GetPropertyBuiltIns(propertyId, value, &result))
        {LOGMEIN("JavascriptRegularExpression.cpp] 1123\n");
            return result;
        }

        return DynamicObject::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    BOOL JavascriptRegExp::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("JavascriptRegularExpression.cpp] 1131\n");
        BOOL result;
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && GetPropertyBuiltIns(propertyRecord->GetPropertyId(), value, &result))
        {LOGMEIN("JavascriptRegularExpression.cpp] 1137\n");
            return result;
        }

        return DynamicObject::GetProperty(originalInstance, propertyNameString, value, info, requestContext);
    }

    bool JavascriptRegExp::GetPropertyBuiltIns(PropertyId propertyId, Var* value, BOOL* result)
    {LOGMEIN("JavascriptRegularExpression.cpp] 1145\n");
        const ScriptConfiguration* scriptConfig = this->GetScriptContext()->GetConfig();

#define GET_FLAG(patternMethod) \
        if (!scriptConfig->IsES6RegExPrototypePropertiesEnabled()) \
        {LOGMEIN("JavascriptRegularExpression.cpp] 1150\n"); \
            *value = this->GetLibrary()->CreateBoolean(this->GetPattern()->##patternMethod##()); \
            *result = true; \
            return true; \
        } \
        else \
        { \
            return false; \
        }

        switch (propertyId)
        {LOGMEIN("JavascriptRegularExpression.cpp] 1161\n");
        case PropertyIds::lastIndex:
            if (this->lastIndexVar == nullptr)
            {LOGMEIN("JavascriptRegularExpression.cpp] 1164\n");
                Assert(lastIndexOrFlag <= MaxCharCount);
                this->lastIndexVar = JavascriptNumber::ToVar(lastIndexOrFlag, GetScriptContext());
            }
            *value = this->lastIndexVar;
            *result = true;
            return true;
        case PropertyIds::global:
            GET_FLAG(IsGlobal)
        case PropertyIds::multiline:
            GET_FLAG(IsMultiline)
        case PropertyIds::ignoreCase:
            GET_FLAG(IsIgnoreCase)
        case PropertyIds::unicode:
            GET_FLAG(IsUnicode)
        case PropertyIds::sticky:
            GET_FLAG(IsSticky)
        case PropertyIds::source:
            if (!scriptConfig->IsES6RegExPrototypePropertiesEnabled())
            {LOGMEIN("JavascriptRegularExpression.cpp] 1183\n");
                *value = this->ToString(true);
                *result = true;
                return true;
            }
            else
            {
                return false;
            }
        case PropertyIds::options:
            if (!scriptConfig->IsES6RegExPrototypePropertiesEnabled())
            {LOGMEIN("JavascriptRegularExpression.cpp] 1194\n");
                *value = GetOptions();
                *result = true;
                return true;
            }
            else
            {
                return false;
            }
        default:
            return false;
        }

#undef GET_FLAG
    }

    BOOL JavascriptRegExp::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("JavascriptRegularExpression.cpp] 1211\n");
        BOOL result;
        if (SetPropertyBuiltIns(propertyId, value, flags, &result))
        {LOGMEIN("JavascriptRegularExpression.cpp] 1214\n");
            return result;
        }

        return DynamicObject::SetProperty(propertyId, value, flags, info);
    }

    BOOL JavascriptRegExp::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("JavascriptRegularExpression.cpp] 1222\n");
        BOOL result;
        PropertyRecord const * propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && SetPropertyBuiltIns(propertyRecord->GetPropertyId(), value, flags, &result))
        {LOGMEIN("JavascriptRegularExpression.cpp] 1228\n");
            return result;
        }

        return DynamicObject::SetProperty(propertyNameString, value, flags, info);
    }

    bool JavascriptRegExp::SetPropertyBuiltIns(PropertyId propertyId, Var value, PropertyOperationFlags flags, BOOL* result)
    {LOGMEIN("JavascriptRegularExpression.cpp] 1236\n");
        const ScriptConfiguration* scriptConfig = this->GetScriptContext()->GetConfig();

#define SET_PROPERTY(ownProperty) \
        if (ownProperty) \
        {LOGMEIN("JavascriptRegularExpression.cpp] 1241\n"); \
            JavascriptError::ThrowCantAssignIfStrictMode(flags, this->GetScriptContext()); \
            *result = false; \
            return true; \
        } \
        return false;

        switch (propertyId)
        {LOGMEIN("JavascriptRegularExpression.cpp] 1249\n");
        case PropertyIds::lastIndex:
            this->lastIndexVar = value;
            lastIndexOrFlag = NotCachedValue;
            *result = true;
            return true;
        case PropertyIds::global:
        case PropertyIds::multiline:
        case PropertyIds::ignoreCase:
        case PropertyIds::source:
        case PropertyIds::options:
            SET_PROPERTY(!scriptConfig->IsES6RegExPrototypePropertiesEnabled());
        case PropertyIds::unicode:
            SET_PROPERTY(scriptConfig->IsES6UnicodeExtensionsEnabled() && !scriptConfig->IsES6RegExPrototypePropertiesEnabled());
        case PropertyIds::sticky:
            SET_PROPERTY(scriptConfig->IsES6RegExStickyEnabled() && !scriptConfig->IsES6RegExPrototypePropertiesEnabled());
        default:
            return false;
        }

#undef SET_PROPERTY
    }

    BOOL JavascriptRegExp::InitProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("JavascriptRegularExpression.cpp] 1273\n");
        return SetProperty(propertyId, value, flags, info);
    }

    BOOL JavascriptRegExp::DeleteProperty(PropertyId propertyId, PropertyOperationFlags propertyOperationFlags)
    {LOGMEIN("JavascriptRegularExpression.cpp] 1278\n");
        const ScriptConfiguration* scriptConfig = this->GetScriptContext()->GetConfig();

#define DELETE_PROPERTY(ownProperty) \
        if (ownProperty) \
        {LOGMEIN("JavascriptRegularExpression.cpp] 1283\n"); \
            JavascriptError::ThrowCantDeleteIfStrictMode(propertyOperationFlags, this->GetScriptContext(), this->GetScriptContext()->GetPropertyName(propertyId)->GetBuffer()); \
            return false; \
        } \
        return DynamicObject::DeleteProperty(propertyId, propertyOperationFlags);

        switch (propertyId)
        {LOGMEIN("JavascriptRegularExpression.cpp] 1290\n");
        case PropertyIds::lastIndex:
            DELETE_PROPERTY(true);
        case PropertyIds::global:
        case PropertyIds::multiline:
        case PropertyIds::ignoreCase:
        case PropertyIds::source:
        case PropertyIds::options:
            DELETE_PROPERTY(!scriptConfig->IsES6RegExPrototypePropertiesEnabled());
        case PropertyIds::unicode:
            DELETE_PROPERTY(scriptConfig->IsES6UnicodeExtensionsEnabled() && !scriptConfig->IsES6RegExPrototypePropertiesEnabled());
        case PropertyIds::sticky:
            DELETE_PROPERTY(scriptConfig->IsES6RegExStickyEnabled() && !scriptConfig->IsES6RegExPrototypePropertiesEnabled());
        default:
            return DynamicObject::DeleteProperty(propertyId, propertyOperationFlags);
        }

#undef DELETE_PROPERTY
    }

    BOOL JavascriptRegExp::DeleteProperty(JavascriptString *propertyNameString, PropertyOperationFlags flags)
    {LOGMEIN("JavascriptRegularExpression.cpp] 1311\n");
        const ScriptConfiguration* scriptConfig = this->GetScriptContext()->GetConfig();
        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());

#define DELETE_PROPERTY(ownProperty) \
        if (ownProperty) \
        {LOGMEIN("JavascriptRegularExpression.cpp] 1317\n"); \
            JavascriptError::ThrowCantDeleteIfStrictMode(flags, this->GetScriptContext(), propertyNameString->GetString()); \
            return false; \
        } \
        return DynamicObject::DeleteProperty(propertyNameString, flags);

        if (BuiltInPropertyRecords::lastIndex.Equals(propertyName))
        {LOGMEIN("JavascriptRegularExpression.cpp] 1324\n");
            DELETE_PROPERTY(true);
        }
        else if (BuiltInPropertyRecords::global.Equals(propertyName)
            || BuiltInPropertyRecords::multiline.Equals(propertyName)
            || BuiltInPropertyRecords::ignoreCase.Equals(propertyName)
            || BuiltInPropertyRecords::source.Equals(propertyName)
            || BuiltInPropertyRecords::options.Equals(propertyName))
        {LOGMEIN("JavascriptRegularExpression.cpp] 1332\n");
            DELETE_PROPERTY(!scriptConfig->IsES6RegExPrototypePropertiesEnabled());
        }
        else if (BuiltInPropertyRecords::unicode.Equals(propertyName))
        {LOGMEIN("JavascriptRegularExpression.cpp] 1336\n");
            DELETE_PROPERTY(scriptConfig->IsES6UnicodeExtensionsEnabled() && !scriptConfig->IsES6RegExPrototypePropertiesEnabled());
        }
        else if (BuiltInPropertyRecords::sticky.Equals(propertyName))
        {LOGMEIN("JavascriptRegularExpression.cpp] 1340\n");
            DELETE_PROPERTY(scriptConfig->IsES6RegExStickyEnabled() && !scriptConfig->IsES6RegExPrototypePropertiesEnabled());
        }
        else
        {
            return DynamicObject::DeleteProperty(propertyNameString, flags);
        }

#undef DELETE_PROPERTY

    }

    DescriptorFlags JavascriptRegExp::GetSetter(PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("JavascriptRegularExpression.cpp] 1353\n");
        DescriptorFlags result;
        if (GetSetterBuiltIns(propertyId, info, &result))
        {LOGMEIN("JavascriptRegularExpression.cpp] 1356\n");
            return result;
        }

        return DynamicObject::GetSetter(propertyId, setterValue, info, requestContext);
    }

    DescriptorFlags JavascriptRegExp::GetSetter(JavascriptString* propertyNameString, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("JavascriptRegularExpression.cpp] 1364\n");
        DescriptorFlags result;
        PropertyRecord const * propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && GetSetterBuiltIns(propertyRecord->GetPropertyId(), info, &result))
        {LOGMEIN("JavascriptRegularExpression.cpp] 1370\n");
            return result;
        }

        return DynamicObject::GetSetter(propertyNameString, setterValue, info, requestContext);
    }

    bool JavascriptRegExp::GetSetterBuiltIns(PropertyId propertyId, PropertyValueInfo* info, DescriptorFlags* result)
    {LOGMEIN("JavascriptRegularExpression.cpp] 1378\n");
        const ScriptConfiguration* scriptConfig = this->GetScriptContext()->GetConfig();

#define GET_SETTER(ownProperty) \
        if (ownProperty) \
        {LOGMEIN("JavascriptRegularExpression.cpp] 1383\n"); \
            PropertyValueInfo::SetNoCache(info, this); \
            *result = JavascriptRegExp::IsWritable(propertyId) ? WritableData : Data; \
            return true; \
        } \
        return false;

        switch (propertyId)
        {LOGMEIN("JavascriptRegularExpression.cpp] 1391\n");
        case PropertyIds::lastIndex:
            GET_SETTER(true);
        case PropertyIds::global:
        case PropertyIds::multiline:
        case PropertyIds::ignoreCase:
        case PropertyIds::source:
        case PropertyIds::options:
            GET_SETTER(!scriptConfig->IsES6RegExPrototypePropertiesEnabled());
        case PropertyIds::unicode:
            GET_SETTER(scriptConfig->IsES6UnicodeExtensionsEnabled() && !scriptConfig->IsES6RegExPrototypePropertiesEnabled());
        case PropertyIds::sticky:
            GET_SETTER(scriptConfig->IsES6RegExStickyEnabled() && !scriptConfig->IsES6RegExPrototypePropertiesEnabled());
        default:
            return false;
        }

#undef GET_SETTER
    }

    BOOL JavascriptRegExp::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("JavascriptRegularExpression.cpp] 1412\n");
        Js::InternalString str = pattern->GetSource();
        stringBuilder->Append(str.GetBuffer(), str.GetLength());
        return TRUE;
    }

    BOOL JavascriptRegExp::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("JavascriptRegularExpression.cpp] 1419\n");
        stringBuilder->AppendCppLiteral(JS_DIAG_TYPE_JavascriptRegExp);
        return TRUE;
    }

    BOOL JavascriptRegExp::IsEnumerable(PropertyId propertyId)
    {LOGMEIN("JavascriptRegularExpression.cpp] 1425\n");
        const ScriptConfiguration* scriptConfig = this->GetScriptContext()->GetConfig();

#define IS_ENUMERABLE(ownProperty) \
        return (ownProperty ? false : DynamicObject::IsEnumerable(propertyId));

        switch (propertyId)
        {LOGMEIN("JavascriptRegularExpression.cpp] 1432\n");
        case PropertyIds::lastIndex:
            return false;
        case PropertyIds::global:
        case PropertyIds::multiline:
        case PropertyIds::ignoreCase:
        case PropertyIds::source:
        case PropertyIds::options:
            IS_ENUMERABLE(!scriptConfig->IsES6RegExPrototypePropertiesEnabled());
        case PropertyIds::unicode:
            IS_ENUMERABLE(scriptConfig->IsES6UnicodeExtensionsEnabled() && !scriptConfig->IsES6RegExPrototypePropertiesEnabled());
        case PropertyIds::sticky:
            IS_ENUMERABLE(scriptConfig->IsES6RegExStickyEnabled() && !scriptConfig->IsES6RegExPrototypePropertiesEnabled());
        default:
            return DynamicObject::IsEnumerable(propertyId);
        }

#undef IS_ENUMERABLE
    }

    BOOL JavascriptRegExp::IsConfigurable(PropertyId propertyId)
    {LOGMEIN("JavascriptRegularExpression.cpp] 1453\n");
        const ScriptConfiguration* scriptConfig = this->GetScriptContext()->GetConfig();

#define IS_CONFIGURABLE(ownProperty) \
        return (ownProperty ? false : DynamicObject::IsConfigurable(propertyId));

        switch (propertyId)
        {LOGMEIN("JavascriptRegularExpression.cpp] 1460\n");
        case PropertyIds::lastIndex:
            return false;
        case PropertyIds::global:
        case PropertyIds::multiline:
        case PropertyIds::ignoreCase:
        case PropertyIds::source:
        case PropertyIds::options:
            IS_CONFIGURABLE(!scriptConfig->IsES6RegExPrototypePropertiesEnabled());
        case PropertyIds::unicode:
            IS_CONFIGURABLE(scriptConfig->IsES6UnicodeExtensionsEnabled() && !scriptConfig->IsES6RegExPrototypePropertiesEnabled());
        case PropertyIds::sticky:
            IS_CONFIGURABLE(scriptConfig->IsES6RegExStickyEnabled() && !scriptConfig->IsES6RegExPrototypePropertiesEnabled());
        default:
            return DynamicObject::IsConfigurable(propertyId);
        }

#undef IS_CONFIGURABLE
    }

    BOOL JavascriptRegExp::IsWritable(PropertyId propertyId)
    {LOGMEIN("JavascriptRegularExpression.cpp] 1481\n");
        const ScriptConfiguration* scriptConfig = this->GetScriptContext()->GetConfig();

#define IS_WRITABLE(ownProperty) \
        return (ownProperty ? false : DynamicObject::IsWritable(propertyId));

        switch (propertyId)
        {LOGMEIN("JavascriptRegularExpression.cpp] 1488\n");
        case PropertyIds::lastIndex:
            return true;
        case PropertyIds::global:
        case PropertyIds::multiline:
        case PropertyIds::ignoreCase:
        case PropertyIds::source:
        case PropertyIds::options:
            IS_WRITABLE(!scriptConfig->IsES6RegExPrototypePropertiesEnabled())
        case PropertyIds::unicode:
            IS_WRITABLE(scriptConfig->IsES6UnicodeExtensionsEnabled() && !scriptConfig->IsES6RegExPrototypePropertiesEnabled());
        case PropertyIds::sticky:
            IS_WRITABLE(scriptConfig->IsES6RegExStickyEnabled() && !scriptConfig->IsES6RegExPrototypePropertiesEnabled());
        default:
            return DynamicObject::IsWritable(propertyId);
        }

#undef IS_WRITABLE
    }
    BOOL JavascriptRegExp::GetSpecialPropertyName(uint32 index, Var *propertyName, ScriptContext * requestContext)
    {LOGMEIN("JavascriptRegularExpression.cpp] 1508\n");
        uint length = GetSpecialPropertyCount();

        if (index < length)
        {LOGMEIN("JavascriptRegularExpression.cpp] 1512\n");
            *propertyName = requestContext->GetPropertyString(GetSpecialPropertyIdsInlined()[index]);
            return true;
        }
        return false;
    }

    // Returns the number of special non-enumerable properties this type has.
    uint JavascriptRegExp::GetSpecialPropertyCount() const
    {LOGMEIN("JavascriptRegularExpression.cpp] 1521\n");
        if (GetScriptContext()->GetConfig()->IsES6RegExPrototypePropertiesEnabled())
        {LOGMEIN("JavascriptRegularExpression.cpp] 1523\n");
            return 1; // lastIndex
        }

        uint specialPropertyCount = defaultSpecialPropertyIdsCount;

        if (GetScriptContext()->GetConfig()->IsES6UnicodeExtensionsEnabled())
        {LOGMEIN("JavascriptRegularExpression.cpp] 1530\n");
            specialPropertyCount += 1;
        }

        if (GetScriptContext()->GetConfig()->IsES6RegExStickyEnabled())
        {LOGMEIN("JavascriptRegularExpression.cpp] 1535\n");
            specialPropertyCount += 1;
        }

        return specialPropertyCount;
    }

    // Returns the list of special non-enumerable properties for the type.
    PropertyId const * JavascriptRegExp::GetSpecialPropertyIds() const
    {LOGMEIN("JavascriptRegularExpression.cpp] 1544\n");
        return GetSpecialPropertyIdsInlined();
    }

    inline PropertyId const * JavascriptRegExp::GetSpecialPropertyIdsInlined() const
    {LOGMEIN("JavascriptRegularExpression.cpp] 1549\n");
        return GetScriptContext()->GetConfig()->IsES6UnicodeExtensionsEnabled()
            ? specialPropertyIdsAll
            : specialPropertyIdsWithoutUnicode;
    }

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType JavascriptRegExp::GetSnapTag_TTD() const
    {LOGMEIN("JavascriptRegularExpression.cpp] 1557\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapRegexObject;
    }

    void JavascriptRegExp::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("JavascriptRegularExpression.cpp] 1562\n");
        TTD::NSSnapObjects::SnapRegexInfo* sri = alloc.SlabAllocateStruct<TTD::NSSnapObjects::SnapRegexInfo>();

        UnifiedRegex::RegexPattern* pattern = this->pattern;
        alloc.CopyStringIntoWLength(pattern->GetSource().GetBuffer(), pattern->GetSource().GetLength(), sri->RegexStr);
        //split regex should be automatically generated from regex string and flags so no need to exttract it as well

        sri->Flags = this->GetFlags();
        sri->LastIndexVar = TTD_CONVERT_JSVAR_TO_TTDVAR(this->lastIndexVar);
        sri->LastIndexOrFlag = this->lastIndexOrFlag;

        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapRegexInfo*, TTD::NSSnapObjects::SnapObjectType::SnapRegexObject>(objData, sri);
    }

    void JavascriptRegExp::SetLastIndexInfo_TTD(CharCount lastIndex, Js::Var lastVar)
    {LOGMEIN("JavascriptRegularExpression.cpp] 1577\n");
        this->lastIndexOrFlag = lastIndex;
        this->lastIndexVar = lastVar;
    }
#endif
} // namespace Js
