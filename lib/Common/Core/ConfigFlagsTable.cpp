//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonCorePch.h"
#include "Memory/PageHeapBlockTypeFilter.h"

#include <initguid.h>

#undef DebugBreak

// Initialization order
//  AB AutoSystemInfo
//  AD PerfCounter
//  AE PerfCounterSet
//  AM Output/Configuration
//  AN MemProtectHeap
//  AP DbgHelpSymbolManager
//  AQ CFGLogger
//  AR LeakReport
//  AS JavascriptDispatch/RecyclerObjectDumper
//  AT HeapAllocator/RecyclerHeuristic
//  AU RecyclerWriteBarrierManager
#pragma warning(disable:4075)       // initializers put in unrecognized initialization area on purpose
#pragma init_seg(".CRT$XCAM")
namespace Js
{
    NumberSet::NumberSet() : set(&NoCheckHeapAllocator::Instance) {TRACE_IT(19713);}

    void NumberSet::Add(uint32 x)
    {TRACE_IT(19714);
        set.Item(x);
    }

    bool NumberSet::Contains(uint32 x)
    {TRACE_IT(19715);
        return set.Contains(x);
    }


    NumberPairSet::NumberPairSet() : set(&NoCheckHeapAllocator::Instance) {TRACE_IT(19716);}

    void NumberPairSet::Add(uint32 x, uint32 y)
    {TRACE_IT(19717);
        set.Item(NumberPair(x, y));
    }

    bool NumberPairSet::Contains(uint32 x, uint32 y)
    {TRACE_IT(19718);
        return set.Contains(NumberPair(x, y));
    }

    ///----------------------------------------------------------------------------
    ///----------------------------------------------------------------------------
    ///
    /// class String
    ///
    ///----------------------------------------------------------------------------
    ///----------------------------------------------------------------------------

    String::String()
    {TRACE_IT(19719);
        this->pszValue = NULL;
    }

    String::String(__in_z_opt const char16* psz)
    {TRACE_IT(19720);
        this->pszValue = NULL;
        Set(psz);
    }

    String::~String()
    {TRACE_IT(19721);
        if(NULL != this->pszValue)
        {TRACE_IT(19722);
            NoCheckHeapDeleteArray(wcslen(this->pszValue) + 1, this->pszValue);
        }
    }

    ///----------------------------------------------------------------------------
    ///
    /// String::Set
    ///
    /// Frees the existing string if any
    /// allocates a new buffer to copy the new string
    ///
    ///----------------------------------------------------------------------------

    void
    String::Set(__in_z_opt const char16* pszValue)
    {TRACE_IT(19723);
        if(NULL != this->pszValue)
        {TRACE_IT(19724);
            NoCheckHeapDeleteArray(wcslen(this->pszValue) + 1, this->pszValue);
        }

        if(NULL != pszValue)
        {TRACE_IT(19725);
            size_t size    = 1 + wcslen(pszValue);
            this->pszValue  = NoCheckHeapNewArray(char16, size);
            wcscpy_s(this->pszValue, size, pszValue);
        }
        else
        {TRACE_IT(19726);
            this->pszValue = NULL;
        }
    }

    template <>
    bool RangeUnitContains<SourceFunctionNode>(RangeUnit<SourceFunctionNode> unit, SourceFunctionNode n)
    {TRACE_IT(19727);
        Assert(n.functionId != (uint32)-1);

        if ((n.sourceContextId >= unit.i.sourceContextId) &&
            (n.sourceContextId <= unit.j.sourceContextId)
            )
        {TRACE_IT(19728);
            if ((n.sourceContextId == unit.j.sourceContextId && -2 == unit.j.functionId) ||  //#.#-#.* case
                (n.sourceContextId == unit.i.sourceContextId && -2 == unit.i.functionId)     //#.*-#.# case
                )
            {TRACE_IT(19729);
                return true;
            }

            if ((n.sourceContextId == unit.j.sourceContextId && -1 == unit.j.functionId) || //#.#-#.+ case
                (n.sourceContextId == unit.i.sourceContextId && -1 == unit.i.functionId)     //#.+-#.# case
                )
            {TRACE_IT(19730);
                return n.functionId != 0;
            }

            if ((n.sourceContextId == unit.i.sourceContextId && n.functionId < unit.i.functionId) || //excludes all values less than functionId LHS
                (n.sourceContextId == unit.j.sourceContextId && n.functionId > unit.j.functionId)) ////excludes all values greater than functionId RHS
            {TRACE_IT(19731);
                return false;
            }

            return true;
        }

        return false;
    }

    template <>
    Js::RangeUnit<Js::SourceFunctionNode> GetFullRange()
    {TRACE_IT(19732);
        RangeUnit<SourceFunctionNode> unit;
        unit.i.sourceContextId = 0;
        unit.j.sourceContextId = UINT_MAX;
        unit.i.functionId = 0;
        unit.j.functionId = (uint)-3;
        return unit;
    }

    template <>
    SourceFunctionNode GetPrevious(SourceFunctionNode unit)
    {TRACE_IT(19733);
        SourceFunctionNode prevUnit = unit;
        prevUnit.functionId--;
        if (prevUnit.functionId == UINT_MAX)
        {TRACE_IT(19734);
            prevUnit.sourceContextId--;
        }
        return prevUnit;
    }

    template <>
    SourceFunctionNode GetNext(SourceFunctionNode unit)
    {TRACE_IT(19735);
        SourceFunctionNode nextUnit = unit;
        nextUnit.functionId++;
        if (nextUnit.functionId == 0)
        {TRACE_IT(19736);
            nextUnit.sourceContextId++;
        }
        return nextUnit;
    }

