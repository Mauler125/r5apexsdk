#ifndef THREADTOOLS_H
#define THREADTOOLS_H
#include "dbg.h"

#ifdef BUILDING_MATHLIB
#define _TOOLS
#endif // BUILDING_MATHLIB

#ifndef _TOOLS
#include "jobthread.h"
#endif // _TOOLS

inline void ThreadSleep(unsigned nMilliseconds)
{
#ifdef _WIN32

#ifdef PLATFORM_WINDOWS_PC // This is performed by the game module!
	//static bool bInitialized = false;
	//if (!bInitialized)
	//{
	//	bInitialized = true;
	//	// Set the timer resolution to 1 ms (default is 10.0, 15.6, 2.5, 1.0 or
	//	// some other value depending on hardware and software) so that we can
	//	// use Sleep( 1 ) to avoid wasting CPU time without missing our frame
	//	// rate.
	//	timeBeginPeriod(1);
	//}
#endif

	Sleep(nMilliseconds);
#elif PLATFORM_PS3
	if (nMilliseconds == 0)
	{
		// sys_ppu_thread_yield doesn't seem to function properly, so sleep instead.
		sys_timer_usleep(60);
	}
	else
	{
		sys_timer_usleep(nMilliseconds * 1000);
	}
#elif defined(POSIX)
	usleep(nMilliseconds * 1000);
#endif
}
inline void ThreadPause()
{
#if defined( COMPILER_PS3 )
	__db16cyc();
#elif defined( COMPILER_GCC )
	__asm __volatile("pause");
#elif defined ( COMPILER_MSVC64 )
	_mm_pause();
#elif defined( COMPILER_MSVC32 )
	__asm pause;
#elif defined( COMPILER_MSVCX360 )
	YieldProcessor();
	__asm { or r0, r0, r0 }
	YieldProcessor();
	__asm { or r1, r1, r1 }
#else
#error "implement me"
#endif
}

//-----------------------------------------------------------------------------
//
// Interlock methods. These perform very fast atomic thread
// safe operations. These are especially relevant in a multi-core setting.
//
//-----------------------------------------------------------------------------

FORCEINLINE int32 ThreadInterlockedIncrement(int32 volatile* p) { Assert((size_t)p % 4 == 0); return _InterlockedIncrement((volatile long*)p); }
FORCEINLINE int32 ThreadInterlockedDecrement(int32 volatile* p) { Assert((size_t)p % 4 == 0); return _InterlockedDecrement((volatile long*)p); }

FORCEINLINE int64 ThreadInterlockedIncrement64(int64 volatile* p) { Assert((size_t)p % 8 == 0); return _InterlockedIncrement64((volatile int64*)p); }
FORCEINLINE int64 ThreadInterlockedDecrement64(int64 volatile* p) { Assert((size_t)p % 8 == 0); return _InterlockedDecrement64((volatile int64*)p); }

FORCEINLINE int32 ThreadInterlockedExchangeAdd(int32 volatile* p, int32 value) { Assert((size_t)p % 4 == 0); return _InterlockedExchangeAdd((volatile long*)p, value); }

FORCEINLINE int32 ThreadInterlockedCompareExchange(LONG volatile* pDest, int32 value, int32 comperand)
{
	return _InterlockedCompareExchange(pDest, comperand, value);
}

FORCEINLINE bool ThreadInterlockedAssignIf(LONG volatile* p, int32 value, int32 comperand)
{
	Assert((size_t)p % 4 == 0);
	return _InterlockedCompareExchange(p, comperand, value);
}

FORCEINLINE int64 ThreadInterlockedCompareExchange64(int64 volatile* pDest, int64 value, int64 comperand)
{
	return _InterlockedCompareExchange64(pDest, comperand, value);
}

FORCEINLINE bool ThreadInterlockedAssignIf64(int64 volatile* pDest, int64 value, int64 comperand)
{
	return _InterlockedCompareExchange64(pDest, comperand, value);
}

