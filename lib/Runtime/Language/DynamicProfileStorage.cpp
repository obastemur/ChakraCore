//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

#ifdef DYNAMIC_PROFILE_STORAGE

bool DynamicProfileStorage::initialized = false;
bool DynamicProfileStorage::uninitialized = false;
bool DynamicProfileStorage::enabled = false;
bool DynamicProfileStorage::useCacheDir = false;
bool DynamicProfileStorage::collectInfo = false;
HANDLE DynamicProfileStorage::mutex = nullptr;
char16 DynamicProfileStorage::cacheDrive[_MAX_DRIVE];
char16 DynamicProfileStorage::cacheDir[_MAX_DIR];
char16 DynamicProfileStorage::catalogFilename[_MAX_PATH];
CriticalSection DynamicProfileStorage::cs;
DynamicProfileStorage::InfoMap DynamicProfileStorage::infoMap(&NoCheckHeapAllocator::Instance);
DynamicProfileStorage::TimeType DynamicProfileStorage::creationTime = DynamicProfileStorage::TimeType();
int32 DynamicProfileStorage::lastOffset = 0;
DWORD const DynamicProfileStorage::MagicNumber = 20100526;
DWORD const DynamicProfileStorage::FileFormatVersion = 2;
DWORD DynamicProfileStorage::nextFileId = 0;
#if DBG
bool DynamicProfileStorage::locked = false;
#endif

class DynamicProfileStorageReaderWriter
{
public:
    DynamicProfileStorageReaderWriter() : filename(nullptr), file(nullptr) {LOGMEIN("DynamicProfileStorage.cpp] 31\n");}
    ~DynamicProfileStorageReaderWriter();
    bool Init(char16 const * filename, char16 const * mode, bool deleteNonClosed, errno_t * err);
    template <typename T>
    bool Read(T * t);
    template <typename T>
    bool ReadArray(T * t, size_t len);

    _Success_(return) bool ReadUtf8String(__deref_out_z char16 ** str, __out DWORD * len);

    template <typename T>
    bool Write(T const& t);
    template <typename T>
    bool WriteArray(T * t, size_t len);

    bool WriteUtf8String(char16 const * str);

    bool Seek(int32 offset);
    bool SeekToEnd();
    int32 Size();
    void Close(bool deleteFile = false);

private:
    char16 const * filename;
    FILE * file;
    bool deleteNonClosed;
};

DynamicProfileStorageReaderWriter::~DynamicProfileStorageReaderWriter()
{LOGMEIN("DynamicProfileStorage.cpp] 60\n");
    if (file)
    {LOGMEIN("DynamicProfileStorage.cpp] 62\n");
        Close(deleteNonClosed);
    }
}

bool DynamicProfileStorageReaderWriter::Init(char16 const * filename, char16 const * mode, bool deleteNonClosed, errno_t * err = nullptr)
{LOGMEIN("DynamicProfileStorage.cpp] 68\n");
    Assert(file == nullptr);
    errno_t e = _wfopen_s(&file, filename, mode);
    if (e != 0)
    {LOGMEIN("DynamicProfileStorage.cpp] 72\n");
        if (err)
        {LOGMEIN("DynamicProfileStorage.cpp] 74\n");
            *err = e;
        }
        return false;
    }
    this->filename = filename;
    this->deleteNonClosed = deleteNonClosed;
    return true;
}

template <typename T>
bool DynamicProfileStorageReaderWriter::Read(T * t)
{LOGMEIN("DynamicProfileStorage.cpp] 86\n");
    return ReadArray(t, 1);
}

template <typename T>
bool DynamicProfileStorageReaderWriter::ReadArray(T * t, size_t len)
{LOGMEIN("DynamicProfileStorage.cpp] 92\n");
    Assert(file);
    int32 pos = ftell(file);
    if (fread(t, sizeof(T), len, file) != len)
    {LOGMEIN("DynamicProfileStorage.cpp] 96\n");
        Output::Print(_u("ERROR: DynamicProfileStorage: '%s': File corrupted at %d\n"), filename, pos);
        Output::Flush();
        return false;
    }
    return true;
}

_Success_(return) bool DynamicProfileStorageReaderWriter::ReadUtf8String(__deref_out_z char16 ** str, __out DWORD * len)
{LOGMEIN("DynamicProfileStorage.cpp] 105\n");
    DWORD urllen;
    if (!Read(&urllen))
    {LOGMEIN("DynamicProfileStorage.cpp] 108\n");
        return false;
    }

    utf8char_t* tempBuffer = NoCheckHeapNewArray(utf8char_t, urllen);
    if (tempBuffer == nullptr)
    {LOGMEIN("DynamicProfileStorage.cpp] 114\n");
        Output::Print(_u("ERROR: DynamicProfileStorage: Out of memory reading '%s'\n"), filename);
        Output::Flush();
        return false;
    }

    if (!ReadArray(tempBuffer, urllen))
    {
        HeapDeleteArray(urllen, tempBuffer);
        return false;
    }

    charcount_t length = utf8::ByteIndexIntoCharacterIndex(tempBuffer, urllen);
    char16 * name = NoCheckHeapNewArray(char16, length + 1);
    if (name == nullptr)
    {LOGMEIN("DynamicProfileStorage.cpp] 129\n");
        Output::Print(_u("ERROR: DynamicProfileStorage: Out of memory reading '%s'\n"), filename);
        Output::Flush();
        HeapDeleteArray(urllen, tempBuffer);
        return false;
    }
    utf8::DecodeUnitsIntoAndNullTerminateNoAdvance(name, tempBuffer, tempBuffer + urllen);
    NoCheckHeapDeleteArray(urllen, tempBuffer);
    *str = name;
    *len = length;
    return true;
}

