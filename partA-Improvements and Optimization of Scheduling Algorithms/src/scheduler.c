#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PID 32
#define INF 0x3fffffff
#define RL_ACTIONS 4
#define RL_STATES 27

typedef enum {
    ALG_FCFS,
    ALG_SJF,
    ALG_SRTF,
    ALG_RR,
    ALG_PRIORITY,
    ALG_MLFQ,
    ALG_AAMLFQ,
    ALG_QLEARN
} Algorithm;

typedef enum {
    RL_ACT_SRTF,
    RL_ACT_PRIORITY,
    RL_ACT_MLFQ,
    RL_ACT_RR
} RlAction;

typedef enum {
    ST_NEW,
    ST_READY,
    ST_RUNNING,
    ST_BLOCKED,
    ST_DONE
} State;

typedef struct {
    char pid[MAX_PID];
    int arrival;
    int burst;
    int priority;
    int deadline;
    int io_interval;
    int io_duration;

    int remaining;
    int executed_since_io;
    State state;
    int ready_since;
    int blocked_until;
    int start_time;
    int completion_time;
    int waiting_time;
    int q_level;
    int quantum_left;
} Process;

typedef struct {
    char pid[MAX_PID];
    int start;
    int end;
} Segment;

typedef struct {
    Segment *items;
    int size;
    int cap;
} Trace;

typedef struct {
    Algorithm algorithm;
    const char *algorithm_name;
    const char *input_path;
    const char *workload_name;
    const char *trace_path;
    const char *process_path;
    int quantum;
    int aging;
    int boost;
    int levels;
    double rl_alpha;
    double rl_gamma;
    double rl_epsilon;
    bool csv;
} Config;

typedef struct {
    double q[RL_STATES][RL_ACTIONS];
    unsigned int rng;
    int action_counts[RL_ACTIONS];
} QLearner;

static void die(const char *msg) {
    fprintf(stderr, "error: %s\n", msg);
    exit(EXIT_FAILURE);
}

static void die_errno(const char *msg) {
    fprintf(stderr, "error: %s: %s\n", msg, strerror(errno));
    exit(EXIT_FAILURE);
}

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) {
        s++;
    }
    if (*s == '\0') {
        return s;
    }
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) {
        *end-- = '\0';
    }
    return s;
}

static bool has_alpha(const char *s) {
    while (*s) {
        if (isalpha((unsigned char)*s)) {
            return true;
        }
        s++;
    }
    return false;
}

static int parse_int_field(char *field, int fallback) {
    field = trim(field);
    if (*field == '\0') {
        return fallback;
    }
    char *end = NULL;
    long v = strtol(field, &end, 10);
    if (end == field || *trim(end) != '\0') {
        die("invalid integer field in workload");
    }
    if (v < 0 || v > INF) {
        die("integer field out of range");
    }
    return (int)v;
}

static const char *basename_of(const char *path) {
    const char *a = strrchr(path, '/');
    const char *b = strrchr(path, '\\');
    const char *p = a > b ? a : b;
    return p ? p + 1 : path;
}

static Algorithm parse_algorithm(const char *name) {
    if (strcmp(name, "fcfs") == 0) {
        return ALG_FCFS;
    }
    if (strcmp(name, "sjf") == 0) {
        return ALG_SJF;
    }
    if (strcmp(name, "srtf") == 0) {
        return ALG_SRTF;
    }
    if (strcmp(name, "rr") == 0) {
        return ALG_RR;
    }
    if (strcmp(name, "priority") == 0) {
        return ALG_PRIORITY;
    }
    if (strcmp(name, "mlfq") == 0) {
        return ALG_MLFQ;
    }
    if (strcmp(name, "aamlfq") == 0) {
        return ALG_AAMLFQ;
    }
    if (strcmp(name, "qlearn") == 0) {
        return ALG_QLEARN;
    }
    die("unknown algorithm; use fcfs, sjf, srtf, rr, priority, mlfq, aamlfq, or qlearn");
    return ALG_FCFS;
}