    ///----------------------------------------------------------------------------
    ///----------------------------------------------------------------------------
    ///
    /// class Phases
    ///
    ///----------------------------------------------------------------------------
    ///----------------------------------------------------------------------------

    bool
    Phases::IsEnabled(Phase phase)
    {TRACE_IT(19737);
        return this->phaseList[(int)phase].valid;
    }

    bool
    Phases::IsEnabled(Phase phase, uint sourceContextId, Js::LocalFunctionId functionId)
    {TRACE_IT(19738);
        return  this->phaseList[(int)phase].valid &&
                this->phaseList[(int)phase].range.InRange(SourceFunctionNode(sourceContextId, functionId));
    }

    bool
    Phases::IsEnabledForAll(Phase phase)
    {TRACE_IT(19739);
        return  this->phaseList[(int)phase].valid &&
                this->phaseList[(int)phase].range.ContainsAll();
    }

    Range *
    Phases::GetRange(Phase phase)
    {TRACE_IT(19740);
        return &this->phaseList[(int)phase].range;
    }

    void
    Phases::Enable(Phase phase)
    {TRACE_IT(19741);
        this->phaseList[(int)phase].valid = true;
    }

    void
    Phases::Disable(Phase phase)
    {TRACE_IT(19742);
        this->phaseList[(int)phase].valid = false;
        this->phaseList[(int)phase].range.Clear();
    }

    Phase
    Phases::GetFirstPhase()
    {TRACE_IT(19743);
        int i= -1;
        while(!this->phaseList[++i].valid)
        {TRACE_IT(19744);
            if(i >= PhaseCount - 1)
            {TRACE_IT(19745);
                return InvalidPhase;
            }
        }
        return Phase(i);
    }


    //
    // List of names of all the flags
    //

    const char16* const FlagNames[FlagCount + 1] =
    {
    #define FLAG(type, name, ...) _u(#name),
    #include "ConfigFlagsList.h"
        NULL
    #undef FLAG
    };


    //
    // List of names of all the Phases
    //

    const char16* const PhaseNames[PhaseCount + 1] =
    {
    #define PHASE(name) _u(#name),
    #include "ConfigFlagsList.h"
        NULL
    #undef PHASE
    };


    //
    // Description of flags
    //
    const char16* const FlagDescriptions[FlagCount + 1] =
    {
    #define FLAG(type, name, description, ...) _u(description),
    #include "ConfigFlagsList.h"
        NULL
    #undef FLAG
    };

    //
    // Parent flag categorization of flags
    //
    const Flag FlagParents[FlagCount + 1] =
    {
    #define FLAG(type, name, description, defaultValue, parentName, ...) parentName##Flag,
    #include "ConfigFlagsList.h"
        InvalidFlag
    #undef FLAG
    };

    ///
    ///----------------------------------------------------------------------------
    ///----------------------------------------------------------------------------
    ///
    /// class ConfigFlagsTable
    ///
    ///----------------------------------------------------------------------------
    ///----------------------------------------------------------------------------


    ///----------------------------------------------------------------------------
    ///
    /// ConfigFlagsTable::ConfigFlagsTable
    ///
    /// Constructor initializes all the flags with their default values. The nDummy
    /// variable is used to prevent the compiler error due to the trailing comma
    /// when we generate the list of flags.
    ///
    ///----------------------------------------------------------------------------

#define FLAG(type, name, description, defaultValue, ...) \
        \
        name ## ( ## defaultValue ##), \