template <typename T>
bool DynamicProfileStorageReaderWriter::Write(T const& t)
{LOGMEIN("DynamicProfileStorage.cpp] 144\n");
    return WriteArray(&t, 1);
}

template <typename T>
bool DynamicProfileStorageReaderWriter::WriteArray(T * t, size_t len)
{LOGMEIN("DynamicProfileStorage.cpp] 150\n");
    Assert(file);
    if (fwrite(t, sizeof(T), len, file) != len)
    {LOGMEIN("DynamicProfileStorage.cpp] 153\n");
        Output::Print(_u("ERROR: DynamicProfileStorage: Unable to write to file '%s'\n"), filename);
        Output::Flush();
        return false;
    }
    return true;
}

bool DynamicProfileStorageReaderWriter::WriteUtf8String(char16 const * str)
{LOGMEIN("DynamicProfileStorage.cpp] 162\n");
    charcount_t len = static_cast<charcount_t>(wcslen(str));
    utf8char_t * tempBuffer = NoCheckHeapNewArray(utf8char_t, len * 3);
    if (tempBuffer == nullptr)
    {LOGMEIN("DynamicProfileStorage.cpp] 166\n");
        Output::Print(_u("ERROR: DynamicProfileStorage: Out of memory writing to file '%s'\n"), filename);
        Output::Flush();
        return false;
    }
    DWORD cbNeeded = (DWORD)utf8::EncodeInto(tempBuffer, str, len);
    bool success = Write(cbNeeded) && WriteArray(tempBuffer, cbNeeded);
    NoCheckHeapDeleteArray(len * 3, tempBuffer);
    return success;
}

bool DynamicProfileStorageReaderWriter::Seek(int32 offset)
{LOGMEIN("DynamicProfileStorage.cpp] 178\n");
    Assert(file);
    return fseek(file, offset, SEEK_SET) == 0;
}

bool DynamicProfileStorageReaderWriter::SeekToEnd()
{LOGMEIN("DynamicProfileStorage.cpp] 184\n");
    Assert(file);
    return fseek(file, 0, SEEK_END) == 0;
}

int32 DynamicProfileStorageReaderWriter::Size()
{LOGMEIN("DynamicProfileStorage.cpp] 190\n");
    Assert(file);
    int32 current = ftell(file);
    SeekToEnd();
    int32 end = ftell(file);
    fseek(file, current, SEEK_SET);
    return end;
}

void DynamicProfileStorageReaderWriter::Close(bool deleteFile)
{LOGMEIN("DynamicProfileStorage.cpp] 200\n");
    Assert(file);
    fflush(file);
    fclose(file);
    file = nullptr;
    if (deleteFile)
    {LOGMEIN("DynamicProfileStorage.cpp] 206\n");
        _wunlink(filename);
    }
    filename = nullptr;
}

void DynamicProfileStorage::StorageInfo::GetFilename(_Out_writes_z_(_MAX_PATH) char16 filename[_MAX_PATH]) const
{LOGMEIN("DynamicProfileStorage.cpp] 213\n");
    char16 tempFile[_MAX_PATH];
    wcscpy_s(tempFile, _u("jsdpcache_file"));
    _itow_s(this->fileId, tempFile + _countof(_u("jsdpcache_file")) - 1, _countof(tempFile) - _countof(_u("jsdpcache_file")) + 1, 10);
    _wmakepath_s(filename, _MAX_PATH, cacheDrive, cacheDir, tempFile, _u(".dpd"));
}

char const * DynamicProfileStorage::StorageInfo::ReadRecord() const
{LOGMEIN("DynamicProfileStorage.cpp] 221\n");
    char16 cacheFilename[_MAX_PATH];
    this->GetFilename(cacheFilename);
    DynamicProfileStorageReaderWriter reader;
    if (!reader.Init(cacheFilename, _u("rb"), false))
    {LOGMEIN("DynamicProfileStorage.cpp] 226\n");
#if DBG_DUMP
        if (DynamicProfileStorage::DoTrace())
        {LOGMEIN("DynamicProfileStorage.cpp] 229\n");
            Output::Print(_u("TRACE: DynamicProfileStorage: Unable to open cache dir file '%s'"), cacheFilename);
            Output::Flush();
        }
#endif
        return nullptr;
    }

    int32 size = reader.Size();
    char * record = AllocRecord(size);
    if (record == nullptr)
    {LOGMEIN("DynamicProfileStorage.cpp] 240\n");
        Output::Print(_u("ERROR: DynamicProfileStorage: Out of memory reading '%s'"), cacheFilename);
        Output::Flush();
        return nullptr;
    }

    if (!reader.ReadArray(GetRecordBuffer(record), size))
    {LOGMEIN("DynamicProfileStorage.cpp] 247\n");
        DeleteRecord(record);
        return nullptr;
    }
    return record;
}

bool DynamicProfileStorage::StorageInfo::WriteRecord(__in_ecount(sizeof(DWORD) + *record)char const * record) const
{LOGMEIN("DynamicProfileStorage.cpp] 255\n");
    char16 cacheFilename[_MAX_PATH];
    this->GetFilename(cacheFilename);
    DynamicProfileStorageReaderWriter writer;
    if (!writer.Init(cacheFilename, _u("wcb"), true))
    {LOGMEIN("DynamicProfileStorage.cpp] 260\n");
        Output::Print(_u("ERROR: DynamicProfileStorage: Unable open record file '%s'"), cacheFilename);
        Output::Flush();
        return false;
    }
    if (!writer.WriteArray(GetRecordBuffer(record), GetRecordSize(record)))
    {LOGMEIN("DynamicProfileStorage.cpp] 266\n");
        return false;
    }
    // Success
    writer.Close();
    return true;
}

