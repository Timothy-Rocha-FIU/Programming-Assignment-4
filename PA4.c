#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#define MAX_MEM_BLKS 100
#define MAX_PROC 20
#define MEM_VIS_SIZE 60
#define DEFAULT_IN_FILE "input.txt"
#define MAX_LINE_LEN 1024

int mem_capacity;

typedef enum
{
    FIRST_APPROACH,
    BEST_APPROACH,
    WORST_APPROACH
} AllocMethod;

typedef enum
{
    PROC_NEW,
    PROC_ACTIVE,
    PROC_DONE
} ProcStatus;

typedef struct
{
    int begin_addr;
    int chunk_size;
    bool available;
    int proc_id;
} MemBlock;

typedef struct
{
    int id;
    int req_size;
    ProcStatus status;
    int block_idx;
} Proc;

typedef struct
{
    int full_size;
    int avail_size;
    int num_blocks;
    MemBlock segments[MAX_MEM_BLKS];
    AllocMethod method;
} MemMgr;

typedef struct
{
    int alloc_tries;
    int alloc_success;
    int alloc_fails;
    double avg_usage;
    double max_usage;
    int ext_frag;
    double frag_percent;
    double avg_frag_size;
} Stats;

void init_mem_mgr(MemMgr *mgr, AllocMethod method);
int find_first_fit(MemMgr *mgr, int size);
int find_best_fit(MemMgr *mgr, int size);
int find_worst_fit(MemMgr *mgr, int size);
bool allocate_mem(MemMgr *mgr, Proc *proc);
void free_mem(MemMgr *mgr, Proc *proc);
bool merge_blocks(MemMgr *mgr, Proc procs[]);
bool load_procs_from_file(const char *filename, Proc procs[], int *num_procs, int *mem_capacity);
void print_mem_simple(MemMgr *mgr, Proc procs[], int num_procs);
void print_mem_detailed(MemMgr *mgr, Proc procs[], int num_procs);
void update_frag_metrics(MemMgr *mgr, Proc procs[], int num_procs, Stats *stats);
void run_sim(MemMgr *mgr, AllocMethod method, Proc procs[], int num_procs, Stats *stats);

int find_first_fit(MemMgr *mgr, int size)
{
    for (int i = 0; i < mgr->num_blocks; i++)
    {
        if (mgr->segments[i].available && mgr->segments[i].chunk_size >= size)
        {
            return i;
        }
    }
    return -1;
}

int find_best_fit(MemMgr *mgr, int size)
{
    int best_idx = -1;
    int min_diff = INT_MAX;

    for (int i = 0; i < mgr->num_blocks; i++)
    {
        if (mgr->segments[i].available && mgr->segments[i].chunk_size >= size)
        {
            int size_diff = mgr->segments[i].chunk_size - size;
            if (size_diff < min_diff)
            {
                min_diff = size_diff;
                best_idx = i;
            }
        }
    }
    return best_idx;
}

int find_worst_fit(MemMgr *mgr, int size)
{
    int worst_idx = -1;
    int max_diff = -1;

    for (int i = 0; i < mgr->num_blocks; i++)
    {
        if (mgr->segments[i].available && mgr->segments[i].chunk_size >= size)
        {
            int size_diff = mgr->segments[i].chunk_size - size;
            if (size_diff > max_diff)
            {
                max_diff = size_diff;
                worst_idx = i;
            }
        }
    }
    return worst_idx;
}

bool merge_blocks(MemMgr *mgr, Proc procs[])
{
    bool did_merge = false;
    for (int i = 0; i < mgr->num_blocks - 1; i++)
    {
        if (mgr->segments[i].available && mgr->segments[i + 1].available)
        {
            mgr->segments[i].chunk_size += mgr->segments[i + 1].chunk_size;
            for (int j = i + 1; j < mgr->num_blocks - 1; j++)
            {
                mgr->segments[j] = mgr->segments[j + 1];
            }
            mgr->num_blocks--;
            did_merge = true;

            for (int k = 0; k < MAX_PROC; k++)
            {
                if (procs[k].block_idx > i + 1)
                {
                    procs[k].block_idx--;
                }
            }
            i--;
        }
    }
    return did_merge;
}