//-----------------------------------------------------------------------------
//
// Thread checking methods.
//
//-----------------------------------------------------------------------------
FORCEINLINE ThreadId_t ThreadGetCurrentId()
{
#ifdef _WIN32
	return GetCurrentThreadId();
#elif defined( _PS3 )
	sys_ppu_thread_t th = 0;
	sys_ppu_thread_get_id(&th);
	return th;
#elif defined(POSIX)
	return (ThreadId_t)pthread_self();
#else
	Assert(0);
	DebuggerBreak();
	return 0;
#endif
}

#ifndef _TOOLS

extern ThreadId_t* g_ThreadMainThreadID;
extern ThreadId_t* g_ThreadServerFrameThreadID;
inline JobID_t* g_CurrentServerFrameJobID;
inline JobID_t* g_AllocatedServerFrameJobID;

PLATFORM_INTERFACE bool ThreadInMainThread();
PLATFORM_INTERFACE bool ThreadInServerFrameThread();
PLATFORM_INTERFACE bool ThreadInMainOrServerFrameThread();
PLATFORM_INTERFACE bool ThreadCouldDoServerWork();
PLATFORM_INTERFACE void ThreadJoinServerJob();

#endif // !_TOOLS

#ifdef _WIN32
#define NOINLINE
#elif defined( _PS3 )
#define NOINLINE __attribute__ ((noinline))
#elif defined(POSIX)
#define NOINLINE __attribute__ ((noinline))
#endif

#if defined( _X360 ) || defined( _PS3 )
#define ThreadMemoryBarrier() __lwsync()
#elif defined(COMPILER_MSVC)
// Prevent compiler reordering across this barrier. This is
// sufficient for most purposes on x86/x64.
#define ThreadMemoryBarrier() _ReadWriteBarrier()
#elif defined(COMPILER_GCC)
// Prevent compiler reordering across this barrier. This is
// sufficient for most purposes on x86/x64.
// http://preshing.com/20120625/memory-ordering-at-compile-time
#define ThreadMemoryBarrier() asm volatile("" ::: "memory")
#else
#error Every platform needs to define ThreadMemoryBarrier to at least prevent compiler reordering
#endif

//-----------------------------------------------------------------------------
//
// A super-fast thread-safe integer A simple class encapsulating the notion of an 
// atomic integer used across threads that uses the built in and faster 
// "interlocked" functionality rather than a full-blown mutex. Useful for simple 
// things like reference counts, etc.
//
//-----------------------------------------------------------------------------

template <typename T>
class CInterlockedIntT
{
public:
	CInterlockedIntT() : m_value(0) { static_assert((sizeof(T) == sizeof(int32)) || (sizeof(T) == sizeof(int64))); }

	CInterlockedIntT(T value) : m_value(value) {}

	T operator()(void) const { return m_value; }
	operator T() const { return m_value; }

	bool operator!() const { return (m_value == 0); }
	bool operator==(T rhs) const { return (m_value == rhs); }
	bool operator!=(T rhs) const { return (m_value != rhs); }

	T operator++() {
		if (sizeof(T) == sizeof(int32))
			return (T)ThreadInterlockedIncrement((int32*)&m_value);
		else
			return (T)ThreadInterlockedIncrement64((int64*)&m_value);
	}
	T operator++(int) { return operator++() - 1; }

	T operator--() {
		if (sizeof(T) == sizeof(int32))
			return (T)ThreadInterlockedDecrement((int32*)&m_value);
		else
			return (T)ThreadInterlockedDecrement64((int64*)&m_value);
	}

	T operator--(int) { return operator--() + 1; }

	bool AssignIf(T conditionValue, T newValue)
	{
		if V_CONSTEXPR(sizeof(T) == sizeof(int32))
			return ThreadInterlockedAssignIf((LONG*)&m_value, (int32)newValue, (int32)conditionValue);
		else
			return ThreadInterlockedAssignIf64((int64*)&m_value, (int64)newValue, (int64)conditionValue);
	}


	T operator=(T newValue) {
		if (sizeof(T) == sizeof(int32))
			ThreadInterlockedExchange((int32*)&m_value, newValue);
		else
			ThreadInterlockedExchange64((int64*)&m_value, newValue);
		return m_value;
	}