static const char *algorithm_name(Algorithm alg) {
    switch (alg) {
    case ALG_FCFS:
        return "fcfs";
    case ALG_SJF:
        return "sjf";
    case ALG_SRTF:
        return "srtf";
    case ALG_RR:
        return "rr";
    case ALG_PRIORITY:
        return "priority";
    case ALG_MLFQ:
        return "mlfq";
    case ALG_AAMLFQ:
        return "aamlfq";
    case ALG_QLEARN:
        return "qlearn";
    }
    return "unknown";
}

static bool uses_feedback_queues(Algorithm alg) {
    return alg == ALG_MLFQ || alg == ALG_AAMLFQ || alg == ALG_QLEARN;
}

static bool uses_quantum(Algorithm alg) {
    return alg == ALG_RR || alg == ALG_MLFQ || alg == ALG_AAMLFQ || alg == ALG_QLEARN;
}

static void trace_push(Trace *trace, const char *pid, int start, int end) {
    if (trace->size > 0) {
        Segment *last = &trace->items[trace->size - 1];
        if (strcmp(last->pid, pid) == 0 && last->end == start) {
            last->end = end;
            return;
        }
    }
    if (trace->size == trace->cap) {
        int next = trace->cap == 0 ? 64 : trace->cap * 2;
        Segment *items = realloc(trace->items, (size_t)next * sizeof(*items));
        if (!items) {
            die_errno("realloc trace");
        }
        trace->items = items;
        trace->cap = next;
    }
    Segment *seg = &trace->items[trace->size++];
    snprintf(seg->pid, sizeof(seg->pid), "%s", pid);
    seg->start = start;
    seg->end = end;
}

static int effective_priority(const Process *p, int now, const Config *cfg) {
    int wait_boost = cfg->aging > 0 ? (now - p->ready_since) / cfg->aging : 0;
    int value = p->priority - wait_boost;
    if (p->deadline > 0) {
        int slack = p->deadline - now - p->remaining;
        if (slack <= cfg->quantum) {
            value -= 1000;
        }
    }
    return value;
}

static int effective_level(const Process *p, int now, const Config *cfg) {
    if (cfg->algorithm == ALG_AAMLFQ && p->deadline > 0) {
        int slack = p->deadline - now - p->remaining;
        if (slack <= cfg->quantum * 2) {
            return 0;
        }
    }
    return p->q_level;
}

static int mlfq_quantum(const Config *cfg, int level) {
    int q = cfg->quantum;
    for (int i = 0; i < level; i++) {
        q *= 2;
    }
    return q;
}

static bool better_ready(const Process *p, const Process *best, int now, const Config *cfg) {
    if (!best) {
        return true;
    }
    switch (cfg->algorithm) {
    case ALG_FCFS:
        if (p->ready_since != best->ready_since) {
            return p->ready_since < best->ready_since;
        }
        return p->arrival < best->arrival;
    case ALG_SJF:
    case ALG_SRTF:
        if (p->remaining != best->remaining) {
            return p->remaining < best->remaining;
        }
        return p->ready_since < best->ready_since;
    case ALG_RR:
        return p->ready_since < best->ready_since;
    case ALG_PRIORITY: {
        int pa = effective_priority(p, now, cfg);
        int pb = effective_priority(best, now, cfg);
        if (pa != pb) {
            return pa < pb;
        }
        return p->ready_since < best->ready_since;
    }
    case ALG_QLEARN:
    case ALG_AAMLFQ:
    case ALG_MLFQ: {
        int la = effective_level(p, now, cfg);
        int lb = effective_level(best, now, cfg);
        if (la != lb) {
            return la < lb;
        }
        if (cfg->algorithm == ALG_AAMLFQ && (p->deadline > 0 || best->deadline > 0)) {
            int da = p->deadline > 0 ? p->deadline : INF;
            int db = best->deadline > 0 ? best->deadline : INF;
            if (da != db) {
                return da < db;
            }
        }
        return p->ready_since < best->ready_since;
    }
    }
    return false;
}

static int pick_ready(Process *procs, int n, int now, const Config *cfg) {
    int best = -1;
    for (int i = 0; i < n; i++) {
        if (procs[i].state != ST_READY) {
            continue;
        }
        if (better_ready(&procs[i], best >= 0 ? &procs[best] : NULL, now, cfg)) {
            best = i;
        }
    }
    return best;
}

