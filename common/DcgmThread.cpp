/*
 * Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include "DcgmThread.h"
#include "DcgmLogging.h"
#include <stdio.h>

#ifdef __linux__
#include <sys/syscall.h> //syscall()
#include <unistd.h>      //usleep()
#endif

/*****************************************************************************/
/*
    Helper static function to pass to CreateThread
*/
void *dcgmthread_starter(void *parm)
{
    DcgmThread *dcgmThread = (DcgmThread *)parm;
    dcgmThread->RunInternal();
    return 0;
}

/*****************************************************************************/
DcgmThread::DcgmThread(bool sendSignalOnStop, std::string threadName)
{
    m_sendSignalOnStop = sendSignalOnStop;
    m_threadName       = std::move(threadName);
    resetStatusFlags();

#ifdef __linux__
    m_pthread = 0;
#else
    m_handle   = NULL;
    m_threadId = 0;
#endif
}

/*****************************************************************************/
DcgmThread::~DcgmThread()
{}

void DcgmThread::resetStatusFlags()
{
    m_shouldStop = 0;
    m_hasStarted = 0;
    m_hasRun     = 0;
    m_hasExited  = 0;

#ifdef __linux__
    m_alreadyJoined = false;
#endif
}

/*****************************************************************************/
int DcgmThread::Start()
{
    if (!m_hasExited)
    {
        if (m_hasRun)
        {
            PRINT_ERROR("%u", "Can't start thread. Already running as handle %u", (unsigned int)m_pthread);
            return -100;
        }
        else if (m_hasStarted)
        {
            PRINT_ERROR("", "Can't start thread. Thread is already about to start running");
            return -101;
        }
    }

    /* Reset the status flags before we start the thread since the thread will set m_hasRun and
       may even do it before pthread_create returns */
    resetStatusFlags();

    int st = pthread_create(&m_pthread, 0, dcgmthread_starter, this);
    if (st)
    {
        m_pthread = 0;
        PRINT_ERROR("%d", "Unable to pthread_create. errno=%d", st);
        return -200;
    }

    m_hasStarted = 1;

    if (!m_threadName.empty())
    {
        st = pthread_setname_np(m_pthread, m_threadName.c_str());
        if (st != 0)
        {
            DCGM_LOG_ERROR << "Got error " << st << " from pthread_setname_np with name " << m_threadName;
            /* Still let the thread run. It just won't be named */
        }
    }

    DCGM_LOG_INFO << "Created thread named \"" << m_threadName << "\" ID " << (unsigned int)m_pthread
                  << " DcgmThread ptr 0x" << std::hex << (void *)this;
    return 0;
}

/*****************************************************************************/
void DcgmThread::Stop()
{
    /* Use a lock per documentation of std::condition_variable */
    m_mutex.lock();
    m_shouldStop = true;
    m_stopCond.notify_all();
    m_mutex.unlock();

    /* Wake this thread up */
    if (m_hasStarted && m_hasRun && !m_hasExited && m_sendSignalOnStop)
        SendSignal(DCGM_THREAD_SIGNUM);

    OnStop();
}

/*****************************************************************************/
void DcgmThread::Kill()
{
    if (!m_hasStarted || m_hasExited)
    {
        /* Nothing to do */
        return;
    }

    int st = pthread_cancel(m_pthread);
    if (st == 0 || st == ESRCH)
        return; /* Thread terminated */

    PRINT_WARNING("%u %d", "pthread_cancel(%u) returned %d", (unsigned int)m_pthread, st);
}

/*****************************************************************************/
void DcgmThread::SendSignal(int signum)
{
    PRINT_DEBUG("%u %d", "Signalling thread %u with signum %d", (unsigned int)m_pthread, signum);
    pthread_kill(m_pthread, signum);
}