#if DBG_DUMP
bool DynamicProfileStorage::DoTrace()
{LOGMEIN("DynamicProfileStorage.cpp] 276\n");
    return Js::Configuration::Global.flags.Trace.IsEnabled(Js::DynamicProfileStoragePhase);
}
#endif

char16 const * DynamicProfileStorage::GetMessageType()
{LOGMEIN("DynamicProfileStorage.cpp] 282\n");
    if (!DynamicProfileStorage::DoCollectInfo())
    {LOGMEIN("DynamicProfileStorage.cpp] 284\n");
        return _u("WARNING");
    }
#if DBG_DUMP
    if (DynamicProfileStorage::DoTrace())
    {LOGMEIN("DynamicProfileStorage.cpp] 289\n");
        return _u("TRACE");
    }
#endif
    return nullptr;
}

bool DynamicProfileStorage::Initialize()
{
    AssertMsg(!initialized, "Initialize called multiple times");
    if (initialized)
    {LOGMEIN("DynamicProfileStorage.cpp] 300\n");
        return true;
    }

    bool success = true;
    initialized = true;

#ifdef FORCE_DYNAMIC_PROFILE_STORAGE
    enabled = true;
    collectInfo = true;
    if (!SetupCacheDir(nullptr))
    {LOGMEIN("DynamicProfileStorage.cpp] 311\n");
        success = false;
    }

#else
    if (Js::Configuration::Global.flags.IsEnabled(Js::DynamicProfileCacheDirFlag))
    {LOGMEIN("DynamicProfileStorage.cpp] 317\n");
        enabled = true;
        collectInfo = true;
        if (!SetupCacheDir(Js::Configuration::Global.flags.DynamicProfileCacheDir))
        {LOGMEIN("DynamicProfileStorage.cpp] 321\n");
            success = false;
        }
    }
#endif

    // If -DynamicProfileInput is specified, the file specified in -DynamicProfileCache
    // will not be imported and will be overwritten
    if (Js::Configuration::Global.flags.IsEnabled(Js::DynamicProfileInputFlag))
    {LOGMEIN("DynamicProfileStorage.cpp] 330\n");
        enabled = true;
        ClearCacheCatalog();

        // -DynamicProfileInput
        //      Without other -DynamicProfile flags - enable in memory profile cache without exporting
        //      With -DynamicProfileCache           - override the dynamic profile cache file
        //      With -DynamicProfileCacheDir        - clear the dynamic profile cache directory

        if (Js::Configuration::Global.flags.DynamicProfileInput != nullptr)
        {LOGMEIN("DynamicProfileStorage.cpp] 340\n");
            // Error if we can't in the profile info if we are not using a cache file or directory.
            collectInfo = collectInfo || Js::Configuration::Global.flags.IsEnabled(Js::DynamicProfileCacheFlag);

            // Try to open the DynamicProfileInput.
            // If failure to open, retry at 100 ms intervals until a timeout.

            const uint32 MAX_DELAY = 2000;  // delay at most 2 seconds
            const uint32 DELAY_INTERVAL = 100;
            const uint32 MAX_TRIES = MAX_DELAY / DELAY_INTERVAL;
            bool readSuccessful = false;

            for (uint32 i = 0; i < MAX_TRIES; i++)
            {LOGMEIN("DynamicProfileStorage.cpp] 353\n");
                readSuccessful = ImportFile(Js::Configuration::Global.flags.DynamicProfileInput, false);
                if (readSuccessful)
                {LOGMEIN("DynamicProfileStorage.cpp] 356\n");
                    break;
                }

                Sleep(DELAY_INTERVAL);
                if (Js::Configuration::Global.flags.Verbose)
                {LOGMEIN("DynamicProfileStorage.cpp] 362\n");
                    Output::Print(_u("  Retrying load of dynamic profile from '%s' (attempt %d)...\n"),
                        (char16 const *)Js::Configuration::Global.flags.DynamicProfileInput, i + 1);
                    Output::Flush();
                }
            }

            if (!readSuccessful)
            {LOGMEIN("DynamicProfileStorage.cpp] 370\n");
                // If file cannot be read, behave as if DynamicProfileInput == null.
                collectInfo = true;
            }
        }
        else
        {
            // Don't error if we can't find the profile info
            collectInfo = true;
        }
    }
    else if (Js::Configuration::Global.flags.IsEnabled(Js::DynamicProfileCacheFlag))
    {LOGMEIN("DynamicProfileStorage.cpp] 382\n");
        enabled = true;
        collectInfo = true;
        if (Js::Configuration::Global.flags.DynamicProfileCache)
        {LOGMEIN("DynamicProfileStorage.cpp] 386\n");
            if (!ImportFile(Js::Configuration::Global.flags.DynamicProfileCache, true))
            {LOGMEIN("DynamicProfileStorage.cpp] 388\n");
                success = false;
            }
        }
    }

    return success;
}

// We used to have problem with dynamic profile being corrupt and this is to verify it.
// We don't see this any more so we will just disable it to speed up unittest
#if 0
#if DBG && defined(_M_AMD64)
#define DYNAMIC_PROFILE_EXPORT_FILE_CHECK
#endif
#endif