static bool should_preempt(Process *procs, int n, int current, int now, const Config *cfg) {
    if (current < 0 || procs[current].state != ST_RUNNING) {
        return false;
    }
    int best = pick_ready(procs, n, now, cfg);
    if (best < 0) {
        if (uses_quantum(cfg->algorithm) && procs[current].quantum_left <= 0) {
            return true;
        }
        return false;
    }

    switch (cfg->algorithm) {
    case ALG_FCFS:
    case ALG_SJF:
        return false;
    case ALG_SRTF:
        return procs[best].remaining < procs[current].remaining;
    case ALG_RR:
        return procs[current].quantum_left <= 0;
    case ALG_PRIORITY:
        return effective_priority(&procs[best], now, cfg) <
               effective_priority(&procs[current], now, cfg);
    case ALG_QLEARN:
    case ALG_AAMLFQ:
    case ALG_MLFQ:
        if (procs[current].quantum_left <= 0) {
            return true;
        }
        return effective_level(&procs[best], now, cfg) <
               effective_level(&procs[current], now, cfg);
    }
    return false;
}

static Process *read_workload(const char *path, int *out_n) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        die_errno("open workload");
    }

    int cap = 16;
    int n = 0;
    Process *procs = calloc((size_t)cap, sizeof(*procs));
    if (!procs) {
        die_errno("calloc workload");
    }

    char line[512];
    int lineno = 0;
    while (fgets(line, sizeof(line), fp)) {
        lineno++;
        char *s = trim(line);
        if (*s == '\0' || *s == '#') {
            continue;
        }
        if (lineno == 1 && has_alpha(s) && strstr(s, "pid")) {
            continue;
        }
        if (n == cap) {
            cap *= 2;
            Process *next = realloc(procs, (size_t)cap * sizeof(*procs));
            if (!next) {
                die_errno("realloc workload");
            }
            procs = next;
        }
        Process p;
        memset(&p, 0, sizeof(p));
        p.priority = 5;
        p.deadline = 0;
        p.io_interval = 0;
        p.io_duration = 0;

        char *fields[7] = {0};
        int count = 0;
        char *tok = strtok(s, ",");
        while (tok && count < 7) {
            fields[count++] = tok;
            tok = strtok(NULL, ",");
        }
        if (count < 3) {
            fprintf(stderr, "error: %s:%d requires at least pid,arrival,burst\n", path, lineno);
            exit(EXIT_FAILURE);
        }
        snprintf(p.pid, sizeof(p.pid), "%s", trim(fields[0]));
        if (p.pid[0] == '\0') {
            die("empty pid in workload");
        }
        p.arrival = parse_int_field(fields[1], 0);
        p.burst = parse_int_field(fields[2], 0);
        p.priority = count > 3 ? parse_int_field(fields[3], 5) : 5;
        p.deadline = count > 4 ? parse_int_field(fields[4], 0) : 0;
        p.io_interval = count > 5 ? parse_int_field(fields[5], 0) : 0;
        p.io_duration = count > 6 ? parse_int_field(fields[6], 0) : 0;
        if (p.burst <= 0) {
            die("burst must be positive");
        }
        if (p.io_interval < 0 || p.io_duration < 0) {
            die("I/O fields must be non-negative");
        }
        p.remaining = p.burst;
        p.state = ST_NEW;
        p.ready_since = p.arrival;
        p.start_time = -1;
        p.completion_time = -1;
        procs[n++] = p;
    }
    fclose(fp);
    if (n == 0) {
        die("empty workload");
    }
    *out_n = n;
    return procs;
}

static void maybe_release(Process *procs, int n, int now, const Config *cfg) {
    for (int i = 0; i < n; i++) {
        Process *p = &procs[i];
        if (p->state == ST_NEW && p->arrival <= now) {
            p->state = ST_READY;
            p->ready_since = now;
            p->q_level = 0;
            p->quantum_left = uses_feedback_queues(cfg->algorithm) ? mlfq_quantum(cfg, 0) : cfg->quantum;
        } else if (p->state == ST_BLOCKED && p->blocked_until <= now) {
            p->state = ST_READY;
            p->ready_since = now;
            if (cfg->algorithm == ALG_AAMLFQ && p->q_level > 0) {
                p->q_level--;
            }
            p->quantum_left = uses_feedback_queues(cfg->algorithm) ? mlfq_quantum(cfg, p->q_level) : cfg->quantum;
        }
    }
}

