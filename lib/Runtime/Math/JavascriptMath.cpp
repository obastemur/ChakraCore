//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
namespace Js
{
#ifdef SSE2MATH
    namespace SSE2
    {
#endif
        Var JavascriptMath::Negate_Full(Var aRight, ScriptContext* scriptContext)
        {LOGMEIN("JavascriptMath.cpp] 11\n");
            // Special case for zero. Must return -0
            if( aRight == TaggedInt::ToVarUnchecked(0) )
            {LOGMEIN("JavascriptMath.cpp] 14\n");
                return scriptContext->GetLibrary()->GetNegativeZero();
            }

            double value = Negate_Helper(aRight, scriptContext);
            return JavascriptNumber::ToVarNoCheck(value, scriptContext);
        }

        Var JavascriptMath::Negate_InPlace(Var aRight, ScriptContext* scriptContext, JavascriptNumber* result)
        {LOGMEIN("JavascriptMath.cpp] 23\n");
            // Special case for zero. Must return -0
            if( aRight == TaggedInt::ToVarUnchecked(0) )
            {LOGMEIN("JavascriptMath.cpp] 26\n");
                return scriptContext->GetLibrary()->GetNegativeZero();
            }

            double value = Negate_Helper(aRight, scriptContext);
            return JavascriptNumber::InPlaceNew(value, scriptContext, result);
        }

        Var JavascriptMath::Not_Full(Var aRight, ScriptContext* scriptContext)
        {LOGMEIN("JavascriptMath.cpp] 35\n");
#if _M_IX86
            AssertMsg(!TaggedInt::Is(aRight), "Should be detected");
#endif
            int nValue = JavascriptConversion::ToInt32(aRight, scriptContext);
            return JavascriptNumber::ToVar(~nValue, scriptContext);
        }

        Var JavascriptMath::Not_InPlace(Var aRight, ScriptContext* scriptContext, JavascriptNumber* result)
        {LOGMEIN("JavascriptMath.cpp] 44\n");
            AssertMsg(!TaggedInt::Is(aRight), "Should be detected");

            int nValue = JavascriptConversion::ToInt32(aRight, scriptContext);
            return JavascriptNumber::ToVarInPlace(~nValue, scriptContext, result);
        }

        Var JavascriptMath::Increment_InPlace(Var aRight, ScriptContext* scriptContext, JavascriptNumber* result)
        {LOGMEIN("JavascriptMath.cpp] 52\n");
            if (TaggedInt::Is(aRight))
            {LOGMEIN("JavascriptMath.cpp] 54\n");
                return TaggedInt::Increment(aRight, scriptContext);
            }

            double inc = Increment_Helper(aRight, scriptContext);
            return JavascriptNumber::InPlaceNew(inc, scriptContext, result);
        }

        Var JavascriptMath::Increment_Full(Var aRight, ScriptContext* scriptContext)
        {LOGMEIN("JavascriptMath.cpp] 63\n");
            if (TaggedInt::Is(aRight))
            {LOGMEIN("JavascriptMath.cpp] 65\n");
                return TaggedInt::Increment(aRight, scriptContext);
            }

            double inc = Increment_Helper(aRight, scriptContext);
            return JavascriptNumber::ToVarNoCheck(inc, scriptContext);
        }

        Var JavascriptMath::Decrement_InPlace(Var aRight, ScriptContext* scriptContext, JavascriptNumber* result)
        {LOGMEIN("JavascriptMath.cpp] 74\n");
            if (TaggedInt::Is(aRight))
            {LOGMEIN("JavascriptMath.cpp] 76\n");
                return TaggedInt::Decrement(aRight, scriptContext);
            }

            double dec = Decrement_Helper(aRight,scriptContext);
            return JavascriptNumber::InPlaceNew(dec, scriptContext, result);
        }

        Var JavascriptMath::Decrement_Full(Var aRight, ScriptContext* scriptContext)
        {LOGMEIN("JavascriptMath.cpp] 85\n");
            if (TaggedInt::Is(aRight))
            {LOGMEIN("JavascriptMath.cpp] 87\n");
                return TaggedInt::Decrement(aRight, scriptContext);
            }

            double dec = Decrement_Helper(aRight,scriptContext);
            return JavascriptNumber::ToVarNoCheck(dec, scriptContext);
        }

