#define _GNU_SOURCE

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum {
    MODE_RACE,
    MODE_MUTEX,
    MODE_SPIN,
    MODE_ATOMIC,
    MODE_LOCAL
} bench_mode_t;

typedef struct {
    bench_mode_t mode;
    int thread_id;
    long long iters;
    long long local_count;
} thread_arg_t;

static volatile long long race_counter = 0;
static long long shared_counter = 0;
static _Atomic long long atomic_counter = 0;
static pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_spinlock_t counter_spin;

static const char *mode_to_string(bench_mode_t mode) {
    switch (mode) {
    case MODE_RACE:
        return "race";
    case MODE_MUTEX:
        return "mutex";
    case MODE_SPIN:
        return "spin";
    case MODE_ATOMIC:
        return "atomic";
    case MODE_LOCAL:
        return "local";
    default:
        return "unknown";
    }
}

static int parse_mode(const char *text, bench_mode_t *mode) {
    if (strcmp(text, "race") == 0) {
        *mode = MODE_RACE;
    } else if (strcmp(text, "mutex") == 0) {
        *mode = MODE_MUTEX;
    } else if (strcmp(text, "spin") == 0) {
        *mode = MODE_SPIN;
    } else if (strcmp(text, "atomic") == 0) {
        *mode = MODE_ATOMIC;
    } else if (strcmp(text, "local") == 0) {
        *mode = MODE_LOCAL;
    } else {
        return 0;
    }
    return 1;
}

static long long parse_positive_ll(const char *text, const char *name) {
    char *end = NULL;
    errno = 0;
    long long value = strtoll(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value <= 0) {
        fprintf(stderr, "invalid %s: %s\n", name, text);
        exit(EXIT_FAILURE);
    }
    return value;
}

static double elapsed_seconds(struct timespec start, struct timespec end) {
    time_t sec = end.tv_sec - start.tv_sec;
    long nsec = end.tv_nsec - start.tv_nsec;
    if (nsec < 0) {
        sec -= 1;
        nsec += 1000000000L;
    }
    return (double)sec + (double)nsec / 1000000000.0;
}

static void *worker(void *data) {
    thread_arg_t *arg = (thread_arg_t *)data;

    switch (arg->mode) {
    case MODE_RACE:
        for (long long i = 0; i < arg->iters; ++i) {
            race_counter++;
        }
        break;
    case MODE_MUTEX:
        for (long long i = 0; i < arg->iters; ++i) {
            pthread_mutex_lock(&counter_mutex);
            shared_counter++;
            pthread_mutex_unlock(&counter_mutex);
        }
        break;
    case MODE_SPIN:
        for (long long i = 0; i < arg->iters; ++i) {
            pthread_spin_lock(&counter_spin);
            shared_counter++;
            pthread_spin_unlock(&counter_spin);
        }
        break;
    case MODE_ATOMIC:
        for (long long i = 0; i < arg->iters; ++i) {
            atomic_fetch_add_explicit(&atomic_counter, 1, memory_order_relaxed);
        }
        break;
    case MODE_LOCAL: {
        volatile long long local = 0;
        for (long long i = 0; i < arg->iters; ++i) {
            local++;
        }
        arg->local_count = (long long)local;
        break;
    }
    default:
        break;
    }

    (void)arg->thread_id;
    return NULL;
}

static void usage(const char *program) {
    fprintf(stderr,
            "usage: %s --mode race|mutex|spin|atomic|local "
            "--threads N --iters TOTAL_ITERS --repeat R\n",
            program);
}

int main(int argc, char **argv) {
    bench_mode_t mode = MODE_MUTEX;
    int threads = 1;
    long long total_iters = 10000000LL;
    int repeat = 1;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            if (!parse_mode(argv[++i], &mode)) {
                fprintf(stderr, "invalid mode: %s\n", argv[i]);
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            long long value = parse_positive_ll(argv[++i], "threads");
            if (value > 1024) {
                fprintf(stderr, "threads is too large: %lld\n", value);
                return EXIT_FAILURE;
            }
            threads = (int)value;
        } else if (strcmp(argv[i], "--iters") == 0 && i + 1 < argc) {
            total_iters = parse_positive_ll(argv[++i], "iters");
        } else if (strcmp(argv[i], "--repeat") == 0 && i + 1 < argc) {
            long long value = parse_positive_ll(argv[++i], "repeat");
            if (value > 1000000) {
                fprintf(stderr, "repeat is too large: %lld\n", value);
                return EXIT_FAILURE;
            }
            repeat = (int)value;
        } else {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    pthread_t *tids = calloc((size_t)threads, sizeof(*tids));
    thread_arg_t *args = calloc((size_t)threads, sizeof(*args));
    if (tids == NULL || args == NULL) {
        perror("calloc");
        free(tids);
        free(args);
        return EXIT_FAILURE;
    }

    if (mode == MODE_SPIN && pthread_spin_init(&counter_spin, PTHREAD_PROCESS_PRIVATE) != 0) {
        perror("pthread_spin_init");
        free(tids);
        free(args);
        return EXIT_FAILURE;
    }

    for (int r = 1; r <= repeat; ++r) {
        race_counter = 0;
        shared_counter = 0;
        atomic_store_explicit(&atomic_counter, 0, memory_order_relaxed);
        memset(args, 0, (size_t)threads * sizeof(*args));

        long long base = total_iters / threads;
        long long remainder = total_iters % threads;
        for (int t = 0; t < threads; ++t) {
            args[t].mode = mode;
            args[t].thread_id = t;
            args[t].iters = base + (t < remainder ? 1 : 0);
        }

        struct timespec start;
        struct timespec end;
        if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
            perror("clock_gettime");
            return EXIT_FAILURE;
        }

        for (int t = 0; t < threads; ++t) {
            int rc = pthread_create(&tids[t], NULL, worker, &args[t]);
            if (rc != 0) {
                fprintf(stderr, "pthread_create: %s\n", strerror(rc));
                return EXIT_FAILURE;
            }
        }
        for (int t = 0; t < threads; ++t) {
            int rc = pthread_join(tids[t], NULL);
            if (rc != 0) {
                fprintf(stderr, "pthread_join: %s\n", strerror(rc));
                return EXIT_FAILURE;
            }
        }

        if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
            perror("clock_gettime");
            return EXIT_FAILURE;
        }

        long long final_counter = 0;
        if (mode == MODE_RACE) {
            final_counter = race_counter;
        } else if (mode == MODE_ATOMIC) {
            final_counter = atomic_load_explicit(&atomic_counter, memory_order_relaxed);
        } else if (mode == MODE_LOCAL) {
            for (int t = 0; t < threads; ++t) {
                final_counter += args[t].local_count;
            }
        } else {
            final_counter = shared_counter;
        }

        double elapsed = elapsed_seconds(start, end);
        double ns_per_op = elapsed * 1000000000.0 / (double)total_iters;
        double throughput = (double)total_iters / elapsed;
        int correct = (final_counter == total_iters);

        printf("%s,%d,%lld,%d,%.9f,%.3f,%.3f,%lld,%d\n",
               mode_to_string(mode), threads, total_iters, r, elapsed,
               ns_per_op, throughput, final_counter, correct);
    }

    if (mode == MODE_SPIN) {
        pthread_spin_destroy(&counter_spin);
    }
    free(tids);
    free(args);
    return EXIT_SUCCESS;
}
