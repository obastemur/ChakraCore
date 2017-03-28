//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
//

/*++



Module Name:

    init/pal.cpp

Abstract:

    Implementation of PAL exported functions not part of the Win32 API.



--*/

#include "pal/thread.hpp"
#include "pal/synchobjects.hpp"
#include "pal/procobj.hpp"
#include "pal/cs.hpp"
#include "pal/file.hpp"
#include "pal/map.hpp"
#include "../objmgr/shmobjectmanager.hpp"
#include "pal/palinternal.h"
#include "pal/dbgmsg.h"
#include "pal/shmemory.h"
#include "pal/process.h"
#include "../thread/procprivate.hpp"
#include "pal/module.h"
#include "pal/virtual.h"
#include "pal/misc.h"
#include "pal/utils.h"
#include "pal/debug.h"
#include "pal/locale.h"
#include "pal/init.h"

#if HAVE_MACH_EXCEPTIONS
#include "../exception/machexception.h"
#else
#include "../exception/signal.hpp"
#endif

#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>

#if HAVE_POLL
#include <poll.h>
#else
#include "pal/fakepoll.h"
#endif  // HAVE_POLL

#if defined(__APPLE__)
#include <sys/sysctl.h>
int CacheLineSize;
#endif //__APPLE__

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif // __APPLE__

using namespace CorUnix;

//
// $$TODO The C++ compiler doesn't like pal/cruntime.h so duplicate the
// necessary prototype here
//

extern "C" BOOL CRTInitStdStreams( void );


SET_DEFAULT_DEBUG_CHANNEL(PAL);

Volatile<INT> init_count PAL_GLOBAL = 0;
Volatile<BOOL> shutdown_intent PAL_GLOBAL = 0;
Volatile<LONG> g_chakraCoreInitialized PAL_GLOBAL = 0;
static BOOL g_fThreadDataAvailable = FALSE;
static pthread_mutex_t init_critsec_mutex = PTHREAD_MUTEX_INITIALIZER;

/* critical section to protect access to init_count. This is allocated on the
   very first PAL_Initialize call, and is freed afterward. */
static PCRITICAL_SECTION init_critsec = NULL;

static LPWSTR INIT_FormatCommandLine (int argc, const char * const *argv);
static LPWSTR INIT_FindEXEPath(LPCSTR exe_name);

#ifdef _DEBUG
extern void PROCDumpThreadList(void);
#endif

// ***** LOGGING IMPL. STARTS *****
#include <stdarg.h>
#define FIRST_LOG_THRESHOLD 2 * 1024 * 1024
#define SKIP_FIRST 1 * 32 * 1024
char LOGS[FIRST_LOG_THRESHOLD * 2];
const size_t MAX_LOG_SIZE = FIRST_LOG_THRESHOLD * 4;

long last_log = 0;
bool skipped_enough = false;
size_t total_bytes = 0;
bool no_more_log = false;

#define RE_FORMAT_LOGMEIN()   \
  va_list args;               \
  va_start(args, fmt);        \
  size_t len_log = snprintf(log, 12048, fmt, args); \
  va_end(args);               \
  if (len_log == -1)          \
      abort();                \
  log[len_log] = 0

void TRACE_IT3(const char* fmt, ...) {
  if (no_more_log) return;

  char log[12048];
  size_t len = 0;

  if (last_log == -1) {
    RE_FORMAT_LOGMEIN();
    fprintf(stdout, "%s", log);
    total_bytes += strlen(log);
    if (total_bytes > MAX_LOG_SIZE) {
      fprintf(stdout, "NO_MORE_LOGS > 10Mb \n");
      no_more_log = true;
    }
    fflush(stderr);
    return;
  }

  if (skipped_enough)
  {
    RE_FORMAT_LOGMEIN();

    for (; len < len_log; len++ ) {
      LOGS[last_log + len] = log[len];
    }
  }
  else
  {
    for (; fmt[len] != 0; len++ ) {
      LOGS[last_log + len] = fmt[len];
    }
  }

  if (!skipped_enough) {
    total_bytes += len;
    if (total_bytes > SKIP_FIRST) {
      skipped_enough = true;
      total_bytes = 0;
    } else {
      last_log = 0;
      return;
    }
  }
  last_log += len;
  if (last_log > FIRST_LOG_THRESHOLD) {
    LOGS[last_log] = 0;
    fprintf(stdout, "%s", LOGS);
    total_bytes += last_log;
    last_log = -1;
  }
}