	// Atomic add is like += except it returns the previous value as its return value
	T AtomicAdd(T add) {
		if (sizeof(T) == sizeof(int32))
			return (T)ThreadInterlockedExchangeAdd((int32*)&m_value, (int32)add);
		else
			return (T)ThreadInterlockedExchangeAdd64((int64*)&m_value, (int64)add);
	}


	void operator+=(T add) {
		if (sizeof(T) == sizeof(int32))
			ThreadInterlockedExchangeAdd((int32*)&m_value, (int32)add);
		else
			ThreadInterlockedExchangeAdd64((int64*)&m_value, (int64)add);
	}

	void operator-=(T subtract) { operator+=(-subtract); }
	void operator*=(T multiplier) {
		T original, result;
		do
		{
			original = m_value;
			result = original * multiplier;
		} while (!AssignIf(original, result));
	}
	void operator/=(T divisor) {
		T original, result;
		do
		{
			original = m_value;
			result = original / divisor;
		} while (!AssignIf(original, result));
	}

	T operator+(T rhs) const { return m_value + rhs; }
	T operator-(T rhs) const { return m_value - rhs; }

	T InterlockedExchange(T newValue) {
		if (sizeof(T) == sizeof(int32))
			return (T)ThreadInterlockedExchange((int32*)&m_value, newValue);
		else
			return (T)ThreadInterlockedExchange64((int64*)&m_value, newValue);
	}

private:
	volatile T m_value;
};

typedef CInterlockedIntT<int> CInterlockedInt;
typedef CInterlockedIntT<unsigned> CInterlockedUInt;

#ifndef _TOOLS
//=============================================================================


#endif // !_TOOLS

//-----------------------------------------------------------------------------
//
// Platform independent for critical sections management
//
//-----------------------------------------------------------------------------

struct CThreadMutex
{
public:
	CThreadMutex();
	~CThreadMutex();

	//------------------------------------------------------
	// Mutex acquisition/release. Const intentionally defeated.
	//------------------------------------------------------
	void Lock();
	void Lock() const { (const_cast<CThreadMutex*>(this))->Lock(); }
	void Unlock();
	void Unlock() const { (const_cast<CThreadMutex*>(this))->Unlock(); }

	void LockSilent(); // A Lock() operation which never spews.  Required by the logging system to prevent badness.
	void UnlockSilent(); // An Unlock() operation which never spews.  Required by the logging system to prevent badness.

	//------------------------------------------------------
	// Use this to make deadlocks easier to track by asserting
	// when it is expected that the current thread owns the mutex
	//------------------------------------------------------
	bool AssertOwnedByCurrentThread();

	//------------------------------------------------------
	// On windows with THREAD_MUTEX_TRACING_ENABLED defined, this returns
	// true if the mutex is owned by the current thread.
	//------------------------------------------------------
	bool IsOwnedByCurrentThread_DebugOnly();

	//------------------------------------------------------
	// Enable tracing to track deadlock problems
	//------------------------------------------------------
	void SetTrace(bool);

private:
	// Disallow copying
	CThreadMutex(const CThreadMutex&);
	CThreadMutex& operator=(const CThreadMutex&);

#if defined( _WIN32 )
	CRITICAL_SECTION m_CriticalSection;
#elif defined( _PS3 )
	sys_mutex_t m_Mutex;
#elif defined(POSIX)
	pthread_mutex_t m_Mutex;
	pthread_mutexattr_t m_Attr;
#else
#error
#endif

#ifdef THREAD_MUTEX_TRACING_SUPPORTED
	// Debugging (always herge to allow mixed debug/release builds w/o changing size)
	uint	m_currentOwnerID;
	uint16	m_lockCount;
	bool	m_bTrace;
#endif
};

//-----------------------------------------------------------------------------
//
// CThreadMutex. Inlining to reduce overhead and to allow client code
// to decide debug status (tracing)
//
//-----------------------------------------------------------------------------

#ifdef MSVC
typedef struct _RTL_CRITICAL_SECTION RTL_CRITICAL_SECTION;
typedef RTL_CRITICAL_SECTION CRITICAL_SECTION;