int main(int argc, char *argv[])
{
    char in_file[256] = DEFAULT_IN_FILE;

    if (argc > 1)
        strcpy(in_file, argv[1]);

    srand(time(NULL));

    Proc procs[MAX_PROC];
    int num_procs = 0;
    int mem_capacity = 0;

    if (!load_procs_from_file(in_file, procs, &num_procs, &mem_capacity))
    {
        fprintf(stderr, "Failed to read processes from input file.\n");
        return EXIT_FAILURE;
    }

    printf("\n===== STATIC MEMORY ALLOCATION SIMULATION =====\n\n");
    printf("Input file: %s\n", in_file);
    printf("Memory size: %d KB\n", mem_capacity);
    printf("Number of processes: %d\n\n", num_procs);

    printf("-------------------------------------------------\n");
    printf("Processes Loaded:\n");
    printf("%-10s %-10s\n", "ProcessID", "Size (KB)");
    printf("-------------------------------------------------\n");
    for (int i = 0; i < num_procs; i++)
    {
        printf("%-10d %-10d\n", procs[i].id, procs[i].req_size);
    }
    printf("\n");

    Stats perf_stats[3] = {0};
    AllocMethod methods[3] = {FIRST_APPROACH, BEST_APPROACH, WORST_APPROACH};

    for (int i = 0; i < 3; i++)
    {
        MemMgr mgr;
        init_mem_mgr(&mgr, methods[i]);

        mgr.full_size = mem_capacity;
        mgr.avail_size = mem_capacity;
        mgr.segments[0].chunk_size = mem_capacity;

        Proc sim_procs[MAX_PROC];
        memcpy(sim_procs, procs, sizeof(Proc) * num_procs);

        run_sim(&mgr, methods[i], sim_procs, num_procs, &perf_stats[i]);
    }

    printf("\n=== Summary of Allocation Methods ===\n");
    printf("%-10s %-15s %-15s %-15s\n", "Strategy", "Success Rate", "Fragmentation", "Block Count");
    printf("----------------------------------------------------------\n");

    for (int i = 0; i < 3; i++)
    {
        char *method_name;
        switch (methods[i])
        {
        case FIRST_APPROACH:
            method_name = "First Fit";
            break;
        case BEST_APPROACH:
            method_name = "Best Fit";
            break;
        case WORST_APPROACH:
            method_name = "Worst Fit";
            break;
        default:
            method_name = "Unknown";
            break;
        }

        double success_rate =
            (perf_stats[i].alloc_tries > 0) ? ((double)perf_stats[i].alloc_success / perf_stats[i].alloc_tries * 100.0) : 0.0;

        char success_str[20], frag_str[20];
        sprintf(success_str, "%.1f%%", success_rate);
        sprintf(frag_str, "%.1f%%", perf_stats[i].frag_percent);

        printf("%-10s %-15s %-15s %-15d\n",
               method_name,
               success_str,
               frag_str,
               perf_stats[i].ext_frag);
    }
}

void init_mem_mgr(MemMgr *mgr, AllocMethod method)
{
    mgr->full_size = mem_capacity;
    mgr->avail_size = mgr->full_size;
    mgr->num_blocks = 1;
    mgr->method = method;

    mgr->segments[0].begin_addr = 0;
    mgr->segments[0].chunk_size = mgr->full_size;
    mgr->segments[0].available = true;
    mgr->segments[0].proc_id = -1;
}

