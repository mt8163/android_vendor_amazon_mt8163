#ifndef FDLEAK_H

#ifdef __cplusplus
extern "C" {
#endif

#define FDLEAKDEBUG_EXPORT __attribute__ ((visibility("default")))

typedef int (*FDDebugClose)(int);
typedef int (*FDDebugOpen)(const char *, int, ...);
typedef int (*FDDebugOpenat)(int, const char *, int, ...);
typedef int (*FDDebugCreat)(const char*, mode_t);
typedef int (*FDDebugPipe)(int*);
typedef int (*FDDebugPipe2)(int *, int);
typedef int (*FDDebugSocket)(int, int, int);
typedef int (*FDDebugAccept)(int, struct sockaddr*, socklen_t*);
typedef int (*FDDebugSocketpair)(int, int, int, int *);
typedef int (*FDDebugDup)(int);
typedef int (*FDDebugDup2)(int, int);
typedef int (*FDDebugDup3)(int, int, int);
typedef int (*FDDebugFcntl)(int, int, ...);
typedef int (*FDDebugEventfd)(unsigned int, int);
typedef int (*FDDebugEpoll_create)(int);

#define FD_BACKTRACE_SIZE 10  // max-depth default 10

#define FD_RECORD_THD   256
#define FD_TABLE_SIZE   1000  //record MAX 1000, resmon warning threshold 512
#define FD_MAX_SIZE     1024
#define FD_BIT_MAP_SIZE (FD_MAX_SIZE/sizeof(int))
#define FD_HASH_TABLE_SIZE      10

// store fd backtrace entry
typedef struct fd_backtrace_entry {
    size_t slot;
    size_t allocations;
    struct fd_backtrace_entry* prev;
    struct fd_backtrace_entry* next;
    size_t numEntries;
    int fd_bit;
    int fd_bit_map[FD_BIT_MAP_SIZE];
    uintptr_t backtrace[0];
}FdBtEntry, *PFdBtEntry;

typedef struct {
    PFdBtEntry gPMaxFdBtEntry;  // record max alloc FdBtEntry pointer
    PFdBtEntry pbtentry_list[FD_HASH_TABLE_SIZE];
} FDBACKTRACEHashTable, *PFDBACKTRACEHashTable;

typedef struct {
    unsigned int flag; //1: FD in use, 0: FD freed
    PFdBtEntry pbtentry;
} FDBACKTRACETable, *PFDBACKTRACETable;

// =============================================================================
//  FD leakage debugging backtrace record and remove routines.
// =============================================================================
void fdleak_record_backtrace(int fd);
void fdleak_remove_backtrace(int fd);


#ifdef __cplusplus
}
#endif
#endif

