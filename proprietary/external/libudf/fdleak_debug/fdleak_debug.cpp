#include <assert.h>
#include <ubrd_config.h>
#include "fdleak_debug.h"
#include "../include/backtrace.h"
#include "../include/recorder.h"

#include <aee.h>
#include <pthread.h>

#include <unwindstack/Memory.h>
#include <unwindstack/Elf.h>
#include <unwindstack/MapInfo.h>
#include <unwindstack/Maps.h>

#ifdef __cplusplus
extern "C" {
#endif

void *gFDBtMspace = NULL;
static uintptr_t debug_flag_backtrace[20];

//  fix logd deadlock
pthread_key_t g_disable_flag;
static bool DisableDebugCalls() {
  if (pthread_getspecific(g_disable_flag) != nullptr) {
    return true;
  }
  return false;
}

static void SetDebugDisable(bool disable) {
  if (disable) {
    pthread_setspecific(g_disable_flag, reinterpret_cast<void*>(1));
  } else {
    pthread_setspecific(g_disable_flag, nullptr);
  }
}

class ScopedDisableDebug {
 public:
  ScopedDisableDebug() : disabled_(DisableDebugCalls()) {
    if (!disabled_) {
      SetDebugDisable(true);
    }
  }
  ~ScopedDisableDebug() {
    if (!disabled_) {
      SetDebugDisable(false);
    }
  }

 private:
  bool disabled_;

  ScopedDisableDebug(const ScopedDisableDebug&) = delete;
  void operator=(const ScopedDisableDebug&) = delete;
};

// =============================================================================
// fd related system call need rehook
// =============================================================================
static FDDebugClose real_close = NULL;
static FDDebugOpen real_open = NULL;
static FDDebugOpenat real_openat = NULL;
static FDDebugCreat real_creat = NULL;
static FDDebugPipe real_pipe = NULL;
static FDDebugPipe2 real_pipe2 = NULL;
static FDDebugSocket real_socket = NULL;
static FDDebugAccept real_accept = NULL;
static FDDebugSocketpair real_socketpair = NULL;
static FDDebugDup real_dup = NULL;
static FDDebugDup2 real_dup2 = NULL;
static FDDebugDup3 real_dup3 = NULL;
static FDDebugFcntl real_fcntl = NULL;
static FDDebugEventfd real_eventfd = NULL;
static FDDebugEpoll_create real_epoll_create = NULL;
static void *fdleak_libc_handle = NULL;

static char fdleak_init_error_msg[128];
static int fdleak_inti_flag = 0;

typedef int (*AEE_SYSTEM_EXCEPTION_FUNC)(const char *, const char *, unsigned int, const char *,...);
void fdleak_aee_system_exception(char *aee_output_message)
{
    void *handle = NULL;
    void *libc = NULL;
    char c, content[1024], lib_aed[15];
    int i,fd;
    ssize_t ret;
    memset(content, 0x0, sizeof(content));
    int (*open_ptr)(const char *, int, ...) = NULL;
    AEE_SYSTEM_EXCEPTION_FUNC func = NULL;
    snprintf(lib_aed, sizeof(lib_aed), "libaed.so");
    libc =(void *)dlopen("libc.so", RTLD_NOW|RTLD_GLOBAL);
    if (libc) {
        open_ptr = (typeof(open_ptr))dlsym(libc, "open");
        if (open_ptr) {
            fd = open_ptr("/proc/self/cmdline", O_RDONLY);
            if (fd >= 0) {
                i = 0;
                ret = read(fd, content, sizeof(content));
                if (ret != -1) {
                    content[1023] = '\0';
                    if (strstr(content, "vendor")) {
                        snprintf(lib_aed, sizeof(lib_aed), "libaedv.so");
                    }
                } else {
                    *((volatile size_t *)0)= 0xdeadffff;
                }
            } else {
                *((volatile size_t *)0)= 0xdeadffff;
            }
        } else {
            *((volatile size_t *)0)= 0xdeadffff;
        }
        dlclose(libc);
    } else {
        *((volatile size_t *)0)= 0xdeadffff;
    }
    handle =(void *)dlopen(lib_aed, RTLD_NOW);
    if (handle) {
        func = (AEE_SYSTEM_EXCEPTION_FUNC)(dlsym(handle, "aee_system_exception"));
        if (func) {
            if (func("[FDLEAK_DEBUG]\nBacktrace", NULL, DB_OPT_PROCESS_COREDUMP, " \n%s", aee_output_message) == -1)
                abort();
        } else {
            *((volatile size_t *)0)= 0xdeadffff;
        }
        dlclose(handle);
    } else {
        *((volatile size_t *)0)= 0xdeadffff;
    }
}

static void fd_func_rehook(void) {
    if (fdleak_libc_handle == NULL) {
        fdleak_libc_handle = dlopen("libc.so", RTLD_NOW|RTLD_GLOBAL);
        if (fdleak_libc_handle == NULL) {
            ubrd_error_log("%s could not open libc.so: %s\n", __FUNCTION__, dlerror());
            return;
        }
    }

    real_close = (FDDebugClose)dlsym(fdleak_libc_handle, "close");
    real_open = (FDDebugOpen)dlsym(fdleak_libc_handle, "open");
    real_openat = (FDDebugOpenat)dlsym(fdleak_libc_handle, "openat");
    real_creat = (FDDebugCreat)dlsym(fdleak_libc_handle, "creat");
    real_pipe = (FDDebugPipe)dlsym(fdleak_libc_handle, "pipe");
    real_pipe2 = (FDDebugPipe2)dlsym(fdleak_libc_handle, "pipe2");
    real_socket = (FDDebugSocket)dlsym(fdleak_libc_handle, "socket");
    real_accept = (FDDebugAccept)dlsym(fdleak_libc_handle, "accept");
    real_socketpair = (FDDebugSocketpair)dlsym(fdleak_libc_handle, "socketpair");
    real_dup = (FDDebugDup)dlsym(fdleak_libc_handle, "dup");
    real_dup2 = (FDDebugDup2)dlsym(fdleak_libc_handle, "dup2");
    real_dup3 = (FDDebugDup3)dlsym(fdleak_libc_handle, "dup3");
    real_fcntl = (FDDebugFcntl)dlsym(fdleak_libc_handle, "fcntl");
    real_eventfd = (FDDebugEventfd)dlsym(fdleak_libc_handle, "eventfd");
    real_epoll_create = (FDDebugEpoll_create)dlsym(fdleak_libc_handle, "epoll_create");
    ubrd_debug_log("[FDLEAK_DEBUG]real_open:%p,real_openat:%p,real_creat:%p\n",
                    real_open, real_openat, real_creat);
}

FDLEAKDEBUG_EXPORT
int close(int fd) {
    if (DisableDebugCalls()) {
        if (!real_close) fd_func_rehook();
        return real_close(fd);
    }
    ScopedDisableDebug disable;

    fdleak_remove_backtrace(fd);
    if (!real_close) fd_func_rehook();
    return real_close(fd);
}

FDLEAKDEBUG_EXPORT
int open(const char *pathname, int flags, ...) {
    mode_t mode = 0;
    int tmpfd = -1;

    if (!real_open) fd_func_rehook();
    if (flags & O_CREAT)
    {
        va_list  args;
        va_start(args, flags);
        mode = (mode_t) va_arg(args, int);
        va_end(args);
        tmpfd = real_open(pathname, flags, mode);
    }
    else {
        tmpfd = real_open(pathname, flags);
    }

    if (DisableDebugCalls()) {
        return tmpfd;
    }
    ScopedDisableDebug disable;

    fdleak_record_backtrace(tmpfd);
    return tmpfd;
}

FDLEAKDEBUG_EXPORT
int openat(int dirfd, const char *pathname, int flags, ...)
{
    mode_t mode = 0;
    int tmpfd = -1;

    if (!real_openat) fd_func_rehook();
    if (flags & O_CREAT)
    {
        va_list  args;
        va_start(args, flags);
        mode = (mode_t) va_arg(args, int);
        va_end(args);
        tmpfd = real_openat(dirfd, pathname, flags, mode);
    }
    else {
        tmpfd = real_openat(dirfd, pathname, flags);
    }

    if (DisableDebugCalls()) {
        return tmpfd;
    }
    ScopedDisableDebug disable;

    fdleak_record_backtrace(tmpfd);
    return tmpfd;
}

FDLEAKDEBUG_EXPORT
int creat(const char *path, mode_t mode)
{
    int fd = -1;

    if (!real_creat) fd_func_rehook();
    fd = real_creat(path, mode);

    if (DisableDebugCalls()) {
        return fd;
    }
    ScopedDisableDebug disable;

    fdleak_record_backtrace(fd);
    return fd;
}

FDLEAKDEBUG_EXPORT
int pipe(int *pipefds) {
    int ret;

    if (!real_pipe) fd_func_rehook();
    ret = real_pipe(pipefds);

    if (DisableDebugCalls()) {
        return ret;
    }
    ScopedDisableDebug disable;

    if (ret == 0) {
        fdleak_record_backtrace(pipefds[0]);
        fdleak_record_backtrace(pipefds[1]);
    }
    return ret;
}

FDLEAKDEBUG_EXPORT
int pipe2(int *pipefds, int flag) {
    int ret;

    ubrd_debug_log("[FDLEAK_DEBUG]:%s\n", __FUNCTION__);
    if (!real_pipe2) fd_func_rehook();
    ret = real_pipe2(pipefds, flag);

    if (DisableDebugCalls()) {
        return ret;
    }
    ScopedDisableDebug disable;

    if (ret == 0) {
        fdleak_record_backtrace(pipefds[0]);
        fdleak_record_backtrace(pipefds[1]);
    }
    return ret;
}

FDLEAKDEBUG_EXPORT
int socket(int domain, int type, int protocol) {
    int fd;
    if (!real_socket) fd_func_rehook();
    fd = real_socket(domain, type, protocol);

    if (DisableDebugCalls()) {
        return fd;
    }
    ScopedDisableDebug disable;

    fdleak_record_backtrace(fd);
    return fd;
}

FDLEAKDEBUG_EXPORT
int accept(int serverfd, struct sockaddr *addr, socklen_t *addrlen) {
    int fd;

    if (!real_accept) fd_func_rehook();
    fd = real_accept(serverfd, addr, addrlen);

    if (DisableDebugCalls()) {
        return fd;
    }
    ScopedDisableDebug disable;

    fdleak_record_backtrace(fd);
    return fd;
}

FDLEAKDEBUG_EXPORT
int socketpair(int domain, int type, int protocal, int sv[2])
{
    int ret;

    if (!real_socketpair) fd_func_rehook();
    ret = real_socketpair(domain, type, protocal, sv);

    if (DisableDebugCalls()) {
        return ret;
    }
    ScopedDisableDebug disable;

    if (ret == 0){
        fdleak_record_backtrace(sv[0]);
        fdleak_record_backtrace(sv[1]);
    }
    return ret;
}

FDLEAKDEBUG_EXPORT
int dup(int oldfd) {
    int newfd = 0;

    if (!real_dup) fd_func_rehook();
    newfd = real_dup(oldfd);

    if (DisableDebugCalls()) {
        return newfd;
    }
    ScopedDisableDebug disable;

    fdleak_record_backtrace(newfd);
    return newfd;
}

FDLEAKDEBUG_EXPORT
int dup2(int oldfd, int targetfd) {
    int newfd = 0;

    if (!real_dup2) fd_func_rehook();
    fdleak_remove_backtrace(targetfd);
    newfd = real_dup2(oldfd, targetfd);

    if (DisableDebugCalls()) {
        return newfd;
    }
    ScopedDisableDebug disable;

    fdleak_record_backtrace(newfd);
    return newfd;
}

FDLEAKDEBUG_EXPORT
int dup3(int oldfd, int targetfd, int flags) {
    int newfd = 0;

    if (!real_dup3) fd_func_rehook();
    fdleak_remove_backtrace(targetfd);
    newfd = real_dup3(oldfd, targetfd, flags);

    if (DisableDebugCalls()) {
        return newfd;
    }
    ScopedDisableDebug disable;

    fdleak_record_backtrace(newfd);
    return newfd;
}

FDLEAKDEBUG_EXPORT
int fcntl(int fd, int cmd, ...)
{
    va_list args;
    va_start(args, cmd);
    void *arg = va_arg(args, void *);
    va_end(args);

    if (!real_fcntl) fd_func_rehook();
    int ret = real_fcntl(fd, cmd, arg);
    if (DisableDebugCalls()) {
        return ret;
    }

    ScopedDisableDebug disable;
    if ((cmd == F_DUPFD) || (cmd == F_DUPFD_CLOEXEC)) {
        fdleak_record_backtrace(ret);
    }
    return ret;
}

FDLEAKDEBUG_EXPORT
int eventfd(unsigned int initval, int flags)
{
    int fd = -1;

    if (!real_eventfd) fd_func_rehook();
    fd = real_eventfd(initval, flags);

    if (DisableDebugCalls()) {
        return fd;
    }
    ScopedDisableDebug disable;

    fdleak_record_backtrace(fd);
    return fd;
}

FDLEAKDEBUG_EXPORT
int epoll_create(int size)
{
    int fd = -1;

    if (!real_epoll_create) fd_func_rehook();
    fd = real_epoll_create(size);

    if (DisableDebugCalls()) {
        return fd;
    }
    ScopedDisableDebug disable;

    fdleak_record_backtrace(fd);
    return fd;
}

// =============================================================================
// Global variables
// =============================================================================
static pthread_mutex_t gFDLeakMutex = PTHREAD_MUTEX_INITIALIZER;
static PFDBACKTRACETable gPFDBACKTRACETable = NULL; // record fd backtrace
static void* gFDMspace = NULL;
static size_t gFDMspaceSize = 0;
static volatile void* gFDMspaceBackup = NULL;
static int fd_record_thd = FD_RECORD_THD;
static volatile int rlimit_flag = 0;
static volatile int aee_flag = 0;
static int fd_bt2log = 0;

// manage fd backtrace
static PFDBACKTRACEHashTable gPFDBtEntryTable = NULL;

#ifdef MTK_USE_RESERVED_EXT_MEM
#define EXM_DEV "/dev/exm0"
static void *gFDMspaceBuffer = MAP_FAILED;
#endif

//using pthread_atfork for fork deadlock scenario issue
static void fdleak_debug_prepare(void) {
    pthread_mutex_lock(&gFDLeakMutex);

    gFDMspaceBackup = gFDMspace;
    gFDMspace = NULL; //force NULL to avoid fd backtrace record

#ifdef MTK_USE_RESERVED_EXT_MEM
    // create anon memory
    gFDMspaceBuffer = mmap(NULL, gFDMspaceSize,
                    PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (gFDMspaceBuffer == MAP_FAILED) {
        ubrd_error_log("[FDLEAK_DEBUG]prepare: anon mem MAP_FAILED\n");
        return;
    }
    ubrd_debug_log("[FDLEAK_DEBUG]prepare: anon mmap %p\n", gFDMspaceBuffer);

    // copy external memory to anon memory
    ubrd_debug_log("[FDLEAK_DEBUG]prepare: start copy ext to anon mem");
    memcpy(gFDMspaceBuffer, (const void*)gFDMspaceBackup, gFDMspaceSize);
    ubrd_debug_log("[FDLEAK_DEBUG]prepare: copy done");
#endif
    ubrd_debug_log("[FDLEAK_DEBUG]prepare: gFDMspaceBackup %p\n", gFDMspaceBackup);
}

static void fdleak_debug_parent(void) {
#ifdef MTK_USE_RESERVED_EXT_MEM
    // free anon memory mmap in pthread_atfork->prepare
    ubrd_debug_log("[FDLEAK_DEBUG]parent: free anon mem %p", gFDMspaceBuffer);
    if (gFDMspaceBuffer != MAP_FAILED)
        munmap(gFDMspaceBuffer, gFDMspaceSize);
#endif

    //restore for fd backtrace record
    gFDMspace = (void*)gFDMspaceBackup;
    pthread_mutex_unlock(&gFDLeakMutex);
    ubrd_debug_log("[FDLEAK_DEBUG]parent: restore gFDMspace:%p", gFDMspace);
}

static void fdleak_debug_child(void) {
    pthread_mutexattr_t attr;
#ifdef MTK_USE_RESERVED_EXT_MEM
    int fd = -1;
    void *gFDMspaceTmp = MAP_FAILED;
    void *old_mmap_addr = (void*)gFDMspaceBackup;

    if(gFDMspaceBuffer == MAP_FAILED) {
        ubrd_error_log("[FDLEAK_DEBUG]child: gFDMspaceBuffer MAP_FAILED\n");
        return;
    }

    ubrd_debug_log("[FDLEAK_DEBUG]child: child is called");

    // create external memory with map_fixed
    fd = open(EXM_DEV, O_RDWR);
    if (fd >= 0) {
        gFDMspaceTmp = mmap(old_mmap_addr, gFDMspaceSize,
                             PROT_READ|PROT_WRITE, MAP_SHARED|MAP_FIXED, fd, 0);
        if(gFDMspaceTmp == MAP_FAILED) {
            ubrd_error_log("[FDLEAK_DEBUG]child: ext mem MAP_FAILED\n");
        }
        close(fd);
    }
    else {
        ubrd_error_log("[FDLEAK_DEBUG]child: open %s for ext mem, fail\n", EXM_DEV);
    }
    
    // use anon memory
    if (gFDMspaceTmp == MAP_FAILED) {
        gFDMspaceTmp = mmap(old_mmap_addr, gFDMspaceSize,
                             PROT_READ|PROT_WRITE,
                             MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0);
        if (gFDMspaceTmp == MAP_FAILED) {
            ubrd_error_log("[FDLEAK_DEBUG]child: anon mem MAP_FAILED\n");
            assert(0);
            return;
        }
    }

    assert(gFDMspaceTmp == gFDMspaceBackup);

    // copy anon memory to external memory.
    ubrd_debug_log("[FDLEAK_DEBUG]child: start copy anon to ext mem");
    memcpy(gFDMspaceTmp, gFDMspaceBuffer, gFDMspaceSize);
    ubrd_debug_log("[FDLEAK_DEBUG]child: copy done");

    // free anon memory mmap in pthread_atfork->prepare
    ubrd_debug_log("[FDLEAK_DEBUG]child: free anon mem %p", gFDMspaceBuffer);
    munmap(gFDMspaceBuffer, gFDMspaceSize);
#endif

    //restore for fd backtrace record
    gFDMspace = (void*)gFDMspaceBackup;

    // reinit mutex for child
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&gFDLeakMutex, &attr);
    ubrd_debug_log("[FDLEAK_DEBUG]child: restore gFDMspace:%p", gFDMspace);
}

//add to init section
FDLEAKDEBUG_EXPORT
void fdleak_debug_initialize(void) {
    int fdleak_debug_enable = 0;
    char env[PROP_VALUE_MAX];
    char debug_program[PROP_VALUE_MAX];
    const char *progname = NULL;
    int fd = -1;

    fd_func_rehook();
#ifdef _MTK_ENG_
    fdleak_debug_enable = 1;
#else
    fdleak_debug_enable = 0;
#endif

    if (__system_property_get("persist.vendor.debug.fdleak", env)) {
        fdleak_debug_enable = atoi(env);
    }

    if(!fdleak_debug_enable) {
        return;
    }

    progname = getprogname();
    if (!progname) {
        return;
    }
    if (strstr(progname, "/system/bin/logd")) {
        fdleak_inti_flag = 1;
    }

    // white list: filter process
    if (strstr(progname, "system/bin/aee") || //bypass aee and aee_dumpstate
        strstr(progname, "/system/bin/logd")) {
        fdleak_debug_enable = 0;
    }

    // control for only one specific program to enable fdleak debug.
    if (__system_property_get("persist.vendor.debug.fdleak.program", debug_program)) {
        if (strstr(progname, debug_program)) {
            fdleak_debug_enable = 1;
        }
        else {
            fdleak_debug_enable = 0;
        }
    }

    if(!fdleak_debug_enable) {
        return;
    }

    if (__system_property_get("persist.vendor.debug.fdleak.bt2log", env)) {
        fd_bt2log = atoi(env);
    }

    if (__system_property_get("persist.vendor.debug.fdleak.thd", env)) {
        fd_record_thd = atoi(env);
    }

    gFDMspace = MAP_FAILED;
    // default FD 256~1000, 512 size maybe no problem
    gFDMspaceSize = ALIGN_UP_TO_PAGE_SIZE((sizeof(FdBtEntry)+FD_BACKTRACE_SIZE*sizeof(size_t))*(FD_MAX_SIZE>>1));

#ifdef MTK_USE_RESERVED_EXT_MEM
    fd = open(EXM_DEV, O_RDWR);
    if(fd >= 0) {
        gFDMspace = mmap(NULL, gFDMspaceSize,
                         PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        if (gFDMspace == MAP_FAILED) {
            if (fdleak_inti_flag) {
                snprintf(fdleak_init_error_msg, sizeof(fdleak_init_error_msg), "map ext mem fail!\n");
            } else {
                ubrd_error_log("map ext mem fail!\n");
            }
        }
        close(fd);
        fd = -1;
    }
#endif
    if (gFDMspace == MAP_FAILED) {
        gFDMspace = mmap(NULL, gFDMspaceSize,
                             PROT_READ|PROT_WRITE|PROT_MALLOCFROMBIONIC, MAP_PRIVATE|MAP_ANONYMOUS, fd, 0);
        if (gFDMspace == MAP_FAILED) {
            if (fdleak_inti_flag) {
                snprintf(fdleak_init_error_msg, sizeof(fdleak_init_error_msg), "[FDLEAK_DEBUG] logd map anon mem fail!\n");
            } else {
                ubrd_error_log("[FDLEAK_DEBUG]%s map anon mem fail!\n", progname);
            }
            gFDMspace = NULL;
            return;
        }
        prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, gFDMspace, gFDMspaceSize, "FDLEAKDebug");
    }

    memset(gFDMspace, 0x0, gFDMspaceSize);
    gFDBtMspace = create_mspace_with_base(gFDMspace, gFDMspaceSize, 0);
    gPFDBACKTRACETable = (PFDBACKTRACETable)mspace_malloc(gFDBtMspace, FD_MAX_SIZE * sizeof(FDBACKTRACETable));
    if (!gPFDBACKTRACETable) {
        if (fdleak_inti_flag) {
            snprintf(fdleak_init_error_msg, sizeof(fdleak_init_error_msg), "[ERROR]gPFDBACKTRACETable mspace_malloc fails, entry\n");
        } else {
            ubrd_error_log("[ERROR]gPFDBACKTRACETable mspace_malloc fails, entry\n");
        }
        return;
    }

    gPFDBtEntryTable = (PFDBACKTRACEHashTable)mspace_malloc(gFDBtMspace, sizeof(FDBACKTRACEHashTable));
    if (!gPFDBtEntryTable) {
        if (fdleak_inti_flag) {
            snprintf(fdleak_init_error_msg, sizeof(fdleak_init_error_msg), "[ERROR]gPFDBtEntryTable mspace_malloc fails, entry\n");
        } else {
            ubrd_error_log("[ERROR]gPFDBtEntryTable mspace_malloc fails, entry\n");
        }
        return;
    }
    memset(gPFDBtEntryTable, 0x0, sizeof(FDBACKTRACEHashTable));

    if(pthread_atfork(fdleak_debug_prepare, fdleak_debug_parent, fdleak_debug_child)) {
        if (fdleak_inti_flag) {
            snprintf(fdleak_init_error_msg, sizeof(fdleak_init_error_msg), "[FDLEAK_DEBUG] logd pthread_atfork fail\n");
        } else {
            ubrd_error_log("[FDLEAK_DEBUG]%s: pthread_atfork fail\n", progname);
        }
    }
    ubrd_debug_log("[FDLEAK_DEBUG]mmap:%p-%x,FD_TABLE_SIZE:%d,backtrace max-depth:%d\n",
                      (void *)gPFDBACKTRACETable, gFDMspaceSize, FD_TABLE_SIZE, FD_BACKTRACE_SIZE);
    int error = pthread_key_create(&g_disable_flag, nullptr);
    if (error != 0) {
        ubrd_error_log("[FDLEAK_DEBUG] pthread_key_create failed: %s", strerror(error));
        return;
    }
    pthread_setspecific(g_disable_flag, nullptr);
    return;
}

static void dump_bt2log(uintptr_t* backtrace, size_t numEntries, int fd, int alloc)
{
    char buf[32];
    char tmp[32*FD_BACKTRACE_SIZE];
    size_t i;

    tmp[0] = 0; // Need to initialize tmp[0] for the first strcat
    for (i=0; i<numEntries; i++) {
        snprintf(buf, sizeof buf, "%zu: %p\n", i, (void *)backtrace[i]);
        strlcat(tmp, buf, sizeof tmp);
    }
    ubrd_info_log("%s fd %d call stack:\n%s", alloc==1?"alloc":"free", fd, tmp);
}

// =============================================================================
// return value
//      0 : success
//     -1: fail
// =============================================================================

int unlink_entry(PFdBtEntry entry) {
    PFdBtEntry bk = entry->prev;
    PFdBtEntry fd = entry->next;
    if (!bk) {  // head
        gPFDBtEntryTable->pbtentry_list[entry->slot] = fd;
        if (fd) fd->prev = NULL;  // not only one entry in the slot
    } else if (!fd) {  // tail
        bk->next = NULL;
    } else {  // middle
        bk->next = fd;
        fd->prev = bk;
    }

    return 0;
}

int insert_entry(PFdBtEntry entry, size_t slot) {
    // insert the entry to the double link list without head node
    entry->prev = NULL;
    if (gPFDBtEntryTable->pbtentry_list[slot] == NULL) {
        entry->next = NULL;
    } else {
        (gPFDBtEntryTable->pbtentry_list[slot])->prev = entry;
        entry->next = gPFDBtEntryTable->pbtentry_list[slot];
    }
    gPFDBtEntryTable->pbtentry_list[slot] = entry;

    return 0;
}

static int descend_compare_bytes(unsigned char *e1, unsigned char *e2, size_t n) {
    const unsigned char*  p1   = e1;
    const unsigned char*  start1 = e1 - n + 1;
    const unsigned char*  p2   = e2;
    int                   d = 0;

    while (1) {
        if (d || p1 < start1) break;
        d = (int)*p1-- - (int)*p2--;

        if (d || p1 < start1) break;
        d = (int)*p1-- - (int)*p2--;

        if (d || p1 < start1) break;
        d = (int)*p1-- - (int)*p2--;

        if (d || p1 < start1) break;
        d = (int)*p1-- - (int)*p2--;
    }

    return d;
}

static PFdBtEntry find_entry(uintptr_t* backtrace, size_t numEntries, size_t slot) {
    PFdBtEntry entry = gPFDBtEntryTable->pbtentry_list[slot];
    while (entry) {
        /*
        * See if the entry matches exactly.
        */
        size_t cmp_bytes = numEntries * sizeof(intptr_t);
        unsigned char* end1 = (unsigned char*)(backtrace)+cmp_bytes-1;
        unsigned char* end2 = (unsigned char*)(entry->backtrace)+cmp_bytes-1;
        if (!descend_compare_bytes(end1, end2, cmp_bytes))
            return entry;

        entry = entry->next;
    }

    return NULL;
}

static inline size_t get_hash(uintptr_t* backtrace, size_t numEntries) {
    if (backtrace == NULL) return 0;

    size_t hash = 0;
    size_t i;
    for (i = 0 ; i < numEntries ; i++) {
        hash = (hash * 33) + (backtrace[i] >> 2);
    }

    return hash;
}

static inline void record_fd_info(uintptr_t* backtrace, size_t numEntries, int fd)
{
    //record fd_record_thd < fd < FD_TABLE_SIZE
    PFDBACKTRACETable pfdbacktrace = gPFDBACKTRACETable+fd;
    if (pfdbacktrace->flag) {
        pfdbacktrace->pbtentry->allocations--;
        if (pfdbacktrace->pbtentry->allocations <= 0) {
            unlink_entry(pfdbacktrace->pbtentry);
            if (pfdbacktrace->pbtentry == gPFDBtEntryTable->gPMaxFdBtEntry) {
                gPFDBtEntryTable->gPMaxFdBtEntry = NULL;
            }
            mspace_free(gFDBtMspace, pfdbacktrace->pbtentry);
        }
    }

    pfdbacktrace->flag = 0x1;
    PFdBtEntry head, entry;

    size_t hash = get_hash(backtrace, numEntries);
    size_t slot = hash % FD_HASH_TABLE_SIZE;
    entry = find_entry(backtrace, numEntries, slot);
    if (entry != NULL) {
        ubrd_debug_log("find entry: %p\n", entry);
        entry->allocations++;
        if (entry->allocations >= 1024) {
            abort();
        }
        entry->fd_bit |= 1 << (fd/32);
        entry->fd_bit_map[(fd/32)] |= 1 << (fd % 32);
        if (!(gPFDBtEntryTable->gPMaxFdBtEntry)) {
            gPFDBtEntryTable->gPMaxFdBtEntry = entry;
        } else {
            if (entry->allocations > gPFDBtEntryTable->gPMaxFdBtEntry->allocations)
                gPFDBtEntryTable->gPMaxFdBtEntry = entry;
        }
    } else {
        // create a new entry
        if (gFDMspace == NULL) {
            ubrd_error_log("[%s]gDebugMspace == NULL\n", __FUNCTION__);
            return;
        }
        entry = (PFdBtEntry)mspace_malloc(gFDBtMspace, sizeof(FdBtEntry) + numEntries*sizeof(intptr_t));
        ubrd_debug_log("mspace_malloc bt_entry: %p", entry);
        if (!entry) {
            ubrd_error_log("[ERROR]mspace_malloc fails, entry\n");
            return;
        }

        entry->slot = slot;
        entry->allocations = 1;
        entry->fd_bit = 0;
        entry->fd_bit |= 1 << (fd / 32);
        memset(entry->fd_bit_map, 0, sizeof(entry->fd_bit_map));
        entry->fd_bit_map[(fd/32)] |= 1 << (fd % 32);
        entry->numEntries = numEntries;
        memcpy(entry->backtrace, backtrace, numEntries * sizeof(intptr_t));

        // bin index ++
        insert_entry(entry, slot);
    }
    pfdbacktrace->pbtentry = entry;
    ubrd_debug_log("record fd: %d\n", fd);
}

// =============================================================================
// return value
//      0 : success
//     -1: fail
// =============================================================================
static inline void remove_fd_info(int fd)
{
    //remove fd_record_thd < fd < FD_TABLE_SIZE
#if 0
    if(!gPFDBACKTRACETable[fd].flag) {
        uintptr_t backtrace[FD_BACKTRACE_SIZE];
        size_t numEntries = 0;
        int i = 0;
        ubrd_error_log("[%d] remove an unexist fd: %d\n", getpid(), fd);
        numEntries = ubrd_get_backtrace(backtrace, FD_BACKTRACE_SIZE, 1);

        for (i = 0; i < numEntries; i++) {
            ubrd_error_log("bt: %p\n", backtrace[i]);
        }
    }
#endif
    if (gPFDBACKTRACETable[fd].flag) {
        gPFDBACKTRACETable[fd].flag = 0;
        PFdBtEntry entry = gPFDBACKTRACETable[fd].pbtentry;
        if (!entry) return;
        entry->fd_bit_map[(fd/32)] &= ~(1 << (fd % 32));
        if (entry->fd_bit_map[fd/32] == 0)
            entry->fd_bit &= ~(1 << (fd/32));
        entry->allocations--;
        if (entry->allocations <= 0) {
            unlink_entry(entry);
            if (entry == gPFDBtEntryTable->gPMaxFdBtEntry) {
                gPFDBtEntryTable->gPMaxFdBtEntry = NULL;
            }
            mspace_free(gFDBtMspace, entry);
        }
        gPFDBACKTRACETable[fd].pbtentry = NULL;
        ubrd_debug_log("remove fd: %d\n", fd);
    }
}

FDLEAKDEBUG_EXPORT
void fdleak_record_backtrace(int fd) {
    if (gFDMspace && (fd >= fd_record_thd) && (fd < FD_TABLE_SIZE)) {
        uintptr_t backtrace[FD_BACKTRACE_SIZE];
        size_t numEntries = 0;

    #if defined(__LP64__)
        numEntries = ubrd_get_backtrace(backtrace, FD_BACKTRACE_SIZE, 1);
    #else
        numEntries = ubrd_get_backtrace_common(NULL,
                                               backtrace,
                                               FD_BACKTRACE_SIZE,
                                               UBRD_CORKSCREW_UNWIND_BACKTRACE);
    #endif
        if (numEntries > 0) {
            pthread_mutex_lock(&gFDLeakMutex);
            record_fd_info(backtrace, numEntries, fd);
            pthread_mutex_unlock(&gFDLeakMutex);
            if (fd_bt2log)
                dump_bt2log(backtrace, numEntries, fd, 1);
        } else {
            gPFDBACKTRACETable[fd].flag = 1;
        }
    }
    else if (gFDMspace && (fd >= FD_TABLE_SIZE)) {
        static struct rlimit r;
        if(!rlimit_flag) {
            if(!getrlimit(RLIMIT_NOFILE, &r)) {
                ubrd_debug_log("[FDLEAK_DEBUG]fd over FD_TABLE_SIZE:%d\n", FD_TABLE_SIZE);
                rlimit_flag = 1;
            }
        }
        else if((fd >= 1024) && (!aee_flag)) {
            aee_flag = 1;
            ubrd_debug_log("[FDLEAK_DEBUG]fd over RLIMIT_NOFILE:%ld\n", r.rlim_cur);

            PFdBtEntry entry = gPFDBtEntryTable->gPMaxFdBtEntry;
            if (!entry) {
                ubrd_error_log("max fd is opened, but pmaxentry is empty return directly\n");
                return;
            }

            char buf[64];
            char tmp[64*(FD_BACKTRACE_SIZE+1)];  // +1 for print max count
            memset(tmp, 0, sizeof(tmp));
            uintptr_t relativ_pc = 0;

            unwindstack::LocalMaps maps;
            bool success = maps.Parse();
            std::shared_ptr<unwindstack::Memory> process_memory;

            // print max count to exp_main
            snprintf(buf, sizeof(buf), "Max fd_bt backtrace use %zu fd\n", entry->allocations);
            strlcat(tmp, buf, sizeof(tmp));

            // dump all fd backtrace
            size_t i;
            uintptr_t pc;
            for (i = 0; i < entry->numEntries; i++) {
                pc = entry->backtrace[i];

                if (success) {
                    unwindstack::MapInfo *map_info = maps.Find(pc);

                    if (map_info) {
                        unwindstack::Elf *elf = map_info->GetElf(process_memory, true);
                        relativ_pc = elf->GetRelPc(pc, map_info);
                    } else {
                        relativ_pc = pc;
                    }
                } else {
                    relativ_pc = pc;
                }

                snprintf(buf, sizeof(buf), "  #0%zu fd %p %p\n", i, (void*)(entry->backtrace[i]), (void*)(relativ_pc));
                strlcat(tmp, buf, sizeof(tmp));
            }

            // trigger system_exception_api
            fdleak_aee_system_exception(tmp);
        }
    }
}

FDLEAKDEBUG_EXPORT
void fdleak_remove_backtrace(int fd) {
    //add for double close issue tracking
    if (gFDMspace && (fd >= fd_record_thd) && (fd < FD_TABLE_SIZE)) {
        pthread_mutex_lock(&gFDLeakMutex);
        remove_fd_info(fd);
        pthread_mutex_unlock(&gFDLeakMutex);
        if (fd_bt2log) {
            uintptr_t backtrace[FD_BACKTRACE_SIZE];
            size_t numEntries = 0;
        #if defined(__LP64__)
            numEntries = ubrd_get_backtrace(backtrace, FD_BACKTRACE_SIZE, 1);
        #else
            numEntries = ubrd_get_backtrace_common(NULL,
                                               backtrace,
                                               FD_BACKTRACE_SIZE,
                                               UBRD_CORKSCREW_UNWIND_BACKTRACE);
        #endif
            if (numEntries > 0)
                dump_bt2log(backtrace, numEntries, fd, 0);
        }
    }
}

#ifdef __cplusplus
}
#endif