bool allocate_mem(MemMgr *mgr, Proc *proc)
{
    if (proc->req_size > mgr->avail_size)
    {
        return false;
    }

    int block_idx = -1;

    switch (mgr->method)
    {
    case FIRST_APPROACH:
        block_idx = find_first_fit(mgr, proc->req_size);
        break;
    case BEST_APPROACH:
        block_idx = find_best_fit(mgr, proc->req_size);
        break;
    case WORST_APPROACH:
        block_idx = find_worst_fit(mgr, proc->req_size);
        break;
    }

    if (block_idx == -1)
    {
        return false;
    }

    if (mgr->segments[block_idx].chunk_size > proc->req_size + 10)
    {
        if (mgr->num_blocks >= MAX_MEM_BLKS)
        {
            return false;
        }

        for (int i = mgr->num_blocks; i > block_idx + 1; i--)
        {
            mgr->segments[i] = mgr->segments[i - 1];
        }

        mgr->segments[block_idx + 1].begin_addr =
            mgr->segments[block_idx].begin_addr + proc->req_size;
        mgr->segments[block_idx + 1].chunk_size =
            mgr->segments[block_idx].chunk_size - proc->req_size;
        mgr->segments[block_idx + 1].available = true;
        mgr->segments[block_idx + 1].proc_id = -1;

        mgr->segments[block_idx].chunk_size = proc->req_size;

        mgr->num_blocks++;
    }

    mgr->segments[block_idx].available = false;
    mgr->segments[block_idx].proc_id = proc->id;
    proc->block_idx = block_idx;
    proc->status = PROC_ACTIVE;
    mgr->avail_size -= proc->req_size;

    return true;
}

void free_mem(MemMgr *mgr, Proc *proc)
{
    if (proc->block_idx == -1)
    {
        return;
    }

    int idx = proc->block_idx;

    mgr->segments[idx].available = true;
    mgr->segments[idx].proc_id = -1;
    mgr->avail_size += mgr->segments[idx].chunk_size;

    proc->status = PROC_DONE;
    proc->block_idx = -1;

    bool merged;
    int merge_ops = 0;

    printf("\nCoalescing Process: Checking for adjacent free blocks after P%d termination\n", proc->id);

    do
    {
        merged = false;

        for (int i = 0; i < mgr->num_blocks - 1; i++)
        {
            if (mgr->segments[i].available && mgr->segments[i + 1].available)
            {
                printf("  Coalescing blocks at addresses %d and %d (sizes: %d KB + %d KB = %d KB)\n",
                       mgr->segments[i].begin_addr,
                       mgr->segments[i + 1].begin_addr,
                       mgr->segments[i].chunk_size,
                       mgr->segments[i + 1].chunk_size,
                       mgr->segments[i].chunk_size + mgr->segments[i + 1].chunk_size);

                mgr->segments[i].chunk_size += mgr->segments[i + 1].chunk_size;

                for (int j = i + 1; j < mgr->num_blocks - 1; j++)
                {
                    mgr->segments[j] = mgr->segments[j + 1];
                }

                for (int j = 0; j < MAX_PROC; j++)
                {
                    if (proc[j].block_idx > i + 1)
                    {
                        proc[j].block_idx--;
                    }
                }

                mgr->num_blocks--;
                merged = true;
                merge_ops++;
                break;
            }
        }
    } while (merged);

    if (merge_ops == 0)
    {
        printf("  No adjacent free blocks found for coalescing\n");
    }
    else
    {
        printf("  Completed %d coalescing operations\n", merge_ops);
    }
}

bool load_procs_from_file(const char *filename, Proc procs[], int *num_procs, int *mem_capacity)
{
    FILE *in_file = fopen(filename, "r");
    if (in_file == NULL)
    {
        fprintf(stderr, "Error: Could not open input file '%s'\n", filename);
        return false;
    }

    char line[MAX_LINE_LEN];
    int line_num = 0;

    if (fgets(line, MAX_LINE_LEN, in_file) != NULL)
    {
        line_num++;
        int mem_size;
        if (sscanf(line, "%d", &mem_size) == 1)
        {
            *mem_capacity = mem_size;
        }
    }

    *num_procs = 0;
    while (fgets(line, MAX_LINE_LEN, in_file) != NULL && *num_procs < MAX_PROC)
    {
        line_num++;

        if (line[0] == '\n' || line[0] == '#')
        {
            continue;
        }

        int id, size, arrival_time = 0, duration = 10;
        int fields = sscanf(line, "%d %d %d %d", &id, &size, &arrival_time, &duration);

        if (fields < 2)
        {
            fprintf(stderr, "Warning: Line %d in input file has invalid format, skipping\n", line_num);
            continue;
        }

        if (size <= 0)
        {
            fprintf(stderr, "Warning: Line %d in input file has invalid process size (%d), skipping\n", line_num, size);
            continue;
        }

        procs[*num_procs].id = id;
        procs[*num_procs].req_size = size;
        procs[*num_procs].status = PROC_NEW;
        procs[*num_procs].block_idx = -1;

        (*num_procs)++;
    }

    fclose(in_file);

    if (*num_procs == 0)
    {
        fprintf(stderr, "Warning: No valid processes found in input file\n");
        return false;
    }

    return true;
}

