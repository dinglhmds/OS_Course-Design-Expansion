# Real-Time Scheduling Algorithms

## 1. RMS (Rate Monotonic Scheduling)

### 1.1 Basic Principle

RMS is a **static priority** scheduling algorithm that assigns higher priority to tasks with shorter periods.

> **Priority rule**: Shorter period → Higher priority

### 1.2 Schedulability Analysis

#### Liu & Layland Bound (Sufficient Condition)

For n independent implicit-deadline periodic tasks, if:

```
U = sum(C_i / T_i) <= n * (2^(1/n) - 1)
```

then the task set is RMS-schedulable.

- n = 1: U <= 1.0
- n = 2: U <= 0.828
- n = 3: U <= 0.780
- n -> infinity: U <= ln(2) ≈ 0.693

#### Hyperbolic Bound (Bini et al., tighter sufficient condition)

```
product(U_i + 1) <= 2
```

#### Response Time Analysis RTA (Exact Condition)

After sorting by priority, compute worst-case response time R_i for each task tau_i:

```
R_i^(0) = C_i
R_i^(k+1) = C_i + sum(ceil(R_i^(k) / T_j) * C_j)   (j < i)
```

Iterate until convergence or deadline violation.

### 1.3 Optimality

RMS is the **optimal fixed-priority scheduling algorithm**: for any task set schedulable by fixed priorities, RMS can schedule it.

---

## 2. EDF (Earliest Deadline First)

### 2.1 Basic Principle

EDF is a **dynamic priority** scheduling algorithm that always executes the task with the earliest absolute deadline.

> **Priority rule**: Earlier absolute deadline → Higher priority

Priority is recalculated at each period arrival.

### 2.2 Schedulability Analysis

#### Utilization Test (Implicit deadlines, necessary & sufficient)

```
U = sum(C_i / T_i) <= 1
```

#### Density Test (Constrained deadlines D <= T, sufficient)

```
delta = sum(C_i / D_i) <= 1
```

#### Processor Demand Criterion PDC (Constrained deadlines, necessary & sufficient)

For any time interval L:

```
h(0, L) = sum(max(0, floor((L - D_i) / T_i) + 1) * C_i) <= L
```

### 2.3 Optimality

EDF is the **optimal uniprocessor dynamic priority scheduling algorithm**. For any task set schedulable by dynamic priorities, EDF can schedule it.

### 2.4 Overload Behavior

When U > 1, EDF exhibits the **domino effect**: one missed deadline triggers a chain reaction, causing many tasks to miss their deadlines.

---

## 3. LLF (Least Laxity First)

### 3.1 Basic Principle

Laxity is defined as:

```
L_i(t) = D_i - t - C_i^rem
```

LLF selects the task with the smallest laxity.

### 3.2 Characteristics

- Optimal dynamic priority algorithm (same as EDF)
- May cause **churning**: frequent switches when two tasks have equal laxity
- Rarely used in practice but theoretically important

---

## 4. DMS (Deadline Monotonic Scheduling)

### 4.1 Basic Principle

DMS is a generalization of RMS for **constrained deadline** tasks (D <= T).

> **Priority rule**: Shorter relative deadline → Higher priority

When all D_i = T_i, DMS reduces to RMS.

### 4.2 Schedulability Analysis

Uses the same RTA as RMS, but sorts by deadline instead of period.

---

## 5. Algorithm Comparison Summary

| Feature | RMS | EDF | LLF | DMS |
|---------|-----|-----|-----|-----|
| Priority type | Static | Dynamic | Dynamic | Static |
| Preemptive | Yes | Yes | Yes | Yes |
| Optimality | Optimal (fixed-priority) | Optimal (dynamic) | Optimal (dynamic) | Optimal (fixed, D<=T) |
| Schedulability bound | n(2^(1/n)-1) | U<=1 (D=T) | U<=1 (D=T) | RTA exact |
| Implementation complexity | Low | Medium | Medium | Low |
| Context switches | Fewer | Medium | More (churning) | Fewer |
| Overload behavior | Graceful degradation | Domino effect | Domino effect | Graceful degradation |

---

## 6. Theory and Practice

### 6.1 Why do real-time systems need specialized scheduling?

General-purpose OS (e.g., Linux CFS) pursues fairness and throughput, while real-time systems pursue **predictability** and **deadline guarantee**.

### 6.2 RMS vs EDF Selection

- **Choose RMS/DMS**: Embedded systems, resource-constrained, simple implementation acceptable, utilization below 70%
- **Choose EDF**: High utilization needed (close to 100%), dynamic priority overhead acceptable

### 6.3 Applications in Real Systems

- **FreeRTOS**: Preemptive priority scheduling (similar to RMS)
- **VxWorks**: Supports RMS and EDF
- **Linux PREEMPT_RT**: Real-time extensions with priority scheduling
- **AUTOSAR OS**: Automotive standard based on fixed-priority scheduling