#ifndef _X360
extern "C"
{
	void __declspec(dllimport) __stdcall InitializeCriticalSection(CRITICAL_SECTION*);
	void __declspec(dllimport) __stdcall EnterCriticalSection(CRITICAL_SECTION*);
	void __declspec(dllimport) __stdcall LeaveCriticalSection(CRITICAL_SECTION*);
	void __declspec(dllimport) __stdcall DeleteCriticalSection(CRITICAL_SECTION*);
};
#endif
#endif

//---------------------------------------------------------
#if !defined(POSIX) || defined( _GAMECONSOLE )

inline void CThreadMutex::Lock()
{
#if defined(_PS3)
#ifndef NO_THREAD_SYNC
	sys_mutex_lock(m_Mutex, 0);
#endif
#else
#if defined( THREAD_MUTEX_TRACING_ENABLED )
	uint thisThreadID = ThreadGetCurrentId();
	if (m_bTrace && m_currentOwnerID && (m_currentOwnerID != thisThreadID))
		Msg(eDLL_T::COMMON, _T("Thread %u about to wait for lock %p owned by %u\n"), ThreadGetCurrentId(), &m_CriticalSection, m_currentOwnerID);
#endif

	LockSilent();

#ifdef THREAD_MUTEX_TRACING_ENABLED
	if (m_lockCount == 0)
	{
		// we now own it for the first time.  Set owner information
		m_currentOwnerID = thisThreadID;
		if (m_bTrace)
			Msg(eDLL_T::COMMON, _T("Thread %u now owns lock 0x%p\n"), m_currentOwnerID, &m_CriticalSection);
	}
	m_lockCount++;
#endif
#endif
}

//---------------------------------------------------------

inline void CThreadMutex::Unlock()
{
#if defined( _PS3 )

#ifndef NO_THREAD_SYNC
	sys_mutex_unlock(m_Mutex);
#endif

#else
#ifdef THREAD_MUTEX_TRACING_ENABLED
	AssertMsg(m_lockCount >= 1, "Invalid unlock of thread lock");
	m_lockCount--;
	if (m_lockCount == 0)
	{
		if (m_bTrace)
			Msg(eDLL_T::COMMON, _T("Thread %u releasing lock 0x%p\n"), m_currentOwnerID, &m_CriticalSection);
		m_currentOwnerID = 0;
	}
#endif
	UnlockSilent();
#endif
}

//---------------------------------------------------------

inline void CThreadMutex::LockSilent()
{
#ifdef MSVC
	EnterCriticalSection(&m_CriticalSection);
#else
	DebuggerBreak();	// should not be called - not defined for this platform/compiler!!!
#endif
}

//---------------------------------------------------------

inline void CThreadMutex::UnlockSilent()
{
#ifdef MSVC
	LeaveCriticalSection(&m_CriticalSection);
#else
	DebuggerBreak();	// should not be called - not defined for this platform/compiler!!!
#endif
}

//---------------------------------------------------------

inline bool CThreadMutex::AssertOwnedByCurrentThread()
{
#ifdef THREAD_MUTEX_TRACING_ENABLED
#ifdef _WIN32
	if (ThreadGetCurrentId() == m_currentOwnerID)
		return true;
	AssertMsg(0, "Expected thread %u as owner of lock 0x%p, but %u owns", ThreadGetCurrentId(), &m_CriticalSection, m_currentOwnerID);
	return false;
#elif defined( _PS3 )
	return true;
#endif
#else
	return true;
#endif
}

//---------------------------------------------------------

inline bool CThreadMutex::IsOwnedByCurrentThread_DebugOnly()
{
#if defined ( THREAD_MUTEX_TRACING_ENABLED ) && defined ( _WIN32 )
	return ThreadGetCurrentId() == m_currentOwnerID;
#else
	return true;
#endif
}

//---------------------------------------------------------

inline void CThreadMutex::SetTrace(bool bTrace)
{
#ifdef _WIN32
#ifdef THREAD_MUTEX_TRACING_ENABLED
	m_bTrace = bTrace;
#endif
#elif defined _PS3
	//EAPS3
#endif
	NOTE_UNUSED(bTrace);
}