    ConfigFlagsTable::ConfigFlagsTable():
        #include "ConfigFlagsList.h"
#undef FLAG
        nDummy(0)
    {TRACE_IT(19746);
        for(int i=0; i < FlagCount; flagPresent[i++] = false);

        // set mark for parent flags
        ZeroMemory(this->flagIsParent, sizeof(this->flagIsParent));
#define FLAG(type, name, description, defaultValue, parentName, ...) \
        if ((int)parentName##Flag < FlagCount) this->flagIsParent[(int) parentName##Flag] = true;
#include "ConfigFlagsList.h"
#undef FLAG

        // set all parent flags to their default (setting all child flags to their right values)
        this->SetAllParentFlagsAsDefaultValue();
    }


    ///----------------------------------------------------------------------------
    ///
    /// ConfigFlagsTable::SetAllParentFlagsAsDefaultValue
    ///
    /// Iterate through all parent flags and set their default value
    ///
    /// Note: only Boolean type supported for now
    ///----------------------------------------------------------------------------

    String *
    ConfigFlagsTable::GetAsString(Flag flag) const
    {TRACE_IT(19747);
        return reinterpret_cast<String* >(GetProperty(flag));
    }

    Phases *
    ConfigFlagsTable::GetAsPhase(Flag flag) const
    {TRACE_IT(19748);
        return reinterpret_cast<Phases*>(GetProperty(flag));
    }

    Flag
    ConfigFlagsTable::GetOppositePhaseFlag(Flag flag) const
    {TRACE_IT(19749);
#if ENABLE_DEBUG_CONFIG_OPTIONS
        switch (flag)
        {
        case OnFlag: return OffFlag;
        case OffFlag: return OnFlag;
        }
#endif
        return InvalidFlag;
    }

    Boolean *
    ConfigFlagsTable::GetAsBoolean(Flag flag)  const
    {TRACE_IT(19750);
        return reinterpret_cast<Boolean*>(GetProperty(flag));
    }

    Number *
    ConfigFlagsTable::GetAsNumber(Flag flag)  const
    {TRACE_IT(19751);
        return reinterpret_cast<Number* >(GetProperty(flag));
    }

    NumberSet *
    ConfigFlagsTable::GetAsNumberSet(Flag flag)  const
    {TRACE_IT(19752);
        return reinterpret_cast<NumberSet* >(GetProperty(flag));
    }

    NumberPairSet *
    ConfigFlagsTable::GetAsNumberPairSet(Flag flag)  const
    {TRACE_IT(19753);
        return reinterpret_cast<NumberPairSet* >(GetProperty(flag));
    }

    NumberRange *
    ConfigFlagsTable::GetAsNumberRange(Flag flag)  const
    {TRACE_IT(19754);
        return reinterpret_cast<NumberRange* >(GetProperty(flag));
    }

    void
    ConfigFlagsTable::Enable(Flag flag)
    {TRACE_IT(19755);
        this->flagPresent[flag] = true;
    }

    void
    ConfigFlagsTable::Disable(Flag flag)
    {TRACE_IT(19756);
        this->flagPresent[flag] = false;
    }

    bool
    ConfigFlagsTable::IsEnabled(Flag flag)
    {TRACE_IT(19757);
        return this->flagPresent[flag];
    }

    bool
    ConfigFlagsTable::IsParentFlag(Flag flag) const
    {TRACE_IT(19758);
        return this->flagIsParent[flag];
    }

    void
    ConfigFlagsTable::SetAllParentFlagsAsDefaultValue()
    {TRACE_IT(19759);
        for (int i = 0; i < FlagCount; i++)
        {TRACE_IT(19760);
            Flag currentFlag = (Flag) i;
            if (this->IsParentFlag(currentFlag))
            {TRACE_IT(19761);
                // only supporting Boolean for now
                AssertMsg(this->GetFlagType(currentFlag) == FlagBoolean, "only supporting boolean flags as parent flags");

                Boolean defaultParentValue = this->GetDefaultValueAsBoolean(currentFlag);
                this->SetAsBoolean(currentFlag, defaultParentValue);
            }
        }
    }

    void ConfigFlagsTable::FinalizeConfiguration()
    {TRACE_IT(19762);
        TransferAcronymFlagConfiguration();
        TranslateFlagConfiguration();
    }

    void ConfigFlagsTable::TransferAcronymFlagConfiguration()
    {TRACE_IT(19763);
        // Transfer acronym flag configuration into the corresponding actual flag
    #define FLAG(...)
    #define FLAGNRA(Type, Name, Acronym, ...) \
        if(!IsEnabled(Name##Flag) && IsEnabled(Acronym##Flag)) \
        {TRACE_IT(19764); \
            Enable(Name##Flag); \
            Name = Acronym; \
        }
    #if ENABLE_DEBUG_CONFIG_OPTIONS
    #define FLAGPRA(Type, ParentName, Name, Acronym, ...) \
        if(!IsEnabled(Name##Flag) && IsEnabled(Acronym##Flag)) \
        {TRACE_IT(19765); \
            Enable(Name##Flag); \
            Name = Acronym; \
        }
        #define FLAGRA(Type, Name, Acronym, ...) FLAGNRA(Type, Name, Acronym, __VA_ARGS__)
    #endif
    #include "ConfigFlagsList.h"
    }

    void ConfigFlagsTable::TranslateFlagConfiguration()
    {TRACE_IT(19766);
        const auto VerifyExecutionModeLimits = [this]()
        {TRACE_IT(19767);
            const Number zero = static_cast<Number>(0);
            const Number maxUint8 = static_cast<Number>(static_cast<uint8>(-1)); // entry point call count is uint8
            const Number maxUint16 = static_cast<Number>(static_cast<uint16>(-1));

        #if ENABLE_DEBUG_CONFIG_OPTIONS
            Assert(MinInterpretCount >= zero);
            Assert(MinInterpretCount <= maxUint16);
            Assert(MaxInterpretCount >= zero);
            Assert(MaxInterpretCount <= maxUint16);
            Assert(MinSimpleJitRunCount >= zero);
            Assert(MinSimpleJitRunCount <= maxUint8);
            Assert(MaxSimpleJitRunCount >= zero);
            Assert(MaxSimpleJitRunCount <= maxUint8);

            Assert(SimpleJitAfter >= zero);
            Assert(SimpleJitAfter <= maxUint8);
            Assert(FullJitAfter >= zero);
            Assert(FullJitAfter <= maxUint16);
        #endif

            Assert(AutoProfilingInterpreter0Limit >= zero);
            Assert(AutoProfilingInterpreter0Limit <= maxUint16);
            Assert(ProfilingInterpreter0Limit >= zero);
            Assert(ProfilingInterpreter0Limit <= maxUint16);
            Assert(AutoProfilingInterpreter1Limit >= zero);
            Assert(AutoProfilingInterpreter1Limit <= maxUint16);
            Assert(SimpleJitLimit >= zero);
            Assert(SimpleJitLimit <= maxUint8);
            Assert(ProfilingInterpreter1Limit >= zero);
            Assert(ProfilingInterpreter1Limit <= maxUint16);
            Assert(
                (
                    AutoProfilingInterpreter0Limit +
                    ProfilingInterpreter0Limit +
                    AutoProfilingInterpreter1Limit +
                    SimpleJitLimit +
                    ProfilingInterpreter1Limit
                ) <= maxUint16);
        };
        VerifyExecutionModeLimits();

    #if ENABLE_DEBUG_CONFIG_OPTIONS
    #if !DISABLE_JIT
        if(ForceDynamicProfile)
        {TRACE_IT(19768);
            Force.Enable(DynamicProfilePhase);
        }
        if(ForceJITLoopBody)
        {TRACE_IT(19769);
            Force.Enable(JITLoopBodyPhase);
        }
    #endif
        if(NoDeferParse)
        {TRACE_IT(19770);
            Off.Enable(DeferParsePhase);
        }
    #endif

    #if ENABLE_DEBUG_CONFIG_OPTIONS && !DISABLE_JIT
        bool dontEnforceLimitsForSimpleJitAfterOrFullJitAfter = false;
        if((IsEnabled(MinInterpretCountFlag) || IsEnabled(MaxInterpretCountFlag)) &&
            !(IsEnabled(SimpleJitAfterFlag) || IsEnabled(FullJitAfterFlag)))
        {TRACE_IT(19771);
            if(Off.IsEnabled(SimpleJitPhase))
            {TRACE_IT(19772);
                Enable(FullJitAfterFlag);
                if(IsEnabled(MaxInterpretCountFlag))
                {TRACE_IT(19773);
                    FullJitAfter = MaxInterpretCount;
                }
                else
                {TRACE_IT(19774);
                    FullJitAfter = MinInterpretCount;
                    dontEnforceLimitsForSimpleJitAfterOrFullJitAfter = true;
                }
            }
            else
            {TRACE_IT(19775);
                Enable(SimpleJitAfterFlag);
                if(IsEnabled(MaxInterpretCountFlag))
                {TRACE_IT(19776);
                    SimpleJitAfter = MaxInterpretCount;
                }
                else
                {TRACE_IT(19777);
                    SimpleJitAfter = MinInterpretCount;
                    dontEnforceLimitsForSimpleJitAfterOrFullJitAfter = true;
                }
                if((IsEnabled(MinInterpretCountFlag) && IsEnabled(MinSimpleJitRunCountFlag)) ||
                    IsEnabled(MaxSimpleJitRunCountFlag))
                {TRACE_IT(19778);
                    Enable(FullJitAfterFlag);
                    FullJitAfter = SimpleJitAfter;
                    if(IsEnabled(MaxSimpleJitRunCountFlag))
                    {TRACE_IT(19779);
                        FullJitAfter += MaxSimpleJitRunCount;
                    }
                    else
                    {TRACE_IT(19780);
                        FullJitAfter += MinSimpleJitRunCount;
                        Assert(dontEnforceLimitsForSimpleJitAfterOrFullJitAfter);
                    }
                }
            }
        }

        // Configure execution mode limits
        do
        {TRACE_IT(19781);
            if(IsEnabled(AutoProfilingInterpreter0LimitFlag) ||
                IsEnabled(ProfilingInterpreter0LimitFlag) ||
                IsEnabled(AutoProfilingInterpreter1LimitFlag) ||
                IsEnabled(SimpleJitLimitFlag) ||
                IsEnabled(ProfilingInterpreter1LimitFlag))
            {TRACE_IT(19782);
                break;
            }

            if(IsEnabled(ExecutionModeLimitsFlag))
            {TRACE_IT(19783);
                uint autoProfilingInterpreter0Limit;
                uint profilingInterpreter0Limit;
                uint autoProfilingInterpreter1Limit;
                uint simpleJitLimit;
                uint profilingInterpreter1Limit;
                const int scannedCount =
                    swscanf_s(
                        static_cast<LPCWSTR>(ExecutionModeLimits),
                        _u("%u.%u.%u.%u.%u"),
                        &autoProfilingInterpreter0Limit,
                        &profilingInterpreter0Limit,
                        &autoProfilingInterpreter1Limit,
                        &simpleJitLimit,
                        &profilingInterpreter1Limit);
                Assert(scannedCount == 5);

                Enable(AutoProfilingInterpreter0LimitFlag);
                Enable(ProfilingInterpreter0LimitFlag);
                Enable(AutoProfilingInterpreter1LimitFlag);
                Enable(SimpleJitLimitFlag);
                Enable(ProfilingInterpreter1LimitFlag);

                AutoProfilingInterpreter0Limit = autoProfilingInterpreter0Limit;
                ProfilingInterpreter0Limit = profilingInterpreter0Limit;
                AutoProfilingInterpreter1Limit = autoProfilingInterpreter1Limit;
                SimpleJitLimit = simpleJitLimit;
                ProfilingInterpreter1Limit = profilingInterpreter1Limit;
                break;
            }

            if(!NewSimpleJit)
            {TRACE_IT(19784);
                // Use the defaults for old simple JIT. The flags are not enabled here because the values can be changed later
                // based on other flags, only the defaults values are adjusted here.
                AutoProfilingInterpreter0Limit = DEFAULT_CONFIG_AutoProfilingInterpreter0Limit;
                ProfilingInterpreter0Limit = DEFAULT_CONFIG_ProfilingInterpreter0Limit;
                CompileAssert(
                    DEFAULT_CONFIG_AutoProfilingInterpreter0Limit <= DEFAULT_CONFIG_AutoProfilingInterpreterLimit_OldSimpleJit);
                AutoProfilingInterpreter1Limit =
                    DEFAULT_CONFIG_AutoProfilingInterpreterLimit_OldSimpleJit - DEFAULT_CONFIG_AutoProfilingInterpreter0Limit;
                CompileAssert(DEFAULT_CONFIG_ProfilingInterpreter0Limit <= DEFAULT_CONFIG_SimpleJitLimit_OldSimpleJit);
                SimpleJitLimit = DEFAULT_CONFIG_SimpleJitLimit_OldSimpleJit - DEFAULT_CONFIG_ProfilingInterpreter0Limit;
                ProfilingInterpreter1Limit = 0;
                VerifyExecutionModeLimits();
            }

            if (IsEnabled(SimpleJitAfterFlag))
            {TRACE_IT(19785);
                Enable(AutoProfilingInterpreter0LimitFlag);
                Enable(ProfilingInterpreter0LimitFlag);
                Enable(AutoProfilingInterpreter1LimitFlag);
                Enable(EnforceExecutionModeLimitsFlag);

                {TRACE_IT(19786);
                    Js::Number iterationsNeeded = SimpleJitAfter;
                    ProfilingInterpreter0Limit = min(ProfilingInterpreter0Limit, iterationsNeeded);
                    iterationsNeeded -= ProfilingInterpreter0Limit;
                    AutoProfilingInterpreter0Limit = iterationsNeeded;
                    AutoProfilingInterpreter1Limit = 0;
                }

                if(IsEnabled(FullJitAfterFlag))
                {TRACE_IT(19787);
                    Enable(SimpleJitLimitFlag);
                    Enable(ProfilingInterpreter1LimitFlag);

                    Assert(SimpleJitAfter <= FullJitAfter);
                    Js::Number iterationsNeeded = FullJitAfter - SimpleJitAfter;
                    Js::Number profilingIterationsNeeded =
                        min(NewSimpleJit
                                ? DEFAULT_CONFIG_MinProfileIterations
                                : DEFAULT_CONFIG_MinProfileIterations_OldSimpleJit,
                            FullJitAfter) -
                        ProfilingInterpreter0Limit;
                    if(NewSimpleJit)
                    {TRACE_IT(19788);
                        ProfilingInterpreter1Limit = min(ProfilingInterpreter1Limit, iterationsNeeded);
                        iterationsNeeded -= ProfilingInterpreter1Limit;
                        profilingIterationsNeeded -= ProfilingInterpreter1Limit;
                        SimpleJitLimit = iterationsNeeded;
                    }
                    else
                    {TRACE_IT(19789);
                        SimpleJitLimit = iterationsNeeded;
                        profilingIterationsNeeded -= min(SimpleJitLimit, profilingIterationsNeeded);
                        ProfilingInterpreter1Limit = 0;
                    }

                    if(profilingIterationsNeeded != 0)
                    {TRACE_IT(19790);
                        Js::Number iterationsToMove = min(AutoProfilingInterpreter1Limit, profilingIterationsNeeded);
                        AutoProfilingInterpreter1Limit -= iterationsToMove;
                        ProfilingInterpreter0Limit += iterationsToMove;
                        profilingIterationsNeeded -= iterationsToMove;

                        iterationsToMove = min(AutoProfilingInterpreter0Limit, profilingIterationsNeeded);
                        AutoProfilingInterpreter0Limit -= iterationsToMove;
                        ProfilingInterpreter0Limit += iterationsToMove;
                        profilingIterationsNeeded -= iterationsToMove;

                        Assert(profilingIterationsNeeded == 0);
                    }

                    Assert(
                        (
                            AutoProfilingInterpreter0Limit +
                            ProfilingInterpreter0Limit +
                            AutoProfilingInterpreter1Limit +
                            SimpleJitLimit +
                            ProfilingInterpreter1Limit
                        ) == FullJitAfter);
                }

                Assert(
                    (
                        AutoProfilingInterpreter0Limit +
                        ProfilingInterpreter0Limit +
                        AutoProfilingInterpreter1Limit
                    ) == SimpleJitAfter);
                EnforceExecutionModeLimits = true;
                break;
            }

            if(IsEnabled(FullJitAfterFlag))
            {TRACE_IT(19791);
                Enable(AutoProfilingInterpreter0LimitFlag);
                Enable(ProfilingInterpreter0LimitFlag);
                Enable(AutoProfilingInterpreter1LimitFlag);
                Enable(SimpleJitLimitFlag);
                Enable(ProfilingInterpreter1LimitFlag);
                Enable(EnforceExecutionModeLimitsFlag);

                Js::Number iterationsNeeded = FullJitAfter;
                if(NewSimpleJit)
                {TRACE_IT(19792);
                    ProfilingInterpreter1Limit = min(ProfilingInterpreter1Limit, iterationsNeeded);
                    iterationsNeeded -= ProfilingInterpreter1Limit;
                }
                else
                {TRACE_IT(19793);
                    ProfilingInterpreter1Limit = 0;
                    SimpleJitLimit = min(SimpleJitLimit, iterationsNeeded);
                    iterationsNeeded -= SimpleJitLimit;
                }
                ProfilingInterpreter0Limit = min(ProfilingInterpreter0Limit, iterationsNeeded);
                iterationsNeeded -= ProfilingInterpreter0Limit;
                if(NewSimpleJit)
                {TRACE_IT(19794);
                    SimpleJitLimit = min(SimpleJitLimit, iterationsNeeded);
                    iterationsNeeded -= SimpleJitLimit;
                }
                AutoProfilingInterpreter0Limit = min(AutoProfilingInterpreter0Limit, iterationsNeeded);
                iterationsNeeded -= AutoProfilingInterpreter0Limit;
                AutoProfilingInterpreter1Limit = iterationsNeeded;

                Assert(
                    (
                        AutoProfilingInterpreter0Limit +
                        ProfilingInterpreter0Limit +
                        AutoProfilingInterpreter1Limit +
                        SimpleJitLimit +
                        ProfilingInterpreter1Limit
                    ) == FullJitAfter);
                EnforceExecutionModeLimits = true;
                break;
            }
            if (IsEnabled(MaxTemplatizedJitRunCountFlag))
            {TRACE_IT(19795);
                if (MaxTemplatizedJitRunCount >= 0)
                {TRACE_IT(19796);
                    MinTemplatizedJitRunCount = MaxTemplatizedJitRunCount;
                }
            }
            if (IsEnabled(MaxAsmJsInterpreterRunCountFlag))
            {TRACE_IT(19797);
                if (MaxAsmJsInterpreterRunCount >= 0)
                {TRACE_IT(19798);
                    MinAsmJsInterpreterRunCount = MaxAsmJsInterpreterRunCount;
                }
            }

        } while(false);
    #endif

        if( (
            #ifdef ENABLE_PREJIT
                Prejit ||
            #endif
                ForceNative
            ) &&
            !NoNative)
        {TRACE_IT(19799);
            Enable(AutoProfilingInterpreter0LimitFlag);
            Enable(ProfilingInterpreter0LimitFlag);
            Enable(AutoProfilingInterpreter1LimitFlag);
            Enable(EnforceExecutionModeLimitsFlag);

            // Override any relevant automatic configuration above
            AutoProfilingInterpreter0Limit = 0;
            ProfilingInterpreter0Limit = 0;
            AutoProfilingInterpreter1Limit = 0;
        #if ENABLE_DEBUG_CONFIG_OPTIONS
            if(Off.IsEnabled(SimpleJitPhase))
            {TRACE_IT(19800);
                Enable(SimpleJitLimitFlag);
                Enable(ProfilingInterpreter1LimitFlag);

                SimpleJitLimit = 0;
                ProfilingInterpreter1Limit = 0;
            }
        #endif

            EnforceExecutionModeLimits = true;
        }

        VerifyExecutionModeLimits();
    }

    ///----------------------------------------------------------------------------
    ///
    /// ConfigFlagsTable::GetFlag
    ///
    /// Given a string finds the corresponding enum Flag. The comparison is case
    /// in-sensitive
    ///
    ///----------------------------------------------------------------------------

    Flag
    ConfigFlagsTable::GetFlag(__in LPCWSTR str)
    {TRACE_IT(19801);
        for(int i=0; i < FlagCount; i++)
        {TRACE_IT(19802);
            if(0 == _wcsicmp(str, FlagNames[i]))
            {TRACE_IT(19803);
                return Flag(i);
            }
        }
        return InvalidFlag;
    }

    ///----------------------------------------------------------------------------
    ///
    /// ConfigFlagsTable::GetPhase
    ///
    /// Given a string finds the corresponding enum Phase. The comparison is case
    /// in-sensitive
    ///
    ///----------------------------------------------------------------------------

    Phase
    ConfigFlagsTable::GetPhase(__in LPCWSTR str)
    {TRACE_IT(19804);
        for(int i=0; i < PhaseCount; i++)
        {TRACE_IT(19805);
            if(0 == _wcsicmp(str, PhaseNames[i]))
            {TRACE_IT(19806);
                return Phase(i);
            }
        }
        return InvalidPhase;
    }

    void
    ConfigFlagsTable::PrintUsageString()
    {TRACE_IT(19807);
        printf("List of Phases:\n");
        for(int i = 0; i < PhaseCount; i++)
        {TRACE_IT(19808);
            if (i % 4 == 0)
            {TRACE_IT(19809);
                printf("\n  ");
            }
            printf("%-40ls ", PhaseNames[i]);
        }

        printf("\n\nList of flags:\n\n");
        for(int i = 0; i < FlagCount; i++)
        {TRACE_IT(19810);
            printf("%60ls ", FlagNames[i]);
            switch(GetFlagType(Flag(i)))
            {
            case InvalidFlagType:
                break;
            case FlagString:
                printf("[:String]        ");
                break;
            case FlagPhases:
                printf("[:Phase]         ");
                break;
            case FlagNumber:
                printf("[:Number]        ");
                break;
            case FlagBoolean:
                printf("                 ");
                break;
            case FlagNumberSet:
                printf("[:NumberSet]     ");
                break;
            case FlagNumberPairSet:
                printf("[:NumberPairSet] ");
                break;
            case FlagNumberRange:
                printf("[:NumberRange]   ");
                break;
            default:
                Assert(false);
                __assume(false);
            }
            printf("%ls\n", FlagDescriptions[i]);
        }
    }

    ///----------------------------------------------------------------------------
    ///
    /// ConfigFlagsTable::GetFlagType
    ///
    /// Given a flag it returns the type (PhaseFlag, StringFlag ...). This could
    /// easily have been a lookup table like FlagNames and PhaseNames but this
    /// seems more concise
    ///
    ///----------------------------------------------------------------------------


    FlagTypes
    ConfigFlagsTable::GetFlagType(Flag flag)
    {TRACE_IT(19811);
        switch(flag)
        {
    #define FLAG(type, name, ...) \
            case name##Flag : \
                return Flag##type; \

    #include "ConfigFlagsList.h"

            default:
                return InvalidFlagType;
        }
    }


    ///----------------------------------------------------------------------------
    ///
    /// ConfigFlagsTable::GetProperty
    ///
    /// Get the field corresponding to the flag. used as an internal method for
    /// the various GetAs* methods.
    ///
    ///----------------------------------------------------------------------------


    void *
    ConfigFlagsTable::GetProperty(Flag flag) const
    {TRACE_IT(19812);
        switch(flag)
        {
        #define FLAG(type, name, ...) \
            \
            case name##Flag : \
                return reinterpret_cast<void*>(const_cast<type*>(&##name)); \

        #include "ConfigFlagsList.h"

            default:
                return NULL;
        }
    }


    void
    ConfigFlagsTable::VerboseDump()
    {TRACE_IT(19813);
#define FLAG(type, name, ...) \
        if (IsEnabled(name##Flag)) \
        {TRACE_IT(19814); \
            Output::Print(_u("-%s"), _u(#name)); \
            switch (Flag##type) \
            { \
            case FlagBoolean: \
                if (!*GetAsBoolean(name##Flag)) \
                {TRACE_IT(19815); \
                    Output::Print(_u("-")); \
                } \
                break; \
            case FlagString: \
                if (GetAsString(name##Flag) != nullptr) \
                {TRACE_IT(19816); \
                    Output::Print(_u(":%s"), (LPCWSTR)*GetAsString(name##Flag)); \
                } \
                break; \
            case FlagNumber: \
                Output::Print(_u(":%d"), *GetAsNumber(name##Flag)); \
                break; \
            default: \
                break; \
            }; \
            Output::Print(_u("\n")); \
        }

#include "ConfigFlagsList.h"
#undef FLAG
    }

    ///----------------------------------------------------------------------------
    ///
    /// ConfigFlagsTable::GetDefaultValueAsBoolean
    ///
    /// Get the default value of a given boolean flag. If the flag is not of boolean
    /// type, will assert on CHK or return FALSE on FRE.
    ///
    ///----------------------------------------------------------------------------
    Boolean
    ConfigFlagsTable::GetDefaultValueAsBoolean(Flag flag) const
    {TRACE_IT(19817);
        Boolean retValue = FALSE;

        switch (flag)
        {
#define FLAG(type, name, description, defaultValue, ...) FLAGDEFAULT##type(name, defaultValue)
            // define an overload for each FlagTypes - type
            //   * all defaults we don't care about
#define FLAGDEFAULTPhases(name, defaultValue)
#define FLAGDEFAULTString(name, defaultValue)
#define FLAGDEFAULTNumber(name, defaultValue)
#define FLAGDEFAULTNumberSet(name, defaultValue)
#define FLAGDEFAULTNumberRange(name, defaultValue)
#define FLAGDEFAULTNumberPairSet(name, defaultValue)
            //   * and those we do care about
#define FLAGDEFAULTBoolean(name, defaultValue) \
        case name##Flag: \
            retValue = (Boolean) defaultValue; \
            break; \

#include "ConfigFlagsList.h"

#undef FLAGDEFAULTBoolean
#undef FLAGDEFAULTNumberRange
#undef FLAGDEFAULTNumberPairSet
#undef FLAGDEFAULTNumberSet
#undef FLAGDEFAULTNumber
#undef FLAGDEFAULTString
#undef FLAGDEFAULTPhases
#undef FLAG

#undef FLAGREGOVREXPBoolean
#undef FLAGREGOVREXPNumberRange
#undef FLAGREGOVREXPNumberPairSet
#undef FLAGREGOVREXPNumberSet
#undef FLAGREGOVREXPNumber
#undef FLAGREGOVREXPString
#undef FLAGREGOVREXPPhases

#undef FLAGREGOVRBoolean
#undef FLAGREGOVRNumberRange
#undef FLAGREGOVRNumberPairSet
#undef FLAGREGOVRNumberSet
#undef FLAGREGOVRNumber
#undef FLAGREGOVRString
#undef FLAGREGOVRPhases

        default:
            // not found - or not a boolean flag
            Assert(false);
        }

        return retValue;
    }

    ///----------------------------------------------------------------------------
    ///
    /// ConfigFlagsTable::SetAsBoolean
    ///
    /// Set the value of a boolean flag. If the flag is a parent flag, all children flag
    //  will be set accordingly.
    ///
    ///----------------------------------------------------------------------------
    void
    ConfigFlagsTable::SetAsBoolean(Flag flag, Boolean value)
    {TRACE_IT(19818);
        AssertMsg(this->GetFlagType(flag) == FlagBoolean, "flag not a boolean type");

        Boolean* settingAsBoolean = this->GetAsBoolean(flag);
        Assert(settingAsBoolean != nullptr);

        Output::VerboseNote(_u("FLAG %s = %d\n"), FlagNames[(int) flag], value);
        *settingAsBoolean = value;

        // check if parent flag
        if (this->IsParentFlag(flag))
        {TRACE_IT(19819);
            // parent flag, will iterate through all child flags
            Flag childFlag = GetNextChildFlag(flag, /* no currentChildFlag */ InvalidFlag);
            while (childFlag != InvalidFlag)
            {TRACE_IT(19820);
                Boolean childDefaultValue = GetDefaultValueAsBoolean(childFlag);

                // if the parent flag is TRUE, the children flag values are based on their default values
                // if the parent flag is FALSE, the children flag values are FALSE (always - as disabled)
                Boolean childValue = value == TRUE ? childDefaultValue : FALSE;

                Output::VerboseNote(_u("FLAG %s = %d - setting child flag %s = %d\n"), FlagNames[(int) flag], value, FlagNames[(int) childFlag], childValue);
                this->SetAsBoolean(childFlag, childValue);

                // get next child flag
                childFlag = GetNextChildFlag(flag, /* currentChildFlag */ childFlag);
            }
        }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        // in case the flag is marked as 'callback' - to call the method
#define FLAG(type, name, description, defaultValue, parentName, hasCallback) FLAGCALLBACK##hasCallback(type, name)
#define FLAGCALLBACKFALSE(type, name)
#define FLAGCALLBACKTRUE(type, name)    FLAGDOCALLBACK##type(name)

        // define an overload for each FlagTypes - type
            //   * all defaults we don't care about - should assert
#define FLAGDOCALLBACKNumberRange(name)        Assert(false);
#define FLAGDOCALLBACKPhases(name)        Assert(false);
#define FLAGDOCALLBACKString(name)        Assert(false);
#define FLAGDOCALLBACKNumber(name)        Assert(false);
#define FLAGDOCALLBACKNumberSet(name)     Assert(false);
#define FLAGDOCALLBACKNumberPairSet(name) Assert(false);
            //   * and those we do care about
#define FLAGDOCALLBACKBoolean(name)       if( flag == name##Flag ) this->FlagSetCallback_##name(value);

#include "ConfigFlagsList.h"

#undef FLAGDOCALLBACKBoolean
#undef FLAGDOCALLBACKNumberRange
#undef FLAGDOCALLBACKNumberPairSet
#undef FLAGDOCALLBACKNumberSet
#undef FLAGDOCALLBACKNumber
#undef FLAGDOCALLBACKString
#undef FLAGDOCALLBACKPhases
#undef FLAGCALLBACKTRUE
#undef FLAGCALLBACKFALSE
#undef FLAG
#endif
    }

    ///----------------------------------------------------------------------------
    ///
    /// ConfigFlagsTable::GetParentFlag
    ///
    /// Get the parent flag corresponding to the flag, if any, otherwise returns NoParentFlag
    ///
    ///----------------------------------------------------------------------------
    Flag
    ConfigFlagsTable::GetParentFlag(Flag flag) const
    {TRACE_IT(19821);
        Flag parentFlag = FlagParents[(int)flag];

        return parentFlag;
    }

    ///----------------------------------------------------------------------------
    ///
    /// ConfigFlagsTable::GetNextChildFlag
    ///
    /// Get the next child flag for a given parent flag. If no currentChildFlag, use
    /// InvalidFlag or NoParentFlag as start iterator.
    ///
    ///----------------------------------------------------------------------------
    Flag
    ConfigFlagsTable::GetNextChildFlag(Flag parentFlag, Flag currentChildFlag)  const
    {TRACE_IT(19822);
        // start at the current+1
        int startIndex = (int)currentChildFlag + 1;

        // otherwise start from beginning
        if (currentChildFlag == InvalidFlag || currentChildFlag == NoParentFlag)
        {TRACE_IT(19823);
            // reset the start index
            startIndex = 0;
        }

        for(int i=startIndex; i < FlagCount; i++)
        {TRACE_IT(19824);
            Flag currentFlag = (Flag)i;
            Flag parentFlagForCurrentFlag = GetParentFlag(currentFlag);

            if(parentFlagForCurrentFlag == parentFlag)
            {TRACE_IT(19825);
                // found a match
                return currentFlag;
            }
        }

        // no more
        return InvalidFlag;
    }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    //
    // Special overrides for flags being set
    //
    void
    ConfigFlagsTable::FlagSetCallback_ES6All(Boolean value)
    {TRACE_IT(19826);
        // iterate through all ES6 flags - and set them explicitly (except ES6Verbose)
        Flag parentFlag = ES6Flag;

        // parent ES6 flag, will iterate through all child ES6 flags
        Flag childFlag = GetNextChildFlag(parentFlag, /* no currentChildFlag */ InvalidFlag);
        while (childFlag != InvalidFlag)
        {TRACE_IT(19827);
            // skip verbose
            if (childFlag != ES6VerboseFlag)
            {TRACE_IT(19828);
                Boolean childValue = value;

                Output::VerboseNote(_u("FLAG %s = %d - setting child flag %s = %d\n"), FlagNames[(int) parentFlag], value, FlagNames[(int) childFlag], childValue);
                this->SetAsBoolean(childFlag, childValue);
            }

            // get next child flag
            childFlag = GetNextChildFlag(parentFlag, /* currentChildFlag */ childFlag);
        }
    }

    void
    ConfigFlagsTable::FlagSetCallback_ES6Experimental(Boolean value)
    {TRACE_IT(19829);
        if (value)
        {TRACE_IT(19830);
            EnableExperimentalFlag();
        }
    }

#endif

    void
    ConfigFlagsTable::EnableExperimentalFlag()
    {TRACE_IT(19831);
        AutoCriticalSection autocs(&csExperimentalFlags);
#define FLAG_REGOVR_EXP(type, name, description, defaultValue, parentName, hasCallback) this->SetAsBoolean(Js::Flag::name##Flag, true);
#include "ConfigFlagsList.h"
#undef FLAG_REGOVR_EXP
    }

    //
    // Configuration options
    //

    Configuration::Configuration()
    {TRACE_IT(19832);
    }

    bool Configuration::EnableJitInDebugMode()
    {TRACE_IT(19833);
        return CONFIG_FLAG(EnableJitInDiagMode);
    }

    Configuration        Configuration::Global;


} //namespace Js