/*****************************************************************************/
int DcgmThread::Wait(int timeoutMs)
{
    void *retVal;

    // thread has not been started, therefore it cannot be running
    if (!m_hasStarted)
    {
        DCGM_LOG_DEBUG << "Thread " << m_pthread << " has not started.";
        return 0;
    }

    /* Infinite timeout? */
    if (timeoutMs == 0)
    {
        /* Does this thread exist yet? */
        while (!m_hasRun)
        {
            usleep(1000);
        }

        /* Calling pthread_join a second time results in undefined behavior */
        bool expected = false;
        if (m_alreadyJoined.compare_exchange_strong(expected, true))
        {
            int st = pthread_join(m_pthread, &retVal);
            if (st)
            {
                // The thread is guaranteed to have exited, only if the pthread_join call succeeded
                m_alreadyJoined.store(false);
                PRINT_ERROR("%p %d", "pthread_join(%p) returned st %d", (void *)m_pthread, st);
                return 1;
            }

            DCGM_LOG_DEBUG << "Thread " << m_pthread << " is gone (expected).";

            if (m_exception)
            {
                std::rethrow_exception(m_exception);
            }

            return 0; /* Thread is gone */
        }

        DCGM_LOG_DEBUG << "Thread " << m_pthread << " had m_alreadyJoined == true.";

        // Do not rethrow and exception here, as it'll be thrown by the caller who managed to call pthread_join
        return 0;
    }

    /* Does this thread exist yet? */
    while (!m_hasRun && timeoutMs > 0)
    {
        /* Sleep for small intervals until we exhaust our timeout */
        usleep(1000);
        timeoutMs -= 1;
    }

    if (timeoutMs < 0)
    {
        DCGM_LOG_DEBUG << "Thread " << m_pthread << " had timeoutMs < 0: " << timeoutMs;
        return 1; /* Hasn't started yet. I guess it's running */
    }

#if 1
    while (!m_hasExited && timeoutMs > 0)
    {
        usleep(1000);
        timeoutMs -= 1;
    }

    if (!m_hasExited)
    {
        DCGM_LOG_DEBUG << "Thread " << m_pthread << " had !m_hasExited";
        return 1; /* Running still */
    }

    DCGM_LOG_DEBUG << "Thread " << m_pthread << " had m_alreadyJoined " << m_alreadyJoined;

    /* Calling pthread_join a second time results in undefined behavior */
    bool expected = false;
    if (m_alreadyJoined.compare_exchange_strong(expected, true))
    {
        int st = pthread_join(m_pthread, &retVal);
        if (st)
        {
            // The thread is guaranteed to have exited, only if the pthread_join call succeeded
            m_alreadyJoined.store(false);
            PRINT_ERROR("%p %d", "pthread_join(%p) returned st %d", (void *)m_pthread, st);
            return 1;
        }
    }

    if (m_exception)
    {
        std::rethrow_exception(m_exception);
    }

    return 0;


#else /* This code won't work until we upgrade to glibc 2.2.3 */
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeoutMs / 1000;
    ts.tv_nsec += (timeoutMs % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000)
    {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }

    int st = pthread_timedjoin_np(m_pthread, &retVal, &ts);
    if (st == ETIMEDOUT)
        return 1;
    else if (!st)
        return 0; /* Thread was gone */

    PRINT_ERROR("%u %d", "pthread_timedjoin_np(%u) returned st %d", m_pthread, st);
    return 1;
#endif
}

/*****************************************************************************/
int DcgmThread::StopAndWait(int timeoutMs)
{
    int st;

    if (!m_hasStarted)
    {
        /* Already terminated */
        return 0;
    }

    Stop();

    st = Wait(timeoutMs);
    return st;
}

/*****************************************************************************/
void DcgmThread::RunInternal(void)
{
    m_hasRun = true;

    PRINT_DEBUG("%u", "Thread handle %u running", (unsigned int)m_pthread);
    try
    {
        run();
    }
    catch (...)
    {
        m_exception = std::current_exception();
    }
    PRINT_DEBUG("%u", "Thread id %u stopped", (unsigned int)m_pthread);

    m_hasExited = true;
}

/*****************************************************************************/
int DcgmThread::ShouldStop(void)
{
    return m_shouldStop; // || main_should_stop; /* If we later add a global variable for dcgm should stop, reference it
                         // here */
}

/*****************************************************************************/
unsigned long DcgmThread::GetTid()
{
#ifdef __linux__
    pid_t tid = syscall(SYS_gettid);
#else
    DWORD tid = GetCurrentThreadId();
#endif

    return (unsigned long)tid;
}

/*****************************************************************************/
void DcgmThread::Sleep(long long howLongUsec)
{
    if (ShouldStop())
    {
        DCGM_LOG_DEBUG << "Ignoring sleep due to ShouldStop() == true";
        return; /* Return immediately if we're supposed to shut down */
    }
    if (howLongUsec < 0)
    {
        DCGM_LOG_ERROR << "Negative value " << howLongUsec << "passed to Sleep()";
        return; /* Bad value */
    }

    std::unique_lock<std::mutex> uniqueLock(m_mutex);

    if (m_stopCond.wait_for(uniqueLock, std::chrono::microseconds(howLongUsec), [this] {
            return m_shouldStop.load(std::memory_order_relaxed);
        }))
    {
        DCGM_LOG_DEBUG << "Sleep hit m_shouldStop";
    }
}

/*****************************************************************************/
int DcgmThread::HasRun()
{
    return m_hasRun;
}

/*****************************************************************************/
int DcgmThread::HasExited()
{
    return m_hasExited;
}

/*****************************************************************************/
static void dcgm_thread_signal_handler(int /*signum*/)
{
    /* Do nothing, especially not IO */
}

/*****************************************************************************/
void DcgmThread::InstallSignalHandler(void)
{
    struct sigaction currentSigHandler;

    /* See if this process already has a signal handler */
    int ret = sigaction(DCGM_THREAD_SIGNUM, NULL, &currentSigHandler);
    if (ret < 0)
    {
        PRINT_ERROR("%d", "Got st %d from sigaction", ret);
        return;
    }
    if (currentSigHandler.sa_handler != SIG_DFL && currentSigHandler.sa_handler != SIG_IGN)
    {
        PRINT_INFO("%d", "Signal %d is already handled. Nothing to do.", DCGM_THREAD_SIGNUM);
        return;
    }

    /* Install our handler */
    struct sigaction newSigHandler;
    sigemptyset(&newSigHandler.sa_mask);
    newSigHandler.sa_flags   = 0;
    newSigHandler.sa_handler = dcgm_thread_signal_handler;

    ret = sigaction(DCGM_THREAD_SIGNUM, &newSigHandler, NULL);
    if (ret < 0)
    {
        PRINT_ERROR("%d", "Got error %d from sigaction while adding our signal handler.", ret);
    }
}

/*****************************************************************************/