        Var JavascriptMath::And_Full(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {LOGMEIN("JavascriptMath.cpp] 96\n");
            int32 value = And_Helper(aLeft, aRight, scriptContext);
            return JavascriptNumber::ToVar(value, scriptContext);
        }

        Var JavascriptMath::And_InPlace(Var aLeft, Var aRight, ScriptContext* scriptContext, JavascriptNumber* result)
        {LOGMEIN("JavascriptMath.cpp] 102\n");
            int32 value = And_Helper(aLeft, aRight, scriptContext);
            return JavascriptNumber::ToVarInPlace(value, scriptContext, result);
        }

        Var JavascriptMath::Or_Full(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {LOGMEIN("JavascriptMath.cpp] 108\n");
            int32 value = Or_Helper(aLeft, aRight, scriptContext);
            return JavascriptNumber::ToVar(value, scriptContext);
        }

        Var JavascriptMath::Or_InPlace(Var aLeft, Var aRight, ScriptContext* scriptContext, JavascriptNumber* result)
        {LOGMEIN("JavascriptMath.cpp] 114\n");
            int32 value = Or_Helper(aLeft, aRight, scriptContext);
            return JavascriptNumber::ToVarInPlace(value, scriptContext, result);
        }

        Var JavascriptMath::Xor_Full(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {LOGMEIN("JavascriptMath.cpp] 120\n");
            int32 nLeft = TaggedInt::Is(aLeft) ? TaggedInt::ToInt32(aLeft) : JavascriptConversion::ToInt32(aLeft, scriptContext);
            int32 nRight = TaggedInt::Is(aRight) ? TaggedInt::ToInt32(aRight) : JavascriptConversion::ToInt32(aRight, scriptContext);

            return JavascriptNumber::ToVar(nLeft ^ nRight,scriptContext);
        }

        Var JavascriptMath::Xor_InPlace(Var aLeft, Var aRight, ScriptContext* scriptContext,  JavascriptNumber* result)
        {LOGMEIN("JavascriptMath.cpp] 128\n");
            int32 nLeft = TaggedInt::Is(aLeft) ? TaggedInt::ToInt32(aLeft) : JavascriptConversion::ToInt32(aLeft, scriptContext);
            int32 nRight = TaggedInt::Is(aRight) ? TaggedInt::ToInt32(aRight) : JavascriptConversion::ToInt32(aRight, scriptContext);

            return JavascriptNumber::ToVarInPlace(nLeft ^ nRight, scriptContext, result);
        }

        Var JavascriptMath::ShiftLeft_Full(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {LOGMEIN("JavascriptMath.cpp] 136\n");
            int32 nValue    = JavascriptConversion::ToInt32(aLeft, scriptContext);
            uint32 nShift   = JavascriptConversion::ToUInt32(aRight, scriptContext);
            int32 nResult   = nValue << (nShift & 0x1F);

            return JavascriptNumber::ToVar(nResult,scriptContext);
        }

        Var JavascriptMath::ShiftRight_Full(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {LOGMEIN("JavascriptMath.cpp] 145\n");
            int32 nValue    = JavascriptConversion::ToInt32(aLeft, scriptContext);
            uint32 nShift   = JavascriptConversion::ToUInt32(aRight, scriptContext);

            int32 nResult   = nValue >> (nShift & 0x1F);

            return JavascriptNumber::ToVar(nResult,scriptContext);
        }

        Var JavascriptMath::ShiftRightU_Full(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {LOGMEIN("JavascriptMath.cpp] 155\n");
            uint32 nValue   = JavascriptConversion::ToUInt32(aLeft, scriptContext);
            uint32 nShift   = JavascriptConversion::ToUInt32(aRight, scriptContext);

            uint32 nResult  = nValue >> (nShift & 0x1F);

            return JavascriptNumber::ToVar(nResult,scriptContext);
        }

#if FLOATVAR
        Var JavascriptMath::Add_Full(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {LOGMEIN("JavascriptMath.cpp] 166\n");
            Assert(aLeft != nullptr);
            Assert(aRight != nullptr);
            Assert(scriptContext != nullptr);

            // If both sides are numbers, then we can do the addition directly, otherwise
            // we need to call the helper.
            if(JavascriptNumber::Is(aLeft))
            {LOGMEIN("JavascriptMath.cpp] 174\n");
                if(JavascriptNumber::Is(aRight))
                {LOGMEIN("JavascriptMath.cpp] 176\n");
                    double sum = JavascriptNumber::GetValue(aLeft) + JavascriptNumber::GetValue(aRight);
                    return JavascriptNumber::ToVarNoCheck(sum, scriptContext);
                }
                else if(TaggedInt::Is(aRight))
                {LOGMEIN("JavascriptMath.cpp] 181\n");
                    double sum = TaggedInt::ToDouble(aRight) + JavascriptNumber::GetValue(aLeft);
                    return JavascriptNumber::ToVarNoCheck(sum, scriptContext);
                }
            }
            else if(JavascriptNumber::Is(aRight))
            {LOGMEIN("JavascriptMath.cpp] 187\n");
                if(TaggedInt::Is(aLeft))
                {LOGMEIN("JavascriptMath.cpp] 189\n");
                    double sum = TaggedInt::ToDouble(aLeft) + JavascriptNumber::GetValue(aRight);
                    return JavascriptNumber::ToVarNoCheck(sum, scriptContext);
                }
            }
            else if(TaggedInt::Is(aLeft))
            {LOGMEIN("JavascriptMath.cpp] 195\n");
                if(TaggedInt::Is(aRight))
                {LOGMEIN("JavascriptMath.cpp] 197\n");
                    __int64 sum = TaggedInt::ToInt64(aLeft) + TaggedInt::ToInt64(aRight);
                    return JavascriptNumber::ToVar(sum, scriptContext);
                }
            }
            else if (TaggedInt::Is(aRight))
            {LOGMEIN("JavascriptMath.cpp] 203\n");
                return Add_FullHelper_Wrapper(aLeft, aRight, scriptContext, nullptr, false);
            }
            else if (RecyclableObject::FromVar(aLeft)->GetTypeId() == TypeIds_String && RecyclableObject::FromVar(aRight)->GetTypeId() == TypeIds_String)
            {LOGMEIN("JavascriptMath.cpp] 207\n");
                return JavascriptString::Concat(JavascriptString::FromVar(aLeft), JavascriptString::FromVar(aRight));
            }
            return Add_FullHelper_Wrapper(aLeft, aRight, scriptContext, nullptr, false);
         }
#else
        Var JavascriptMath::Add_Full(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {LOGMEIN("JavascriptMath.cpp] 214\n");
            Assert(aLeft != nullptr);
            Assert(aRight != nullptr);
            Assert(scriptContext != nullptr);

            Js::TypeId typeLeft = JavascriptOperators::GetTypeId(aLeft);
            Js::TypeId typeRight = JavascriptOperators::GetTypeId(aRight);

            // Handle combinations of TaggedInt and Number or String pairs directly,
            // otherwise call the helper.
            switch( typeLeft )
            {LOGMEIN("JavascriptMath.cpp] 225\n");
                case TypeIds_Integer:
                {LOGMEIN("JavascriptMath.cpp] 227\n");
                    switch( typeRight )
                    {LOGMEIN("JavascriptMath.cpp] 229\n");
                        case TypeIds_Integer:
                        {LOGMEIN("JavascriptMath.cpp] 231\n");

                            // Compute the sum using integer addition, then convert to double.
                            // That way there's only one int->float conversion.
#if INT32VAR
                            int64 sum = TaggedInt::ToInt64(aLeft) + TaggedInt::ToInt64(aRight);
#else
                            int32 sum = TaggedInt::ToInt32(aLeft) + TaggedInt::ToInt32(aRight);
#endif
                            return JavascriptNumber::ToVar(sum, scriptContext );
                        }

                        case TypeIds_Number:
                        {LOGMEIN("JavascriptMath.cpp] 244\n");
                            double sum = TaggedInt::ToDouble(aLeft) + JavascriptNumber::GetValue(aRight);
                            return JavascriptNumber::NewInlined( sum, scriptContext );
                        }
                    }
                    break;
                }

                case TypeIds_Number:
                {LOGMEIN("JavascriptMath.cpp] 253\n");
                    switch( typeRight )
                    {LOGMEIN("JavascriptMath.cpp] 255\n");
                        case TypeIds_Integer:
                        {LOGMEIN("JavascriptMath.cpp] 257\n");
                            double sum = JavascriptNumber::GetValue(aLeft) + TaggedInt::ToDouble(aRight);
                            return JavascriptNumber::NewInlined( sum, scriptContext );
                        }

                        case TypeIds_Number:
                        {LOGMEIN("JavascriptMath.cpp] 263\n");
                            double sum = JavascriptNumber::GetValue(aLeft) + JavascriptNumber::GetValue(aRight);
                            return JavascriptNumber::NewInlined( sum, scriptContext );
                        }
                    }
                    break;
                }

                case TypeIds_String:
                {LOGMEIN("JavascriptMath.cpp] 272\n");
                    if( typeRight == TypeIds_String )
                    {LOGMEIN("JavascriptMath.cpp] 274\n");
                        JavascriptString* leftString = JavascriptString::FromVar(aLeft);
                        JavascriptString* rightString = JavascriptString::FromVar(aRight);
                        return JavascriptString::Concat(leftString, rightString);
                    }
                    break;
                }
            }

            return Add_FullHelper_Wrapper(aLeft, aRight, scriptContext, nullptr, false);
        }
#endif
        Var JavascriptMath::Add_InPlace(Var aLeft, Var aRight, ScriptContext* scriptContext, JavascriptNumber* result)
        {LOGMEIN("JavascriptMath.cpp] 287\n");
            Assert(aLeft != nullptr);
            Assert(aRight != nullptr);
            Assert(scriptContext != nullptr);
            Assert(result != nullptr);

            // If both sides are numbers, then we can do the addition directly, otherwise
            // we need to call the helper.
            if( TaggedInt::Is(aLeft) )
            {LOGMEIN("JavascriptMath.cpp] 296\n");
                if( TaggedInt::Is(aRight) )
                {LOGMEIN("JavascriptMath.cpp] 298\n");
                    // Compute the sum using integer addition, then convert to double.
                    // That way there's only one int->float conversion.
#if INT32VAR
                    int64 sum = TaggedInt::ToInt64(aLeft) + TaggedInt::ToInt64(aRight);
#else
                    int32 sum = TaggedInt::ToInt32(aLeft) + TaggedInt::ToInt32(aRight);
#endif

                    return JavascriptNumber::ToVarInPlace(sum, scriptContext, result);
                }
                else if( JavascriptNumber::Is_NoTaggedIntCheck(aRight) )
                {LOGMEIN("JavascriptMath.cpp] 310\n");
                    double sum = TaggedInt::ToDouble(aLeft) + JavascriptNumber::GetValue(aRight);
                    return JavascriptNumber::InPlaceNew( sum, scriptContext, result );
                }
            }
            else if( TaggedInt::Is(aRight) )
            {LOGMEIN("JavascriptMath.cpp] 316\n");
                if( JavascriptNumber::Is_NoTaggedIntCheck(aLeft) )
                {LOGMEIN("JavascriptMath.cpp] 318\n");
                    double sum = JavascriptNumber::GetValue(aLeft) + TaggedInt::ToDouble(aRight);
                    return JavascriptNumber::InPlaceNew( sum, scriptContext, result );
                }
            }
            else if( JavascriptNumber::Is_NoTaggedIntCheck(aLeft) && JavascriptNumber::Is_NoTaggedIntCheck(aRight) )
            {LOGMEIN("JavascriptMath.cpp] 324\n");
                double sum = JavascriptNumber::GetValue(aLeft) + JavascriptNumber::GetValue(aRight);
                return JavascriptNumber::InPlaceNew( sum, scriptContext, result );
            }

            return Add_FullHelper_Wrapper(aLeft, aRight, scriptContext, result, false);
        }

        Var JavascriptMath::AddLeftDead(Var aLeft, Var aRight, ScriptContext* scriptContext, JavascriptNumber *result)
        {LOGMEIN("JavascriptMath.cpp] 333\n");
            if (JavascriptOperators::GetTypeId(aLeft) == TypeIds_String)
            {LOGMEIN("JavascriptMath.cpp] 335\n");
                JavascriptString* leftString = JavascriptString::FromVar(aLeft);
                JavascriptString* rightString;
                TypeId rightType = JavascriptOperators::GetTypeId(aRight);
                switch(rightType)
                {LOGMEIN("JavascriptMath.cpp] 340\n");
                    case TypeIds_String:
                        rightString = JavascriptString::FromVar(aRight);

StringCommon:
                        return leftString->ConcatDestructive(rightString);

                    case TypeIds_Integer:
                        rightString = scriptContext->GetIntegerString(aRight);
                        goto StringCommon;

                    case TypeIds_Number:
                        rightString = JavascriptNumber::ToStringRadix10(JavascriptNumber::GetValue(aRight), scriptContext);
                        goto StringCommon;
                }
            }

            if (TaggedInt::Is(aLeft))
            {LOGMEIN("JavascriptMath.cpp] 358\n");
                if (TaggedInt::Is(aRight))
                {LOGMEIN("JavascriptMath.cpp] 360\n");
                    return TaggedInt::Add(aLeft, aRight, scriptContext);
                }
                else if (JavascriptNumber::Is_NoTaggedIntCheck(aRight))
                {LOGMEIN("JavascriptMath.cpp] 364\n");
                    return JavascriptNumber::ToVarMaybeInPlace(TaggedInt::ToDouble(aLeft) + JavascriptNumber::GetValue(aRight), scriptContext, result);
                }
            }
            else if (TaggedInt::Is(aRight))
            {LOGMEIN("JavascriptMath.cpp] 369\n");
                if (JavascriptNumber::Is_NoTaggedIntCheck(aLeft))
                {LOGMEIN("JavascriptMath.cpp] 371\n");
                    return JavascriptNumber::ToVarMaybeInPlace(JavascriptNumber::GetValue(aLeft) + TaggedInt::ToDouble(aRight), scriptContext, result);
                }
            }
            else if (JavascriptNumber::Is_NoTaggedIntCheck(aLeft) && JavascriptNumber::Is_NoTaggedIntCheck(aRight))
            {LOGMEIN("JavascriptMath.cpp] 376\n");
                return JavascriptNumber::ToVarMaybeInPlace(JavascriptNumber::GetValue(aLeft) + JavascriptNumber::GetValue(aRight), scriptContext, result);
            }
            return Add_FullHelper_Wrapper(aLeft, aRight, scriptContext, result, true);
        }

        Var JavascriptMath::Add_FullHelper_Wrapper(Var aLeft, Var aRight, ScriptContext* scriptContext, JavascriptNumber* result, bool leftIsDead)
        {LOGMEIN("JavascriptMath.cpp] 383\n");
            Var aLeftToPrim = JavascriptConversion::ToPrimitive(aLeft, JavascriptHint::None, scriptContext);
            Var aRightToPrim = JavascriptConversion::ToPrimitive(aRight, JavascriptHint::None, scriptContext);
            return Add_FullHelper(aLeftToPrim, aRightToPrim, scriptContext, result, leftIsDead);
        }

        Var JavascriptMath::Add_FullHelper(Var primLeft, Var primRight, ScriptContext* scriptContext, JavascriptNumber *result, bool leftIsDead)
        {LOGMEIN("JavascriptMath.cpp] 390\n");
            // If either side is a string, then the result is also a string
            if (JavascriptOperators::GetTypeId(primLeft) == TypeIds_String)
            {LOGMEIN("JavascriptMath.cpp] 393\n");
                JavascriptString* stringLeft = JavascriptString::FromVar(primLeft);
                JavascriptString* stringRight = nullptr;

                if (JavascriptOperators::GetTypeId(primRight) == TypeIds_String)
                {LOGMEIN("JavascriptMath.cpp] 398\n");
                    stringRight = JavascriptString::FromVar(primRight);
                }
                else
                {
                    stringRight = JavascriptConversion::ToString(primRight, scriptContext);
                }

                if(leftIsDead)
                {LOGMEIN("JavascriptMath.cpp] 407\n");
                    return stringLeft->ConcatDestructive(stringRight);
                }
                return JavascriptString::Concat(stringLeft, stringRight);
            }

            if (JavascriptOperators::GetTypeId(primRight) == TypeIds_String)
            {LOGMEIN("JavascriptMath.cpp] 414\n");
                JavascriptString* stringLeft = JavascriptConversion::ToString(primLeft, scriptContext);
                JavascriptString* stringRight = JavascriptString::FromVar(primRight);

                if(leftIsDead)
                {LOGMEIN("JavascriptMath.cpp] 419\n");
                    return stringLeft->ConcatDestructive(stringRight);
                }
                return JavascriptString::Concat(stringLeft, stringRight);
            }

            double sum = Add_Helper(primLeft, primRight, scriptContext);
            return JavascriptNumber::ToVarMaybeInPlace(sum, scriptContext, result);
        }

        Var JavascriptMath::MulAddLeft(Var mulLeft, Var mulRight, Var addLeft, ScriptContext* scriptContext,  JavascriptNumber* result)
        {LOGMEIN("JavascriptMath.cpp] 430\n");
            if(TaggedInt::Is(mulLeft))
            {LOGMEIN("JavascriptMath.cpp] 432\n");
                if(TaggedInt::Is(mulRight))
                {LOGMEIN("JavascriptMath.cpp] 434\n");
                    // Compute the sum using integer addition, then convert to double.
                    // That way there's only one int->float conversion.
                    JavascriptNumber mulTemp(0, scriptContext->GetLibrary()->GetNumberTypeStatic());
                    Var mulResult = TaggedInt::MultiplyInPlace(mulLeft, mulRight, scriptContext, &mulTemp);

                    if (result)
                    {LOGMEIN("JavascriptMath.cpp] 441\n");
                        return JavascriptMath::Add_InPlace(addLeft, mulResult, scriptContext, result);
                    }
                    else
                    {
                        return JavascriptMath::Add_Full(addLeft, mulResult, scriptContext);
                    }
                }
                else if(JavascriptNumber::Is_NoTaggedIntCheck(mulRight))
                {LOGMEIN("JavascriptMath.cpp] 450\n");
                    double mulResult = TaggedInt::ToDouble(mulLeft) * JavascriptNumber::GetValue(mulRight);

                    return JavascriptMath::Add_DoubleHelper(addLeft, mulResult, scriptContext, result);
                }
            }
            else if(TaggedInt::Is(mulRight))
            {LOGMEIN("JavascriptMath.cpp] 457\n");
                if(JavascriptNumber::Is_NoTaggedIntCheck(mulLeft))
                {LOGMEIN("JavascriptMath.cpp] 459\n");
                    double mulResult = JavascriptNumber::GetValue(mulLeft) * TaggedInt::ToDouble(mulRight);

                    return JavascriptMath::Add_DoubleHelper(addLeft, mulResult, scriptContext, result);
                }
            }
            else if(JavascriptNumber::Is_NoTaggedIntCheck(mulLeft) && JavascriptNumber::Is_NoTaggedIntCheck(mulRight))
            {LOGMEIN("JavascriptMath.cpp] 466\n");
                double mulResult = JavascriptNumber::GetValue(mulLeft) * JavascriptNumber::GetValue(mulRight);

                return JavascriptMath::Add_DoubleHelper(addLeft, mulResult, scriptContext, result);
            }

            Var aMul;
            JavascriptNumber mulTemp(0, scriptContext->GetLibrary()->GetNumberTypeStatic());
            aMul = JavascriptMath::Multiply_InPlace(mulLeft, mulRight, scriptContext, &mulTemp);
            if (result)
            {LOGMEIN("JavascriptMath.cpp] 476\n");
                return JavascriptMath::Add_InPlace(addLeft, aMul, scriptContext, result);
            }
            else
            {
                return JavascriptMath::Add_Full(addLeft, aMul, scriptContext);
            }
        }

        Var JavascriptMath::MulAddRight(Var mulLeft, Var mulRight, Var addRight, ScriptContext* scriptContext, JavascriptNumber* result)
        {LOGMEIN("JavascriptMath.cpp] 486\n");
            if(TaggedInt::Is(mulLeft))
            {LOGMEIN("JavascriptMath.cpp] 488\n");
                if(TaggedInt::Is(mulRight))
                {LOGMEIN("JavascriptMath.cpp] 490\n");
                    // Compute the sum using integer addition, then convert to double.
                    // That way there's only one int->float conversion.
                    JavascriptNumber mulTemp(0, scriptContext->GetLibrary()->GetNumberTypeStatic());
                    Var mulResult = TaggedInt::MultiplyInPlace(mulLeft, mulRight, scriptContext, &mulTemp);

                    if (result)
                    {LOGMEIN("JavascriptMath.cpp] 497\n");
                        return JavascriptMath::Add_InPlace(mulResult, addRight, scriptContext, result);
                    }
                    else
                    {
                        return JavascriptMath::Add_Full(mulResult, addRight, scriptContext);
                    }
                }
                else if(JavascriptNumber::Is_NoTaggedIntCheck(mulRight))
                {LOGMEIN("JavascriptMath.cpp] 506\n");
                    double mulResult = TaggedInt::ToDouble(mulLeft) * JavascriptNumber::GetValue(mulRight);

                    return JavascriptMath::Add_DoubleHelper(mulResult, addRight, scriptContext, result);
                }
            }
            else if(TaggedInt::Is(mulRight))
            {LOGMEIN("JavascriptMath.cpp] 513\n");
                if(JavascriptNumber::Is_NoTaggedIntCheck(mulLeft))
                {LOGMEIN("JavascriptMath.cpp] 515\n");
                    double mulResult = JavascriptNumber::GetValue(mulLeft) * TaggedInt::ToDouble(mulRight);

                    return JavascriptMath::Add_DoubleHelper(mulResult, addRight, scriptContext, result);
                }
            }
            else if(JavascriptNumber::Is_NoTaggedIntCheck(mulLeft) && JavascriptNumber::Is_NoTaggedIntCheck(mulRight))
            {LOGMEIN("JavascriptMath.cpp] 522\n");
                double mulResult = JavascriptNumber::GetValue(mulLeft) * JavascriptNumber::GetValue(mulRight);

                return JavascriptMath::Add_DoubleHelper(mulResult, addRight, scriptContext, result);
            }

            Var aMul;
            JavascriptNumber mulTemp(0, scriptContext->GetLibrary()->GetNumberTypeStatic());
            aMul = JavascriptMath::Multiply_InPlace(mulLeft, mulRight, scriptContext, &mulTemp);
            if (result)
            {LOGMEIN("JavascriptMath.cpp] 532\n");
                return JavascriptMath::Add_InPlace(aMul, addRight, scriptContext, result);
            }
            else
            {
                return JavascriptMath::Add_Full(aMul, addRight, scriptContext);
            }
        }

        Var JavascriptMath::MulSubLeft(Var mulLeft, Var mulRight, Var subLeft, ScriptContext* scriptContext, JavascriptNumber* result)
        {LOGMEIN("JavascriptMath.cpp] 542\n");
            if(TaggedInt::Is(mulLeft))
            {LOGMEIN("JavascriptMath.cpp] 544\n");
                if(TaggedInt::Is(mulRight))
                {LOGMEIN("JavascriptMath.cpp] 546\n");
                    // Compute the sum using integer addition, then convert to double.
                    // That way there's only one int->float conversion.
                    JavascriptNumber mulTemp(0, scriptContext->GetLibrary()->GetNumberTypeStatic());
                    Var mulResult = TaggedInt::MultiplyInPlace(mulLeft, mulRight, scriptContext, &mulTemp);

                    if (result)
                    {LOGMEIN("JavascriptMath.cpp] 553\n");
                        return JavascriptMath::Subtract_InPlace(subLeft, mulResult, scriptContext, result);
                    }
                    else
                    {
                        return JavascriptMath::Subtract_Full(subLeft, mulResult, scriptContext);
                    }
                }
                else if(JavascriptNumber::Is_NoTaggedIntCheck(mulRight))
                {LOGMEIN("JavascriptMath.cpp] 562\n");
                    double mulResult = TaggedInt::ToDouble(mulLeft) * JavascriptNumber::GetValue(mulRight);

                    return JavascriptMath::Subtract_DoubleHelper(subLeft, mulResult, scriptContext, result);
                }
            }
            else if(TaggedInt::Is(mulRight))
            {LOGMEIN("JavascriptMath.cpp] 569\n");
                if(JavascriptNumber::Is_NoTaggedIntCheck(mulLeft))
                {LOGMEIN("JavascriptMath.cpp] 571\n");
                    double mulResult = JavascriptNumber::GetValue(mulLeft) * TaggedInt::ToDouble(mulRight);

                    return JavascriptMath::Subtract_DoubleHelper(subLeft, mulResult, scriptContext, result);
                }
            }
            else if(JavascriptNumber::Is_NoTaggedIntCheck(mulLeft) && JavascriptNumber::Is_NoTaggedIntCheck(mulRight))
            {LOGMEIN("JavascriptMath.cpp] 578\n");
                double mulResult = JavascriptNumber::GetValue(mulLeft) * JavascriptNumber::GetValue(mulRight);

                return JavascriptMath::Subtract_DoubleHelper(subLeft, mulResult, scriptContext, result);
            }

            Var aMul;
            JavascriptNumber mulTemp(0, scriptContext->GetLibrary()->GetNumberTypeStatic());
            aMul = JavascriptMath::Multiply_InPlace(mulLeft, mulRight, scriptContext, &mulTemp);
            if (result)
            {LOGMEIN("JavascriptMath.cpp] 588\n");
                return JavascriptMath::Subtract_InPlace(subLeft, aMul, scriptContext, result);
            }
            else
            {
                return JavascriptMath::Subtract_Full(subLeft, aMul, scriptContext);
            }
        }

        Var JavascriptMath::MulSubRight(Var mulLeft, Var mulRight, Var subRight, ScriptContext* scriptContext, JavascriptNumber* result)
        {LOGMEIN("JavascriptMath.cpp] 598\n");
            if(TaggedInt::Is(mulLeft))
            {LOGMEIN("JavascriptMath.cpp] 600\n");
                if(TaggedInt::Is(mulRight))
                {LOGMEIN("JavascriptMath.cpp] 602\n");
                    // Compute the sum using integer addition, then convert to double.
                    // That way there's only one int->float conversion.
                    JavascriptNumber mulTemp(0, scriptContext->GetLibrary()->GetNumberTypeStatic());
                    Var mulResult = TaggedInt::MultiplyInPlace(mulLeft, mulRight, scriptContext, &mulTemp);

                    if (result)
                    {LOGMEIN("JavascriptMath.cpp] 609\n");
                        return JavascriptMath::Subtract_InPlace(mulResult, subRight, scriptContext, result);
                    }
                    else
                    {
                        return JavascriptMath::Subtract_Full(mulResult, subRight, scriptContext);
                    }
                }
                else if(JavascriptNumber::Is_NoTaggedIntCheck(mulRight))
                {LOGMEIN("JavascriptMath.cpp] 618\n");
                    double mulResult = TaggedInt::ToDouble(mulLeft) * JavascriptNumber::GetValue(mulRight);

                    return JavascriptMath::Subtract_DoubleHelper(mulResult, subRight, scriptContext, result);
                }
            }
            else if(TaggedInt::Is(mulRight))
            {LOGMEIN("JavascriptMath.cpp] 625\n");
                if(JavascriptNumber::Is_NoTaggedIntCheck(mulLeft))
                {LOGMEIN("JavascriptMath.cpp] 627\n");
                    double mulResult = JavascriptNumber::GetValue(mulLeft) * TaggedInt::ToDouble(mulRight);

                    return JavascriptMath::Subtract_DoubleHelper(mulResult, subRight, scriptContext, result);
                }
            }
            else if(JavascriptNumber::Is_NoTaggedIntCheck(mulLeft) && JavascriptNumber::Is_NoTaggedIntCheck(mulRight))
            {LOGMEIN("JavascriptMath.cpp] 634\n");
                double mulResult = JavascriptNumber::GetValue(mulLeft) * JavascriptNumber::GetValue(mulRight);

                return JavascriptMath::Subtract_DoubleHelper(mulResult, subRight, scriptContext, result);
            }

            Var aMul;
            JavascriptNumber mulTemp(0, scriptContext->GetLibrary()->GetNumberTypeStatic());
            aMul = JavascriptMath::Multiply_InPlace(mulLeft, mulRight, scriptContext, &mulTemp);
            if (result)
            {LOGMEIN("JavascriptMath.cpp] 644\n");
                return JavascriptMath::Subtract_InPlace(aMul, subRight, scriptContext, result);
            }
            else
            {
                return JavascriptMath::Subtract_Full(aMul, subRight, scriptContext);
            }
        }

        Var inline JavascriptMath::Add_DoubleHelper(double dblLeft, Var addRight, ScriptContext* scriptContext, JavascriptNumber*result)
        {LOGMEIN("JavascriptMath.cpp] 654\n");
            if (TaggedInt::Is(addRight))
            {LOGMEIN("JavascriptMath.cpp] 656\n");
                double sum =  dblLeft + TaggedInt::ToDouble(addRight);

                return JavascriptNumber::ToVarMaybeInPlace(sum, scriptContext, result);
            }
            else if (JavascriptNumber::Is_NoTaggedIntCheck(addRight))
            {LOGMEIN("JavascriptMath.cpp] 662\n");
                double sum = dblLeft + JavascriptNumber::GetValue(addRight);

                return JavascriptNumber::ToVarMaybeInPlace(sum, scriptContext, result);
            }
            else
            {
                Var aLeft = JavascriptNumber::ToVarMaybeInPlace(dblLeft, scriptContext, result);

                return Add_Full(aLeft, addRight, scriptContext);
            }
        }

        Var inline JavascriptMath::Add_DoubleHelper(Var addLeft, double dblRight, ScriptContext* scriptContext, JavascriptNumber*result)
        {LOGMEIN("JavascriptMath.cpp] 676\n");
            if (TaggedInt::Is(addLeft))
            {LOGMEIN("JavascriptMath.cpp] 678\n");
                double sum =  TaggedInt::ToDouble(addLeft) + dblRight;

                return JavascriptNumber::ToVarMaybeInPlace(sum, scriptContext, result);
            }
            else if (JavascriptNumber::Is_NoTaggedIntCheck(addLeft))
            {LOGMEIN("JavascriptMath.cpp] 684\n");
                double sum = JavascriptNumber::GetValue(addLeft) + dblRight;

                return JavascriptNumber::ToVarMaybeInPlace(sum, scriptContext, result);
            }
            else
            {
                Var aRight = JavascriptNumber::ToVarMaybeInPlace(dblRight, scriptContext, result);

                return Add_Full(addLeft, aRight, scriptContext);
            }
        }

        Var inline JavascriptMath::Subtract_DoubleHelper(double dblLeft, Var subRight, ScriptContext* scriptContext, JavascriptNumber* result)
        {LOGMEIN("JavascriptMath.cpp] 698\n");
            if (TaggedInt::Is(subRight))
            {LOGMEIN("JavascriptMath.cpp] 700\n");
                double sum =  dblLeft - TaggedInt::ToDouble(subRight);

                return JavascriptNumber::ToVarMaybeInPlace(sum, scriptContext, result);
            }
            else if (JavascriptNumber::Is_NoTaggedIntCheck(subRight))
            {LOGMEIN("JavascriptMath.cpp] 706\n");
                double sum = dblLeft - JavascriptNumber::GetValue(subRight);

                return JavascriptNumber::ToVarMaybeInPlace(sum, scriptContext, result);
            }
            else
            {
                Var aLeft = JavascriptNumber::ToVarMaybeInPlace(dblLeft, scriptContext, result);

                return Subtract_Full(aLeft, subRight, scriptContext);
            }
        }

        Var inline JavascriptMath::Subtract_DoubleHelper(Var subLeft, double dblRight, ScriptContext* scriptContext, JavascriptNumber*result)
        {LOGMEIN("JavascriptMath.cpp] 720\n");
            if (TaggedInt::Is(subLeft))
            {LOGMEIN("JavascriptMath.cpp] 722\n");
                double sum =  TaggedInt::ToDouble(subLeft) - dblRight;

                return JavascriptNumber::ToVarMaybeInPlace(sum, scriptContext, result);
            }
            else if (JavascriptNumber::Is_NoTaggedIntCheck(subLeft))
            {LOGMEIN("JavascriptMath.cpp] 728\n");
                double sum = JavascriptNumber::GetValue(subLeft) - dblRight;

                return JavascriptNumber::ToVarMaybeInPlace(sum, scriptContext, result);
            }
            else
            {
                Var aRight = JavascriptNumber::ToVarMaybeInPlace(dblRight, scriptContext, result);

                return Subtract_Full(subLeft, aRight, scriptContext);
            }
        }

        Var JavascriptMath::Subtract_Full(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {LOGMEIN("JavascriptMath.cpp] 742\n");
            double difference = Subtract_Helper(aLeft, aRight, scriptContext);
            return JavascriptNumber::ToVarNoCheck(difference, scriptContext);
        }

        Var JavascriptMath::Subtract_InPlace(Var aLeft, Var aRight, ScriptContext* scriptContext, JavascriptNumber* result)
        {LOGMEIN("JavascriptMath.cpp] 748\n");
            double difference = Subtract_Helper(aLeft, aRight, scriptContext);
            return JavascriptNumber::InPlaceNew(difference, scriptContext, result);
        }

        Var JavascriptMath::Divide_Full(Var aLeft,Var aRight, ScriptContext* scriptContext)
        {LOGMEIN("JavascriptMath.cpp] 754\n");
            // If both arguments are TaggedInt, then try to do integer division
            // This case is not handled by the lowerer.
            if (TaggedInt::IsPair(aLeft, aRight))
            {LOGMEIN("JavascriptMath.cpp] 758\n");
                return TaggedInt::Divide(aLeft, aRight, scriptContext);
            }

            return JavascriptNumber::NewInlined( Divide_Helper(aLeft, aRight, scriptContext), scriptContext );
        }

        Var JavascriptMath::Exponentiation_Full(Var aLeft, Var aRight, ScriptContext *scriptContext)
        {LOGMEIN("JavascriptMath.cpp] 766\n");
            double x = JavascriptConversion::ToNumber(aLeft, scriptContext);
            double y = JavascriptConversion::ToNumber(aRight, scriptContext);
            return JavascriptNumber::ToVarNoCheck(Math::Pow(x, y), scriptContext);
        }

        Var JavascriptMath::Exponentiation_InPlace(Var aLeft, Var aRight, ScriptContext* scriptContext, JavascriptNumber* result)
        {LOGMEIN("JavascriptMath.cpp] 773\n");
            // The IEEE 754 floating point spec ensures that NaNs are preserved in all operations
            double dblLeft = JavascriptConversion::ToNumber(aLeft, scriptContext);
            double dblRight = JavascriptConversion::ToNumber(aRight, scriptContext);

            return JavascriptNumber::InPlaceNew(Math::Pow(dblLeft, dblRight), scriptContext, result);
        }

        Var JavascriptMath::Multiply_Full(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {LOGMEIN("JavascriptMath.cpp] 782\n");
            Assert(aLeft != nullptr);
            Assert(aRight != nullptr);
            Assert(scriptContext != nullptr);

            if(JavascriptNumber::Is(aLeft))
            {LOGMEIN("JavascriptMath.cpp] 788\n");
                if(JavascriptNumber::Is(aRight))
                {LOGMEIN("JavascriptMath.cpp] 790\n");
                    double product = JavascriptNumber::GetValue(aLeft) * JavascriptNumber::GetValue(aRight);
                    return JavascriptNumber::ToVarNoCheck(product, scriptContext);
                }
                else if(TaggedInt::Is(aRight))
                {LOGMEIN("JavascriptMath.cpp] 795\n");
                    double product = TaggedInt::ToDouble(aRight) * JavascriptNumber::GetValue(aLeft);
                    return JavascriptNumber::ToVarNoCheck(product, scriptContext);
                }
            }
            else if(JavascriptNumber::Is(aRight))
            {LOGMEIN("JavascriptMath.cpp] 801\n");
                if(TaggedInt::Is(aLeft))
                {LOGMEIN("JavascriptMath.cpp] 803\n");
                    double product = TaggedInt::ToDouble(aLeft) * JavascriptNumber::GetValue(aRight);
                    return JavascriptNumber::ToVarNoCheck(product, scriptContext);
                }
            }
            else if(TaggedInt::IsPair(aLeft, aRight))
            {LOGMEIN("JavascriptMath.cpp] 809\n");
                return TaggedInt::Multiply(aLeft, aRight, scriptContext);
            }
            double product = Multiply_Helper(aLeft, aRight, scriptContext);
            return JavascriptNumber::ToVarNoCheck(product, scriptContext);
        }

        Var JavascriptMath::Multiply_InPlace(Var aLeft, Var aRight, ScriptContext* scriptContext, JavascriptNumber* result)
        {LOGMEIN("JavascriptMath.cpp] 817\n");
            if(JavascriptNumber::Is(aLeft))
            {LOGMEIN("JavascriptMath.cpp] 819\n");
                if(JavascriptNumber::Is(aRight))
                {LOGMEIN("JavascriptMath.cpp] 821\n");
                    return JavascriptNumber::ToVarInPlace(
                        JavascriptNumber::GetValue(aLeft) * JavascriptNumber::GetValue(aRight), scriptContext, result);
                }
                else if (TaggedInt::Is(aRight))
                {LOGMEIN("JavascriptMath.cpp] 826\n");
                    return JavascriptNumber::ToVarInPlace(
                        JavascriptNumber::GetValue(aLeft) * TaggedInt::ToDouble(aRight), scriptContext, result);
                }
            }
            else if(JavascriptNumber::Is(aRight))
            {LOGMEIN("JavascriptMath.cpp] 832\n");
                if(TaggedInt::Is(aLeft))
                {LOGMEIN("JavascriptMath.cpp] 834\n");
                    return JavascriptNumber::ToVarInPlace(
                        TaggedInt::ToDouble(aLeft) * JavascriptNumber::GetValue(aRight), scriptContext, result);
                }
            }
            else if(TaggedInt::IsPair(aLeft, aRight))
            {LOGMEIN("JavascriptMath.cpp] 840\n");
                return TaggedInt::MultiplyInPlace(aLeft, aRight, scriptContext, result);
            }

            double product = Multiply_Helper(aLeft, aRight, scriptContext);
            return JavascriptNumber::InPlaceNew(product, scriptContext, result);
        }

        Var JavascriptMath::Divide_InPlace(Var aLeft, Var aRight, ScriptContext* scriptContext, JavascriptNumber* result)
        {LOGMEIN("JavascriptMath.cpp] 849\n");
            // If both arguments are TaggedInt, then try to do integer division
            // This case is not handled by the lowerer.
            if (TaggedInt::IsPair(aLeft, aRight))
            {LOGMEIN("JavascriptMath.cpp] 853\n");
                return TaggedInt::DivideInPlace(aLeft, aRight, scriptContext, result);
            }

            double quotient = Divide_Helper(aLeft, aRight, scriptContext);
            return JavascriptNumber::InPlaceNew(quotient, scriptContext, result);
        }

        Var JavascriptMath::Modulus_Full(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {LOGMEIN("JavascriptMath.cpp] 862\n");
            // If both arguments are TaggedInt, then try to do integer modulus.
            // This case is not handled by the lowerer.
            if (TaggedInt::IsPair(aLeft, aRight))
            {LOGMEIN("JavascriptMath.cpp] 866\n");
                return TaggedInt::Modulus(aLeft, aRight, scriptContext);
            }

            double remainder = Modulus_Helper(aLeft, aRight, scriptContext);
            return JavascriptNumber::ToVarNoCheck(remainder, scriptContext);
        }

        Var JavascriptMath::Modulus_InPlace(Var aLeft, Var aRight, ScriptContext* scriptContext, JavascriptNumber* result)
        {LOGMEIN("JavascriptMath.cpp] 875\n");
            Assert(aLeft != nullptr);
            Assert(aRight != nullptr);
            Assert(scriptContext != nullptr);

            // If both arguments are TaggedInt, then try to do integer division
            // This case is not handled by the lowerer.
            if (TaggedInt::IsPair(aLeft, aRight))
            {LOGMEIN("JavascriptMath.cpp] 883\n");
                return TaggedInt::Modulus(aLeft, aRight, scriptContext);
            }

            double remainder = Modulus_Helper(aLeft, aRight, scriptContext);
            return JavascriptNumber::InPlaceNew(remainder, scriptContext, result);
        }


        Var JavascriptMath::FinishOddDivByPow2(int32 value, ScriptContext *scriptContext)
        {LOGMEIN("JavascriptMath.cpp] 893\n");
            return JavascriptNumber::New((double)(value + 0.5), scriptContext);
        }

        Var JavascriptMath::FinishOddDivByPow2_InPlace(int32 value, ScriptContext *scriptContext, JavascriptNumber* result)
        {LOGMEIN("JavascriptMath.cpp] 898\n");
            return JavascriptNumber::InPlaceNew((double)(value + 0.5), scriptContext, result);
        }

        Var JavascriptMath::MaxInAnArray(RecyclableObject * function, CallInfo callInfo, ...)
        {
            PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

            ARGUMENTS(args, callInfo);
            Assert(args.Info.Count == 2);
            Var thisArg = args[0];
            Var arrayArg = args[1];

            ScriptContext * scriptContext = function->GetScriptContext();

            TypeId typeId = JavascriptOperators::GetTypeId(arrayArg);
            if (!JavascriptNativeArray::Is(typeId) && !(TypedArrayBase::Is(typeId) && typeId != TypeIds_CharArray && typeId != TypeIds_BoolArray))
            {LOGMEIN("JavascriptMath.cpp] 915\n");
                if (JavascriptArray::IsVarArray(typeId) && JavascriptArray::FromVar(arrayArg)->GetLength() == 0)
                {LOGMEIN("JavascriptMath.cpp] 917\n");
                    return scriptContext->GetLibrary()->GetNegativeInfinite();
                }
                return JavascriptFunction::CalloutHelper<false>(function, thisArg, /* overridingNewTarget = */nullptr, arrayArg, scriptContext);
            }

            if (JavascriptNativeArray::Is(typeId))
            {LOGMEIN("JavascriptMath.cpp] 924\n");
#if ENABLE_COPYONACCESS_ARRAY
                JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(arrayArg);
#endif
                JavascriptNativeArray * argsArray = JavascriptNativeArray::FromVar(arrayArg);
                uint len = argsArray->GetLength();
                if (len == 0)
                {LOGMEIN("JavascriptMath.cpp] 931\n");
                    return scriptContext->GetLibrary()->GetNegativeInfinite();
                }

                if (argsArray->GetHead()->next != nullptr || !argsArray->HasNoMissingValues() ||
                    argsArray->GetHead()->length != len)
                {LOGMEIN("JavascriptMath.cpp] 937\n");
                    return JavascriptFunction::CalloutHelper<false>(function, thisArg, /* overridingNewTarget = */nullptr, arrayArg, scriptContext);
                }

                return argsArray->FindMinOrMax(scriptContext, true /*findMax*/);
            }
            else
            {
                TypedArrayBase * argsArray = TypedArrayBase::FromVar(arrayArg);
                uint len = argsArray->GetLength();
                if (len == 0)
                {LOGMEIN("JavascriptMath.cpp] 948\n");
                    return scriptContext->GetLibrary()->GetNegativeInfinite();
                }
                Var max = argsArray->FindMinOrMax(scriptContext, typeId, true /*findMax*/);
                if (max == nullptr)
                {LOGMEIN("JavascriptMath.cpp] 953\n");
                    return JavascriptFunction::CalloutHelper<false>(function, thisArg, /* overridingNewTarget = */nullptr, arrayArg, scriptContext);
                }
                return max;
            }
        }

        Var JavascriptMath::MinInAnArray(RecyclableObject * function, CallInfo callInfo, ...)
        {
            PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

            ARGUMENTS(args, callInfo);
            Assert(args.Info.Count == 2);
            Var thisArg = args[0];
            Var arrayArg = args[1];

            ScriptContext * scriptContext = function->GetScriptContext();

            TypeId typeId = JavascriptOperators::GetTypeId(arrayArg);
            if (!JavascriptNativeArray::Is(typeId) && !(TypedArrayBase::Is(typeId) && typeId != TypeIds_CharArray && typeId != TypeIds_BoolArray))
            {LOGMEIN("JavascriptMath.cpp] 973\n");
                if (JavascriptArray::Is(typeId) && JavascriptArray::FromVar(arrayArg)->GetLength() == 0)
                {LOGMEIN("JavascriptMath.cpp] 975\n");
                    return scriptContext->GetLibrary()->GetPositiveInfinite();
                }
                return JavascriptFunction::CalloutHelper<false>(function, thisArg, /* overridingNewTarget = */nullptr, arrayArg, scriptContext);
            }

            if (JavascriptNativeArray::Is(typeId))
            {LOGMEIN("JavascriptMath.cpp] 982\n");
#if ENABLE_COPYONACCESS_ARRAY
                JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(arrayArg);
#endif
                JavascriptNativeArray * argsArray = JavascriptNativeArray::FromVar(arrayArg);
                uint len = argsArray->GetLength();
                if (len == 0)
                {LOGMEIN("JavascriptMath.cpp] 989\n");
                    return scriptContext->GetLibrary()->GetPositiveInfinite();
                }

                if (argsArray->GetHead()->next != nullptr || !argsArray->HasNoMissingValues() ||
                    argsArray->GetHead()->length != len)
                {LOGMEIN("JavascriptMath.cpp] 995\n");
                    return JavascriptFunction::CalloutHelper<false>(function, thisArg, /* overridingNewTarget = */nullptr, arrayArg, scriptContext);
                }

                return argsArray->FindMinOrMax(scriptContext, false /*findMax*/);
            }
            else
            {
                TypedArrayBase * argsArray = TypedArrayBase::FromVar(arrayArg);
                uint len = argsArray->GetLength();
                if (len == 0)
                {LOGMEIN("JavascriptMath.cpp] 1006\n");
                    return scriptContext->GetLibrary()->GetPositiveInfinite();
                }
                Var min = argsArray->FindMinOrMax(scriptContext, typeId, false /*findMax*/);
                if (min == nullptr)
                {LOGMEIN("JavascriptMath.cpp] 1011\n");
                    return JavascriptFunction::CalloutHelper<false>(function, thisArg, /* overridingNewTarget = */nullptr, arrayArg, scriptContext);
                }
                return min;
            }
        }

        void InitializeRandomSeeds(uint64 *seed0, uint64 *seed1, ScriptContext *scriptContext)
        {LOGMEIN("JavascriptMath.cpp] 1019\n");
#if DBG
            if (CONFIG_FLAG(PRNGSeed0) && CONFIG_FLAG(PRNGSeed1))
            {LOGMEIN("JavascriptMath.cpp] 1022\n");
                *seed0 = CONFIG_FLAG(PRNGSeed0);
                *seed1 = CONFIG_FLAG(PRNGSeed1);
            }
            else
#endif
            {
                LARGE_INTEGER s0;
                LARGE_INTEGER s1;

                if (!rand_s(reinterpret_cast<unsigned int*>(&s0.LowPart)) &&
                    !rand_s(reinterpret_cast<unsigned int*>(&s0.HighPart)) &&
                    !rand_s(reinterpret_cast<unsigned int*>(&s1.LowPart)) &&
                    !rand_s(reinterpret_cast<unsigned int*>(&s1.HighPart)))
                {LOGMEIN("JavascriptMath.cpp] 1036\n");
                    *seed0 = s0.QuadPart;
                    *seed1 = s1.QuadPart;
                }
                else
                {
                    AssertMsg(false, "Unable to initialize PRNG seeds with rand_s. Revert to using entropy.");
#ifdef ENABLE_CUSTOM_ENTROPY
                    ThreadContext *threadContext = scriptContext->GetThreadContext();

                    threadContext->GetEntropy().AddThreadCycleTime();
                    threadContext->GetEntropy().AddIoCounters();
                    *seed0 = threadContext->GetEntropy().GetRand();

                    threadContext->GetEntropy().AddThreadCycleTime();
                    threadContext->GetEntropy().AddIoCounters();
                    *seed1 = threadContext->GetEntropy().GetRand();
#endif
                }
            }
        }

        double ConvertRandomSeedsToDouble(const uint64 seed0, const uint64 seed1)
        {LOGMEIN("JavascriptMath.cpp] 1059\n");
            const uint64 mExp  = 0x3FF0000000000000;
            const uint64 mMant = 0x000FFFFFFFFFFFFF;

            // Take lower 52 bits of the sum of two seeds to make a double
            // Subtract 1.0 to negate the implicit integer bit of 1. Final range: [0.0, 1.0)
            // See IEEE754 Double-precision floating-point format for details
            //   https://en.wikipedia.org/wiki/Double-precision_floating-point_format
            uint64 resplusone_ui64 = ((seed0 + seed1) & mMant) | mExp;
            double res = *(reinterpret_cast<double*>(&resplusone_ui64)) - 1.0;
            return res;
        }

        void Xorshift128plus(uint64 *seed0, uint64 *seed1)
        {LOGMEIN("JavascriptMath.cpp] 1073\n");
            uint64 s1 = *seed0;
            uint64 s0 = *seed1;
            *seed0 = s0;
            s1 ^= s1 << 23;
            s1 ^= s1 >> 17;
            s1 ^= s0;
            s1 ^= s0 >> 26;
            *seed1 = s1;
        }

        double JavascriptMath::Random(ScriptContext *scriptContext)
        {LOGMEIN("JavascriptMath.cpp] 1085\n");
            uint64 seed0;
            uint64 seed1;

            if (!scriptContext->GetLibrary()->IsPRNGSeeded())
            {
                InitializeRandomSeeds(&seed0, &seed1, scriptContext);
#if DBG_DUMP
                OUTPUT_TRACE(Js::PRNGPhase, _u("[PRNG:%x] INIT %I64x %I64x\n"), scriptContext, seed0, seed1);
#endif
                scriptContext->GetLibrary()->SetIsPRNGSeeded(true);

#if ENABLE_TTD
                if(scriptContext->ShouldPerformReplayAction())
                {LOGMEIN("JavascriptMath.cpp] 1099\n");
                    scriptContext->GetThreadContext()->TTDLog->ReplayExternalEntropyRandomEvent(&seed0, &seed1);
                }
                else if(scriptContext->ShouldPerformRecordAction())
                {LOGMEIN("JavascriptMath.cpp] 1103\n");
                    scriptContext->GetThreadContext()->TTDLog->RecordExternalEntropyRandomEvent(seed0, seed1);
                }
                else
                {
                    ;
                }
#endif
            }
            else
            {
                seed0 = scriptContext->GetLibrary()->GetRandSeed0();
                seed1 = scriptContext->GetLibrary()->GetRandSeed1();
            }

#if DBG_DUMP
            OUTPUT_TRACE(Js::PRNGPhase, _u("[PRNG:%x] SEED %I64x %I64x\n"), scriptContext, seed0, seed1);
#endif

            Xorshift128plus(&seed0, &seed1);

            //update the seeds in script context
            scriptContext->GetLibrary()->SetRandSeed0(seed0);
            scriptContext->GetLibrary()->SetRandSeed1(seed1);

            double res = ConvertRandomSeedsToDouble(seed0, seed1);
#if DBG_DUMP
            OUTPUT_TRACE(Js::PRNGPhase, _u("[PRNG:%x] RAND %I64x\n"), scriptContext, *((uint64 *)&res));
#endif
            return res;
        }

        uint32 JavascriptMath::ToUInt32(double T1)
        {LOGMEIN("JavascriptMath.cpp] 1136\n");
            // Same as doing ToInt32 and reinterpret the bits as uint32
            return (uint32)ToInt32Core(T1);
        }

        int32 JavascriptMath::ToInt32(double T1)
        {LOGMEIN("JavascriptMath.cpp] 1142\n");
            return JavascriptMath::ToInt32Core(T1);
        }

        int32 JavascriptMath::ToInt32_Full(Var aValue, ScriptContext* scriptContext)
        {LOGMEIN("JavascriptMath.cpp] 1147\n");
            AssertMsg(!TaggedInt::Is(aValue), "Should be detected");

            // This is used when TaggedInt's overflow but remain under int32
            // so Number is our most critical case:

            TypeId typeId = JavascriptOperators::GetTypeId(aValue);

            if (typeId == TypeIds_Number)
            {LOGMEIN("JavascriptMath.cpp] 1156\n");
                return JavascriptMath::ToInt32Core(JavascriptNumber::GetValue(aValue));
            }

            return JavascriptConversion::ToInt32_Full(aValue, scriptContext);
        }
#ifdef SSE2MATH
      }
#endif
}
