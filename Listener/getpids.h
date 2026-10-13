#ifndef GETPIDS_H
#define GETPIDS_H

#include <sched.h>
#include <stdlib.h>

#ifdef __cplusplus
# define EXTERN_C_BEGIN extern "C" {
# define EXTERN_C_END }
#else
# define EXTERN_C_BEGIN
# define EXTERN_C_END
#endif

#define MAX_SECONDARY_PIDS 128
/* Must match the size of idleStats.secondary_ctrs in shmem.h */
#define MAX_SECONDARY_CTRS 64

extern pid_t curr_secondary_pids[MAX_SECONDARY_PIDS];

EXTERN_C_BEGIN
int get_containers(const char *pod_id, int any_parent);
EXTERN_C_END

#endif // GETPIDS_H