//---------------------------------------------------------

#elif defined(POSIX) && !defined( _GAMECONSOLE )

inline CThreadMutex::CThreadMutex()
{
	// enable recursive locks as we need them
	pthread_mutexattr_init(&m_Attr);
	pthread_mutexattr_settype(&m_Attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&m_Mutex, &m_Attr);
}

//---------------------------------------------------------

inline CThreadMutex::~CThreadMutex()
{
	pthread_mutex_destroy(&m_Mutex);
}

//---------------------------------------------------------

inline void CThreadMutex::Lock()
{
	pthread_mutex_lock(&m_Mutex);
}

//---------------------------------------------------------

inline void CThreadMutex::Unlock()
{
	pthread_mutex_unlock(&m_Mutex);
}

//---------------------------------------------------------

inline void CThreadMutex::LockSilent()
{
	pthread_mutex_lock(&m_Mutex);
}

//---------------------------------------------------------

inline void CThreadMutex::UnlockSilent()
{
	pthread_mutex_unlock(&m_Mutex);
}

//---------------------------------------------------------

inline bool CThreadMutex::AssertOwnedByCurrentThread()
{
	return true;
}

//---------------------------------------------------------

inline void CThreadMutex::SetTrace(bool fTrace)
{
}

#else 
#error
#endif // POSIX

//-----------------------------------------------------------------------------
//
// An alternative mutex that is useful for cases when thread contention is 
// rare, but a mutex is required. Instances should be declared volatile.
// Sleep of 0 may not be sufficient to keep high priority threads from starving 
// lesser threads. This class is not a suitable replacement for a critical
// section if the resource contention is high.
//
//-----------------------------------------------------------------------------

class CThreadFastMutex
{
public:
	CThreadFastMutex()
		: m_nOwnerID(NULL)
		, m_nDepth(NULL)
		, m_lAddend(NULL)
		, m_hSemaphore(NULL)
	{
	}

	int Lock(void);
	int Unlock(void);

	inline uint32  GetOwnerId(void)   const { return m_nOwnerID; }
	inline int     GetDepth(void)     const { return m_nDepth; }
	inline int     GetAddend(void)    const { return m_lAddend; }
	inline HANDLE  GetSemaphore(void) const { return m_hSemaphore; }

private:
	volatile uint32_t m_nOwnerID;
	int               m_nDepth;
	volatile int      m_lAddend;
	HANDLE            m_hSemaphore;
};

///////////////////////////////////////////////////////////////////////////////
template <class MUTEX_TYPE = CThreadFastMutex>
class CAutoLockT
{
public:
	FORCEINLINE CAutoLockT(MUTEX_TYPE& lock)
		: m_lock(lock)
	{
		m_lock.Lock();
	}

	FORCEINLINE CAutoLockT(const MUTEX_TYPE& lock)
		: m_lock(const_cast<MUTEX_TYPE&>(lock))
	{
		m_lock.Lock();
	}

	FORCEINLINE ~CAutoLockT()
	{
		m_lock.Unlock();
	}


private:
	MUTEX_TYPE& m_lock;

	// Disallow copying
	CAutoLockT<MUTEX_TYPE>(const CAutoLockT<MUTEX_TYPE>&);
	CAutoLockT<MUTEX_TYPE>& operator=(const CAutoLockT<MUTEX_TYPE>&);
};

///////////////////////////////////////////////////////////////////////////////
template <size_t size>	struct CAutoLockTypeDeducer {};
template <> struct CAutoLockTypeDeducer<sizeof(CThreadMutex)> { typedef CThreadMutex Type_t; };
template <> struct CAutoLockTypeDeducer<sizeof(CThreadFastMutex)> { typedef CThreadFastMutex Type_t; };

#define AUTO_LOCK_( type, mutex ) \
	CAutoLockT< type > UNIQUE_ID( static_cast<const type &>( mutex ) )

#if defined(__clang__)
#define AUTO_LOCK(mutex) \
	AUTO_LOCK_(typename CAutoLockTypeDeducer<sizeof(mutex)>::Type_t, mutex)
