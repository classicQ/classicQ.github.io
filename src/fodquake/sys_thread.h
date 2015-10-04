
struct SysThread;
struct SysMutex;
struct SysSignal;

enum SysThreadPriority
{
	SYSTHREAD_PRIORITY_LOW,
	SYSTHREAD_PRIORITY_NORMAL,
	SYSTHREAD_PRIORITY_HIGH,
};

struct SysThread *Sys_Thread_CreateThread(void (*entrypoint)(void *), void *argument);
void Sys_Thread_DeleteThread(struct SysThread *thread);
int Sys_Thread_SetThreadPriority(struct SysThread *thread, enum SysThreadPriority priority);

struct SysMutex *Sys_Thread_CreateMutex(void);
void Sys_Thread_DeleteMutex(struct SysMutex *mutex);
void Sys_Thread_LockMutex(struct SysMutex *mutex);
void Sys_Thread_UnlockMutex(struct SysMutex *mutex);

struct SysSignal *Sys_Thread_CreateSignal(void);
void Sys_Thread_DeleteSignal(struct SysSignal *signal);
void Sys_Thread_WaitSignal(struct SysSignal *signal);
void Sys_Thread_SendSignal(struct SysSignal *signal);