static void apply_mlfq_aging(Process *procs, int n, int now, const Config *cfg) {
    if (cfg->algorithm != ALG_AAMLFQ) {
        return;
    }
    if (cfg->boost > 0 && now > 0 && now % cfg->boost == 0) {
        for (int i = 0; i < n; i++) {
            if (procs[i].state == ST_READY) {
                procs[i].q_level = 0;
                procs[i].quantum_left = mlfq_quantum(cfg, 0);
                procs[i].ready_since = now;
            }
        }
    }
    if (cfg->aging <= 0) {
        return;
    }
    for (int i = 0; i < n; i++) {
        Process *p = &procs[i];
        if (p->state == ST_READY && p->q_level > 0 && now - p->ready_since >= cfg->aging) {
            p->q_level--;
            p->quantum_left = mlfq_quantum(cfg, p->q_level);
            p->ready_since = now;
        }
    }
}

static void begin_running(Process *p, int now, const Config *cfg) {
    p->state = ST_RUNNING;
    if (p->start_time < 0) {
        p->start_time = now;
    }
    if (p->quantum_left <= 0 || cfg->algorithm == ALG_FCFS || cfg->algorithm == ALG_SJF ||
        cfg->algorithm == ALG_SRTF || cfg->algorithm == ALG_PRIORITY) {
        p->quantum_left = uses_feedback_queues(cfg->algorithm) ? mlfq_quantum(cfg, p->q_level) : cfg->quantum;
    }
}

static void preempt_current(Process *p, int now, const Config *cfg) {
    if (uses_feedback_queues(cfg->algorithm) && p->quantum_left <= 0 && p->q_level < cfg->levels - 1) {
        p->q_level++;
    }
    p->state = ST_READY;
    p->ready_since = now;
    p->quantum_left = uses_feedback_queues(cfg->algorithm) ? mlfq_quantum(cfg, p->q_level) : cfg->quantum;
}

static void write_trace(const char *path, const Trace *trace) {
    if (!path) {
        return;
    }
    FILE *fp = fopen(path, "w");
    if (!fp) {
        die_errno("write trace");
    }
    fprintf(fp, "pid,start,end,duration\n");
    for (int i = 0; i < trace->size; i++) {
        const Segment *seg = &trace->items[i];
        fprintf(fp, "%s,%d,%d,%d\n", seg->pid, seg->start, seg->end, seg->end - seg->start);
    }
    fclose(fp);
}

static void write_processes(const char *path, const Process *procs, int n) {
    if (!path) {
        return;
    }
    FILE *fp = fopen(path, "w");
    if (!fp) {
        die_errno("write process csv");
    }
    fprintf(fp, "pid,arrival,burst,priority,deadline,completion,turnaround,waiting,response,slowdown,deadline_missed\n");
    for (int i = 0; i < n; i++) {
        const Process *p = &procs[i];
        int turnaround = p->completion_time - p->arrival;
        int response = p->start_time - p->arrival;
        double slowdown = (double)turnaround / (double)p->burst;
        int missed = p->deadline > 0 && p->completion_time > p->deadline;
        fprintf(fp, "%s,%d,%d,%d,%d,%d,%d,%d,%d,%.6f,%d\n",
                p->pid, p->arrival, p->burst, p->priority, p->deadline,
                p->completion_time, turnaround, p->waiting_time, response, slowdown, missed);
    }
    fclose(fp);
}


static Algorithm qlearn_action_algorithm(RlAction action) {
    switch (action) {
    case RL_ACT_SRTF:
        return ALG_SRTF;
    case RL_ACT_PRIORITY:
        return ALG_PRIORITY;
    case RL_ACT_MLFQ:
        return ALG_MLFQ;
    case RL_ACT_RR:
        return ALG_RR;
    }
    return ALG_MLFQ;
}