#else
#define AUTO_LOCK(mutex) \
	AUTO_LOCK_(CAutoLockTypeDeducer<sizeof(mutex)>::Type_t, mutex)
#endif

//-----------------------------------------------------------------------------
//
// CThreadSpinRWLock inline functions
//
//-----------------------------------------------------------------------------

class ALIGN8 PLATFORM_CLASS CThreadSpinRWLock
{
public:
	CThreadSpinRWLock()
	{
		m_lockInfo.m_i32 = 0;
		m_writerId = 0;
#ifdef REENTRANT_THREAD_SPIN_RW_LOCK
		m_iWriteDepth = 0;
#endif
	}

	bool IsLockedForWrite();
	bool IsLockedForRead();

	FORCEINLINE bool TryLockForWrite();
	bool TryLockForWrite_UnforcedInline();

	void LockForWrite();
	void SpinLockForWrite();

	FORCEINLINE bool TryLockForRead();
	bool TryLockForRead_UnforcedInline();

	void LockForRead();
	void SpinLockForRead();

	void UnlockWrite();
	void UnlockRead();

	bool TryLockForWrite() const { return const_cast<CThreadSpinRWLock*>(this)->TryLockForWrite(); }
	bool TryLockForRead() const { return const_cast<CThreadSpinRWLock*>(this)->TryLockForRead(); }
	void LockForRead() const { const_cast<CThreadSpinRWLock*>(this)->LockForRead(); }
	void UnlockRead() const { const_cast<CThreadSpinRWLock*>(this)->UnlockRead(); }
	void LockForWrite() const { const_cast<CThreadSpinRWLock*>(this)->LockForWrite(); }
	void UnlockWrite() const { const_cast<CThreadSpinRWLock*>(this)->UnlockWrite(); }

private:
	enum
	{
		THREAD_SPIN = (8 * 1024)
	};

#pragma warning(push)
#pragma warning(disable : 4201)
	union LockInfo_t
	{
		struct
		{
#if PLAT_LITTLE_ENDIAN
			uint16 m_nReaders;
			uint16 m_fWriting;
#else
			uint16 m_fWriting;
			uint16 m_nReaders;
#endif
		};
		uint32 m_i32;
	};
#pragma warning(pop)

	LockInfo_t	m_lockInfo;
	ThreadId_t	m_writerId;
#ifdef REENTRANT_THREAD_SPIN_RW_LOCK
	int			m_iWriteDepth;
	uint32		pad;
#endif
} ALIGN8_POST;

inline bool CThreadSpinRWLock::IsLockedForWrite()
{
	return (m_lockInfo.m_fWriting == 1);
}

inline bool CThreadSpinRWLock::IsLockedForRead()
{
	return (m_lockInfo.m_nReaders > 0);
}

FORCEINLINE bool CThreadSpinRWLock::TryLockForWrite()
{
	volatile LockInfo_t& curValue = m_lockInfo;
	if (!(curValue.m_i32 & 0x00010000) && ThreadInterlockedAssignIf((LONG*)&curValue.m_i32, 0x00010000, 0))
	{
		ThreadMemoryBarrier();
		Assert(m_writerId == 0);
		m_writerId = ThreadGetCurrentId();
#ifdef REENTRANT_THREAD_SPIN_RW_LOCK
		Assert(m_iWriteDepth == 0);
		m_iWriteDepth++;
#endif
		return true;
	}

	return false;
}

inline bool CThreadSpinRWLock::TryLockForWrite_UnforcedInline()
{
	if (TryLockForWrite())
	{
		return true;
	}

#ifdef REENTRANT_THREAD_SPIN_RW_LOCK
	if (m_writerId != ThreadGetCurrentId())
	{
		return false;
	}
	m_iWriteDepth++;
	return true;
#else
	return false;
#endif
}

FORCEINLINE void CThreadSpinRWLock::LockForWrite()
{
	if (!TryLockForWrite())
	{
		SpinLockForWrite();
	}
}