void print_mem_simple(MemMgr *mgr, Proc procs[], int num_procs)
{
    printf("\nMemory Summary: Used: %d KB (%.1f%%), Free: %d KB (%.1f%%)\n",
           mgr->full_size - mgr->avail_size,
           ((double)(mgr->full_size - mgr->avail_size) / mgr->full_size) * 100.0,
           mgr->avail_size,
           ((double)mgr->avail_size / mgr->full_size) * 100.0);

    int free_count = 0;
    for (int i = 0; i < mgr->num_blocks; i++)
    {
        if (mgr->segments[i].available)
            free_count++;
    }

    printf("Blocks: Total: %d, Free: %d\n", mgr->num_blocks, free_count);

    int running = 0, terminated = 0, new_count = 0;
    for (int i = 0; i < num_procs; i++)
    {
        if (procs[i].status == PROC_ACTIVE)
            running++;
        else if (procs[i].status == PROC_DONE)
            terminated++;
        else if (procs[i].status == PROC_NEW)
            new_count++;
    }

    printf("Processes: Running: %d, Terminated: %d, Unallocated: %d\n",
           running, terminated, new_count);
}

void print_mem_detailed(MemMgr *mgr, Proc procs[], int num_procs)
{
    printf("\nMemory Allocation Table:\n");
    printf("%-4s %-15s %-12s %-12s\n", "ID", "State", "Size", "Location");
    printf("------------------------------------------\n");

    for (int i = 0; i < num_procs; i++)
    {
        if (procs[i].status != PROC_NEW)
        {
            const char *state_str = (procs[i].status == PROC_ACTIVE) ? "Running" : "Terminated";

            printf("%-4d %-15s %-12d ",
                   procs[i].id,
                   state_str,
                   procs[i].req_size);

            if (procs[i].block_idx != -1)
            {
                printf("%-12d\n", mgr->segments[procs[i].block_idx].begin_addr);
            }
            else
            {
                printf("N/A\n");
            }
        }
    }

    printf("\nMemory Status:\n");
    printf("Total Memory: %d KB, Used: %d KB, Free: %d KB\n",
           mgr->full_size,
           mgr->full_size - mgr->avail_size,
           mgr->avail_size);

    printf("\nBlock List Details:\n");
    printf("%-8s %-8s %-16s %-8s\n", "Start", "Size", "Status", "Process");
    printf("------------------------------------------\n");

    for (int i = 0; i < mgr->num_blocks; i++)
    {
        printf("%-8d %-8d %-16s %-8d\n",
               mgr->segments[i].begin_addr,
               mgr->segments[i].chunk_size,
               mgr->segments[i].available ? "Free" : "Allocated",
               mgr->segments[i].proc_id);
    }

    printf("\n");
}

void update_frag_metrics(MemMgr *mgr, Proc procs[],
                               int num_procs, Stats *stats)
{
    stats->ext_frag = 0;
    stats->frag_percent = 0.0;
    stats->avg_frag_size = 0.0;

    int total_free_size = 0;
    int free_block_count = 0;

    for (int i = 0; i < mgr->num_blocks; i++)
    {
        if (mgr->segments[i].available)
        {
            stats->ext_frag++;
            total_free_size += mgr->segments[i].chunk_size;
            free_block_count++;
        }
    }

    if (free_block_count > 0)
    {
        stats->avg_frag_size = (double)total_free_size / free_block_count;
    }

    if (mgr->avail_size > 0)
    {
        if (free_block_count > 1)
        {
            int largest_free_block = 0;
            for (int i = 0; i < mgr->num_blocks; i++)
            {
                if (mgr->segments[i].available && mgr->segments[i].chunk_size > largest_free_block)
                {
                    largest_free_block = mgr->segments[i].chunk_size;
                }
            }

            stats->frag_percent =
                ((double)(mgr->avail_size - largest_free_block) / mgr->avail_size) * 100.0;
        }
    }
}