static int pick_ready_with_algorithm(Process *procs, int n, int now, const Config *cfg, Algorithm alg) {
    Config local = *cfg;
    local.algorithm = alg;
    return pick_ready(procs, n, now, &local);
}

static bool should_preempt_with_algorithm(Process *procs, int n, int current, int now,
                                          const Config *cfg, Algorithm alg) {
    Config local = *cfg;
    local.algorithm = alg;
    return should_preempt(procs, n, current, now, &local);
}

static unsigned int qlearn_rand(QLearner *rl) {
    rl->rng = rl->rng * 1103515245u + 12345u;
    return rl->rng;
}

static int qlearn_ready_count(Process *procs, int n) {
    int count = 0;
    for (int i = 0; i < n; i++) {
        if (procs[i].state == ST_READY) {
            count++;
        }
    }
    return count;
}

static int qlearn_max_wait(Process *procs, int n, int now) {
    int max_wait = 0;
    for (int i = 0; i < n; i++) {
        if (procs[i].state == ST_READY) {
            int wait = now - procs[i].ready_since;
            if (wait > max_wait) {
                max_wait = wait;
            }
        }
    }
    return max_wait;
}

static int qlearn_min_deadline_slack(Process *procs, int n, int now) {
    int best = INF;
    for (int i = 0; i < n; i++) {
        if (procs[i].state == ST_READY && procs[i].deadline > 0) {
            int slack = procs[i].deadline - now - procs[i].remaining;
            if (slack < best) {
                best = slack;
            }
        }
    }
    return best;
}

static int qlearn_state(Process *procs, int n, int now, const Config *cfg) {
    int ready = qlearn_ready_count(procs, n);
    int wait = qlearn_max_wait(procs, n, now);
    int slack = qlearn_min_deadline_slack(procs, n, now);

    int ready_bucket = ready <= 1 ? 0 : (ready <= 3 ? 1 : 2);
    int wait_bucket = wait < cfg->aging / 2 + 1 ? 0 : (wait < cfg->aging * 2 + 1 ? 1 : 2);
    int deadline_bucket = 0;
    if (slack != INF) {
        deadline_bucket = slack <= cfg->quantum * 2 ? 2 : 1;
    }
    return ready_bucket * 9 + wait_bucket * 3 + deadline_bucket;
}

static void qlearn_init(QLearner *rl) {
    memset(rl, 0, sizeof(*rl));
    rl->rng = 0xC0FFEEu;
    for (int s = 0; s < RL_STATES; s++) {
        int ready_bucket = s / 9;
        int wait_bucket = (s / 3) % 3;
        int deadline_bucket = s % 3;
        rl->q[s][RL_ACT_SRTF] = 0.20;
        rl->q[s][RL_ACT_PRIORITY] = 0.15;
        rl->q[s][RL_ACT_MLFQ] = 0.25;
        rl->q[s][RL_ACT_RR] = 0.05;
        if (ready_bucket == 0) {
            rl->q[s][RL_ACT_SRTF] += 0.25;
        }
        if (ready_bucket == 2) {
            rl->q[s][RL_ACT_MLFQ] += 0.25;
            rl->q[s][RL_ACT_RR] += 0.10;
        }
        if (wait_bucket == 2) {
            rl->q[s][RL_ACT_MLFQ] += 0.35;
            rl->q[s][RL_ACT_RR] += 0.15;
        }
        if (deadline_bucket == 2) {
            rl->q[s][RL_ACT_PRIORITY] += 0.45;
            rl->q[s][RL_ACT_SRTF] += 0.15;
        }
    }
}

static int qlearn_select_action(QLearner *rl, int state, const Config *cfg) {
    double r = (double)(qlearn_rand(rl) % 10000u) / 10000.0;
    if (r < cfg->rl_epsilon) {
        int action = (int)(qlearn_rand(rl) % RL_ACTIONS);
        rl->action_counts[action]++;
        return action;
    }
    int best = 0;
    for (int a = 1; a < RL_ACTIONS; a++) {
        if (rl->q[state][a] > rl->q[state][best]) {
            best = a;
        }
    }
    rl->action_counts[best]++;
    return best;
}