static thread_local char tlogs[8192];
static thread_local int tlog_count = 0;
static int thread_counter = 1;
static thread_local int thread_id = 0;
void TRACE_IT2(const char* fmt, ...) {
  if (no_more_log) return;
  if (thread_id == 0) thread_id = thread_counter++;

  tlogs[tlog_count++] = ((int)'0') + thread_id;
  tlogs[tlog_count++] = ']';
  size_t len = 0;
  for (; fmt[len] != 0; len++ ) {
    tlogs[tlog_count + len] = fmt[len];
  }

  tlog_count += len;

  if (tlogs[tlog_count - 1] != '\n') {
    tlogs[tlog_count++] = '\n';
  }

  if (tlog_count > 5000) {
    char lll[8000];
    for(int i=0;i<tlog_count;i++)lll[i] = tlogs[i];
    lll[tlog_count] = 0;
    tlog_count = 0;

    TRACE_IT3(lll);
  }
}
#undef RE_FORMAT_LOGMEIN
// ***** LOGGING IMPL. ENDS *****

/*++
Function:
  Initialize

Abstract:
  Common PAL initialization function.

Return:
  0 if successful
  -1 if it failed

--*/
static int
Initialize()
{
    PAL_ERROR palError = ERROR_GEN_FAILURE;
    CPalThread *pThread = NULL;
    CSharedMemoryObjectManager *pshmom = NULL;
    int retval = -1;
    bool fFirstTimeInit = false;

    /* the first ENTRY within the first call to PAL_Initialize is a special
       case, since debug channels are not initialized yet. So in that case the
       ENTRY will be called after the DBG channels initialization */
    ENTRY_EXTERNAL("PAL_Initialize()\n");

    /*Firstly initiate a lastError */
    SetLastError(ERROR_GEN_FAILURE);

    CriticalSectionSubSysInitialize();

    if(NULL == init_critsec)
    {
        pthread_mutex_lock(&init_critsec_mutex); // prevents race condition of two threads
                                                 // initializing the critical section.
        if(NULL == init_critsec)
        {
            static CRITICAL_SECTION temp_critsec;

            // Want this critical section to NOT be internal to avoid the use of unsafe region markers.
            InternalInitializeCriticalSectionAndSpinCount(&temp_critsec, 0, false);

            if(NULL != InterlockedCompareExchangePointer(&init_critsec, &temp_critsec, NULL))
            {
                // Another thread got in before us! shouldn't happen, if the PAL
                // isn't initialized there shouldn't be any other threads
                WARN("Another thread initialized the critical section\n");
                InternalDeleteCriticalSection(&temp_critsec);
            }
        }
        pthread_mutex_unlock(&init_critsec_mutex);
    }

    InternalEnterCriticalSection(pThread, init_critsec); // here pThread is always NULL

    if (init_count == 0)
    {
        // Set our pid.
        gPID = getpid();

        fFirstTimeInit = true;

        // Initialize the TLS lookaside cache
        if (FALSE == TLSInitialize())
        {
            goto done;
        }

        // Initialize the environment.
        if (FALSE == MiscInitialize())
        {
            goto done;
        }

        // Initialize debug channel settings before anything else.
        // This depends on the environment, so it must come after
        // MiscInitialize.
        if (FALSE == DBG_init_channels())
        {
            goto done;
        }
#if _DEBUG
        // Verify that our page size is what we think it is. If it's
        // different, we can't run.
        if (VIRTUAL_PAGE_SIZE != getpagesize())
        {
            ASSERT("VIRTUAL_PAGE_SIZE is incorrect for this system!\n"
                "Change include/pal/virtual.h and clr/src/inc/stdmacros.h "
                "to reflect the correct page size of %d.\n", getpagesize());
        }
#endif  // _DEBUG

        /* initialize the shared memory infrastructure */
        if (!SHMInitialize())
        {
            ERROR("Shared memory initialization failed!\n");
            goto CLEANUP0;
        }

        //
        // Initialize global process data
        //
        palError = InitializeProcessData();
        if (NO_ERROR != palError)
        {
            ERROR("Unable to initialize process data\n");
            goto CLEANUP1;
        }

#if HAVE_MACH_EXCEPTIONS
        // Mach exception port needs to be set up before the thread
        // data or threads are set up.
        if (!SEHInitializeMachExceptions())
        {
            ERROR("SEHInitializeMachExceptions failed!\n");
            palError = ERROR_GEN_FAILURE;
            goto CLEANUP1;
        }
#endif // HAVE_MACH_EXCEPTIONS

        //
        // Initialize global thread data
        //
        palError = InitializeGlobalThreadData();
        if (NO_ERROR != palError)
        {
            ERROR("Unable to initialize thread data\n");
            goto CLEANUP1;
        }

        //
        // Allocate the initial thread data
        //

        palError = CreateThreadData(&pThread);
        if (NO_ERROR != palError)
        {
            ERROR("Unable to create initial thread data\n");
            goto CLEANUP1a;
        }
        PROCAddThread(pThread, pThread);

        //
        // Initialize mutex and condition variable used to synchronize the ending threads count
        //

        palError = InitializeEndingThreadsData();
        if (NO_ERROR != palError)
        {
            ERROR("Unable to create ending threads data\n");
            goto CLEANUP1b;
        }

        //
        // It's now safe to access our thread data
        //
        g_fThreadDataAvailable = TRUE;

        //
        // Initialize module manager
        //
        if (FALSE == LOADInitializeModules())
        {
            ERROR("Unable to initialize module manager\n");
            palError = ERROR_INTERNAL_ERROR;
            goto CLEANUP1b;
        }

        //
        // Initialize the object manager
        //
        pshmom = InternalNew<CSharedMemoryObjectManager>();
        if (NULL == pshmom)
        {
            ERROR("Unable to allocate new object manager\n");
            palError = ERROR_OUTOFMEMORY;
            goto CLEANUP1b;
        }

        palError = pshmom->Initialize();
        if (NO_ERROR != palError)
        {
            ERROR("object manager initialization failed!\n");
            InternalDelete(pshmom);
            goto CLEANUP1b;
        }

        g_pObjectManager = pshmom;

        //
        // Initialize the synchronization manager
        //
        g_pSynchronizationManager =
            CPalSynchMgrController::CreatePalSynchronizationManager();

        if (NULL == g_pSynchronizationManager)
        {
            palError = ERROR_NOT_ENOUGH_MEMORY;
            ERROR("Failure creating synchronization manager\n");
            goto CLEANUP1c;
        }
    }
    else
    {
        pThread = InternalGetCurrentThread();
    }

    palError = ERROR_GEN_FAILURE;

    if (init_count == 0)
    {
        //
        // Create the initial process and thread objects
        //
        palError = CreateInitialProcessAndThreadObjects(pThread);
        if (NO_ERROR != palError)
        {
            ERROR("Unable to create initial process and thread objects\n");
            goto CLEANUP5;
        }

#if !HAVE_MACH_EXCEPTIONS
        if(!SEHInitializeSignals())
        {
            goto CLEANUP5;
        }
#endif

        palError = ERROR_GEN_FAILURE;

        if (FALSE == TIMEInitialize())
        {
            ERROR("Unable to initialize TIME support\n");
            goto CLEANUP6;
        }

        /* Initialize the File mapping critical section. */
        if (FALSE == MAPInitialize())
        {
            ERROR("Unable to initialize file mapping support\n");
            goto CLEANUP6;
        }

        /* create file objects for standard handles */
        if(!FILEInitStdHandles())
        {
            ERROR("Unable to initialize standard file handles\n");
            goto CLEANUP13;
        }

        if (FALSE == CRTInitStdStreams())
        {
            ERROR("Unable to initialize CRT standard streams\n");
            goto CLEANUP15;
        }

        TRACE("First-time PAL initialization complete.\n");
        init_count++;

        /* Set LastError to a non-good value - functions within the
           PAL startup may set lasterror to a nonzero value. */
        SetLastError(NO_ERROR);
        retval = 0;
    }
    else
    {
        init_count++;

        // Behave the same wrt entering the PAL independent of whether this
        // is the first call to PAL_Initialize or not.  The first call implied
        // PAL_Enter by virtue of creating the CPalThread for the current
        // thread, and its starting state is to be in the PAL.
        (void)PAL_Enter(PAL_BoundaryTop);

        TRACE("Initialization count increases to %d\n", init_count.Load());

        SetLastError(NO_ERROR);
        retval = 0;
    }
    goto done;

    /* No cleanup required for CRTInitStdStreams */
CLEANUP15:
    FILECleanupStdHandles();
CLEANUP13:
    VIRTUALCleanup();
CLEANUP10:
    MAPCleanup();
CLEANUP6:
CLEANUP5:
    PROCCleanupInitialProcess();
CLEANUP1c:
    // Cleanup object manager
CLEANUP1b:
    // Cleanup initial thread data
CLEANUP1a:
    // Cleanup global process data
CLEANUP1:
    SHMCleanup();
CLEANUP0:
    ERROR("PAL_Initialize failed\n");
    SetLastError(palError);

done:
#ifdef PAL_PERF
    if( retval == 0)
    {
         PERFEnableProcessProfile();
         PERFEnableThreadProfile(FALSE);
         PERFCalibrate("Overhead of PERF entry/exit");
    }
#endif

    InternalLeaveCriticalSection(pThread, init_critsec);

    if (fFirstTimeInit && 0 == retval)
    {
        _ASSERTE(NULL != pThread);
    }

    if (retval != 0 && GetLastError() == ERROR_SUCCESS)
    {
        ASSERT("returning failure, but last error not set\n");
    }

    LOGEXIT("PAL_Initialize returns int %d\n", retval);
    return retval;
}