bool DynamicProfileStorage::Uninitialize()
{
    AssertMsg(!uninitialized, "Uninitialize called multiple times");
    if (!initialized || uninitialized)
    {LOGMEIN("DynamicProfileStorage.cpp] 409\n");
        return true;
    }
#ifdef DYNAMIC_PROFILE_EXPORT_FILE_CHECK
    bool exportFile = false;
#endif

    uninitialized = true;
    bool success = true;
    if (Js::Configuration::Global.flags.DynamicProfileCache != nullptr)
    {LOGMEIN("DynamicProfileStorage.cpp] 419\n");
        Assert(enabled);
        if (!ExportFile(Js::Configuration::Global.flags.DynamicProfileCache))
        {LOGMEIN("DynamicProfileStorage.cpp] 422\n");
            success = false;
        }
#ifdef DYNAMIC_PROFILE_EXPORT_FILE_CHECK
        exportFile = true;
#endif
    }

    if (mutex != nullptr)
    {LOGMEIN("DynamicProfileStorage.cpp] 431\n");
        CloseHandle(mutex);
    }
#ifdef DYNAMIC_PROFILE_EXPORT_FILE_CHECK
    uint32 oldCount = infoMap.Count();
#endif

    ClearInfoMap(false);
#ifdef DYNAMIC_PROFILE_EXPORT_FILE_CHECK
    if (exportFile)
    {LOGMEIN("DynamicProfileStorage.cpp] 441\n");
        HRESULT hr;
        BEGIN_TRANSLATE_EXCEPTION_AND_ERROROBJECT_TO_HRESULT
        {LOGMEIN("DynamicProfileStorage.cpp] 444\n");
            if (!ImportFile(Js::Configuration::Global.flags.DynamicProfileCache, false))
            {LOGMEIN("DynamicProfileStorage.cpp] 446\n");
                success = false;
            }
            Assert(oldCount == infoMap.Count());
        }
        END_TRANSLATE_EXCEPTION_AND_ERROROBJECT_TO_HRESULT(hr)
        ClearInfoMap(false);
    }
#endif

    return success;
}

void DynamicProfileStorage::ClearInfoMap(bool deleteFileStorage)
{LOGMEIN("DynamicProfileStorage.cpp] 460\n");
    uint recordCount = infoMap.Count();
    uint recordFreed = 0;
    for (uint i = 0; recordFreed < recordCount; i++)
    {LOGMEIN("DynamicProfileStorage.cpp] 464\n");
        char16 const * name = infoMap.GetKeyAt(i);
        if (name == nullptr)
        {LOGMEIN("DynamicProfileStorage.cpp] 467\n");
            continue;
        }
        NoCheckHeapDeleteArray(wcslen(name) + 1, name);

        StorageInfo const& info = infoMap.GetValueAt(i);
        if (info.isFileStorage)
        {LOGMEIN("DynamicProfileStorage.cpp] 474\n");
            Assert(useCacheDir);
            if (deleteFileStorage)
            {LOGMEIN("DynamicProfileStorage.cpp] 477\n");
                char16 filename[_MAX_PATH];
                info.GetFilename(filename);
                _wunlink(filename);
            }
        }
        else
        {
            DeleteRecord(info.record);
        }

        recordFreed++;
    }
    infoMap.Clear();
}

bool DynamicProfileStorage::ImportFile(__in_z char16 const * filename, bool allowNonExistingFile)
{LOGMEIN("DynamicProfileStorage.cpp] 494\n");
    Assert(enabled);
    DynamicProfileStorageReaderWriter reader;
    errno_t e;
    if (!reader.Init(filename, _u("rb"), false, &e))
    {LOGMEIN("DynamicProfileStorage.cpp] 499\n");
        if (allowNonExistingFile)
        {LOGMEIN("DynamicProfileStorage.cpp] 501\n");
            return true;
        }
        else
        {
            if (Js::Configuration::Global.flags.Verbose)
            {LOGMEIN("DynamicProfileStorage.cpp] 507\n");
                Output::Print(_u("ERROR: DynamicProfileStorage: Unable to open file '%s' to import (%d)\n"), filename, e);

                char16 error_string[256];
                _wcserror_s(error_string, e);
                Output::Print(_u("ERROR:   For file '%s': %s (%d)\n"), filename, error_string, e);
                Output::Flush();
            }
            return false;
        }
    }

    DWORD magic;
    DWORD version;
    DWORD recordCount;
    if (!reader.Read(&magic)
        || !reader.Read(&version)
        || !reader.Read(&recordCount))
    {LOGMEIN("DynamicProfileStorage.cpp] 525\n");
        return false;
    }

    if (magic != MagicNumber)
    {LOGMEIN("DynamicProfileStorage.cpp] 530\n");
        Output::Print(_u("ERROR: DynamicProfileStorage: '%s' is not a dynamic profile data file"), filename);
        Output::Flush();
        return false;
    }
    if (version != FileFormatVersion)
    {LOGMEIN("DynamicProfileStorage.cpp] 536\n");
        if (allowNonExistingFile)
        {LOGMEIN("DynamicProfileStorage.cpp] 538\n");
            // Treat version mismatch as non-existent file
            return true;
        }
        Output::Print(_u("ERROR: DynamicProfileStorage: '%s' has format version %d; version %d expected"), filename,
            version, FileFormatVersion);
        Output::Flush();
        return false;
    }

    for (uint i = 0; i < recordCount; i++)
    {LOGMEIN("DynamicProfileStorage.cpp] 549\n");
        DWORD len;
        char16 * name;
        if (!reader.ReadUtf8String(&name, &len))
        {LOGMEIN("DynamicProfileStorage.cpp] 553\n");
            Assert(false);
            return false;
        }

        DWORD recordLen;
        if (!reader.Read(&recordLen))
        {LOGMEIN("DynamicProfileStorage.cpp] 560\n");
            Assert(false);
            return false;
        }

        char * record = AllocRecord(recordLen);
        if (record == nullptr)
        {LOGMEIN("DynamicProfileStorage.cpp] 567\n");
            Output::Print(_u("ERROR: DynamicProfileStorage: Out of memory importing '%s'\n"), filename);
            Output::Flush();
            NoCheckHeapDeleteArray(len + 1, name);
            return false;
        }

        if (!reader.ReadArray(GetRecordBuffer(record), recordLen))
        {LOGMEIN("DynamicProfileStorage.cpp] 575\n");
            NoCheckHeapDeleteArray(len + 1, name);
            DeleteRecord(record);
            Assert(false);
            return false;
        }

        SaveRecord(name, record);

        // Save record will make a copy of the name
        NoCheckHeapDeleteArray(len + 1, name);
    }
#if DBG_DUMP
    if (DynamicProfileStorage::DoTrace())
    {LOGMEIN("DynamicProfileStorage.cpp] 589\n");
        Output::Print(_u("TRACE: DynamicProfileStorage: Imported file: '%s'\n"), filename);
        Output::Flush();
    }
#endif
    AssertMsg(recordCount == (uint)infoMap.Count(), "failed to read all the records");
    return true;
}

