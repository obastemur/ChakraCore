//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

#if ENABLE_PROFILE_INFO
#ifdef ENABLE_WININET_PROFILE_DATA_CACHE
#include "activscp_private.h"
#endif
namespace Js
{
    ExecutionFlags
    SourceDynamicProfileManager::IsFunctionExecuted(Js::LocalFunctionId functionId)
    {TRACE_IT(52621);
        if (cachedStartupFunctions == nullptr || cachedStartupFunctions->Length() <= functionId)
        {TRACE_IT(52622);
            return ExecutionFlags_HasNoInfo;
        }
        return (ExecutionFlags)cachedStartupFunctions->Test(functionId);
    }

    DynamicProfileInfo *
    SourceDynamicProfileManager::GetDynamicProfileInfo(FunctionBody * functionBody)
    {TRACE_IT(52623);
        Js::LocalFunctionId functionId = functionBody->GetLocalFunctionId();
        DynamicProfileInfo * dynamicProfileInfo = nullptr;
        if (dynamicProfileInfoMap.Count() > 0 && dynamicProfileInfoMap.TryGetValue(functionId, &dynamicProfileInfo))
        {TRACE_IT(52624);
            if (dynamicProfileInfo->MatchFunctionBody(functionBody))
            {TRACE_IT(52625);
                return dynamicProfileInfo;
            }

#if DBG_DUMP
            if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::DynamicProfilePhase))
            {TRACE_IT(52626);
                Output::Print(_u("TRACE: DynamicProfile: Profile data rejected for function %d in %s\n"),
                    functionId, functionBody->GetSourceContextInfo()->url);
                Output::Flush();
            }
#endif
            // NOTE: We have profile mismatch, we can invalidate all other profile here.
        }
        return nullptr;
    }

    void SourceDynamicProfileManager::UpdateDynamicProfileInfo(LocalFunctionId functionId, DynamicProfileInfo * dynamicProfileInfo)
    {TRACE_IT(52627);
        Assert(dynamicProfileInfo != nullptr);

        dynamicProfileInfoMap.Item(functionId, dynamicProfileInfo);
    }

    void SourceDynamicProfileManager::MarkAsExecuted(LocalFunctionId functionId)
    {TRACE_IT(52628);
        Assert(startupFunctions != nullptr);
        Assert(functionId <= startupFunctions->Length());
        startupFunctions->Set(functionId);
    }

    void SourceDynamicProfileManager::EnsureStartupFunctions(uint numberOfFunctions)
    {TRACE_IT(52629);
        Assert(numberOfFunctions != 0);
        if(!startupFunctions || numberOfFunctions > startupFunctions->Length())
        {TRACE_IT(52630);
            BVFixed* oldStartupFunctions = this->startupFunctions;
            startupFunctions = BVFixed::New(numberOfFunctions, this->GetRecycler());
            if(oldStartupFunctions)
            {TRACE_IT(52631);
                this->startupFunctions->Copy(oldStartupFunctions);
            }
        }
    }

    //
    // Enables re-use of profile managers across script contexts - on every re-use the
    // previous script contexts list of startup functions are transferred over as input to this new script context.
    //
    void SourceDynamicProfileManager::Reuse()
    {TRACE_IT(52632);
        AssertMsg(profileDataCache == nullptr, "Persisted profiles cannot be re-used");
        cachedStartupFunctions = startupFunctions;
    }

    //
    // Loads the profile from the WININET cache
    //
    bool SourceDynamicProfileManager::LoadFromProfileCache(IActiveScriptDataCache* profileDataCache, LPCWSTR url)
    {TRACE_IT(52633);
    #ifdef ENABLE_WININET_PROFILE_DATA_CACHE
        AssertMsg(CONFIG_FLAG(WininetProfileCache), "Profile caching should be enabled for us to get here");
        Assert(profileDataCache);
        AssertMsg(!IsProfileLoadedFromWinInet(), "Duplicate profile cache loading?");

        // Keep a copy of this and addref it
        profileDataCache->AddRef();
        this->profileDataCache = profileDataCache;

        IStream* readStream;
        HRESULT hr = profileDataCache->GetReadDataStream(&readStream);
        if(SUCCEEDED(hr))
        {TRACE_IT(52634);
            Assert(readStream != nullptr);
            // stream reader owns the stream and will close it on destruction
            SimpleStreamReader streamReader(readStream);
            DWORD jscriptMajorVersion;
            DWORD jscriptMinorVersion;
            if(FAILED(AutoSystemInfo::GetJscriptFileVersion(&jscriptMajorVersion, &jscriptMinorVersion)))
            {TRACE_IT(52635);
                return false;
            }

            DWORD majorVersion;
            if(!streamReader.Read(&majorVersion) || majorVersion != jscriptMajorVersion)
            {TRACE_IT(52636);
                return false;
            }

            DWORD minorVersion;
            if(!streamReader.Read(&minorVersion) || minorVersion != jscriptMinorVersion)
            {TRACE_IT(52637);
                return false;
            }

            uint numberOfFunctions;
            if(!streamReader.Read(&numberOfFunctions) || numberOfFunctions > MAX_FUNCTION_COUNT)
            {TRACE_IT(52638);
                return false;
            }
            BVFixed* functions = BVFixed::New(numberOfFunctions, this->recycler);
            if(!streamReader.ReadArray(functions->GetData(), functions->WordCount()))
            {TRACE_IT(52639);
                return false;
            }
            this->cachedStartupFunctions = functions;
            OUTPUT_TRACE(Js::DynamicProfilePhase, _u("Profile load succeeded. Function count: %d  %s\n"), numberOfFunctions, url);
#if DBG_DUMP
            if(PHASE_TRACE1(Js::DynamicProfilePhase) && Js::Configuration::Global.flags.Verbose)
            {TRACE_IT(52640);
                OUTPUT_VERBOSE_TRACE(Js::DynamicProfilePhase, _u("Profile loaded:\n"));
                functions->Dump();
            }
#endif
            return true;
        }
        else if (hr == HRESULT_FROM_WIN32(ERROR_WRITE_PROTECT))
        {TRACE_IT(52641);
            this->isNonCachableScript = true;
            OUTPUT_VERBOSE_TRACE(Js::DynamicProfilePhase, _u("Profile load failed. Non-cacheable resource. %s\n"), url);
        }
        else
        {TRACE_IT(52642);
            OUTPUT_TRACE(Js::DynamicProfilePhase, _u("Profile load failed. No read stream. %s\n"), url);
        }
#endif
        return false;
    }

    //
    // Saves the profile to the WININET cache and returns the bytes written
    //
    uint SourceDynamicProfileManager::SaveToProfileCacheAndRelease(SourceContextInfo* info)
    {TRACE_IT(52643);
        uint bytesWritten = 0;
#ifdef ENABLE_WININET_PROFILE_DATA_CACHE
        if(profileDataCache)
        {TRACE_IT(52644);
            if(ShouldSaveToProfileCache(info))
            {TRACE_IT(52645);
                OUTPUT_TRACE(Js::DynamicProfilePhase, _u("Saving profile. Number of functions: %d Url: %s...\n"), startupFunctions->Length(), info->url);

                bytesWritten = SaveToProfileCache();

                if(bytesWritten == 0)
                {TRACE_IT(52646);
                    OUTPUT_TRACE(Js::DynamicProfilePhase, _u("Profile saving FAILED\n"));
                }
            }

            profileDataCache->Release();
            profileDataCache = nullptr;
        }
#endif
        return bytesWritten;
    }

    //
    // Saves the profile to the WININET cache
    //
    uint SourceDynamicProfileManager::SaveToProfileCache()
    {
        AssertMsg(CONFIG_FLAG(WininetProfileCache), "Profile caching should be enabled for us to get here");
        Assert(startupFunctions);

        uint bytesWritten = 0;
#ifdef ENABLE_WININET_PROFILE_DATA_CACHE
        //TODO: Add some diffing logic to not write unless necessary
        IStream* writeStream;
        HRESULT hr = profileDataCache->GetWriteDataStream(&writeStream);
        if(FAILED(hr))
        {TRACE_IT(52647);
            return 0;
        }
        Assert(writeStream != nullptr);
        // stream writer owns the stream and will close it on destruction
        SimpleStreamWriter streamWriter(writeStream);

        DWORD jscriptMajorVersion;
        DWORD jscriptMinorVersion;
        if(FAILED(AutoSystemInfo::GetJscriptFileVersion(&jscriptMajorVersion, &jscriptMinorVersion)))
        {TRACE_IT(52648);
            return 0;
        }

        if(!streamWriter.Write(jscriptMajorVersion))
        {TRACE_IT(52649);
            return 0;
        }

        if(!streamWriter.Write(jscriptMinorVersion))
        {TRACE_IT(52650);
            return 0;
        }

        if(!streamWriter.Write(startupFunctions->Length()))
        {TRACE_IT(52651);
            return 0;
        }
        if(streamWriter.WriteArray(startupFunctions->GetData(), startupFunctions->WordCount()))
        {TRACE_IT(52652);
            STATSTG stats;
            if(SUCCEEDED(writeStream->Stat(&stats, STATFLAG_NONAME)))
            {TRACE_IT(52653);
                bytesWritten = stats.cbSize.LowPart;
                Assert(stats.cbSize.LowPart > 0);
                AssertMsg(stats.cbSize.HighPart == 0, "We should not be writing such long data that the high part is non-zero");
            }

            hr = profileDataCache->SaveWriteDataStream(writeStream);
            if(FAILED(hr))
            {TRACE_IT(52654);
                return 0;
            }
#if DBG_DUMP
            if(PHASE_TRACE1(Js::DynamicProfilePhase) && Js::Configuration::Global.flags.Verbose)
            {TRACE_IT(52655);
                OUTPUT_VERBOSE_TRACE(Js::DynamicProfilePhase, _u("Saved profile:\n"));
                startupFunctions->Dump();
            }
#endif
        }
#endif
        return bytesWritten;
    }

    //
    // Do not save the profile:
    //      - If it is a non-cacheable WININET resource
    //      - If there are no or small number of functions executed
    //      - If there is not substantial difference in number of functions executed.
    //
    bool SourceDynamicProfileManager::ShouldSaveToProfileCache(SourceContextInfo* info) const
    {TRACE_IT(52656);
        if(isNonCachableScript)
        {TRACE_IT(52657);
            OUTPUT_VERBOSE_TRACE(Js::DynamicProfilePhase, _u("Skipping save of profile. Non-cacheable resource. %s\n"), info->url);
            return false;
        }

        if(!startupFunctions || startupFunctions->Length() <= DEFAULT_CONFIG_MinProfileCacheSize)
        {TRACE_IT(52658);
            OUTPUT_VERBOSE_TRACE(Js::DynamicProfilePhase, _u("Skipping save of profile. Small number of functions. %s\n"), info->url);
            return false;
        }

        if(cachedStartupFunctions)
        {TRACE_IT(52659);
            AssertMsg(cachedStartupFunctions != startupFunctions, "Ensure they are not shallow copies of each other - Reuse() does this for dynamic sources. We should not be invoked for dynamic sources");
            uint numberOfBitsDifferent = cachedStartupFunctions->DiffCount(startupFunctions);
            uint saveThreshold = (cachedStartupFunctions->Length() * DEFAULT_CONFIG_ProfileDifferencePercent) / 100;
            if(numberOfBitsDifferent <= saveThreshold)
            {TRACE_IT(52660);
                OUTPUT_VERBOSE_TRACE(Js::DynamicProfilePhase, _u("Skipping save of profile. Number of functions different: %d %s\n"), numberOfBitsDifferent, info->url);
                return false;
            }
            else
            {TRACE_IT(52661);
                OUTPUT_VERBOSE_TRACE(Js::DynamicProfilePhase, _u("Number of functions different: %d "), numberOfBitsDifferent);
            }
        }
        return true;
    }

    SourceDynamicProfileManager *
    SourceDynamicProfileManager::LoadFromDynamicProfileStorage(SourceContextInfo* info, ScriptContext* scriptContext, IActiveScriptDataCache* profileDataCache)
    {TRACE_IT(52662);
        SourceDynamicProfileManager* manager = nullptr;
        Recycler* recycler = scriptContext->GetRecycler();

#ifdef DYNAMIC_PROFILE_STORAGE
        if(DynamicProfileStorage::IsEnabled() && info->url != nullptr)
        {TRACE_IT(52663);
            manager = DynamicProfileStorage::Load(info->url, [recycler](char const * buffer, uint length) -> SourceDynamicProfileManager *
            {
                BufferReader reader(buffer, length);
                return SourceDynamicProfileManager::Deserialize(&reader, recycler);
            });
        }
#endif
        if(manager == nullptr)
        {TRACE_IT(52664);
            manager = RecyclerNew(recycler, SourceDynamicProfileManager, recycler);
        }
        if(profileDataCache != nullptr)
        {TRACE_IT(52665);
            bool profileLoaded = manager->LoadFromProfileCache(profileDataCache, info->url);
            if(profileLoaded)
            {TRACE_IT(52666);
                JS_ETW(EventWriteJSCRIPT_PROFILE_LOAD(info->dwHostSourceContext, scriptContext));
            }
        }
        return manager;
    }

