//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#pragma warning(push)
#pragma warning(disable:6200) // C6200: Index is out of valid index range, compiler complains here we use variable length array

namespace Js
{
    template<class T, typename CountT = typename T::CounterFields>
    struct CompactCounters
    {
        struct Fields {
            union {
                uint8 u8Fields[static_cast<size_t>(CountT::Max)];
                int8 i8Fields[static_cast<size_t>(CountT::Max)];
                uint16 u16Fields[static_cast<size_t>(CountT::Max)];
                int16 i16Fields[static_cast<size_t>(CountT::Max)];
                uint32 u32Fields[static_cast<size_t>(CountT::Max)];
                int32 i32Fields[static_cast<size_t>(CountT::Max)];
            };
            Fields() {TRACE_IT(33506);}
        };

        FieldWithBarrier(uint8) fieldSize;
#if DBG

        mutable FieldWithBarrier(bool) bgThreadCallStarted;
        FieldWithBarrier(bool) isCleaningUp;
#endif
        typename FieldWithBarrier(Fields*) fields;

        CompactCounters() {TRACE_IT(33507); }
        CompactCounters(T* host)
            :fieldSize(0)
#if DBG
            , bgThreadCallStarted(false), isCleaningUp(false)
#endif
        {
            AllocCounters<uint8>(host);
        }

        uint32 Get(CountT typeEnum) const
        {TRACE_IT(33508);
#if DBG
            if (!bgThreadCallStarted && ThreadContext::GetContextForCurrentThread() == nullptr)
            {TRACE_IT(33509);
                bgThreadCallStarted = true;
            }
#endif
            uint8 type = static_cast<uint8>(typeEnum);
            uint8 localFieldSize = fieldSize;
            uint32 value = 0;
            if (localFieldSize == 1)
            {TRACE_IT(33510);
                value = this->fields->u8Fields[type];
            }
            else if (localFieldSize == 2)
            {TRACE_IT(33511);
                value = this->fields->u16Fields[type];
            }
            else if (localFieldSize == 4)
            {TRACE_IT(33512);
                value = this->fields->u32Fields[type];
            }
            else
            {TRACE_IT(33513);
                Assert(localFieldSize == 0 && this->isCleaningUp && this->fields == nullptr); // OOM when initial allocation failed
            }

            return value;
        }

        uint32 Set(CountT typeEnum, uint32 val, T* host)
        {TRACE_IT(33514);
            Assert(bgThreadCallStarted == false || isCleaningUp == true
                || host->GetScriptContext()->GetThreadContext()->GetEtwRundownCriticalSection()->IsLockedByAnyThread());

            uint8 type = static_cast<uint8>(typeEnum);
            if (fieldSize == 1)
            {TRACE_IT(33515);
                if (val <= UINT8_MAX)
                {TRACE_IT(33516);
                    return this->fields->u8Fields[type] = static_cast<uint8>(val);
                }
                else
                {TRACE_IT(33517);
                    (val <= UINT16_MAX) ? AllocCounters<uint16>(host) : AllocCounters<uint32>(host);
                    return host->counters.Set(typeEnum, val, host);
                }
            }

            if (fieldSize == 2)
            {TRACE_IT(33518);
                if (val <= UINT16_MAX)
                {TRACE_IT(33519);
                    return this->fields->u16Fields[type] = static_cast<uint16>(val);
                }
                else
                {TRACE_IT(33520);
                    AllocCounters<uint32>(host);
                    return host->counters.Set(typeEnum, val, host);
                }
            }

            if (fieldSize == 4)
            {TRACE_IT(33521);
                return this->fields->u32Fields[type] = val;
            }

            Assert(fieldSize == 0 && this->isCleaningUp && this->fields == nullptr && val == 0); // OOM when allocating the counters structure
            return val;
        }

        uint32 Increase(CountT typeEnum, T* host)
        {TRACE_IT(33522);
            Assert(bgThreadCallStarted == false);

            uint8 type = static_cast<uint8>(typeEnum);
            if (fieldSize == 1)
            {TRACE_IT(33523);
                if (this->fields->u8Fields[type] < UINT8_MAX)
                {TRACE_IT(33524);
                    return this->fields->u8Fields[type]++;
                }
                else
                {TRACE_IT(33525);
                    AllocCounters<uint16>(host);
                    return host->counters.Increase(typeEnum, host);
                }
            }

            if (fieldSize == 2)
            {TRACE_IT(33526);
                if (this->fields->u16Fields[type] < UINT16_MAX)
                {TRACE_IT(33527);
                    return this->fields->u16Fields[type]++;
                }
                else
                {TRACE_IT(33528);
                    AllocCounters<uint32>(host);
                    return host->counters.Increase(typeEnum, host);
                }
            }

            Assert(fieldSize == 4);
            return this->fields->u32Fields[type]++;
        }

        template<typename FieldT>
        void AllocCounters(T* host)
        {TRACE_IT(33529);
            Assert(ThreadContext::GetContextForCurrentThread() || ThreadContext::GetCriticalSection()->IsLocked());
            Assert(host->GetRecycler() != nullptr);

            const uint8 max = static_cast<uint8>(CountT::Max);
            typedef CompactCounters<T, CountT> CounterT;
            CounterT::Fields* fieldsArray = (CounterT::Fields*)RecyclerNewArrayLeafZ(host->GetRecycler(), FieldT, sizeof(FieldT)*max);
            CounterT::Fields* oldFieldsArray = host->counters.fields;
            uint8 i = 0;
            if (this->fieldSize == 1)
            {TRACE_IT(33530);
                if (sizeof(FieldT) == 2)
                {TRACE_IT(33531);
                    for (; i < max; i++)
                    {TRACE_IT(33532);
                        fieldsArray->u16Fields[i] = oldFieldsArray->u8Fields[i];
                    }
                }
                else if (sizeof(FieldT) == 4)
                {TRACE_IT(33533);
                    for (; i < max; i++)
                    {TRACE_IT(33534);
                        fieldsArray->u32Fields[i] = oldFieldsArray->u8Fields[i];
                    }
                }
            }
            else if (this->fieldSize == 2)
            {TRACE_IT(33535);
                for (; i < max; i++)
                {TRACE_IT(33536);
                    fieldsArray->u32Fields[i] = oldFieldsArray->u16Fields[i];
                }
            }
            else
            {TRACE_IT(33537);
                Assert(this->fieldSize==0);
            }

            if (this->fieldSize == 0)
            {TRACE_IT(33538);
                this->fieldSize = sizeof(FieldT);
                this->fields = fieldsArray;
            }
            else
            {TRACE_IT(33539);
                AutoCriticalSection autoCS(host->GetScriptContext()->GetThreadContext()->GetEtwRundownCriticalSection());
                this->fieldSize = sizeof(FieldT);
                this->fields = fieldsArray;
            }
        }

    };
}

#pragma warning(pop)
