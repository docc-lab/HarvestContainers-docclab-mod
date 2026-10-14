#include <sched.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include "shmem.h"
#include "listener.h"
#include "getpids.h"

#define PATH_MAX 4096
/* Depth below the cgroup root at which pod directories are searched for.
 * cgroupfs driver:  kubepods/besteffort/pod<uid>                       (3)
 * systemd driver:   kubepods.slice/kubepods-besteffort.slice/...pod<uid>.slice (3)
 * Guaranteed pods sit one level higher; 4 leaves headroom. */
#define POD_SEARCH_DEPTH 4

pid_t curr_secondary_pids[MAX_SECONDARY_PIDS] = {0};

/* Candidate roots for pod cgroups. The v2 roots come first: on a v2 host
 * an empty cgroup can exist at cpuset/kubepods/.../pod<uid> (a plain v2
 * child named "cpuset"), and matching it first hides the real pod. The
 * v2 roots don't exist on a v1 host, so the original setup is unaffected. */
static const char *cgroup_roots[] = {
    "/sys/fs/cgroup/kubepods",
    "/sys/fs/cgroup/kubepods.slice",
    "/sys/fs/cgroup/cpuset/kubepods",
    NULL
};

static int is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* A pod cgroup holds one subdirectory per container */
static int has_subdir(const char *dir)
{
    DIR *d;
    struct dirent *ent;
    char path[PATH_MAX];
    int found = 0;

    d = opendir(dir);
    if (!d) return 0;
    while (!found && (ent = readdir(d))) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) continue;
        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
        found = is_dir(path);
    }
    closedir(d);
    return found;
}

/* Recursively search dir for a subdirectory whose name contains pod_id
 * (dashed form, as in "pod<uid>") or its underscored form (systemd driver). */
static int find_pod_dir_in(const char *dir, const char *pod_id, const char *pod_id_us,
                           int depth, char *out, size_t out_len)
{
    DIR *d;
    struct dirent *ent;
    char path[PATH_MAX];
    int found = 0;

    if (depth <= 0) return 0;
    d = opendir(dir);
    if (!d) return 0;
    while (!found && (ent = readdir(d))) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) continue;
        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
        if (!is_dir(path)) continue;
        if (strstr(ent->d_name, pod_id) || strstr(ent->d_name, pod_id_us)) {
            if (!has_subdir(path)) {
                printf("[!] [Listener] Skipping %s: no container cgroups\n", path);
                continue;
            }
            snprintf(out, out_len, "%s", path);
            found = 1;
            break;
        }
        found = find_pod_dir_in(path, pod_id, pod_id_us, depth - 1, out, out_len);
    }
    closedir(d);
    return found;
}

static int find_pod_dir(const char *pod_id, char *out, size_t out_len)
{
    char pod_id_us[PATH_MAX];
    char *c;
    int i;

    snprintf(pod_id_us, sizeof(pod_id_us), "%s", pod_id);
    for (c = pod_id_us; *c; c++) {
        if (*c == '-') *c = '_';
    }
    /* HC_CGROUP_ROOT overrides the search roots (used for testing) */
    if (getenv("HC_CGROUP_ROOT"))
        return find_pod_dir_in(getenv("HC_CGROUP_ROOT"), pod_id, pod_id_us,
                               POD_SEARCH_DEPTH, out, out_len);
    for (i = 0; cgroup_roots[i] != NULL; i++) {
        if (!is_dir(cgroup_roots[i])) continue;
        if (find_pod_dir_in(cgroup_roots[i], pod_id, pod_id_us, POD_SEARCH_DEPTH, out, out_len))
            return 1;
    }
    return 0;
}

/* Read argv[0] of a task into buf; returns bytes read (0 on failure) */
static size_t read_cmdline(const char *tid, char *buf, size_t len)
{
    char proc_path[PATH_MAX];
    FILE *proc_fp;
    size_t n;

    snprintf(proc_path, sizeof(proc_path), "/proc/%s/cmdline", tid);
    proc_fp = fopen(proc_path, "r");
    if (proc_fp == NULL) return 0;
    n = fread(buf, sizeof(char), len - 1, proc_fp);
    buf[n] = '\0';
    fclose(proc_fp);
    return n;
}