/*++
Function:
  PAL_InitializeChakraCore

Abstract:
  A replacement for PAL_Initialize when starting the host process that
  hosts ChakraCore

  This routine also makes sure the psuedo dynamic libraries like PALRT
  have their initialization methods called.

Return:
  ERROR_SUCCESS if successful
  An error code, if it failed

--*/
#if defined(ENABLE_CC_XPLAT_TRACE) || defined(DEBUG)
bool PAL_InitializeChakraCoreCalled = false;
#endif

int
PALAPI
PAL_InitializeChakraCore()
{
    // this is not thread safe but PAL_InitializeChakraCore is per process
    // besides, calling Jsrt initializer function is thread safe
    if (init_count > 0) return ERROR_SUCCESS;
#if defined(ENABLE_CC_XPLAT_TRACE) || defined(DEBUG)
    PAL_InitializeChakraCoreCalled = true;
#endif

    if (Initialize())
    {
        return GetLastError();
    }

    CPalThread *pThread = InternalGetCurrentThread();
    //
    // Tell the synchronization manager to start its worker thread
    //
    int error = CPalSynchMgrController::StartWorker(pThread);
    if (NO_ERROR != error)
    {
        ERROR("Synch manager failed to start worker thread\n");
        return error;
    }

    if (FALSE == VIRTUALInitialize(true))
    {
        ERROR("Unable to initialize virtual memory support\n");
        return ERROR_GEN_FAILURE;
    }

    // Check for a repeated call (this is a no-op).
    if (InterlockedIncrement(&g_chakraCoreInitialized) > 1)
    {
        PAL_Enter(PAL_BoundaryTop);
        return ERROR_SUCCESS;
    }

    if (!InitializeFlushProcessWriteBuffers())
    {
        return ERROR_GEN_FAILURE;
    }

    return ERROR_SUCCESS;
}