bool DynamicProfileStorage::ExportFile(__in_z char16 const * filename)
{LOGMEIN("DynamicProfileStorage.cpp] 599\n");
    Assert(enabled);

    if (useCacheDir && AcquireLock())
    {LOGMEIN("DynamicProfileStorage.cpp] 603\n");
        if (!LoadCacheCatalog()) // refresh the cache catalog
        {LOGMEIN("DynamicProfileStorage.cpp] 605\n");
            ReleaseLock();
            Assert(FALSE);
            return false;
        }
    }

    DynamicProfileStorageReaderWriter writer;

    if (!writer.Init(filename, _u("wcb"), true))
    {LOGMEIN("DynamicProfileStorage.cpp] 615\n");
        Output::Print(_u("ERROR: DynamicProfileStorage: Unable to open file '%s' to export\n"), filename);
        Output::Flush();
        return false;
    }
    DWORD recordCount = infoMap.Count();
    if (!writer.Write(MagicNumber)
        || !writer.Write(FileFormatVersion)
        || !writer.Write(recordCount))
    {LOGMEIN("DynamicProfileStorage.cpp] 624\n");
        Assert(FALSE);
        return false;
    }
    uint recordWritten = 0;
    for (uint i = 0; recordWritten < recordCount; i++)
    {LOGMEIN("DynamicProfileStorage.cpp] 630\n");
        char16 const * url = infoMap.GetKeyAt(i);
        if (url == nullptr)
        {LOGMEIN("DynamicProfileStorage.cpp] 633\n");
            Assert(false);
            continue;
        }

        StorageInfo const& info = infoMap.GetValueAt(i);

        char const * record;
        if (info.isFileStorage)
        {LOGMEIN("DynamicProfileStorage.cpp] 642\n");
            Assert(useCacheDir);
            record = info.ReadRecord();
            if (record == nullptr)
            {LOGMEIN("DynamicProfileStorage.cpp] 646\n");
                ReleaseLock();
                Assert(FALSE);
                return false;
            }
        }
        else
        {
            Assert(!useCacheDir);
            Assert(!locked);
            record = info.record;
        }
        DWORD recordSize = GetRecordSize(record);

        bool failed = (!writer.WriteUtf8String(url)
            || !writer.Write(recordSize)
            || !writer.WriteArray(GetRecordBuffer(record), recordSize));

        if (useCacheDir)
        {LOGMEIN("DynamicProfileStorage.cpp] 665\n");
            DeleteRecord(record);
        }
        if (failed)
        {LOGMEIN("DynamicProfileStorage.cpp] 669\n");
            if (useCacheDir)
            {LOGMEIN("DynamicProfileStorage.cpp] 671\n");
                ReleaseLock();
            }
            Assert(FALSE);
            return false;
        }

        recordWritten++;
    }
    writer.Close();
#if DBG_DUMP
    if (DynamicProfileStorage::DoTrace())
    {LOGMEIN("DynamicProfileStorage.cpp] 683\n");
        Output::Print(_u("TRACE: DynamicProfileStorage: Exported file: '%s'\n"), filename);
        Output::Flush();
    }
#endif
    return true;
}

void DynamicProfileStorage::DisableCacheDir()
{LOGMEIN("DynamicProfileStorage.cpp] 692\n");
    Assert(useCacheDir);
    ClearInfoMap(false);
    useCacheDir = false;
#ifdef FORCE_DYNAMIC_PROFILE_STORAGE
    Js::Throw::FatalInternalError();
#endif
}

bool DynamicProfileStorage::AcquireLock()
{LOGMEIN("DynamicProfileStorage.cpp] 702\n");
    Assert(mutex != nullptr);
    Assert(!locked);
    DWORD ret = WaitForSingleObject(mutex, INFINITE);
    if (ret == WAIT_OBJECT_0 || ret == WAIT_ABANDONED)
    {LOGMEIN("DynamicProfileStorage.cpp] 707\n");
#if DBG
        locked = true;
#endif
        return true;
    }
    Output::Print(_u("ERROR: DynamicProfileStorage: Unable to acquire mutex %d\n"), ret);
    Output::Flush();
    DisableCacheDir();

    return false;
}

bool DynamicProfileStorage::ReleaseLock()
{LOGMEIN("DynamicProfileStorage.cpp] 721\n");
    Assert(locked);
    Assert(mutex != nullptr);
#if DBG
    locked = false;
#endif
    if (ReleaseMutex(mutex))
    {LOGMEIN("DynamicProfileStorage.cpp] 728\n");
        return true;
    }
    DisableCacheDir();
    Output::Print(_u("ERROR: DynamicProfileStorage: Unable to release mutex"));
    Output::Flush();
    return false;
}