static double qlearn_max_next(const QLearner *rl, int state) {
    double best = rl->q[state][0];
    for (int a = 1; a < RL_ACTIONS; a++) {
        if (rl->q[state][a] > best) {
            best = rl->q[state][a];
        }
    }
    return best;
}

static double qlearn_reward(Process *procs, int n, int now, const Config *cfg,
                            int finished_now, int missed_now, bool switched) {
    int ready = qlearn_ready_count(procs, n);
    int max_wait = qlearn_max_wait(procs, n, now);
    int slack = qlearn_min_deadline_slack(procs, n, now);
    double reward = -0.035 * ready - 0.006 * max_wait;
    if (slack != INF && slack <= cfg->quantum * 2) {
        reward -= 0.20;
    }
    if (switched) {
        reward -= 0.03;
    }
    reward += 1.20 * finished_now;
    reward -= 1.80 * missed_now;
    return reward;
}

static void qlearn_update(QLearner *rl, int state, int action, double reward,
                          int next_state, const Config *cfg) {
    double old = rl->q[state][action];
    double target = reward + cfg->rl_gamma * qlearn_max_next(rl, next_state);
    rl->q[state][action] = old + cfg->rl_alpha * (target - old);
}


static void simulate(Process *procs, int n, const Config *cfg) {
    int now = 0;
    int completed = 0;
    int current = -1;
    int last_cpu = -2;
    int context_switches = 0;
    int busy_ticks = 0;
    Trace trace = {0};
    QLearner rl;
    if (cfg->algorithm == ALG_QLEARN) {
        qlearn_init(&rl);
    }

    while (completed < n) {
        maybe_release(procs, n, now, cfg);
        apply_mlfq_aging(procs, n, now, cfg);

        int rl_state = -1;
        int rl_action = RL_ACT_MLFQ;
        Algorithm decision_alg = cfg->algorithm;
        if (cfg->algorithm == ALG_QLEARN) {
            rl_state = qlearn_state(procs, n, now, cfg);
            rl_action = qlearn_select_action(&rl, rl_state, cfg);
            decision_alg = qlearn_action_algorithm((RlAction)rl_action);
        }

        if (cfg->algorithm == ALG_QLEARN) {
            if (current >= 0 && procs[current].quantum_left <= 0) {
                preempt_current(&procs[current], now, cfg);
                current = -1;
            } else if (should_preempt_with_algorithm(procs, n, current, now, cfg, decision_alg)) {
                preempt_current(&procs[current], now, cfg);
                current = -1;
            }
        } else if (should_preempt(procs, n, current, now, cfg)) {
            preempt_current(&procs[current], now, cfg);
            current = -1;
        }

        if (current < 0) {
            current = cfg->algorithm == ALG_QLEARN
                          ? pick_ready_with_algorithm(procs, n, now, cfg, decision_alg)
                          : pick_ready(procs, n, now, cfg);
            if (current >= 0) {
                begin_running(&procs[current], now, cfg);
            }
        }

        if (current >= 0) {
            bool switched_now = last_cpu != current;
            if (switched_now) {
                if (last_cpu != -2) {
                    context_switches++;
                }
                last_cpu = current;
            }
            trace_push(&trace, procs[current].pid, now, now + 1);
            for (int i = 0; i < n; i++) {
                if (procs[i].state == ST_READY) {
                    procs[i].waiting_time++;
                }
            }
            Process *p = &procs[current];
            p->remaining--;
            p->executed_since_io++;
            p->quantum_left--;
            busy_ticks++;
            now++;

            int finished_now = 0;
            int missed_now = 0;
            if (p->remaining == 0) {
                p->state = ST_DONE;
                p->completion_time = now;
                finished_now = 1;
                missed_now = p->deadline > 0 && p->completion_time > p->deadline;
                completed++;
                current = -1;
            } else if (p->io_interval > 0 && p->executed_since_io >= p->io_interval) {
                p->state = ST_BLOCKED;
                p->blocked_until = now + p->io_duration;
                p->executed_since_io = 0;
                current = -1;
            }
            if (cfg->algorithm == ALG_QLEARN && rl_state >= 0) {
                int next_state = qlearn_state(procs, n, now, cfg);
                double reward = qlearn_reward(procs, n, now, cfg, finished_now, missed_now, switched_now);
                qlearn_update(&rl, rl_state, rl_action, reward, next_state, cfg);
            }
        } else {
            trace_push(&trace, "IDLE", now, now + 1);
            if (last_cpu != -1) {
                if (last_cpu != -2) {
                    context_switches++;
                }
                last_cpu = -1;
            }
            now++;
        }
    }

    double sum_turnaround = 0.0;
    double sum_waiting = 0.0;
    double sum_response = 0.0;
    double sum_slowdown = 0.0;
    double sum_service_ratio = 0.0;
    double sum_service_ratio_sq = 0.0;
    int deadline_misses = 0;
    for (int i = 0; i < n; i++) {
        int turnaround = procs[i].completion_time - procs[i].arrival;
        int response = procs[i].start_time - procs[i].arrival;
        double slowdown = (double)turnaround / (double)procs[i].burst;
        double service_ratio = (double)procs[i].burst / (double)turnaround;
        sum_turnaround += turnaround;
        sum_waiting += procs[i].waiting_time;
        sum_response += response;
        sum_slowdown += slowdown;
        sum_service_ratio += service_ratio;
        sum_service_ratio_sq += service_ratio * service_ratio;
        if (procs[i].deadline > 0 && procs[i].completion_time > procs[i].deadline) {
            deadline_misses++;
        }
    }

    double avg_turnaround = sum_turnaround / n;
    double avg_waiting = sum_waiting / n;
    double avg_response = sum_response / n;
    double avg_slowdown = sum_slowdown / n;
    double throughput = (double)n / (double)now;
    double cpu_util = (double)busy_ticks / (double)now;
    double fairness = (sum_service_ratio * sum_service_ratio) / ((double)n * sum_service_ratio_sq);

    if (cfg->csv) {
        printf("%s,%s,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%d,%.6f,%.6f\n",
               cfg->algorithm_name, cfg->workload_name, n, now, avg_turnaround,
               avg_waiting, avg_response, avg_slowdown, throughput, context_switches,
               deadline_misses, cpu_util, fairness);
    } else {
        printf("algorithm: %s\n", cfg->algorithm_name);
        printf("workload: %s\n", cfg->workload_name);
        printf("processes: %d\n", n);
        printf("makespan: %d\n", now);
        printf("avg_turnaround: %.3f\n", avg_turnaround);
        printf("avg_waiting: %.3f\n", avg_waiting);
        printf("avg_response: %.3f\n", avg_response);
        printf("avg_slowdown: %.3f\n", avg_slowdown);
        printf("throughput: %.6f\n", throughput);
        printf("context_switches: %d\n", context_switches);
        printf("deadline_misses: %d\n", deadline_misses);
        printf("cpu_utilization: %.6f\n", cpu_util);
        printf("fairness: %.6f\n", fairness);
    }

    write_trace(cfg->trace_path, &trace);
    write_processes(cfg->process_path, procs, n);
    free(trace.items);
}

