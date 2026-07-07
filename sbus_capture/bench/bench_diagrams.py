# -*- coding: utf-8 -*-
"""Растровые блок-схемы для документации bench (matplotlib → PNG)."""
from __future__ import annotations

import hashlib
import os

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch, Rectangle

plt.rcParams["font.family"] = "DejaVu Sans"


def _box(ax, cx, cy, w, h, text, fc="#ecf0f1", ec="#2c3e50", fs=8.5, bold=False):
    p = FancyBboxPatch(
        (cx - w / 2, cy - h / 2),
        w,
        h,
        boxstyle="round,pad=0.03,rounding_size=0.08",
        linewidth=1.2,
        edgecolor=ec,
        facecolor=fc,
        zorder=2,
    )
    ax.add_patch(p)
    ax.text(
        cx,
        cy,
        text,
        ha="center",
        va="center",
        fontsize=fs,
        weight="bold" if bold else "normal",
        zorder=3,
        multialignment="center",
    )
    return cx, cy, w, h


def _diamond(ax, cx, cy, w, h, text, fc="#fdebd0"):
    pts = [(cx, cy + h / 2), (cx + w / 2, cy), (cx, cy - h / 2), (cx - w / 2, cy)]
    from matplotlib.patches import Polygon

    ax.add_patch(Polygon(pts, closed=True, facecolor=fc, edgecolor="#7f5533", linewidth=1.2, zorder=2))
    ax.text(cx, cy, text, ha="center", va="center", fontsize=7.5, zorder=3, multialignment="center")


def _arrow(ax, p1, p2, label="", color="#2c3e50", rad=0.0):
    arr = FancyArrowPatch(
        p1,
        p2,
        connectionstyle=f"arc3,rad={rad}",
        arrowstyle="-|>",
        mutation_scale=14,
        linewidth=1.4,
        color=color,
        zorder=1,
    )
    ax.add_patch(arr)
    if label:
        mx = (p1[0] + p2[0]) / 2
        my = (p1[1] + p2[1]) / 2 + 0.15
        ax.text(
            mx,
            my,
            label,
            fontsize=7,
            ha="center",
            color="#555",
            bbox=dict(boxstyle="round,pad=0.1", fc="white", ec="none", alpha=0.9),
        )


def _subgraph(ax, x, y, w, h, title, fc="#f4f6f7"):
    ax.add_patch(
        Rectangle(
            (x, y),
            w,
            h,
            linewidth=1.5,
            edgecolor="#5d6d7e",
            facecolor=fc,
            linestyle="--",
            zorder=0,
        )
    )
    ax.text(x + 0.15, y + h - 0.25, title, fontsize=9, weight="bold", color="#2c3e50", zorder=1)


def _save(fig, path):
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    fig.tight_layout()
    fig.savefig(path, dpi=180, bbox_inches="tight", facecolor="white")
    plt.close(fig)


