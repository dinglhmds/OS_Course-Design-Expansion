#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
实时调度实验结果可视化脚本

自动生成甘特图、对比柱状图、折线图等多种图表，
直观展示各调度算法的性能差异。

依赖: matplotlib, numpy
安装: pip3 install matplotlib numpy
"""

import sys
import os
import csv
import glob

try:
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    import numpy as np
except ImportError:
    print("错误: 缺少依赖库。请运行: pip3 install matplotlib numpy")
    sys.exit(1)

RESULTS_DIR = sys.argv[1] if len(sys.argv) > 1 else "results"
os.makedirs(RESULTS_DIR, exist_ok=True)

plt.rcParams['font.sans-serif'] = ['DejaVu Sans', 'SimHei', 'Arial Unicode MS']
plt.rcParams['axes.unicode_minus'] = False

COLORS = {
    'RMS': '#e74c3c',
    'EDF': '#3498db',
    'LLF': '#2ecc71',
    'DMS': '#9b59b6',
    'FCFS': '#95a5a6',
    'SJF': '#f39c12'
}


def plot_gantt(csv_file, output_file):
    """绘制甘特图"""
    events = []
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            events.append({
                'task': row['task_name'],
                'start': float(row['start']),
                'end': float(row['end']),
                'state': int(row['state'])
            })
    
    if not events:
        return
    
    tasks = sorted(set(e['task'] for e in events if e['task'] != 'IDLE'))
    task_map = {t: i for i, t in enumerate(tasks)}
    
    fig, ax = plt.subplots(figsize=(14, 4))
    
    for e in events:
        if e['task'] == 'IDLE':
            continue
        idx = task_map.get(e['task'], -1)
        if idx < 0:
            continue
        color = plt.cm.Set3(idx % 12)
        ax.barh(idx, e['end'] - e['start'], left=e['start'], height=0.6, color=color, edgecolor='black', linewidth=0.3)
    
    ax.set_yticks(range(len(tasks)))
    ax.set_yticklabels(tasks)
    ax.set_xlabel('Time')
    ax.set_ylabel('Task')
    ax.set_title(f'Gantt Chart ({os.path.basename(csv_file).replace("_gantt.csv", "")})')
    ax.grid(axis='x', alpha=0.3)
    plt.tight_layout()
    plt.savefig(output_file, dpi=150)
    plt.close()
    print(f"  [PLOT] {output_file}")


def plot_comparison(csv_file, output_file):
    """绘制算法对比柱状图"""
    data = []
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            data.append(row)
    
    if not data:
        return
    
    schedulers = [d['scheduler'] for d in data]
    metrics = ['miss_ratio', 'preemptions', 'context_switches', 'cpu_util', 'avg_response']
    titles = ['Deadline Miss Ratio', 'Preemptions', 'Context Switches', 'CPU Utilization', 'Avg Response Time']
    
    fig, axes = plt.subplots(1, 5, figsize=(20, 4))
    
    for idx, (metric, title) in enumerate(zip(metrics, titles)):
        ax = axes[idx]
        values = [float(d[metric]) for d in data]
        colors = [COLORS.get(s, '#333333') for s in schedulers]
        bars = ax.bar(range(len(schedulers)), values, color=colors, edgecolor='black', linewidth=0.5)
        ax.set_xticks(range(len(schedulers)))
        ax.set_xticklabels(schedulers, rotation=45, ha='right')
        ax.set_title(title, fontsize=10)
        ax.grid(axis='y', alpha=0.3)
        
        # 标注数值
        for bar, val in zip(bars, values):
            if val > 0:
                ax.text(bar.get_x() + bar.get_width()/2., bar.get_height(),
                        f'{val:.3f}', ha='center', va='bottom', fontsize=7)
    
    plt.suptitle('Algorithm Comparison', fontsize=14, fontweight='bold')
    plt.tight_layout()
    plt.savefig(output_file, dpi=150)
    plt.close()
    print(f"  [PLOT] {output_file}")


def plot_schedulability(csv_file, output_file):
    """绘制可调度率曲线"""
    data = []
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in data:
            data.append(row)
    
    if not data:
        return
    
    fig, ax = plt.subplots(figsize=(10, 6))
    
    utilizations = []
    tests = ['rms_ll', 'rms_rta', 'edf_util', 'dms_rta']
    labels = ['RMS (LL bound)', 'RMS (RTA)', 'EDF (Util)', 'DMS (RTA)']
    colors_list = ['#e74c3c', '#c0392b', '#3498db', '#9b59b6']
    
    for test, label, color in zip(tests, labels, colors_list):
        x_vals = []
        y_vals = []
        with open(csv_file, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                x_vals.append(float(row['utilization']))
                y_vals.append(float(row[test]))
        
        if x_vals:
            ax.plot(x_vals, y_vals, marker='o', label=label, color=color, linewidth=2, markersize=4)
    
    ax.axhline(y=1.0, color='gray', linestyle='--', alpha=0.5, label='Ideal')
    ax.set_xlabel('Total Utilization', fontsize=12)
    ax.set_ylabel('Schedulability Ratio', fontsize=12)
    ax.set_title('Schedulability vs Utilization', fontsize=14, fontweight='bold')
    ax.legend(loc='lower left')
    ax.grid(alpha=0.3)
    ax.set_ylim(-0.05, 1.05)
    plt.tight_layout()
    plt.savefig(output_file, dpi=150)
    plt.close()
    print(f"  [PLOT] {output_file}")


def plot_overload(csv_file, output_file):
    """绘制过载场景分析图"""
    # 按调度器分组
    sched_data = {}
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            s = row['scheduler']
            if s not in sched_data:
                sched_data[s] = {'u': [], 'miss': [], 'preempt': []}
            sched_data[s]['u'].append(float(row['utilization']))
            sched_data[s]['miss'].append(float(row['miss_ratio']))
            sched_data[s]['preempt'].append(float(row['preemptions']))
    
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5))
    
    for s, d in sched_data.items():
        color = COLORS.get(s, '#333333')
        ax1.plot(d['u'], d['miss'], marker='o', label=s, color=color, linewidth=2, markersize=4)
        ax2.plot(d['u'], d['preempt'], marker='s', label=s, color=color, linewidth=2, markersize=4)
    
    ax1.set_xlabel('Utilization', fontsize=12)
    ax1.set_ylabel('Miss Ratio', fontsize=12)
    ax1.set_title('Deadline Miss Ratio vs Utilization', fontsize=12, fontweight='bold')
    ax1.legend()
    ax1.grid(alpha=0.3)
    
    ax2.set_xlabel('Utilization', fontsize=12)
    ax2.set_ylabel('Preemptions', fontsize=12)
    ax2.set_title('Preemptions vs Utilization', fontsize=12, fontweight='bold')
    ax2.legend()
    ax2.grid(alpha=0.3)
    
    plt.suptitle('Overload Scenario Analysis', fontsize=14, fontweight='bold')
    plt.tight_layout()
    plt.savefig(output_file, dpi=150)
    plt.close()
    print(f"  [PLOT] {output_file}")


def plot_scalability(csv_file, output_file):
    """绘制扩展性分析图"""
    sched_data = {}
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            s = row['scheduler']
            if s not in sched_data:
                sched_data[s] = {'n': [], 'miss': [], 'avg_resp': [], 'cpu': []}
            sched_data[s]['n'].append(int(row['task_count']))
            sched_data[s]['miss'].append(float(row['miss_ratio']))
            sched_data[s]['avg_resp'].append(float(row['avg_response']))
            sched_data[s]['cpu'].append(float(row['cpu_util']))
    
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5))
    
    for s, d in sched_data.items():
        color = COLORS.get(s, '#333333')
        ax1.plot(d['n'], d['miss'], marker='o', label=s, color=color, linewidth=2, markersize=4)
        ax2.plot(d['n'], d['avg_resp'], marker='s', label=s, color=color, linewidth=2, markersize=4)
    
    ax1.set_xlabel('Number of Tasks', fontsize=12)
    ax1.set_ylabel('Miss Ratio', fontsize=12)
    ax1.set_title('Miss Ratio vs Task Count', fontsize=12, fontweight='bold')
    ax1.legend()
    ax1.grid(alpha=0.3)
    
    ax2.set_xlabel('Number of Tasks', fontsize=12)
    ax2.set_ylabel('Avg Response Time', fontsize=12)
    ax2.set_title('Response Time vs Task Count', fontsize=12, fontweight='bold')
    ax2.legend()
    ax2.grid(alpha=0.3)
    
    plt.suptitle('Scalability Analysis', fontsize=14, fontweight='bold')
    plt.tight_layout()
    plt.savefig(output_file, dpi=150)
    plt.close()
    print(f"  [PLOT] {output_file}")


def plot_performance(csv_file, output_file):
    """绘制性能基准对比图"""
    data = []
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            data.append(row)
    
    if not data:
        return
    
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5))
    
    schedulers = [d['scheduler'] for d in data]
    times = [float(d['sim_time_ms']) for d in data]
    speeds = [float(d['speed_factor']) for d in data]
    colors = [COLORS.get(s, '#333333') for s in schedulers]
    
    bars1 = ax1.bar(schedulers, times, color=colors, edgecolor='black', linewidth=0.5)
    ax1.set_ylabel('Execution Time (ms)', fontsize=12)
    ax1.set_title('Scheduler Execution Time', fontsize=12, fontweight='bold')
    ax1.grid(axis='y', alpha=0.3)
    for bar, val in zip(bars1, times):
        ax1.text(bar.get_x() + bar.get_width()/2., bar.get_height(),
                f'{val:.2f}', ha='center', va='bottom', fontsize=8)
    
    bars2 = ax2.bar(schedulers, speeds, color=colors, edgecolor='black', linewidth=0.5)
    ax2.set_ylabel('Speed Factor (relative)', fontsize=12)
    ax2.set_title('Relative Speed Factor', fontsize=12, fontweight='bold')
    ax2.grid(axis='y', alpha=0.3)
    for bar, val in zip(bars2, speeds):
        ax2.text(bar.get_x() + bar.get_width()/2., bar.get_height(),
                f'{val:.2f}x', ha='center', va='bottom', fontsize=8)
    
    plt.suptitle('Performance Benchmark', fontsize=14, fontweight='bold')
    plt.tight_layout()
    plt.savefig(output_file, dpi=150)
    plt.close()
    print(f"  [PLOT] {output_file}")


def plot_period_sensitivity(csv_file, output_file):
    """绘制周期敏感性分析图"""
    sched_data = {}
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            s = row['scheduler']
            if s not in sched_data:
                sched_data[s] = {'ratio': [], 'miss': [], 'avg_resp': []}
            sched_data[s]['ratio'].append(float(row['period_ratio']))
            sched_data[s]['miss'].append(float(row['miss_ratio']))
            sched_data[s]['avg_resp'].append(float(row['avg_response']))
    
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5))
    
    for s, d in sched_data.items():
        color = COLORS.get(s, '#333333')
        ax1.plot(d['ratio'], d['miss'], marker='o', label=s, color=color, linewidth=2, markersize=4)
        ax2.plot(d['ratio'], d['avg_resp'], marker='s', label=s, color=color, linewidth=2, markersize=4)
    
    ax1.set_xlabel('Period Ratio', fontsize=12)
    ax1.set_ylabel('Miss Ratio', fontsize=12)
    ax1.set_title('Miss Ratio vs Period Ratio', fontsize=12, fontweight='bold')
    ax1.legend()
    ax1.grid(alpha=0.3)
    
    ax2.set_xlabel('Period Ratio', fontsize=12)
    ax2.set_ylabel('Avg Response Time', fontsize=12)
    ax2.set_title('Response Time vs Period Ratio', fontsize=12, fontweight='bold')
    ax2.legend()
    ax2.grid(alpha=0.3)
    
    plt.suptitle('Period Sensitivity Analysis', fontsize=14, fontweight='bold')
    plt.tight_layout()
    plt.savefig(output_file, dpi=150)
    plt.close()
    print(f"  [PLOT] {output_file}")


def plot_context_switch(csv_file, output_file):
    """绘制上下文切换对比图"""
    data = []
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            data.append(row)
    
    if not data:
        return
    
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))
    
    schedulers = [d['scheduler'] for d in data]
    cs = [int(d['context_switches']) for d in data]
    preempt = [int(d['preemptions']) for d in data]
    colors = [COLORS.get(s, '#333333') for s in schedulers]
    
    ax1.bar(schedulers, cs, color=colors, edgecolor='black', linewidth=0.5)
    ax1.set_ylabel('Context Switches', fontsize=12)
    ax1.set_title('Context Switches by Scheduler', fontsize=12, fontweight='bold')
    ax1.grid(axis='y', alpha=0.3)
    
    ax2.bar(schedulers, preempt, color=colors, edgecolor='black', linewidth=0.5)
    ax2.set_ylabel('Preemptions', fontsize=12)
    ax2.set_title('Preemptions by Scheduler', fontsize=12, fontweight='bold')
    ax2.grid(axis='y', alpha=0.3)
    
    plt.suptitle('Context Switch Analysis', fontsize=14, fontweight='bold')
    plt.tight_layout()
    plt.savefig(output_file, dpi=150)
    plt.close()
    print(f"  [PLOT] {output_file}")


def main():
    print("=" * 60)
    print("  实时调度实验结果可视化")
    print("=" * 60)
    
    res_dir = RESULTS_DIR
    
    # 甘特图
    for gantt in glob.glob(os.path.join(res_dir, '*_gantt.csv')):
        out = gantt.replace('.csv', '.png')
        plot_gantt(gantt, out)
    
    # 对比图
    comp = os.path.join(res_dir, 'exp2_comparison.csv')
    if os.path.exists(comp):
        plot_comparison(comp, os.path.join(res_dir, 'exp2_comparison.png'))
    
    # 可调度率
    sched = os.path.join(res_dir, 'exp3_schedulability.csv')
    if os.path.exists(sched):
        plot_schedulability(sched, os.path.join(res_dir, 'exp3_schedulability.png'))
    
    # 过载
    over = os.path.join(res_dir, 'exp4_overload.csv')
    if os.path.exists(over):
        plot_overload(over, os.path.join(res_dir, 'exp4_overload.png'))
    
    # 扩展性
    scale = os.path.join(res_dir, 'exp5_scalability.csv')
    if os.path.exists(scale):
        plot_scalability(scale, os.path.join(res_dir, 'exp5_scalability.png'))
    
    # 边界
    bound = os.path.join(res_dir, 'exp6_boundary.csv')
    if os.path.exists(bound):
        plot_comparison(bound, os.path.join(res_dir, 'exp6_boundary.png'))
    
    # 上下文切换
    cs = os.path.join(res_dir, 'exp7_context_switch.csv')
    if os.path.exists(cs):
        plot_context_switch(cs, os.path.join(res_dir, 'exp7_context_switch.png'))
    
    # 周期敏感性
    ps = os.path.join(res_dir, 'exp8_period_sensitivity.csv')
    if os.path.exists(ps):
        plot_period_sensitivity(ps, os.path.join(res_dir, 'exp8_period_sensitivity.png'))
    
    # 性能基准
    perf = os.path.join(res_dir, 'exp10_performance.csv')
    if os.path.exists(perf):
        plot_performance(perf, os.path.join(res_dir, 'exp10_performance.png'))
    
    print("=" * 60)
    print("  可视化完成！")
    print("=" * 60)


if __name__ == '__main__':
    main()