void run_sim(MemMgr *mgr, AllocMethod method,
                     Proc procs[], int num_procs, Stats *stats)
{
    memset(stats, 0, sizeof(Stats));

    printf("\n=== %s Strategy Simulation ===\n",
           method == FIRST_APPROACH ? "First-Fit" : (method == BEST_APPROACH ? "Best-Fit" : "Worst-Fit"));

    printf("\n--- Phase 1: Initial Process Allocation ---\n");
    int num_to_allocate;
    printf("How many processes do you want to allocate initially? (max %d): ", num_procs);
    scanf("%d", &num_to_allocate);

    if (num_to_allocate <= 0)
        num_to_allocate = 1;
    else if (num_to_allocate > num_procs)
        num_to_allocate = num_procs;

    double total_util = 0.0;
    int util_samples = 0;

    for (int i = 0; i < num_to_allocate; i++)
    {
        stats->alloc_tries++;

        if (allocate_mem(mgr, &procs[i]))
        {
            stats->alloc_success++;
            printf("P%d ", procs[i].id);
        }
        else
        {
            stats->alloc_fails++;
            printf("P%d(FAILED) ", procs[i].id);
        }
    }
    printf("\n");

    double current_util = (double)(mgr->full_size - mgr->avail_size) / mgr->full_size;
    total_util += current_util;
    util_samples++;
    stats->max_usage = current_util;

    print_mem_simple(mgr, procs, num_procs);

    printf("\n--- Phase 2: Process Termination ---\n");
    printf("Running processes: ");
    int running_count = 0;
    for (int i = 0; i < num_procs; i++)
    {
        if (procs[i].status == PROC_ACTIVE)
        {
            printf("P%d ", procs[i].id);
            running_count++;
        }
    }
    printf("\n");

    if (running_count > 0)
    {
        printf("Enter number of processes to terminate ([0] for none, [-1] for all, [1-%d] for specific processes): ", running_count);
        int num_to_terminate;
        scanf("%d", &num_to_terminate);

        if (num_to_terminate == -1)
        {
            printf("Terminating all running processes\n");
            for (int i = 0; i < num_procs; i++)
            {
                if (procs[i].status == PROC_ACTIVE)
                {
                    free_mem(mgr, &procs[i]);
                }
            }
        }
        else
        {
            if (num_to_terminate < 0)
                num_to_terminate = 0;
            if (num_to_terminate > running_count)
                num_to_terminate = running_count;

            for (int i = 0; i < num_to_terminate; i++)
            {
                int process_id;
                printf("Enter process ID to terminate: ");
                scanf("%d", &process_id);

                bool found = false;
                for (int j = 0; j < num_procs; j++)
                {
                    if (procs[j].id == process_id && procs[j].status == PROC_ACTIVE)
                    {
                        found = true;
                        free_mem(mgr, &procs[j]);
                        printf("Terminated P%d\n", process_id);
                        break;
                    }
                }

                if (!found)
                {
                    printf("P%d not found or not running\n", process_id);
                }
            }
        }
    }
    else
    {
        printf("No running processes to terminate.\n");
    }

    current_util = (double)(mgr->full_size - mgr->avail_size) / mgr->full_size;
    total_util += current_util;
    util_samples++;

    print_mem_simple(mgr, procs, num_procs);

    printf("\n--- Phase 3: Additional Process Allocation ---\n");
    printf("Remaining unallocated processes: ");
    int unalloc_count = 0;
    for (int i = 0; i < num_procs; i++)
    {
        if (procs[i].status == PROC_NEW)
        {
            printf("P%d ", procs[i].id);
            unalloc_count++;
        }
    }
    printf("\n");

    if (unalloc_count > 0)
    {
        printf("How many more processes do you want to allocate ([0] for none, [1-%d] for specific processes): ", unalloc_count);
        int more_to_allocate;
        scanf("%d", &more_to_allocate);

        if (more_to_allocate < 0)
            more_to_allocate = 0;
        if (more_to_allocate > unalloc_count)
            more_to_allocate = unalloc_count;

        int alloc_count = 0;

        for (int i = 0; i < num_procs && alloc_count < more_to_allocate; i++)
        {
            if (procs[i].status == PROC_NEW)
            {
                stats->alloc_tries++;

                if (allocate_mem(mgr, &procs[i]))
                {
                    stats->alloc_success++;
                    printf("P%d ", procs[i].id);
                }
                else
                {
                    stats->alloc_fails++;
                    printf("P%d(FAILED) ", procs[i].id);
                }

                alloc_count++;
            }
        }
        printf("\n");
    }
    else
    {
        printf("No more processes to allocate.\n");
    }

    current_util = (double)(mgr->full_size - mgr->avail_size) / mgr->full_size;
    total_util += current_util;
    util_samples++;
    if (current_util > stats->max_usage)
    {
        stats->max_usage = current_util;
    }

    print_mem_simple(mgr, procs, num_procs);

    printf("\n--- Phase 4: Large Process Allocation ---\n");
    float pct_input = 0.0f;
    do
    {
        printf("Enter size for a large process (P9999) allocation (as %% of available free memory, 1–100): ");
        if (scanf("%f", &pct_input) != 1)
        {
            printf("Invalid input. Please enter a number.\n");
            while (getchar() != '\n')
                ;
            pct_input = 0.0f;
        }
        else if (pct_input < 1.0f || pct_input > 100.0f)
        {
            printf("Please enter a valid percentage between 1 and 100.\n");
            pct_input = 0.0f;
        }
    } while (pct_input == 0.0f);

    int large_size = (int)((mgr->avail_size * pct_input) / 100.0f);

    Proc large_proc;
    large_proc.id = 9999;
    large_proc.req_size = large_size;
    large_proc.status = PROC_NEW;
    large_proc.block_idx = -1;

    stats->alloc_tries++;
    printf("Attempting large allocation (P9999, %dKB - %.2f%% of available free memory): ", large_proc.req_size, pct_input);

    if (allocate_mem(mgr, &large_proc))
    {
        stats->alloc_success++;
        printf("SUCCESS\n");
        procs[num_procs] = large_proc;
        num_procs++;
    }
    else
    {
        stats->alloc_fails++;
        printf("FAILED (not enough contiguous space)\n");
    }

    current_util = (double)(mgr->full_size - mgr->avail_size) / mgr->full_size;
    total_util += current_util;
    util_samples++;
    if (current_util > stats->max_usage)
    {
        stats->max_usage = current_util;
    }

    if (util_samples > 0)
    {
        stats->avg_usage = total_util / util_samples;
    }

    update_frag_metrics(mgr, procs, num_procs, stats);
    print_mem_simple(mgr, procs, num_procs);

    printf("\n--- Final Memory State (Detailed) ---\n");
    print_mem_detailed(mgr, procs, num_procs);

    printf("\n--- Final Results (%s) ---\n",
           method == FIRST_APPROACH ? "First-Fit" : (method == BEST_APPROACH ? "Best-Fit" : "Worst-Fit"));

    printf("Success Rate: %.1f%% (%d/%d)\n",
           ((double)stats->alloc_success / stats->alloc_tries) * 100.0,
           stats->alloc_success, stats->alloc_tries);
    printf("Peak Memory Usage: %.1f%%\n", stats->max_usage * 100.0);
    printf("Fragmentation: %.1f%%\n", stats->frag_percent);
    printf("Final Block Count: %d\n", mgr->num_blocks);

    printf("\n--- %s Simulation Completed ---\n",
           method == FIRST_APPROACH ? "First-Fit" : (method == BEST_APPROACH ? "Best-Fit" : "Worst-Fit"));
    printf("\n\n****************************************************************************************************************************\n\n");
}