def render_architecture(path: str) -> None:
    fig, ax = plt.subplots(figsize=(14, 8))
    ax.set_xlim(0, 14)
    ax.set_ylim(0, 8)
    ax.axis("off")
    ax.set_title("Архитектура electronic_pilot_bench", fontsize=13, weight="bold", pad=10)

    _subgraph(ax, 0.3, 5.2, 13.4, 2.5, "Аппаратура")
    _box(ax, 2.0, 6.4, 2.2, 0.9, "Камера IMX708\n1920×1080 RGB", fc="#d6eaf8")
    _box(ax, 5.0, 6.4, 1.8, 0.9, "USB-мышь\nevdev", fc="#d6eaf8")
    _box(ax, 11.5, 6.4, 2.0, 0.9, "PAL монитор\nDRM composite", fc="#d6eaf8")
    _box(ax, 11.5, 5.5, 2.0, 0.9, "Гимбал MG946R\nPCA9685 I2C", fc="#d5f5e3")

    _subgraph(ax, 0.3, 1.8, 8.5, 3.1, "Поток track @60 Hz")
    _box(ax, 2.0, 3.6, 2.4, 0.75, "CameraCapture\ngrabLatest")
    _box(ax, 2.0, 2.5, 1.8, 0.65, "MouseInput\npoll")
    _box(ax, 4.2, 2.5, 1.8, 0.65, "RoiSelector\nupdate")
    _box(ax, 4.5, 3.6, 2.2, 0.85, "MarkerTracker\ninit / update", fc="#fcf3cf", bold=True)
    _box(ax, 7.0, 3.6, 2.0, 0.75, "GimbalTracker\nupdate", fc="#d5f5e3")
    _box(ax, 7.0, 2.5, 2.0, 0.75, "DisplayHub\npublish ≈25 Hz")

    _subgraph(ax, 9.0, 1.8, 4.7, 3.1, "Поток PAL @25 Hz")
    y = 3.5
    for i, t in enumerate(
        [
            "DisplayHub\nconsume",
            "makeLores\n720×576",
            "mapDetection\nToPal",
            "OsdRenderer",
            "CompositeOut",
        ]
    ):
        _box(ax, 11.3, y - i * 0.72, 2.2, 0.62, t, fc="#ebf5fb", fs=7.5)
        if i < 4:
            _arrow(ax, (11.3, y - i * 0.72 - 0.35), (11.3, y - (i + 1) * 0.72 + 0.35))

    _arrow(ax, (3.2, 3.6), (3.4, 3.6), color="#1f5fb0")
    _arrow(ax, (2.9, 2.5), (3.3, 2.5))
    _arrow(ax, (5.1, 2.5), (3.4, 3.2), label="roi_main")
    _arrow(ax, (5.6, 3.6), (6.0, 3.6), label="det", color="#c0392b")
    _arrow(ax, (6.0, 3.2), (6.0, 2.85))
    _arrow(ax, (8.0, 3.6), (11.5, 5.9), color="#1e8449", rad=-0.2)
    _arrow(ax, (2.0, 5.95), (2.0, 4.0), color="#1f5fb0")
    _arrow(ax, (5.0, 5.95), (4.2, 2.85))
    _arrow(ax, (10.2, 2.5), (10.2, 3.15))
    _arrow(ax, (12.4, 2.0), (11.5, 5.95), color="#1f5fb0")

    _save(fig, path)


def render_init_mode(path: str) -> None:
    fig, ax = plt.subplots(figsize=(11, 6))
    ax.set_xlim(0, 11)
    ax.set_ylim(0, 6)
    ax.axis("off")
    ax.set_title("Выбор режима трекера при init", fontsize=12, weight="bold")

    _box(ax, 1.5, 4.5, 2.2, 0.9, "ROI оператора\nFullHD", fc="#d6eaf8")
    _box(ax, 4.5, 5.2, 2.4, 0.75, "measureRoi\nContrast")
    _box(ax, 4.5, 3.8, 2.4, 0.85, "initSegmentBbox\nкруг → blob")
    _diamond(ax, 7.5, 4.5, 2.6, 1.4, "Круг + меньше ROI\n+ contrast ≥ 22?")
    _box(ax, 9.8, 5.3, 2.2, 0.9, "SEGMENT\nсегментация + NCC", fc="#fadbd8", bold=True)
    _box(ax, 9.8, 3.5, 2.2, 0.9, "MOSSE\nTrackerMOSSE\n+ probeScale", fc="#d5f5e3", bold=True)

    _arrow(ax, (2.6, 4.7), (3.3, 5.1))
    _arrow(ax, (2.6, 4.3), (3.3, 3.9))
    _arrow(ax, (5.7, 4.5), (6.2, 4.5))
    _arrow(ax, (8.8, 5.0), (8.7, 5.2), label="да")
    _arrow(ax, (8.8, 4.0), (8.7, 3.8), label="нет")

    _save(fig, path)