static void usage(FILE *out) {
    fprintf(out,
            "Usage: scheduler --algo NAME --input FILE [options]\n"
            "\n"
            "Algorithms: fcfs, sjf, srtf, rr, priority, mlfq, aamlfq, qlearn\n"
            "Workload CSV: pid,arrival,burst,priority,deadline,io_interval,io_duration\n"
            "\n"
            "Options:\n"
            "  --quantum N        Base time quantum for RR/MLFQ (default: 4)\n"
            "  --aging N          Aging threshold for priority, AAMLFQ, and Q-learning buckets (default: 20)\n"
            "  --boost N          AAMLFQ global boost period; classic MLFQ ignores it (default: 80)\n"
            "  --levels N         MLFQ/AAMLFQ/Q-learning queue levels (default: 4)\n"
            "  --rl-alpha X       Q-learning learning rate (default: 0.25)\n"
            "  --rl-gamma X       Q-learning discount factor (default: 0.80)\n"
            "  --rl-epsilon X     Q-learning exploration rate (default: 0.08)\n"
            "  --workload NAME    Name printed in summary CSV\n"
            "  --trace FILE       Write Gantt-like execution segments\n"
            "  --process-csv FILE Write per-process metrics\n"
            "  --csv              Print one summary CSV row\n"
            "  --help             Show this message\n");
}