static void add_secondary(pid_t parent)
{
    int n = idleCpuStats->nr_secondary_ctrs;
    idleCpuStats->secondary_pid_list[n] = parent;
    idleCpuStats->secondary_ctrs[n].parent_pid = parent;
    idleCpuStats->secondary_ctrs[n].nr_pids = 0;
    idleCpuStats->nr_secondary_ctrs += 1;
}

/* Register the Secondary container at container_path. A container qualifies
 * if one of its tasks is named harvest_launcher, or, when any_parent is set,
 * if it is not a pause container; the first such task becomes the parent
 * whose css_set the Monitor follows.
 * Returns 1 if registered, 0 if skipped, -1 on error. */
int get_container_tasks(const char *container_path, int any_parent)
{
    printf("[+] [Listener] Getting tasks for container %s\n", container_path);
    char tasks_path[PATH_MAX];
    char proc_name[1024];
    size_t len = 0;
    char *tid = NULL;
    pid_t any_pid = 0;
    int registered = 0;
    FILE *fp;

    if (idleCpuStats->nr_secondary_ctrs >= MAX_SECONDARY_CTRS) {
        printf("[!] [Listener] Already tracking %d Secondary containers. Skipping.\n",
               MAX_SECONDARY_CTRS);
        return -1;
    }

    /* cgroup v2 exposes threads via cgroup.threads; v1 via tasks */
    snprintf(tasks_path, sizeof(tasks_path), "%s/cgroup.threads", container_path);
    fp = fopen(tasks_path, "r");
    if (fp == NULL) {
        snprintf(tasks_path, sizeof(tasks_path), "%s/tasks", container_path);
        fp = fopen(tasks_path, "r");
    }
    if (fp == NULL) {
        printf("[!] [Listener] Could not read tasks for container %s. Skipping.\n", container_path);
        return -1;
    }
    idleCpuStats->updatingPids = 1;
    while (getline(&tid, &len, fp) != -1) {
        tid[strcspn(tid, "\n")] = 0;
        /* Tasks may exit between listing and reading /proc; skip them */
        if (read_cmdline(tid, proc_name, sizeof(proc_name)) == 0) continue;
        if (!strcmp(proc_name, "harvest_launcher")) {
            printf("[*] [Listener] Adding harvest_launcher PID: %s\n", tid);
            add_secondary(atoi(tid));
            registered = 1;
            break;
        }
        if (any_parent && any_pid == 0 && strcmp(proc_name, "/pause") != 0) {
            any_pid = atoi(tid);
        }
    }
    if (!registered && any_pid != 0) {
        printf("[*] [Listener] Adding parent PID: %d\n", any_pid);
        add_secondary(any_pid);
        registered = 1;
    }
    idleCpuStats->updatingPids = 0;
    fclose(fp);
    free(tid);

    if (!registered)
        printf("[!] [Listener] Container %s is not a harvest container. Skipping.\n", container_path);
    return registered;
}

/* Register the Secondary containers of pod_id ("pod<uid>").
 * Returns the number of containers registered, or -1 if the pod's cgroup
 * directory could not be found. */
int get_containers(const char *pod_id, int any_parent)
{
    printf("[+] [Listener] Finding containers for pod %s\n", pod_id);
    int total = 0;
    DIR *pod_dir;
    struct dirent *d;
    char pod_path[PATH_MAX];
    char path[PATH_MAX];

    if (!find_pod_dir(pod_id, pod_path, sizeof(pod_path))) {
        printf("[!] [Listener] No cgroup directory found for pod %s\n", pod_id);
        return -1;
    }
    printf("[+] [Listener] get_containers() checking path %s\n", pod_path);
    pod_dir = opendir(pod_path);
    if (!pod_dir) return -1;
    while ((d = readdir(pod_dir))) {
        if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, "..")) continue;
        snprintf(path, sizeof(path), "%s/%s", pod_path, d->d_name);
        if (!is_dir(path)) continue;
        printf("[+] [Listener] Found container @ %s\n", d->d_name);
        if (get_container_tasks(path, any_parent) > 0) total++;
    }
    closedir(pod_dir);

    printf("[+] [Listener] get_containers() done, registered %d containers\n", total);
    return total;
}