FORCEINLINE bool CThreadSpinRWLock::TryLockForRead()
{
	volatile LockInfo_t& curValue = m_lockInfo;
	if (!(curValue.m_i32 & 0x00010000)) // !m_lockInfo.m_fWriting
	{
		LockInfo_t oldValue;
		LockInfo_t newValue;
		oldValue.m_i32 = (curValue.m_i32 & 0xffff);
		newValue.m_i32 = oldValue.m_i32 + 1;

		if (ThreadInterlockedAssignIf((LONG*)&m_lockInfo.m_i32, newValue.m_i32, oldValue.m_i32))
		{
			ThreadMemoryBarrier();
			Assert(m_lockInfo.m_fWriting == 0);
			return true;
		}
	}
	return false;
}

inline bool CThreadSpinRWLock::TryLockForRead_UnforcedInline()
{
#ifdef REENTRANT_THREAD_SPIN_RW_LOCK
	if (m_lockInfo.m_i32 & 0x00010000) // m_lockInfo.m_fWriting
	{
		if (m_writerId == ThreadGetCurrentId())
		{
			m_lockInfo.m_nReaders++;
			return true;
		}

		return false;
	}
#endif
	return TryLockForRead();
}

FORCEINLINE void CThreadSpinRWLock::LockForRead()
{
	if (!TryLockForRead())
	{
		SpinLockForRead();
	}
}

FORCEINLINE void CThreadSpinRWLock::UnlockWrite()
{
	Assert(m_writerId == ThreadGetCurrentId());
#ifdef REENTRANT_THREAD_SPIN_RW_LOCK
	if (--m_iWriteDepth == 0)
#endif
	{
		m_writerId = 0;
		ThreadMemoryBarrier();
		m_lockInfo.m_i32 = 0;
	}
}

#ifndef REENTRANT_THREAD_SPIN_RW_LOCK
FORCEINLINE
#else
inline
#endif
void CThreadSpinRWLock::UnlockRead()
{
	Assert(m_writerId == 0 || (m_writerId == ThreadGetCurrentId() && m_lockInfo.m_fWriting));
#ifdef REENTRANT_THREAD_SPIN_RW_LOCK
	if (!(m_lockInfo.m_i32 & 0x00010000)) // !m_lockInfo.m_fWriting
#endif
	{
		ThreadMemoryBarrier();
		ThreadInterlockedDecrement((int32*)&m_lockInfo.m_i32);
		Assert(m_writerId == 0 && !m_lockInfo.m_fWriting);
	}
#ifdef REENTRANT_THREAD_SPIN_RW_LOCK
	else if (m_writerId == ThreadGetCurrentId())
	{
		m_lockInfo.m_nReaders--;
	}
	else
	{
		RWLAssert(0);
	}
#endif
}

#ifndef _TOOLS
///////////////////////////////////////////////////////////////////////////////
class VThreadTools : public IDetour
{
	virtual void GetAdr(void) const
	{
		LogVarAdr("g_ThreadMainThreadID", g_ThreadMainThreadID);
		LogVarAdr("g_ThreadServerFrameThreadID", g_ThreadServerFrameThreadID);
		LogVarAdr("g_CurrentServerFrameJobID", g_CurrentServerFrameJobID);
		LogVarAdr("g_AllocatedServerFrameJobID", g_AllocatedServerFrameJobID);
	}
	virtual void GetFun(void) const { }
	virtual void GetVar(void) const
	{
		g_GameDll.FindPatternSIMD("66 89 54 24 ?? 53 55 56 57 41 54 48 81 EC ?? ?? ?? ??")
			.FindPatternSelf("39 05").ResolveRelativeAddressSelf(2, 6).GetPtr(g_CurrentServerFrameJobID);

		g_GameDll.FindPatternSIMD("48 83 EC 28 FF 15 ?? ?? ?? ?? 8B 0D ?? ?? ?? ??")
			.FindPatternSelf("8B 0D").ResolveRelativeAddressSelf(2, 6).GetPtr(g_AllocatedServerFrameJobID);
	}
	virtual void GetCon(void) const { }
	virtual void Detour(const bool /*bAttach*/) const { }
};
///////////////////////////////////////////////////////////////////////////////

#endif // !_TOOLS

#endif // THREADTOOLS_H