def render_update_flow(path: str) -> None:
    fig, ax = plt.subplots(figsize=(10, 11))
    ax.set_xlim(0, 10)
    ax.set_ylim(0, 11)
    ax.axis("off")
    ax.set_title("Алгоритм MarkerTracker::update", fontsize=12, weight="bold")

    y = 10.0
    dy = 1.05
    nodes = [
        ("update(main_rgb, dt)", "#d6eaf8", 3.0, 0.55),
        ("prepareGray / prepareBgr", "#ecf0f1", 3.2, 0.5),
    ]
    for text, fc, w, h in nodes:
        _box(ax, 5, y, w, h, text, fc=fc)
        y -= dy

    _diamond(ax, 5, y, 3.4, 1.0, "miss_frames > 0\nили marker_lost?")
    y -= dy + 0.2
    _box(ax, 2.2, y + dy, 2.6, 0.85, "handleMiss\nSEARCH / LOST", fc="#fadbd8")
    _box(ax, 7.5, y + dy, 2.8, 0.55, "pred = center + v·dt", fc="#ecf0f1")

    _diamond(ax, 5, y, 2.2, 0.9, "режим")
    y -= dy
    _box(ax, 2.5, y, 3.0, 1.0, "SEG: measureTrackBlob\n+ scale / NCC", fc="#fdebd0", fs=7.5)
    _box(ax, 7.5, y, 3.0, 1.1, "MOSSE: updatePosition\nprobeScale / reinit", fc="#d5f5e3", fs=7.5)
    y -= dy + 0.1
    _diamond(ax, 5, y, 2.4, 0.85, "located?")
    y -= dy
    _box(ax, 2.5, y, 2.8, 0.75, "fail_streak++\nhold / SEARCH", fc="#fadbd8", fs=7.5)
    _box(ax, 7.5, y, 3.2, 0.85, "bboxForOutput\nMarkerDetection Live", fc="#abebc6", bold=True)

    _arrow(ax, (5, 9.45), (5, 9.0))
    _arrow(ax, (5, 8.4), (5, 7.85))
    _arrow(ax, (3.8, 7.3), (2.8, 7.3), label="да")
    _arrow(ax, (6.2, 7.3), (7.2, 7.3), label="нет")
    _arrow(ax, (5, 6.55), (5, 6.1))
    _arrow(ax, (4.0, 5.6), (2.8, 5.6), label="seg")
    _arrow(ax, (6.0, 5.6), (7.2, 5.6), label="mosse")
    _arrow(ax, (5, 4.45), (5, 4.0))
    _arrow(ax, (4.0, 3.55), (2.8, 3.55), label="нет")
    _arrow(ax, (6.0, 3.55), (7.2, 3.55), label="да")

    _save(fig, path)


def render_loss_fsm(path: str) -> None:
    fig, ax = plt.subplots(figsize=(12, 4.5))
    ax.set_xlim(0, 12)
    ax.set_ylim(0, 4.5)
    ax.axis("off")
    ax.set_title("Конечный автомат потери метки", fontsize=12, weight="bold")

    states = [
        (1.2, "Live", "#abebc6"),
        (3.8, "Search\n(Predicted)", "#fdebd0"),
        (6.5, "Lost", "#fadbd8"),
        (9.2, "GiveUp\nновый ROI", "#e5e8e8"),
    ]
    for x, name, fc in states:
        _box(ax, x, 2.2, 1.8, 0.9, name, fc=fc, bold=True)

    _arrow(ax, (2.1, 2.2), (2.9, 2.2), label="fail_streak")
    _arrow(ax, (4.7, 2.2), (5.6, 2.2), label="≥2 с")
    _arrow(ax, (7.4, 2.2), (8.3, 2.2), label="≥3 с")
    _arrow(ax, (3.8, 2.65), (3.8, 3.3), rad=0)
    _arrow(ax, (3.3, 3.3), (1.5, 3.3))
    _arrow(ax, (1.5, 3.3), (1.2, 2.65), label="reacquire OK")

    ax.text(6.5, 0.8, "Search: tryReacquire каждые 2 кадра, frozen bbox", ha="center", fontsize=8, color="#555")

    _save(fig, path)


def classify_mermaid(source: str) -> str:
    s = source.lower()
    if "subgraph hw" in s or "поток track @60" in s or "pal_thread" in s:
        return "architecture"
    if "seg_used_circle" in s or "measureroicontrast" in s.replace("_", ""):
        return "init_mode"
    if "statediagram" in s or "giveup" in s:
        return "loss_fsm"
    if "update(main_rgb" in s or "handlemiss" in s.replace("_", ""):
        return "update_flow"
    return "generic"


def render_mermaid_to_png(source: str, out_path: str) -> bool:
    kind = classify_mermaid(source)
    if kind == "architecture":
        render_architecture(out_path)
    elif kind == "init_mode":
        render_init_mode(out_path)
    elif kind == "update_flow":
        render_update_flow(out_path)
    elif kind == "loss_fsm":
        render_loss_fsm(out_path)
    else:
        return False
    return True


def cache_path_for_mermaid(source: str, cache_dir: str) -> str:
    h = hashlib.sha1(source.encode("utf-8")).hexdigest()[:12]
    kind = classify_mermaid(source)
    return os.path.join(cache_dir, f"diagram_{kind}_{h}.png")