/*++
Function:
PAL_IsDebuggerPresent

Abstract:
This function should be used to determine if a debugger is attached to the process.
--*/
PALIMPORT
BOOL
PALAPI
PAL_IsDebuggerPresent()
{
#if defined(__LINUX__)
    BOOL debugger_present = FALSE;
    char buf[2048];

    int status_fd = open("/proc/self/status", O_RDONLY);
    if (status_fd == -1)
    {
        return FALSE;
    }
    ssize_t num_read = read(status_fd, buf, sizeof(buf) - 1);

    if (num_read > 0)
    {
        static const char TracerPid[] = "TracerPid:";
        char *tracer_pid;

        buf[num_read] = '\0';
        tracer_pid = strstr(buf, TracerPid);
        if (tracer_pid)
        {
            debugger_present = !!atoi(tracer_pid + sizeof(TracerPid) - 1);
        }
    }

    close(status_fd);

    return debugger_present;
#elif defined(__APPLE__)
    struct kinfo_proc info = {};
    size_t size = sizeof(info);
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };
    int ret = sysctl(mib, sizeof(mib)/sizeof(*mib), &info, &size, NULL, 0);

    if (ret == 0)
        return ((info.kp_proc.p_flag & P_TRACED) != 0);

    return FALSE;
#else
    return FALSE;
