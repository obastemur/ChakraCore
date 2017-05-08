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
        {TRACE_IT(64659);
            // Special case for zero. Must return -0
            if( aRight == TaggedInt::ToVarUnchecked(0) )
            {TRACE_IT(64660);
                return scriptContext->GetLibrary()->GetNegativeZero();
            }

            double value = Negate_Helper(aRight, scriptContext);
            return JavascriptNumber::ToVarNoCheck(value, scriptContext);
        }

        Var JavascriptMath::Negate_InPlace(Var aRight, ScriptContext* scriptContext, JavascriptNumber* result)
        {TRACE_IT(64661);
            // Special case for zero. Must return -0
            if( aRight == TaggedInt::ToVarUnchecked(0) )
            {TRACE_IT(64662);
                return scriptContext->GetLibrary()->GetNegativeZero();
            }

            double value = Negate_Helper(aRight, scriptContext);
            return JavascriptNumber::InPlaceNew(value, scriptContext, result);
        }

        Var JavascriptMath::Not_Full(Var aRight, ScriptContext* scriptContext)
        {TRACE_IT(64663);
#if _M_IX86
            AssertMsg(!TaggedInt::Is(aRight), "Should be detected");
#endif
            int nValue = JavascriptConversion::ToInt32(aRight, scriptContext);
            return JavascriptNumber::ToVar(~nValue, scriptContext);
        }

        Var JavascriptMath::Not_InPlace(Var aRight, ScriptContext* scriptContext, JavascriptNumber* result)
        {TRACE_IT(64664);
            AssertMsg(!TaggedInt::Is(aRight), "Should be detected");

            int nValue = JavascriptConversion::ToInt32(aRight, scriptContext);
            return JavascriptNumber::ToVarInPlace(~nValue, scriptContext, result);
        }

        Var JavascriptMath::Increment_InPlace(Var aRight, ScriptContext* scriptContext, JavascriptNumber* result)
        {TRACE_IT(64665);
            if (TaggedInt::Is(aRight))
            {TRACE_IT(64666);
                return TaggedInt::Increment(aRight, scriptContext);
            }

            double inc = Increment_Helper(aRight, scriptContext);
            return JavascriptNumber::InPlaceNew(inc, scriptContext, result);
        }

        Var JavascriptMath::Increment_Full(Var aRight, ScriptContext* scriptContext)
        {TRACE_IT(64667);
            if (TaggedInt::Is(aRight))
            {TRACE_IT(64668);
                return TaggedInt::Increment(aRight, scriptContext);
            }

            double inc = Increment_Helper(aRight, scriptContext);
            return JavascriptNumber::ToVarNoCheck(inc, scriptContext);
        }

        Var JavascriptMath::Decrement_InPlace(Var aRight, ScriptContext* scriptContext, JavascriptNumber* result)
        {TRACE_IT(64669);
            if (TaggedInt::Is(aRight))
            {TRACE_IT(64670);
                return TaggedInt::Decrement(aRight, scriptContext);
            }

            double dec = Decrement_Helper(aRight,scriptContext);
            return JavascriptNumber::InPlaceNew(dec, scriptContext, result);
        }

        Var JavascriptMath::Decrement_Full(Var aRight, ScriptContext* scriptContext)
        {TRACE_IT(64671);
            if (TaggedInt::Is(aRight))
            {TRACE_IT(64672);
                return TaggedInt::Decrement(aRight, scriptContext);
            }

            double dec = Decrement_Helper(aRight,scriptContext);
            return JavascriptNumber::ToVarNoCheck(dec, scriptContext);
        }

        Var JavascriptMath::And_Full(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {TRACE_IT(64673);
            int32 value = And_Helper(aLeft, aRight, scriptContext);
            return JavascriptNumber::ToVar(value, scriptContext);
        }

        Var JavascriptMath::And_InPlace(Var aLeft, Var aRight, ScriptContext* scriptContext, JavascriptNumber* result)
        {TRACE_IT(64674);
            int32 value = And_Helper(aLeft, aRight, scriptContext);
            return JavascriptNumber::ToVarInPlace(value, scriptContext, result);
        }

        Var JavascriptMath::Or_Full(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {TRACE_IT(64675);
            int32 value = Or_Helper(aLeft, aRight, scriptContext);
            return JavascriptNumber::ToVar(value, scriptContext);
        }

        Var JavascriptMath::Or_InPlace(Var aLeft, Var aRight, ScriptContext* scriptContext, JavascriptNumber* result)
        {TRACE_IT(64676);
            int32 value = Or_Helper(aLeft, aRight, scriptContext);
            return JavascriptNumber::ToVarInPlace(value, scriptContext, result);
        }

        Var JavascriptMath::Xor_Full(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {TRACE_IT(64677);
            int32 nLeft = TaggedInt::Is(aLeft) ? TaggedInt::ToInt32(aLeft) : JavascriptConversion::ToInt32(aLeft, scriptContext);
            int32 nRight = TaggedInt::Is(aRight) ? TaggedInt::ToInt32(aRight) : JavascriptConversion::ToInt32(aRight, scriptContext);

            return JavascriptNumber::ToVar(nLeft ^ nRight,scriptContext);
        }

        Var JavascriptMath::Xor_InPlace(Var aLeft, Var aRight, ScriptContext* scriptContext,  JavascriptNumber* result)
        {TRACE_IT(64678);
            int32 nLeft = TaggedInt::Is(aLeft) ? TaggedInt::ToInt32(aLeft) : JavascriptConversion::ToInt32(aLeft, scriptContext);
            int32 nRight = TaggedInt::Is(aRight) ? TaggedInt::ToInt32(aRight) : JavascriptConversion::ToInt32(aRight, scriptContext);

            return JavascriptNumber::ToVarInPlace(nLeft ^ nRight, scriptContext, result);
        }

        Var JavascriptMath::ShiftLeft_Full(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {TRACE_IT(64679);
            int32 nValue    = JavascriptConversion::ToInt32(aLeft, scriptContext);
            uint32 nShift   = JavascriptConversion::ToUInt32(aRight, scriptContext);
            int32 nResult   = nValue << (nShift & 0x1F);

            return JavascriptNumber::ToVar(nResult,scriptContext);
        }

        Var JavascriptMath::ShiftRight_Full(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {TRACE_IT(64680);
            int32 nValue    = JavascriptConversion::ToInt32(aLeft, scriptContext);
            uint32 nShift   = JavascriptConversion::ToUInt32(aRight, scriptContext);

            int32 nResult   = nValue >> (nShift & 0x1F);

            return JavascriptNumber::ToVar(nResult,scriptContext);
        }

        Var JavascriptMath::ShiftRightU_Full(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {TRACE_IT(64681);
            uint32 nValue   = JavascriptConversion::ToUInt32(aLeft, scriptContext);
            uint32 nShift   = JavascriptConversion::ToUInt32(aRight, scriptContext);

            uint32 nResult  = nValue >> (nShift & 0x1F);

            return JavascriptNumber::ToVar(nResult,scriptContext);
        }

#if FLOATVAR
        Var JavascriptMath::Add_Full(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {TRACE_IT(64682);
            Assert(aLeft != nullptr);
            Assert(aRight != nullptr);
            Assert(scriptContext != nullptr);

            Js::TypeId typeLeft = JavascriptOperators::GetTypeId(aLeft);
            Js::TypeId typeRight = JavascriptOperators::GetTypeId(aRight);

            if (typeRight == typeLeft)
            {TRACE_IT(64683);
                // If both sides are numbers/string, then we can do the addition directly
                if(typeLeft == TypeIds_Number)
                {TRACE_IT(64684);
                    double sum = JavascriptNumber::GetValue(aLeft) + JavascriptNumber::GetValue(aRight);
                    return JavascriptNumber::ToVarNoCheck(sum, scriptContext);
                }
                else if (typeLeft == TypeIds_Integer)
                {TRACE_IT(64685);
                    __int64 sum = TaggedInt::ToInt64(aLeft) + TaggedInt::ToInt64(aRight);
                    return JavascriptNumber::ToVar(sum, scriptContext);
                }
                else if (typeLeft == TypeIds_String)
                {TRACE_IT(64686);
                    return JavascriptString::Concat(JavascriptString::FromVar(aLeft), JavascriptString::FromVar(aRight));
                }
            }
            else if(typeLeft == TypeIds_Number && typeRight == TypeIds_Integer)
            {TRACE_IT(64687);
                double sum = JavascriptNumber::GetValue(aLeft) + TaggedInt::ToDouble(aRight);
                return JavascriptNumber::ToVarNoCheck(sum, scriptContext);
            }
            else if(typeLeft == TypeIds_Integer && typeRight == TypeIds_Number)
            {TRACE_IT(64688);
                double sum = TaggedInt::ToDouble(aLeft) + JavascriptNumber::GetValue(aRight);
                return JavascriptNumber::ToVarNoCheck(sum, scriptContext);
            }

            return Add_FullHelper_Wrapper(aLeft, aRight, scriptContext, nullptr, false);
         }
#else
        Var JavascriptMath::Add_Full(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {TRACE_IT(64689);
            Assert(aLeft != nullptr);
            Assert(aRight != nullptr);
            Assert(scriptContext != nullptr);

            Js::TypeId typeLeft = JavascriptOperators::GetTypeId(aLeft);
            Js::TypeId typeRight = JavascriptOperators::GetTypeId(aRight);

            // Handle combinations of TaggedInt and Number or String pairs directly,
            // otherwise call the helper.
            switch( typeLeft )
            {
                case TypeIds_Integer:
                {TRACE_IT(64690);
                    switch( typeRight )
                    {
                        case TypeIds_Integer:
                        {TRACE_IT(64691);

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
                        {TRACE_IT(64692);
                            double sum = TaggedInt::ToDouble(aLeft) + JavascriptNumber::GetValue(aRight);
                            return JavascriptNumber::NewInlined( sum, scriptContext );
                        }
                    }
                    break;
                }

                case TypeIds_Number:
                {TRACE_IT(64693);
                    switch( typeRight )
                    {
                        case TypeIds_Integer:
                        {TRACE_IT(64694);
                            double sum = JavascriptNumber::GetValue(aLeft) + TaggedInt::ToDouble(aRight);
                            return JavascriptNumber::NewInlined( sum, scriptContext );
                        }

                        case TypeIds_Number:
                        {TRACE_IT(64695);
                            double sum = JavascriptNumber::GetValue(aLeft) + JavascriptNumber::GetValue(aRight);
                            return JavascriptNumber::NewInlined( sum, scriptContext );
                        }
                    }
                    break;
                }

                case TypeIds_String:
                {TRACE_IT(64696);
                    if( typeRight == TypeIds_String )
                    {TRACE_IT(64697);
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
        {TRACE_IT(64698);
            Assert(aLeft != nullptr);
            Assert(aRight != nullptr);
            Assert(scriptContext != nullptr);
            Assert(result != nullptr);

            // If both sides are numbers, then we can do the addition directly, otherwise
            // we need to call the helper.
            if( TaggedInt::Is(aLeft) )
            {TRACE_IT(64699);
                if( TaggedInt::Is(aRight) )
                {TRACE_IT(64700);
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
                {TRACE_IT(64701);
                    double sum = TaggedInt::ToDouble(aLeft) + JavascriptNumber::GetValue(aRight);
                    return JavascriptNumber::InPlaceNew( sum, scriptContext, result );
                }
            }
            else if( TaggedInt::Is(aRight) )
            {TRACE_IT(64702);
                if( JavascriptNumber::Is_NoTaggedIntCheck(aLeft) )
                {TRACE_IT(64703);
                    double sum = JavascriptNumber::GetValue(aLeft) + TaggedInt::ToDouble(aRight);
                    return JavascriptNumber::InPlaceNew( sum, scriptContext, result );
                }
            }
            else if( JavascriptNumber::Is_NoTaggedIntCheck(aLeft) && JavascriptNumber::Is_NoTaggedIntCheck(aRight) )
            {TRACE_IT(64704);
                double sum = JavascriptNumber::GetValue(aLeft) + JavascriptNumber::GetValue(aRight);
                return JavascriptNumber::InPlaceNew( sum, scriptContext, result );
            }

            return Add_FullHelper_Wrapper(aLeft, aRight, scriptContext, result, false);
        }

        Var JavascriptMath::AddLeftDead(Var aLeft, Var aRight, ScriptContext* scriptContext, JavascriptNumber *result)
        {TRACE_IT(64705);
            if (JavascriptOperators::GetTypeId(aLeft) == TypeIds_String)
            {TRACE_IT(64706);
                JavascriptString* leftString = JavascriptString::FromVar(aLeft);
                JavascriptString* rightString;
                TypeId rightType = JavascriptOperators::GetTypeId(aRight);
                switch(rightType)
                {
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
            {TRACE_IT(64707);
                if (TaggedInt::Is(aRight))
                {TRACE_IT(64708);
                    return TaggedInt::Add(aLeft, aRight, scriptContext);
                }
                else if (JavascriptNumber::Is_NoTaggedIntCheck(aRight))
                {TRACE_IT(64709);
                    return JavascriptNumber::ToVarMaybeInPlace(TaggedInt::ToDouble(aLeft) + JavascriptNumber::GetValue(aRight), scriptContext, result);
                }
            }
            else if (TaggedInt::Is(aRight))
            {TRACE_IT(64710);
                if (JavascriptNumber::Is_NoTaggedIntCheck(aLeft))
                {TRACE_IT(64711);
                    return JavascriptNumber::ToVarMaybeInPlace(JavascriptNumber::GetValue(aLeft) + TaggedInt::ToDouble(aRight), scriptContext, result);
                }
            }
            else if (JavascriptNumber::Is_NoTaggedIntCheck(aLeft) && JavascriptNumber::Is_NoTaggedIntCheck(aRight))
            {TRACE_IT(64712);
                return JavascriptNumber::ToVarMaybeInPlace(JavascriptNumber::GetValue(aLeft) + JavascriptNumber::GetValue(aRight), scriptContext, result);
            }
            return Add_FullHelper_Wrapper(aLeft, aRight, scriptContext, result, true);
        }

        Var JavascriptMath::Add_FullHelper_Wrapper(Var aLeft, Var aRight, ScriptContext* scriptContext, JavascriptNumber* result, bool leftIsDead)
        {TRACE_IT(64713);
            Var aLeftToPrim = JavascriptConversion::ToPrimitive(aLeft, JavascriptHint::None, scriptContext);
            Var aRightToPrim = JavascriptConversion::ToPrimitive(aRight, JavascriptHint::None, scriptContext);
            return Add_FullHelper(aLeftToPrim, aRightToPrim, scriptContext, result, leftIsDead);
        }

        Var JavascriptMath::Add_FullHelper(Var primLeft, Var primRight, ScriptContext* scriptContext, JavascriptNumber *result, bool leftIsDead)
        {TRACE_IT(64714);
            // If either side is a string, then the result is also a string
            if (JavascriptOperators::GetTypeId(primLeft) == TypeIds_String)
            {TRACE_IT(64715);
                JavascriptString* stringLeft = JavascriptString::FromVar(primLeft);
                JavascriptString* stringRight = nullptr;

                if (JavascriptOperators::GetTypeId(primRight) == TypeIds_String)
                {TRACE_IT(64716);
                    stringRight = JavascriptString::FromVar(primRight);
                }
                else
                {TRACE_IT(64717);
                    stringRight = JavascriptConversion::ToString(primRight, scriptContext);
                }

                if(leftIsDead)
                {TRACE_IT(64718);
                    return stringLeft->ConcatDestructive(stringRight);
                }
                return JavascriptString::Concat(stringLeft, stringRight);
            }

            if (JavascriptOperators::GetTypeId(primRight) == TypeIds_String)
            {TRACE_IT(64719);
                JavascriptString* stringLeft = JavascriptConversion::ToString(primLeft, scriptContext);
                JavascriptString* stringRight = JavascriptString::FromVar(primRight);

                if(leftIsDead)
                {TRACE_IT(64720);
                    return stringLeft->ConcatDestructive(stringRight);
                }
                return JavascriptString::Concat(stringLeft, stringRight);
            }

            double sum = Add_Helper(primLeft, primRight, scriptContext);
            return JavascriptNumber::ToVarMaybeInPlace(sum, scriptContext, result);
        }

        Var JavascriptMath::MulAddLeft(Var mulLeft, Var mulRight, Var addLeft, ScriptContext* scriptContext,  JavascriptNumber* result)
        {TRACE_IT(64721);
            if(TaggedInt::Is(mulLeft))
            {TRACE_IT(64722);
                if(TaggedInt::Is(mulRight))
                {TRACE_IT(64723);
                    // Compute the sum using integer addition, then convert to double.
                    // That way there's only one int->float conversion.
                    JavascriptNumber mulTemp(0, scriptContext->GetLibrary()->GetNumberTypeStatic());
                    Var mulResult = TaggedInt::MultiplyInPlace(mulLeft, mulRight, scriptContext, &mulTemp);

                    if (result)
                    {TRACE_IT(64724);
                        return JavascriptMath::Add_InPlace(addLeft, mulResult, scriptContext, result);
                    }
                    else
                    {TRACE_IT(64725);
                        return JavascriptMath::Add_Full(addLeft, mulResult, scriptContext);
                    }
                }
                else if(JavascriptNumber::Is_NoTaggedIntCheck(mulRight))
                {TRACE_IT(64726);
                    double mulResult = TaggedInt::ToDouble(mulLeft) * JavascriptNumber::GetValue(mulRight);

                    return JavascriptMath::Add_DoubleHelper(addLeft, mulResult, scriptContext, result);
                }
            }
            else if(TaggedInt::Is(mulRight))
            {TRACE_IT(64727);
                if(JavascriptNumber::Is_NoTaggedIntCheck(mulLeft))
                {TRACE_IT(64728);
                    double mulResult = JavascriptNumber::GetValue(mulLeft) * TaggedInt::ToDouble(mulRight);

                    return JavascriptMath::Add_DoubleHelper(addLeft, mulResult, scriptContext, result);
                }
            }
            else if(JavascriptNumber::Is_NoTaggedIntCheck(mulLeft) && JavascriptNumber::Is_NoTaggedIntCheck(mulRight))
            {TRACE_IT(64729);
                double mulResult = JavascriptNumber::GetValue(mulLeft) * JavascriptNumber::GetValue(mulRight);

                return JavascriptMath::Add_DoubleHelper(addLeft, mulResult, scriptContext, result);
            }

            Var aMul;
            JavascriptNumber mulTemp(0, scriptContext->GetLibrary()->GetNumberTypeStatic());
            aMul = JavascriptMath::Multiply_InPlace(mulLeft, mulRight, scriptContext, &mulTemp);
            if (result)
            {TRACE_IT(64730);
                return JavascriptMath::Add_InPlace(addLeft, aMul, scriptContext, result);
            }
            else
            {TRACE_IT(64731);
                return JavascriptMath::Add_Full(addLeft, aMul, scriptContext);
            }
        }

        Var JavascriptMath::MulAddRight(Var mulLeft, Var mulRight, Var addRight, ScriptContext* scriptContext, JavascriptNumber* result)
        {TRACE_IT(64732);
            if(TaggedInt::Is(mulLeft))
            {TRACE_IT(64733);
                if(TaggedInt::Is(mulRight))
                {TRACE_IT(64734);
                    // Compute the sum using integer addition, then convert to double.
                    // That way there's only one int->float conversion.
                    JavascriptNumber mulTemp(0, scriptContext->GetLibrary()->GetNumberTypeStatic());
                    Var mulResult = TaggedInt::MultiplyInPlace(mulLeft, mulRight, scriptContext, &mulTemp);

                    if (result)
                    {TRACE_IT(64735);
                        return JavascriptMath::Add_InPlace(mulResult, addRight, scriptContext, result);
                    }
                    else
                    {TRACE_IT(64736);
                        return JavascriptMath::Add_Full(mulResult, addRight, scriptContext);
                    }
                }
                else if(JavascriptNumber::Is_NoTaggedIntCheck(mulRight))
                {TRACE_IT(64737);
                    double mulResult = TaggedInt::ToDouble(mulLeft) * JavascriptNumber::GetValue(mulRight);

                    return JavascriptMath::Add_DoubleHelper(mulResult, addRight, scriptContext, result);
                }
            }
            else if(TaggedInt::Is(mulRight))
            {TRACE_IT(64738);
                if(JavascriptNumber::Is_NoTaggedIntCheck(mulLeft))
                {TRACE_IT(64739);
                    double mulResult = JavascriptNumber::GetValue(mulLeft) * TaggedInt::ToDouble(mulRight);

                    return JavascriptMath::Add_DoubleHelper(mulResult, addRight, scriptContext, result);
                }
            }
            else if(JavascriptNumber::Is_NoTaggedIntCheck(mulLeft) && JavascriptNumber::Is_NoTaggedIntCheck(mulRight))
            {TRACE_IT(64740);
                double mulResult = JavascriptNumber::GetValue(mulLeft) * JavascriptNumber::GetValue(mulRight);

                return JavascriptMath::Add_DoubleHelper(mulResult, addRight, scriptContext, result);
            }

            Var aMul;
            JavascriptNumber mulTemp(0, scriptContext->GetLibrary()->GetNumberTypeStatic());
            aMul = JavascriptMath::Multiply_InPlace(mulLeft, mulRight, scriptContext, &mulTemp);
            if (result)
            {TRACE_IT(64741);
                return JavascriptMath::Add_InPlace(aMul, addRight, scriptContext, result);
            }
            else
            {TRACE_IT(64742);
                return JavascriptMath::Add_Full(aMul, addRight, scriptContext);
            }
        }

        Var JavascriptMath::MulSubLeft(Var mulLeft, Var mulRight, Var subLeft, ScriptContext* scriptContext, JavascriptNumber* result)
        {TRACE_IT(64743);
            if(TaggedInt::Is(mulLeft))
            {TRACE_IT(64744);
                if(TaggedInt::Is(mulRight))
                {TRACE_IT(64745);
                    // Compute the sum using integer addition, then convert to double.
                    // That way there's only one int->float conversion.
                    JavascriptNumber mulTemp(0, scriptContext->GetLibrary()->GetNumberTypeStatic());
                    Var mulResult = TaggedInt::MultiplyInPlace(mulLeft, mulRight, scriptContext, &mulTemp);

                    if (result)
                    {TRACE_IT(64746);
                        return JavascriptMath::Subtract_InPlace(subLeft, mulResult, scriptContext, result);
                    }
                    else
                    {TRACE_IT(64747);
                        return JavascriptMath::Subtract_Full(subLeft, mulResult, scriptContext);
                    }
                }
                else if(JavascriptNumber::Is_NoTaggedIntCheck(mulRight))
                {TRACE_IT(64748);
                    double mulResult = TaggedInt::ToDouble(mulLeft) * JavascriptNumber::GetValue(mulRight);

                    return JavascriptMath::Subtract_DoubleHelper(subLeft, mulResult, scriptContext, result);
                }
            }
            else if(TaggedInt::Is(mulRight))
            {TRACE_IT(64749);
                if(JavascriptNumber::Is_NoTaggedIntCheck(mulLeft))
                {TRACE_IT(64750);
                    double mulResult = JavascriptNumber::GetValue(mulLeft) * TaggedInt::ToDouble(mulRight);

                    return JavascriptMath::Subtract_DoubleHelper(subLeft, mulResult, scriptContext, result);
                }
            }
            else if(JavascriptNumber::Is_NoTaggedIntCheck(mulLeft) && JavascriptNumber::Is_NoTaggedIntCheck(mulRight))
            {TRACE_IT(64751);
                double mulResult = JavascriptNumber::GetValue(mulLeft) * JavascriptNumber::GetValue(mulRight);

                return JavascriptMath::Subtract_DoubleHelper(subLeft, mulResult, scriptContext, result);
            }

            Var aMul;
            JavascriptNumber mulTemp(0, scriptContext->GetLibrary()->GetNumberTypeStatic());
            aMul = JavascriptMath::Multiply_InPlace(mulLeft, mulRight, scriptContext, &mulTemp);
            if (result)
            {TRACE_IT(64752);
                return JavascriptMath::Subtract_InPlace(subLeft, aMul, scriptContext, result);
            }
            else
            {TRACE_IT(64753);
                return JavascriptMath::Subtract_Full(subLeft, aMul, scriptContext);
            }
        }

        Var JavascriptMath::MulSubRight(Var mulLeft, Var mulRight, Var subRight, ScriptContext* scriptContext, JavascriptNumber* result)
        {TRACE_IT(64754);
            if(TaggedInt::Is(mulLeft))
            {TRACE_IT(64755);
                if(TaggedInt::Is(mulRight))
                {TRACE_IT(64756);
                    // Compute the sum using integer addition, then convert to double.
                    // That way there's only one int->float conversion.
                    JavascriptNumber mulTemp(0, scriptContext->GetLibrary()->GetNumberTypeStatic());
                    Var mulResult = TaggedInt::MultiplyInPlace(mulLeft, mulRight, scriptContext, &mulTemp);

                    if (result)
                    {TRACE_IT(64757);
                        return JavascriptMath::Subtract_InPlace(mulResult, subRight, scriptContext, result);
                    }
                    else
                    {TRACE_IT(64758);
                        return JavascriptMath::Subtract_Full(mulResult, subRight, scriptContext);
                    }
                }
                else if(JavascriptNumber::Is_NoTaggedIntCheck(mulRight))
                {TRACE_IT(64759);
                    double mulResult = TaggedInt::ToDouble(mulLeft) * JavascriptNumber::GetValue(mulRight);

                    return JavascriptMath::Subtract_DoubleHelper(mulResult, subRight, scriptContext, result);
                }
            }
            else if(TaggedInt::Is(mulRight))
            {TRACE_IT(64760);
                if(JavascriptNumber::Is_NoTaggedIntCheck(mulLeft))
                {TRACE_IT(64761);
                    double mulResult = JavascriptNumber::GetValue(mulLeft) * TaggedInt::ToDouble(mulRight);

                    return JavascriptMath::Subtract_DoubleHelper(mulResult, subRight, scriptContext, result);
                }
            }
            else if(JavascriptNumber::Is_NoTaggedIntCheck(mulLeft) && JavascriptNumber::Is_NoTaggedIntCheck(mulRight))
            {TRACE_IT(64762);
                double mulResult = JavascriptNumber::GetValue(mulLeft) * JavascriptNumber::GetValue(mulRight);

                return JavascriptMath::Subtract_DoubleHelper(mulResult, subRight, scriptContext, result);
            }

            Var aMul;
            JavascriptNumber mulTemp(0, scriptContext->GetLibrary()->GetNumberTypeStatic());
            aMul = JavascriptMath::Multiply_InPlace(mulLeft, mulRight, scriptContext, &mulTemp);
            if (result)
            {TRACE_IT(64763);
                return JavascriptMath::Subtract_InPlace(aMul, subRight, scriptContext, result);
            }
            else
            {TRACE_IT(64764);
                return JavascriptMath::Subtract_Full(aMul, subRight, scriptContext);
            }
        }

        Var inline JavascriptMath::Add_DoubleHelper(double dblLeft, Var addRight, ScriptContext* scriptContext, JavascriptNumber*result)
        {TRACE_IT(64765);
            if (TaggedInt::Is(addRight))
            {TRACE_IT(64766);
                double sum =  dblLeft + TaggedInt::ToDouble(addRight);

                return JavascriptNumber::ToVarMaybeInPlace(sum, scriptContext, result);
            }
            else if (JavascriptNumber::Is_NoTaggedIntCheck(addRight))
            {TRACE_IT(64767);
                double sum = dblLeft + JavascriptNumber::GetValue(addRight);

                return JavascriptNumber::ToVarMaybeInPlace(sum, scriptContext, result);
            }
            else
            {TRACE_IT(64768);
                Var aLeft = JavascriptNumber::ToVarMaybeInPlace(dblLeft, scriptContext, result);

                return Add_Full(aLeft, addRight, scriptContext);
            }
        }

        Var inline JavascriptMath::Add_DoubleHelper(Var addLeft, double dblRight, ScriptContext* scriptContext, JavascriptNumber*result)
        {TRACE_IT(64769);
            if (TaggedInt::Is(addLeft))
            {TRACE_IT(64770);
                double sum =  TaggedInt::ToDouble(addLeft) + dblRight;

                return JavascriptNumber::ToVarMaybeInPlace(sum, scriptContext, result);
            }
            else if (JavascriptNumber::Is_NoTaggedIntCheck(addLeft))
            {TRACE_IT(64771);
                double sum = JavascriptNumber::GetValue(addLeft) + dblRight;

                return JavascriptNumber::ToVarMaybeInPlace(sum, scriptContext, result);
            }
            else
            {TRACE_IT(64772);
                Var aRight = JavascriptNumber::ToVarMaybeInPlace(dblRight, scriptContext, result);

                return Add_Full(addLeft, aRight, scriptContext);
            }
        }

        Var inline JavascriptMath::Subtract_DoubleHelper(double dblLeft, Var subRight, ScriptContext* scriptContext, JavascriptNumber* result)
        {TRACE_IT(64773);
            if (TaggedInt::Is(subRight))
            {TRACE_IT(64774);
                double sum =  dblLeft - TaggedInt::ToDouble(subRight);

                return JavascriptNumber::ToVarMaybeInPlace(sum, scriptContext, result);
            }
            else if (JavascriptNumber::Is_NoTaggedIntCheck(subRight))
            {TRACE_IT(64775);
                double sum = dblLeft - JavascriptNumber::GetValue(subRight);

                return JavascriptNumber::ToVarMaybeInPlace(sum, scriptContext, result);
            }
            else
            {TRACE_IT(64776);
                Var aLeft = JavascriptNumber::ToVarMaybeInPlace(dblLeft, scriptContext, result);

                return Subtract_Full(aLeft, subRight, scriptContext);
            }
        }

        Var inline JavascriptMath::Subtract_DoubleHelper(Var subLeft, double dblRight, ScriptContext* scriptContext, JavascriptNumber*result)
        {TRACE_IT(64777);
            if (TaggedInt::Is(subLeft))
            {TRACE_IT(64778);
                double sum =  TaggedInt::ToDouble(subLeft) - dblRight;

                return JavascriptNumber::ToVarMaybeInPlace(sum, scriptContext, result);
            }
            else if (JavascriptNumber::Is_NoTaggedIntCheck(subLeft))
            {TRACE_IT(64779);
                double sum = JavascriptNumber::GetValue(subLeft) - dblRight;

                return JavascriptNumber::ToVarMaybeInPlace(sum, scriptContext, result);
            }
            else
            {TRACE_IT(64780);
                Var aRight = JavascriptNumber::ToVarMaybeInPlace(dblRight, scriptContext, result);

                return Subtract_Full(subLeft, aRight, scriptContext);
            }
        }

        Var JavascriptMath::Subtract_Full(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {TRACE_IT(64781);
            double difference = Subtract_Helper(aLeft, aRight, scriptContext);
            return JavascriptNumber::ToVarNoCheck(difference, scriptContext);
        }

        Var JavascriptMath::Subtract_InPlace(Var aLeft, Var aRight, ScriptContext* scriptContext, JavascriptNumber* result)
        {TRACE_IT(64782);
            double difference = Subtract_Helper(aLeft, aRight, scriptContext);
            return JavascriptNumber::InPlaceNew(difference, scriptContext, result);
        }

        Var JavascriptMath::Divide_Full(Var aLeft,Var aRight, ScriptContext* scriptContext)
        {TRACE_IT(64783);
            // If both arguments are TaggedInt, then try to do integer division
            // This case is not handled by the lowerer.
            if (TaggedInt::IsPair(aLeft, aRight))
            {TRACE_IT(64784);
                return TaggedInt::Divide(aLeft, aRight, scriptContext);
            }

            return JavascriptNumber::NewInlined( Divide_Helper(aLeft, aRight, scriptContext), scriptContext );
        }

        Var JavascriptMath::Exponentiation_Full(Var aLeft, Var aRight, ScriptContext *scriptContext)
        {TRACE_IT(64785);
            double x = JavascriptConversion::ToNumber(aLeft, scriptContext);
            double y = JavascriptConversion::ToNumber(aRight, scriptContext);
            return JavascriptNumber::ToVarNoCheck(Math::Pow(x, y), scriptContext);
        }

        Var JavascriptMath::Exponentiation_InPlace(Var aLeft, Var aRight, ScriptContext* scriptContext, JavascriptNumber* result)
        {TRACE_IT(64786);
            // The IEEE 754 floating point spec ensures that NaNs are preserved in all operations
            double dblLeft = JavascriptConversion::ToNumber(aLeft, scriptContext);
            double dblRight = JavascriptConversion::ToNumber(aRight, scriptContext);

            return JavascriptNumber::InPlaceNew(Math::Pow(dblLeft, dblRight), scriptContext, result);
        }

        Var JavascriptMath::Multiply_Full(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {TRACE_IT(64787);
            Assert(aLeft != nullptr);
            Assert(aRight != nullptr);
            Assert(scriptContext != nullptr);

            if(JavascriptNumber::Is(aLeft))
            {TRACE_IT(64788);
                if(JavascriptNumber::Is(aRight))
                {TRACE_IT(64789);
                    double product = JavascriptNumber::GetValue(aLeft) * JavascriptNumber::GetValue(aRight);
                    return JavascriptNumber::ToVarNoCheck(product, scriptContext);
                }
                else if(TaggedInt::Is(aRight))
                {TRACE_IT(64790);
                    double product = TaggedInt::ToDouble(aRight) * JavascriptNumber::GetValue(aLeft);
                    return JavascriptNumber::ToVarNoCheck(product, scriptContext);
                }
            }
            else if(JavascriptNumber::Is(aRight))
            {TRACE_IT(64791);
                if(TaggedInt::Is(aLeft))
                {TRACE_IT(64792);
                    double product = TaggedInt::ToDouble(aLeft) * JavascriptNumber::GetValue(aRight);
                    return JavascriptNumber::ToVarNoCheck(product, scriptContext);
                }
            }
            else if(TaggedInt::IsPair(aLeft, aRight))
            {TRACE_IT(64793);
                return TaggedInt::Multiply(aLeft, aRight, scriptContext);
            }
            double product = Multiply_Helper(aLeft, aRight, scriptContext);
            return JavascriptNumber::ToVarNoCheck(product, scriptContext);
        }

        Var JavascriptMath::Multiply_InPlace(Var aLeft, Var aRight, ScriptContext* scriptContext, JavascriptNumber* result)
        {TRACE_IT(64794);
            if(JavascriptNumber::Is(aLeft))
            {TRACE_IT(64795);
                if(JavascriptNumber::Is(aRight))
                {TRACE_IT(64796);
                    return JavascriptNumber::ToVarInPlace(
                        JavascriptNumber::GetValue(aLeft) * JavascriptNumber::GetValue(aRight), scriptContext, result);
                }
                else if (TaggedInt::Is(aRight))
                {TRACE_IT(64797);
                    return JavascriptNumber::ToVarInPlace(
                        JavascriptNumber::GetValue(aLeft) * TaggedInt::ToDouble(aRight), scriptContext, result);
                }
            }
            else if(JavascriptNumber::Is(aRight))
            {TRACE_IT(64798);
                if(TaggedInt::Is(aLeft))
                {TRACE_IT(64799);
                    return JavascriptNumber::ToVarInPlace(
                        TaggedInt::ToDouble(aLeft) * JavascriptNumber::GetValue(aRight), scriptContext, result);
                }
            }
            else if(TaggedInt::IsPair(aLeft, aRight))
            {TRACE_IT(64800);
                return TaggedInt::MultiplyInPlace(aLeft, aRight, scriptContext, result);
            }

            double product = Multiply_Helper(aLeft, aRight, scriptContext);
            return JavascriptNumber::InPlaceNew(product, scriptContext, result);
        }

        Var JavascriptMath::Divide_InPlace(Var aLeft, Var aRight, ScriptContext* scriptContext, JavascriptNumber* result)
        {TRACE_IT(64801);
            // If both arguments are TaggedInt, then try to do integer division
            // This case is not handled by the lowerer.
            if (TaggedInt::IsPair(aLeft, aRight))
            {TRACE_IT(64802);
                return TaggedInt::DivideInPlace(aLeft, aRight, scriptContext, result);
            }

            double quotient = Divide_Helper(aLeft, aRight, scriptContext);
            return JavascriptNumber::InPlaceNew(quotient, scriptContext, result);
        }

        Var JavascriptMath::Modulus_Full(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {TRACE_IT(64803);
            // If both arguments are TaggedInt, then try to do integer modulus.
            // This case is not handled by the lowerer.
            if (TaggedInt::IsPair(aLeft, aRight))
            {TRACE_IT(64804);
                return TaggedInt::Modulus(aLeft, aRight, scriptContext);
            }

            double remainder = Modulus_Helper(aLeft, aRight, scriptContext);
            return JavascriptNumber::ToVarNoCheck(remainder, scriptContext);
        }

        Var JavascriptMath::Modulus_InPlace(Var aLeft, Var aRight, ScriptContext* scriptContext, JavascriptNumber* result)
        {TRACE_IT(64805);
            Assert(aLeft != nullptr);
            Assert(aRight != nullptr);
            Assert(scriptContext != nullptr);

            // If both arguments are TaggedInt, then try to do integer division
            // This case is not handled by the lowerer.
            if (TaggedInt::IsPair(aLeft, aRight))
            {TRACE_IT(64806);
                return TaggedInt::Modulus(aLeft, aRight, scriptContext);
            }

            double remainder = Modulus_Helper(aLeft, aRight, scriptContext);
            return JavascriptNumber::InPlaceNew(remainder, scriptContext, result);
        }


        Var JavascriptMath::FinishOddDivByPow2(int32 value, ScriptContext *scriptContext)
        {TRACE_IT(64807);
            return JavascriptNumber::New((double)(value + 0.5), scriptContext);
        }

        Var JavascriptMath::FinishOddDivByPow2_InPlace(int32 value, ScriptContext *scriptContext, JavascriptNumber* result)
        {TRACE_IT(64808);
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
            {TRACE_IT(64809);
                if (JavascriptArray::IsVarArray(typeId) && JavascriptArray::FromVar(arrayArg)->GetLength() == 0)
                {TRACE_IT(64810);
                    return scriptContext->GetLibrary()->GetNegativeInfinite();
                }
                return JavascriptFunction::CalloutHelper<false>(function, thisArg, /* overridingNewTarget = */nullptr, arrayArg, scriptContext);
            }

            if (JavascriptNativeArray::Is(typeId))
            {TRACE_IT(64811);
#if ENABLE_COPYONACCESS_ARRAY
                JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(arrayArg);
#endif
                JavascriptNativeArray * argsArray = JavascriptNativeArray::FromVar(arrayArg);
                uint len = argsArray->GetLength();
                if (len == 0)
                {TRACE_IT(64812);
                    return scriptContext->GetLibrary()->GetNegativeInfinite();
                }

                if (argsArray->GetHead()->next != nullptr || !argsArray->HasNoMissingValues() ||
                    argsArray->GetHead()->length != len)
                {TRACE_IT(64813);
                    return JavascriptFunction::CalloutHelper<false>(function, thisArg, /* overridingNewTarget = */nullptr, arrayArg, scriptContext);
                }

                return argsArray->FindMinOrMax(scriptContext, true /*findMax*/);
            }
            else
            {TRACE_IT(64814);
                TypedArrayBase * argsArray = TypedArrayBase::FromVar(arrayArg);
                uint len = argsArray->GetLength();
                if (len == 0)
                {TRACE_IT(64815);
                    return scriptContext->GetLibrary()->GetNegativeInfinite();
                }
                Var max = argsArray->FindMinOrMax(scriptContext, typeId, true /*findMax*/);
                if (max == nullptr)
                {TRACE_IT(64816);
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
            {TRACE_IT(64817);
                if (JavascriptArray::Is(typeId) && JavascriptArray::FromVar(arrayArg)->GetLength() == 0)
                {TRACE_IT(64818);
                    return scriptContext->GetLibrary()->GetPositiveInfinite();
                }
                return JavascriptFunction::CalloutHelper<false>(function, thisArg, /* overridingNewTarget = */nullptr, arrayArg, scriptContext);
            }

            if (JavascriptNativeArray::Is(typeId))
            {TRACE_IT(64819);
#if ENABLE_COPYONACCESS_ARRAY
                JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(arrayArg);
#endif
                JavascriptNativeArray * argsArray = JavascriptNativeArray::FromVar(arrayArg);
                uint len = argsArray->GetLength();
                if (len == 0)
                {TRACE_IT(64820);
                    return scriptContext->GetLibrary()->GetPositiveInfinite();
                }

                if (argsArray->GetHead()->next != nullptr || !argsArray->HasNoMissingValues() ||
                    argsArray->GetHead()->length != len)
                {TRACE_IT(64821);
                    return JavascriptFunction::CalloutHelper<false>(function, thisArg, /* overridingNewTarget = */nullptr, arrayArg, scriptContext);
                }

                return argsArray->FindMinOrMax(scriptContext, false /*findMax*/);
            }
            else
            {TRACE_IT(64822);
                TypedArrayBase * argsArray = TypedArrayBase::FromVar(arrayArg);
                uint len = argsArray->GetLength();
                if (len == 0)
                {TRACE_IT(64823);
                    return scriptContext->GetLibrary()->GetPositiveInfinite();
                }
                Var min = argsArray->FindMinOrMax(scriptContext, typeId, false /*findMax*/);
                if (min == nullptr)
                {TRACE_IT(64824);
                    return JavascriptFunction::CalloutHelper<false>(function, thisArg, /* overridingNewTarget = */nullptr, arrayArg, scriptContext);
                }
                return min;
            }
        }

        void InitializeRandomSeeds(uint64 *seed0, uint64 *seed1, ScriptContext *scriptContext)
        {TRACE_IT(64825);
#if DBG
            if (CONFIG_FLAG(PRNGSeed0) && CONFIG_FLAG(PRNGSeed1))
            {TRACE_IT(64826);
                *seed0 = CONFIG_FLAG(PRNGSeed0);
                *seed1 = CONFIG_FLAG(PRNGSeed1);
            }
            else
#endif
            {TRACE_IT(64827);
                LARGE_INTEGER s0;
                LARGE_INTEGER s1;

                if (!rand_s(reinterpret_cast<unsigned int*>(&s0.LowPart)) &&
                    !rand_s(reinterpret_cast<unsigned int*>(&s0.HighPart)) &&
                    !rand_s(reinterpret_cast<unsigned int*>(&s1.LowPart)) &&
                    !rand_s(reinterpret_cast<unsigned int*>(&s1.HighPart)))
                {TRACE_IT(64828);
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
        {TRACE_IT(64829);
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
        {TRACE_IT(64830);
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
        {TRACE_IT(64831);
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
                {TRACE_IT(64832);
                    scriptContext->GetThreadContext()->TTDLog->ReplayExternalEntropyRandomEvent(&seed0, &seed1);
                }
                else if(scriptContext->ShouldPerformRecordAction())
                {TRACE_IT(64833);
                    scriptContext->GetThreadContext()->TTDLog->RecordExternalEntropyRandomEvent(seed0, seed1);
                }
                else
                {TRACE_IT(64834);
                    ;
                }
#endif
            }
            else
            {TRACE_IT(64835);
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
        {TRACE_IT(64836);
            // Same as doing ToInt32 and reinterpret the bits as uint32
            return (uint32)ToInt32Core(T1);
        }

        int32 JavascriptMath::ToInt32(double T1)
        {TRACE_IT(64837);
            return JavascriptMath::ToInt32Core(T1);
        }

        int32 JavascriptMath::ToInt32_Full(Var aValue, ScriptContext* scriptContext)
        {TRACE_IT(64838);
            AssertMsg(!TaggedInt::Is(aValue), "Should be detected");

            // This is used when TaggedInt's overflow but remain under int32
            // so Number is our most critical case:

            TypeId typeId = JavascriptOperators::GetTypeId(aValue);

            if (typeId == TypeIds_Number)
            {TRACE_IT(64839);
                return JavascriptMath::ToInt32Core(JavascriptNumber::GetValue(aValue));
            }

            return JavascriptConversion::ToInt32_Full(aValue, scriptContext);
        }
#ifdef SSE2MATH
      }
#endif
}
