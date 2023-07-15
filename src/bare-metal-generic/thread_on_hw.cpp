

/**
* @file thread.cc
* @date 2008/04/22 16:50
* @author Lutz Dittrich, Sergio Montenegro
*
*
* A Thread is a schedulable object with own context and stack
*
* @brief %Thread handling
*/
#include "thread.h"

#include <atomic>

#include "rodos.h"
#include "scheduler.h"
#include "hw_specific.h"
#include "platform-parameter.h"

namespace RODOS {

constexpr uint32_t EMPTY_MEMORY_MARKER = 0xDEADBEEF;

InterruptSyncWrapper<int64_t> timeToTryAgainToSchedule = 0;

/** old style constructor */
Thread::Thread(const char* name,
               const int32_t priority,
               const size_t _stackSize) :
    ListElement(threadList, name) {

    this->stackSize = _stackSize;
    stackBegin = static_cast<char*>(xmalloc(stackSize));
    stack = reinterpret_cast<long*>((reinterpret_cast<uintptr_t>(stackBegin) + (stackSize-4)) & (~static_cast<uintptr_t>(7u))); // align 8 byte
    this->priority = priority;

    //Paint the stack space; TODO: Comment out for faster start up
    initializeStack();
}

void Thread::initializeStack() {
    //Paint the stack space TODO: Comment out for faster start up
    uint32_t* stackPaint = (uint32_t*)stack;
    while((intptr_t)stackPaint >= (intptr_t)stackBegin) {
        *stackPaint = EMPTY_MEMORY_MARKER;
        stackPaint--;
    }

    context = hwInitContext(stack, this);
}

Thread::~Thread() {
    if(isShuttingDown) return;
    PRINTF("%s:",getName());
    RODOS_ERROR("Thread deleted");
}

/* called in main() after all constuctors, to create/init thread */
void Thread::create() {
    // only required when implementig in on the top of posix, rtems, freertos, etc
}

extern std::atomic<bool> isSchedulingEnabled; // from scheduler

/** pause execution of this thread and call scheduler */
void Thread::yield() {
    // Optimisation: Avoid unnecessary context switch: see Scheduler::schedule()
    globalAtomarLock();
    Thread* preselection = findNextToRun();
    if(preselection == getCurrentThread()) {
        globalAtomarUnlock();
        return;
    }
    // schedule is required, the scheduler shall not repeat my computations:
    Scheduler::preSelectedNextToRun = preselection;

    // reschedule next timer interrupt to avoid interruptions of while switching
    // -> must first stop timer, then unlock (to prevent rescheduling between unlock and stop)
    Timer::stop();
    globalAtomarUnlock();
    __asmSaveContextAndCallScheduler();
}

/* restore context of this thread and continue execution of this thread */
void Thread::activate() {
    currentThread = this;
    Timer::start();
    __asmSwitchToContext((long*)context);
}


/*******************************************************************/

/* get priority of the thread */
int32_t Thread::getPriority() const {
    return priority;
}

/* set priority of the thread */
void Thread::setPriority(const int32_t prio) {
    priority = prio;
}

Thread* Thread::getCurrentThread() {
    return currentThread;
}



/* resume the thread */
void Thread::resume() {
    timeToTryAgainToSchedule = 0;
    waitingFor     = 0;
    suspendedUntil = 0;
    // yield(); // commented out because resume may be called from an interrupt server
}

/* suspend the thread */
bool Thread::suspendCallerUntil(const int64_t reactivationTime, void* signaler) {

    Thread* caller =  getCurrentThread();
    {
        PRIORITY_CEILER_IN_SCOPE();
        caller->waitingFor = signaler;
        caller->suspendedUntil = reactivationTime;
    }
    yield();

    caller->waitingFor = 0;
    /** after yield: It was resumed (suspendedUntil set to 0) or time was reached ?*/
    if(caller->suspendedUntil == 0) return true; // it was resumed!
    return false; // time was reached
}



void Thread::initializeThreads() {
    xprintf("Threads in System:");
    ITERATE_LIST(Thread, threadList) {
        xprintf("\n   Prio = %7ld Stack = %6lu %s: ",
            static_cast<long>(iter->priority),
            static_cast<unsigned long>(iter->stackSize),
            iter->getName());
        iter->init();
        iter->suspendedUntil = 0;
    }
    xprintf("\n");
    ITERATE_LIST(Thread, threadList) {
        iter->create();
    }
}

// not used in this implementation, the scheduler activates thread
void Thread::startAllThreads() { }


/** non-static C++ member functions cannot be used like normal
   C function pointers. www.function-pointer.org suggests using a
   wrapper function instead. */

void threadStartupWrapper(Thread* thread) {
    Thread::currentThread = thread;
    thread->suspendedUntil = 0;

    thread->run();
    /*
      loop forever
      if run() returns this thread is to be considered terminated
    */

    while(1) {
        thread->suspendedUntil = END_OF_TIME;
        thread->yield();
    }
}


unsigned long long Thread::getScheduleCounter() {
    return Scheduler::getScheduleCounter();
}

/********************************************************************************/


/**
 * @class IdleThread
 * @brief The idle thread.
 *
 * The idle thread. This thread will be executed if no other thread wants to
 * run
 */
class IdleThread : public StaticThread<> {
public:
    IdleThread() : StaticThread("IdleThread", 0) {
    }
    void run();
    void init();
};

void IdleThread::run() {
    while(1) {
        idleCnt = idleCnt + 1;
        setPriority(0); // Due to wrong usage of PRIORITY_CLING in events, once I got highest prio for Idle.
        sp_partition_yield(); // allow other linux processes or ARIC-653 partitions to run
        yield();

#ifdef SLEEP_WHEN_IDLE_ENABLED
        // enter sleep mode if suitable
        int64_t reactivationTime = timeToTryAgainToSchedule;
#ifndef DISABLE_TIMEEVENTS
        {
            ScopeProtector protector{ &TimeEvent::getTimeEventSema() };
            reactivationTime =
                RODOS::min(timeToTryAgainToSchedule.load(), TimeEvent::getNextTriggerTime());
        }
#endif

        int64_t durationToNextTimingEvent = reactivationTime - NOW();
        int64_t timerInterval =
            durationToNextTimingEvent - TIME_WAKEUP_FROM_SLEEP - MIN_SYS_TICK_SPACING;
        if(timerInterval > TIME_WAKEUP_FROM_SLEEP && timerInterval > MIN_SYS_TICK_SPACING) {
            Timer::stop();
            Timer::setInterval(timerInterval / 1000l); // nanoseconds to microseconds
            Timer::start();

            enterSleepMode();

            Timer::stop();
            auto remainingTime = RODOS::max(reactivationTime - NOW(), MIN_SYS_TICK_SPACING);
            Timer::setInterval(remainingTime / 1000l); // nanoseconds to microseconds
            Timer::start();
        }
#endif
    }
}

void IdleThread::init() {
    xprintf("yields all the time");
}


/**
 * The idle thread.
 */
IdleThread idlethread;
Thread* idlethreadP = &idlethread;


/*********************************************************************************/

constexpr int64_t earlier(const int64_t a, const int64_t b) {
    return (a < b) ? a : b;
}

Thread* Thread::findNextToRun() {
    Thread* nextThreadToRun = &idlethread; // Default, if no one else wants
    Scheduler::selectedEarliestSuspendedUntil = END_OF_TIME;
    int64_t timeNow = NOW();
    ITERATE_LIST(Thread, threadList) {
        int64_t iterSuspendedUntil = iter->suspendedUntil.load();
        int64_t iterPrio = iter->getPriority();
        int64_t nextThreadToRunPrio = nextThreadToRun->getPriority();
        if (iterSuspendedUntil < timeNow) { // in the past
			// - thread with highest prio will be executed immediately when this scheduler-call ends
            // - other threads with lower prio will be executed after next scheduler-call
            //   due to suspend() of high-prio thread
            if (iterPrio > nextThreadToRunPrio) {
                nextThreadToRun = iter;
            }
            else if (iterPrio == nextThreadToRunPrio) {
                if (iter->lastActivation.load() < nextThreadToRun->lastActivation.load()) {
                    nextThreadToRun = iter;
                }
            }

        } else { // in the future, find next to be handled
			// if there is a thread with higher or same priority in the future, we must call the scheduler then
			// so that the thread will be executed
            if(iterPrio >= nextThreadToRunPrio) {
                Scheduler::selectedEarliestSuspendedUntil =
                    earlier(Scheduler::selectedEarliestSuspendedUntil, iterSuspendedUntil);
            }
			// threads with lower priority will not be executed until nextThreadToRun suspends
        }
    } // Iterate list

    /** Check stack violations **/
    constexpr size_t stackMargin = 300;
    uintptr_t minimumStackAddr = reinterpret_cast<uintptr_t>(nextThreadToRun->stackBegin)+stackMargin;
    if(nextThreadToRun->getCurrentStackAddr() < minimumStackAddr) {
        xprintf("!StackOverflow! %s DEACTIVATED!: free %d\n",
            nextThreadToRun->name,
            static_cast<int>(nextThreadToRun->getCurrentStackAddr() - reinterpret_cast<uintptr_t>(nextThreadToRun->stackBegin)));
        nextThreadToRun->suspendedUntil = END_OF_TIME;
        nextThreadToRun = &idlethread;
    }
    if ( *reinterpret_cast<uint32_t *>(nextThreadToRun->stackBegin) !=  EMPTY_MEMORY_MARKER) { // this thread is going beyond its stack!
        xprintf("! PANIC %s beyond stack, DEACTIVATED!\n", nextThreadToRun->name);
        nextThreadToRun->suspendedUntil = END_OF_TIME;
        nextThreadToRun = &idlethread;
    }

    return nextThreadToRun;
}
#undef EARLIER


Thread* Thread::findNextWaitingFor(void* signaler) {
    Thread* nextWaiter = &idlethread; // Default, if no one else wants to run

    ITERATE_LIST(Thread, threadList) {
        if (iter->waitingFor == signaler) {
            if (iter->getPriority() > nextWaiter->getPriority()) {
                nextWaiter = iter;
            } else {
                if (iter->getPriority() == nextWaiter->getPriority()) {
                    if (iter->lastActivation < nextWaiter->lastActivation) {
                        nextWaiter = iter;
                    }
                }
            }
        }
    }
    if (nextWaiter == &idlethread) {
        return 0;
    }
    return nextWaiter;
}

size_t Thread::getMaxStackUsage(){
	Thread* currentThread = getCurrentThread();

	//Go to the beginning of the stack(lowest addres)
	uint32_t* stackScan = (uint32_t*)currentThread->stack;
	while((intptr_t)stackScan >= (intptr_t)currentThread->stackBegin){
		stackScan--;
	}
	stackScan++;

	//Go up until empty markers are found and count
	size_t freeStack=0;
	while(stackScan <= (uint32_t*)currentThread->stack && *stackScan == EMPTY_MEMORY_MARKER){
		freeStack +=4;
		stackScan++;
	}

	return currentThread->stackSize-freeStack;
}



} // namespace