#endif
}

/*++
Function:
  PAL_Shutdown

Abstract:
  This function shuts down the PAL WITHOUT exiting the current process.
--*/
void
PALAPI
PAL_Shutdown(
    void)
{
    TerminateCurrentProcessNoExit(FALSE /* bTerminateUnconditionally */);
}

/*++
Function:
  PAL_Terminate

Abstract:
  This function is the called when a thread has finished using the PAL
  library. It shuts down PAL and exits the current process.
--*/
void
PALAPI
PAL_Terminate(
    void)
{
    PAL_TerminateEx(0);
}

/*++
Function:
PAL_TerminateEx

Abstract:
This function is the called when a thread has finished using the PAL
library. It shuts down PAL and exits the current process with
the specified exit code.
--*/
void
PALAPI
PAL_TerminateEx(
    int exitCode)
{
    ENTRY_EXTERNAL("PAL_TerminateEx()\n");

    if (NULL == init_critsec)
    {
        /* note that these macros probably won't output anything, since the
        debug channels haven't been initialized yet */
        ASSERT("PAL_Initialize has never been called!\n");
        LOGEXIT("PAL_Terminate returns.\n");
    }

    PALSetShutdownIntent();

    LOGEXIT("PAL_TerminateEx is exiting the current process.\n");
    exit(exitCode);
}

/*++
Function:
  PALIsThreadDataInitialized

Returns TRUE if startup has reached a point where thread data is available
--*/
BOOL PALIsThreadDataInitialized()
{
    return g_fThreadDataAvailable;
}

/*++
Function:
  PALCommonCleanup

Utility function to prepare for shutdown.

--*/
void
PALCommonCleanup()
{
    static bool cleanupDone = false;

    if (!cleanupDone)
    {
        cleanupDone = true;

        PALSetShutdownIntent();

        //
        // Let the synchronization manager know we're about to shutdown
        //

        CPalSynchMgrController::PrepareForShutdown();

#ifdef _DEBUG
        PROCDumpThreadList();
#endif
    }
}

/*++
Function:
  PALShutdown

  sets the PAL's initialization count to zero, so that PALIsInitialized will
  return FALSE. called by PROCCleanupProcess to tell some functions that the
  PAL isn't fully functional, and that they should use an alternate code path

(no parameters, no retun vale)
--*/
void PALShutdown()
{
    init_count = 0;
}

BOOL PALIsShuttingDown()
{
    /* ROTORTODO: This function may be used to provide a reader/writer-like
       mechanism (or a ref counting one) to prevent PAL APIs that need to access
       PAL runtime data, from working when PAL is shutting down. Each of those API
       should acquire a read access while executing. The shutting down code would
       acquire a write lock, i.e. suspending any new incoming reader, and waiting
       for the current readers to be done. That would allow us to get rid of the
       dangerous suspend-all-other-threads at shutdown time */
    return shutdown_intent;
}

void PALSetShutdownIntent()
{
    /* ROTORTODO: See comment in PALIsShuttingDown */
    shutdown_intent = TRUE;
}

/*++
Function:
  PALInitLock

Take the initializaiton critical section (init_critsec). necessary to serialize
TerminateProcess along with PAL_Terminate and PAL_Initialize

(no parameters)

Return value :
    TRUE if critical section existed (and was acquired)
    FALSE if critical section doens't exist yet
--*/
BOOL PALInitLock(void)
{
    if(!init_critsec)
    {
        return FALSE;
    }

    CPalThread * pThread =
        (PALIsThreadDataInitialized() ? InternalGetCurrentThread() : NULL);

    InternalEnterCriticalSection(pThread, init_critsec);
    return TRUE;
}

/*++
Function:
  PALInitUnlock

Release the initialization critical section (init_critsec).

(no parameters, no return value)
--*/
void PALInitUnlock(void)
{
    if(!init_critsec)
    {
        return;
    }

    CPalThread * pThread =
        (PALIsThreadDataInitialized() ? InternalGetCurrentThread() : NULL);

    InternalLeaveCriticalSection(pThread, init_critsec);
}