int main(int argc, char **argv) {
    Config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.algorithm = ALG_MLFQ;
    cfg.algorithm_name = "mlfq";
    cfg.quantum = 4;
    cfg.aging = 20;
    cfg.boost = 80;
    cfg.levels = 4;
    cfg.rl_alpha = 0.25;
    cfg.rl_gamma = 0.80;
    cfg.rl_epsilon = 0.08;

    enum {
        OPT_ALGO = 1000,
        OPT_INPUT,
        OPT_QUANTUM,
        OPT_AGING,
        OPT_BOOST,
        OPT_LEVELS,
        OPT_RL_ALPHA,
        OPT_RL_GAMMA,
        OPT_RL_EPSILON,
        OPT_WORKLOAD,
        OPT_TRACE,
        OPT_PROCESS,
        OPT_CSV,
        OPT_HELP
    };

    static const struct option opts[] = {
        {"algo", required_argument, NULL, OPT_ALGO},
        {"input", required_argument, NULL, OPT_INPUT},
        {"quantum", required_argument, NULL, OPT_QUANTUM},
        {"aging", required_argument, NULL, OPT_AGING},
        {"boost", required_argument, NULL, OPT_BOOST},
        {"levels", required_argument, NULL, OPT_LEVELS},
        {"rl-alpha", required_argument, NULL, OPT_RL_ALPHA},
        {"rl-gamma", required_argument, NULL, OPT_RL_GAMMA},
        {"rl-epsilon", required_argument, NULL, OPT_RL_EPSILON},
        {"workload", required_argument, NULL, OPT_WORKLOAD},
        {"trace", required_argument, NULL, OPT_TRACE},
        {"process-csv", required_argument, NULL, OPT_PROCESS},
        {"csv", no_argument, NULL, OPT_CSV},
        {"help", no_argument, NULL, OPT_HELP},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "", opts, NULL)) != -1) {
        switch (opt) {
        case OPT_ALGO:
            cfg.algorithm = parse_algorithm(optarg);
            cfg.algorithm_name = algorithm_name(cfg.algorithm);
            break;
        case OPT_INPUT:
            cfg.input_path = optarg;
            break;
        case OPT_QUANTUM:
            cfg.quantum = atoi(optarg);
            break;
        case OPT_AGING:
            cfg.aging = atoi(optarg);
            break;
        case OPT_BOOST:
            cfg.boost = atoi(optarg);
            break;
        case OPT_LEVELS:
            cfg.levels = atoi(optarg);
            break;
        case OPT_RL_ALPHA:
            cfg.rl_alpha = atof(optarg);
            break;
        case OPT_RL_GAMMA:
            cfg.rl_gamma = atof(optarg);
            break;
        case OPT_RL_EPSILON:
            cfg.rl_epsilon = atof(optarg);
            break;
        case OPT_WORKLOAD:
            cfg.workload_name = optarg;
            break;
        case OPT_TRACE:
            cfg.trace_path = optarg;
            break;
        case OPT_PROCESS:
            cfg.process_path = optarg;
            break;
        case OPT_CSV:
            cfg.csv = true;
            break;
        case OPT_HELP:
            usage(stdout);
            return EXIT_SUCCESS;
        default:
            usage(stderr);
            return EXIT_FAILURE;
        }
    }

    if (!cfg.input_path) {
        usage(stderr);
        die("--input is required");
    }
    if (cfg.quantum <= 0 || cfg.aging < 0 || cfg.boost < 0 || cfg.levels <= 0 || cfg.levels > 16 ||
        cfg.rl_alpha <= 0.0 || cfg.rl_alpha > 1.0 || cfg.rl_gamma < 0.0 || cfg.rl_gamma > 1.0 ||
        cfg.rl_epsilon < 0.0 || cfg.rl_epsilon > 1.0) {
        die("invalid scheduler parameters");
    }
    if (!cfg.workload_name) {
        cfg.workload_name = basename_of(cfg.input_path);
    }

    int n = 0;
    Process *procs = read_workload(cfg.input_path, &n);
    simulate(procs, n, &cfg);
    free(procs);
    return EXIT_SUCCESS;
}