bool DynamicProfileStorage::SetupCacheDir(__in_z char16 const * dirname)
{LOGMEIN("DynamicProfileStorage.cpp] 738\n");
    Assert(enabled);

    mutex = CreateMutex(NULL, FALSE, _u("JSDPCACHE"));
    if (mutex == nullptr)
    {LOGMEIN("DynamicProfileStorage.cpp] 743\n");
        Output::Print(_u("ERROR: DynamicProfileStorage: Unable to create mutex"));
        Output::Flush();
        return false;
    }

    useCacheDir = true;
    if (!AcquireLock())
    {LOGMEIN("DynamicProfileStorage.cpp] 751\n");
        return false;
    }

    char16 tempPath[_MAX_PATH];
    if (dirname == nullptr)
    {LOGMEIN("DynamicProfileStorage.cpp] 757\n");
        uint32 len = GetTempPath(_MAX_PATH, tempPath);
        if (len >= _MAX_PATH || wcscat_s(tempPath, _u("jsdpcache")) != 0)
        {LOGMEIN("DynamicProfileStorage.cpp] 760\n");
            DisableCacheDir();
            Output::Print(_u("ERROR: DynamicProfileStorage: Can't setup cache directory: Unable to create directory\n"));
            Output::Flush();
            ReleaseLock();
            return false;
        }

        if (!CreateDirectory(tempPath, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        {LOGMEIN("DynamicProfileStorage.cpp] 769\n");
            DisableCacheDir();
            Output::Print(_u("ERROR: DynamicProfileStorage: Can't setup cache directory: Unable to create directory\n"));
            Output::Flush();
            ReleaseLock();
            return false;
        }
        dirname = tempPath;
    }

    char16 cacheFile[_MAX_FNAME];
    char16 cacheExt[_MAX_EXT];
    _wsplitpath_s(dirname, cacheDrive, cacheDir, cacheFile, cacheExt);
    wcscat_s(cacheDir, cacheFile);
    wcscat_s(cacheDir, cacheExt);

    _wmakepath_s(catalogFilename, cacheDrive, cacheDir, _u("jsdpcache_master"), _u(".dpc"));
    bool succeed = LoadCacheCatalog();
    ReleaseLock();

    return succeed;
}

bool DynamicProfileStorage::CreateCacheCatalog()
{LOGMEIN("DynamicProfileStorage.cpp] 793\n");
    Assert(enabled);
    Assert(useCacheDir);
    Assert(locked);
    nextFileId = 0;
    creationTime = GetCreationTime();
    DynamicProfileStorageReaderWriter catalogFile;
    if (!catalogFile.Init(catalogFilename, _u("wb"), true)
        || !catalogFile.Write(MagicNumber)
        || !catalogFile.Write(FileFormatVersion)
        || !catalogFile.Write(creationTime)
        || !catalogFile.Write(0)) // count
    {LOGMEIN("DynamicProfileStorage.cpp] 805\n");
        DisableCacheDir();
        Output::Print(_u("ERROR: DynamicProfileStorage: Unable to create cache catalog\n"));
        Output::Flush();
        return false;
    }
    lastOffset = catalogFile.Size();
    ClearInfoMap(true);
    catalogFile.Close();

#if DBG_DUMP
    if (DynamicProfileStorage::DoTrace())
    {LOGMEIN("DynamicProfileStorage.cpp] 817\n");
        Output::Print(_u("TRACE: DynamicProfileStorage: Cache directory catalog created: '%s'\n"), catalogFilename);
        Output::Flush();
    }
#endif
    return true;
}

bool DynamicProfileStorage::AppendCacheCatalog(__in_z char16 const * url)
{LOGMEIN("DynamicProfileStorage.cpp] 826\n");
    Assert(enabled);
    Assert(useCacheDir);
    Assert(locked);
    DWORD magic;
    DWORD version;
    DWORD count;
    DWORD time;
    DynamicProfileStorageReaderWriter catalogFile;
    if (!catalogFile.Init(catalogFilename, _u("rcb+"), false))
    {LOGMEIN("DynamicProfileStorage.cpp] 836\n");
        return CreateCacheCatalog();
    }

    if (!catalogFile.Seek(0)
        || !catalogFile.Read(&magic)
        || !catalogFile.Read(&version)
        || !catalogFile.Read(&time)
        || !catalogFile.Read(&count)
        || magic != MagicNumber
        || version < FileFormatVersion)
    {LOGMEIN("DynamicProfileStorage.cpp] 847\n");
        catalogFile.Close();
#if DBG_DUMP
        if (DynamicProfileStorage::DoTrace())
        {LOGMEIN("DynamicProfileStorage.cpp] 851\n");
            Output::Print(_u("TRACE: DynamicProfileStorage: Overwriting file for cache directory catalog: '%s'\n"), catalogFilename);
            Output::Flush();
        }
#endif
        return CreateCacheCatalog();
    }

    if (version > FileFormatVersion)
    {LOGMEIN("DynamicProfileStorage.cpp] 860\n");
        DisableCacheDir();
        Output::Print(_u("ERROR: DynamicProfileStorage: Existing cache catalog has a newer format\n"));
        Output::Flush();
        return false;
    }

    if (time != creationTime || count + 1 != nextFileId)
    {LOGMEIN("DynamicProfileStorage.cpp] 868\n");
        // This should not happen, as we are under lock from the LoadCacheCatalog
        DisableCacheDir();
        Output::Print(_u("ERROR: DynamicProfileStorage: Internal error, file modified under lock\n"));
        Output::Flush();
        return false;
    }

    if (!catalogFile.SeekToEnd() ||
        !catalogFile.WriteUtf8String(url) ||
        !catalogFile.Seek(3 * sizeof(DWORD)) ||
        !catalogFile.Write(nextFileId))
    {LOGMEIN("DynamicProfileStorage.cpp] 880\n");
#if DBG_DUMP
        if (DynamicProfileStorage::DoTrace())
        {LOGMEIN("DynamicProfileStorage.cpp] 883\n");
            Output::Print(_u("TRACE: DynamicProfileStorage: Write failure. Cache directory catalog corrupted: '%s'\n"), catalogFilename);
            Output::Flush();
        }
#endif
        catalogFile.Close();
        return CreateCacheCatalog();
    }

    lastOffset = catalogFile.Size();
    return true;
}

bool DynamicProfileStorage::LoadCacheCatalog()
{LOGMEIN("DynamicProfileStorage.cpp] 897\n");
    Assert(enabled);
    Assert(useCacheDir);
    Assert(locked);
    DynamicProfileStorageReaderWriter catalogFile;
    DWORD magic;
    DWORD version;
    DWORD count;
    DWORD time;
    if (!catalogFile.Init(catalogFilename, _u("rb"), false))
    {LOGMEIN("DynamicProfileStorage.cpp] 907\n");
        return CreateCacheCatalog();
    }
    if (!catalogFile.Read(&magic)
        || !catalogFile.Read(&version)
        || !catalogFile.Read(&time)
        || !catalogFile.Read(&count)
        || magic != MagicNumber
        || version < FileFormatVersion)
    {LOGMEIN("DynamicProfileStorage.cpp] 916\n");
        catalogFile.Close();
#if DBG_DUMP
        if (DynamicProfileStorage::DoTrace())
        {LOGMEIN("DynamicProfileStorage.cpp] 920\n");
            Output::Print(_u("TRACE: DynamicProfileStorage: Overwriting file for cache directory catalog: '%s'\n"), catalogFilename);
            Output::Flush();
        }
#endif
        return CreateCacheCatalog();
    }

    if (version > FileFormatVersion)
    {LOGMEIN("DynamicProfileStorage.cpp] 929\n");
        DisableCacheDir();
        Output::Print(_u("ERROR: DynamicProfileStorage: Existing cache catalog has a newer format.\n"));
        Output::Flush();
        return false;
    }

    DWORD start = 0;

    Assert(useCacheDir);
    if (time == creationTime)
    {LOGMEIN("DynamicProfileStorage.cpp] 940\n");
        // We can reuse existing data
        start = infoMap.Count();
        Assert(count >= start);
        Assert(catalogFile.Size() >= lastOffset);
        if (count == nextFileId)
        {LOGMEIN("DynamicProfileStorage.cpp] 946\n");
            Assert(catalogFile.Size() == lastOffset);
            return true;
        }

        if (!catalogFile.Seek(lastOffset))
        {LOGMEIN("DynamicProfileStorage.cpp] 952\n");
            catalogFile.Close();
            Output::Print(_u("ERROR: DynamicProfileStorage: Unable to seek to last known offset\n"));
            Output::Flush();
            return CreateCacheCatalog();
        }
    }
    else if (creationTime != 0)
    {LOGMEIN("DynamicProfileStorage.cpp] 960\n");
        Output::Print(_u("WARNING: DynamicProfileStorage: Reloading full catalog\n"));
        Output::Flush();
    }

    for (DWORD i = start; i < count; i++)
    {LOGMEIN("DynamicProfileStorage.cpp] 966\n");
        DWORD len;
        char16 * url;
        if (!catalogFile.ReadUtf8String(&url, &len))
        {LOGMEIN("DynamicProfileStorage.cpp] 970\n");
#if DBG_DUMP
            if (DynamicProfileStorage::DoTrace())
            {LOGMEIN("DynamicProfileStorage.cpp] 973\n");
                Output::Print(_u("TRACE: DynamicProfileStorage: Cache dir catalog file corrupted: '%s'\n"), catalogFilename);
                Output::Flush();
            }
#endif
            // REVIEW: the file is corrupted, should we not flush the cache totally?
            catalogFile.Close();
            return CreateCacheCatalog();
        }

        StorageInfo * oldInfo;
        if (infoMap.TryGetReference(url, &oldInfo))
        {LOGMEIN("DynamicProfileStorage.cpp] 985\n");
            Assert(oldInfo->isFileStorage);
            oldInfo->fileId = i;
        }
        else
        {
            StorageInfo newInfo;
            newInfo.isFileStorage = true;
            newInfo.fileId = i;
            infoMap.Add(url, newInfo);
        }
    }

#if DBG_DUMP
    if (creationTime == 0 && DynamicProfileStorage::DoTrace())
    {LOGMEIN("DynamicProfileStorage.cpp] 1000\n");
        Output::Print(_u("TRACE: DynamicProfileStorage: Cache directory catalog loaded: '%s'\n"), catalogFilename);
        Output::Flush();
    }
#endif

    nextFileId = count;
    creationTime = time;
    lastOffset = catalogFile.Size();
    return true;
}

void DynamicProfileStorage::ClearCacheCatalog()
{LOGMEIN("DynamicProfileStorage.cpp] 1013\n");
    Assert(enabled);
    if (useCacheDir)
    {LOGMEIN("DynamicProfileStorage.cpp] 1016\n");
        if (!AcquireLock())
        {LOGMEIN("DynamicProfileStorage.cpp] 1018\n");
            return;
        }
        bool success = CreateCacheCatalog();
        ReleaseLock();
        if (success)
        {LOGMEIN("DynamicProfileStorage.cpp] 1024\n");
#if DBG_DUMP
            if (DynamicProfileStorage::DoTrace())
            {LOGMEIN("DynamicProfileStorage.cpp] 1027\n");
                Output::Print(_u("TRACE: DynamicProfileStorage: Cache dir clears\n"));
                Output::Flush();
            }
#endif
            return;
        }
    }
    else
    {
        ClearInfoMap(false);
    }
}

void DynamicProfileStorage::SaveRecord(__in_z char16 const * filename, __in_ecount(sizeof(DWORD) + *record) char const * record)
{LOGMEIN("DynamicProfileStorage.cpp] 1042\n");
    Assert(enabled);
    AutoCriticalSection autocs(&cs);

    StorageInfo * info;

    if (useCacheDir && AcquireLock())
    {LOGMEIN("DynamicProfileStorage.cpp] 1049\n");
        if (!LoadCacheCatalog()) // refresh the cache catalog
        {LOGMEIN("DynamicProfileStorage.cpp] 1051\n");
            ReleaseLock();
        }
    }

    if (infoMap.TryGetReference(filename, &info))
    {LOGMEIN("DynamicProfileStorage.cpp] 1057\n");
        if (!info->isFileStorage)
        {LOGMEIN("DynamicProfileStorage.cpp] 1059\n");
            Assert(!useCacheDir);
            if (info->record != nullptr)
            {LOGMEIN("DynamicProfileStorage.cpp] 1062\n");
                DeleteRecord(info->record);
            }
            info->record = record;
            return;
        }
        Assert(useCacheDir);

        char16 cacheFilename[_MAX_PATH];
        info->GetFilename(cacheFilename);
        DynamicProfileStorageReaderWriter writer;
        if (info->WriteRecord(record))
        {LOGMEIN("DynamicProfileStorage.cpp] 1074\n");
            // Success
            ReleaseLock();
            return;
        }

        // Fail, try to add it again
        info->fileId = nextFileId++;
        if (info->WriteRecord(record))
        {LOGMEIN("DynamicProfileStorage.cpp] 1083\n");
            if (AppendCacheCatalog(filename))
            {LOGMEIN("DynamicProfileStorage.cpp] 1085\n");
                ReleaseLock();
                return;
            }
        }
        else
        {
            DisableCacheDir();
        }

        // Can't add a new file. Disable and use memory mode
        Assert(!useCacheDir);
        ReleaseLock();
    }

    size_t len = wcslen(filename) + 1;
    char16 * newFilename = NoCheckHeapNewArray(char16, len);
    if (newFilename == nullptr)
    {
        // out of memory, don't save anything
        AssertMsg(false, "OOM");
        DeleteRecord(record);
        if (useCacheDir)
        {LOGMEIN("DynamicProfileStorage.cpp] 1108\n");
            ReleaseLock();
        }
        return;
    }
    wmemcpy_s(newFilename, len, filename, len);

    StorageInfo newInfo;
    if (useCacheDir)
    {LOGMEIN("DynamicProfileStorage.cpp] 1117\n");
        newInfo.isFileStorage = true;
        newInfo.fileId = nextFileId++;

        if (newInfo.WriteRecord(record))
        {LOGMEIN("DynamicProfileStorage.cpp] 1122\n");
            infoMap.Add(newFilename, newInfo);
            if (AppendCacheCatalog(newFilename))
            {LOGMEIN("DynamicProfileStorage.cpp] 1125\n");
                ReleaseLock();
                return;
            }
        }

        // Can't even add a record. Disable and use memory mode
        DisableCacheDir();
        ReleaseLock();
    }

    Assert(!useCacheDir);
    Assert(!locked);

    newInfo.isFileStorage = false;
    newInfo.record = record;
    infoMap.Add(newFilename, newInfo);
}

char * DynamicProfileStorage::AllocRecord(DWORD bufferSize)
{LOGMEIN("DynamicProfileStorage.cpp] 1145\n");
    Assert(enabled);
    char * buffer = NoCheckHeapNewArray(char, bufferSize + sizeof(DWORD));
    if (buffer != nullptr)
    {LOGMEIN("DynamicProfileStorage.cpp] 1149\n");
        *(DWORD *)buffer = bufferSize;
    }
    return buffer;
}

DWORD DynamicProfileStorage::GetRecordSize(__in_ecount(sizeof(DWORD) + *buffer) char const * buffer)
{LOGMEIN("DynamicProfileStorage.cpp] 1156\n");
    Assert(enabled);
    return *(DWORD *)buffer;
}

char const * DynamicProfileStorage::GetRecordBuffer(__in_ecount(sizeof(DWORD) + *buffer) char const * buffer)
{LOGMEIN("DynamicProfileStorage.cpp] 1162\n");
    Assert(enabled);
    return buffer + sizeof(DWORD);
}

char * DynamicProfileStorage::GetRecordBuffer(__in_ecount(sizeof(DWORD) + *buffer) char * buffer)
{LOGMEIN("DynamicProfileStorage.cpp] 1168\n");
    Assert(enabled);
    return buffer + sizeof(DWORD);
}

void DynamicProfileStorage::DeleteRecord(__in_ecount(sizeof(DWORD) + *buffer) char const * buffer)
{LOGMEIN("DynamicProfileStorage.cpp] 1174\n");
    Assert(enabled);
    NoCheckHeapDeleteArray(GetRecordSize(buffer) + sizeof(DWORD), buffer);
}
#endif