#ifdef DYNAMIC_PROFILE_STORAGE
    void SourceDynamicProfileManager::ClearSavingData()
    {TRACE_IT(52667);
        dynamicProfileInfoMapSaving.Reset();
    }

    void SourceDynamicProfileManager::CopySavingData()
    {TRACE_IT(52668);
        dynamicProfileInfoMap.Map([&](LocalFunctionId functionId, DynamicProfileInfo *info)
        {
            dynamicProfileInfoMapSaving.Item(functionId, info);
        });
    }

    void
    SourceDynamicProfileManager::SaveDynamicProfileInfo(LocalFunctionId functionId, DynamicProfileInfo * dynamicProfileInfo)
    {TRACE_IT(52669);
        Assert(dynamicProfileInfo->GetFunctionBody()->HasExecutionDynamicProfileInfo());
        dynamicProfileInfoMapSaving.Item(functionId, dynamicProfileInfo);
    }

    template <typename T>
    SourceDynamicProfileManager *
    SourceDynamicProfileManager::Deserialize(T * reader, Recycler* recycler)
    {TRACE_IT(52670);
        uint functionCount;
        if (!reader->Peek(&functionCount))
        {TRACE_IT(52671);
            return nullptr;
        }

        BVFixed * startupFunctions = BVFixed::New(functionCount, recycler);
        if (!reader->ReadArray(((char *)startupFunctions),
            BVFixed::GetAllocSize(functionCount)))
        {TRACE_IT(52672);
            return nullptr;
        }

        uint profileCount;

        if (!reader->Read(&profileCount))
        {TRACE_IT(52673);
            return nullptr;
        }

        ThreadContext* threadContext = ThreadContext::GetContextForCurrentThread();

        SourceDynamicProfileManager * sourceDynamicProfileManager = RecyclerNew(threadContext->GetRecycler(), SourceDynamicProfileManager, recycler);

        sourceDynamicProfileManager->cachedStartupFunctions = startupFunctions;

#if DBG_DUMP
        if(Configuration::Global.flags.Dump.IsEnabled(DynamicProfilePhase))
        {TRACE_IT(52674);
            Output::Print(_u("Loaded: Startup functions bit vector:"));
            startupFunctions->Dump();
        }
#endif

        for (uint i = 0; i < profileCount; i++)
        {TRACE_IT(52675);
            Js::LocalFunctionId functionId;
            DynamicProfileInfo * dynamicProfileInfo = DynamicProfileInfo::Deserialize(reader, recycler, &functionId);
            if (dynamicProfileInfo == nullptr || functionId >= functionCount)
            {TRACE_IT(52676);
                return nullptr;
            }
            sourceDynamicProfileManager->dynamicProfileInfoMap.Add(functionId, dynamicProfileInfo);
        }
        return sourceDynamicProfileManager;
    }

    template <typename T>
    bool
    SourceDynamicProfileManager::Serialize(T * writer)
    {TRACE_IT(52677);
        // To simulate behavior of in memory profile cache - let's keep functions marked as executed if they were loaded
        // to be so from the profile - this helps with ensure inlined functions are marked as executed.
        if(!this->startupFunctions)
        {TRACE_IT(52678);
            this->startupFunctions = const_cast<BVFixed*>(static_cast<const BVFixed*>(this->cachedStartupFunctions));
        }
        else if(cachedStartupFunctions && this->cachedStartupFunctions->Length() == this->startupFunctions->Length())
        {TRACE_IT(52679);
            this->startupFunctions->Or(cachedStartupFunctions);
        }

        if(this->startupFunctions)
        {TRACE_IT(52680);
#if DBG_DUMP
             if(Configuration::Global.flags.Dump.IsEnabled(DynamicProfilePhase))
            {TRACE_IT(52681);
                Output::Print(_u("Saving: Startup functions bit vector:"));
                this->startupFunctions->Dump();
            }
#endif

            size_t bvSize = BVFixed::GetAllocSize(this->startupFunctions->Length()) ;
            if (!writer->WriteArray((char *)static_cast<BVFixed*>(this->startupFunctions), bvSize)
                || !writer->Write(this->dynamicProfileInfoMapSaving.Count()))
            {TRACE_IT(52682);
                return false;
            }
        }

        for (int i = 0; i < this->dynamicProfileInfoMapSaving.Count(); i++)
        {TRACE_IT(52683);
            DynamicProfileInfo * dynamicProfileInfo = this->dynamicProfileInfoMapSaving.GetValueAt(i);
            if (dynamicProfileInfo == nullptr || !dynamicProfileInfo->HasFunctionBody())
            {TRACE_IT(52684);
                continue;
            }

            if (!dynamicProfileInfo->Serialize(writer))
            {TRACE_IT(52685);
                return false;
            }
        }
        return true;
    }

    void
    SourceDynamicProfileManager::SaveToDynamicProfileStorage(char16 const * url)
    {TRACE_IT(52686);
        Assert(DynamicProfileStorage::IsEnabled());
        BufferSizeCounter counter;
        if (!this->Serialize(&counter))
        {TRACE_IT(52687);
            return;
        }

        if (counter.GetByteCount() > UINT_MAX)
        {TRACE_IT(52688);
            // too big
            return;
        }

        char * record = DynamicProfileStorage::AllocRecord(static_cast<DWORD>(counter.GetByteCount()));
#if DBG_DUMP
        if (PHASE_STATS1(DynamicProfilePhase))
        {TRACE_IT(52689);
            Output::Print(_u("%-180s : %d bytes\n"), url, counter.GetByteCount());
        }
#endif

        BufferWriter writer(DynamicProfileStorage::GetRecordBuffer(record), counter.GetByteCount());
        if (!this->Serialize(&writer))
        {TRACE_IT(52690);
            Assert(false);
            DynamicProfileStorage::DeleteRecord(record);
        }

        DynamicProfileStorage::SaveRecord(url, record);
    }

#endif
};
#endif
