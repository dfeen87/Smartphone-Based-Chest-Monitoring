"""
Real-Data Validation Plots
============================
Generates the three required figures from PAPER.md §5–6 using real or
semi-synthetic PhysioNet BIDMC respiratory signals.

Required figures
----------------
1. Regime 1 — Stable breathing:    ΔΦ(t) ≈ 0, no alarms
2. Regime 2 — Frequency drift:     ΔΦ(t) rises gradually
3. Regime 3 — Intermittent pause:  ΔΦ(t) spikes at onset
4. Baseline comparison:            ΔΦ(t) vs RMS envelope vs FFT peak shift

All figures are saved to validation/figures/ by default.
"""

from pathlib import Path

import matplotlib
matplotlib.use('Agg')   # non-interactive backend (CI / headless environments)
import matplotlib.gridspec as gridspec
import matplotlib.pyplot as plt
import numpy as np

_FIG_DIR = Path(__file__).parent / 'figures'


def _ensure_fig_dir() -> None:
    _FIG_DIR.mkdir(parents=True, exist_ok=True)


def plot_regime(
    time: np.ndarray,
    filtered: np.ndarray,
    delta_phi: np.ndarray,
    threshold: float,
    title: str,
    instability: np.ndarray = None,
    onset_time: float = None,
    outfile: str = None,
) -> str:
    """
    Two-panel figure:
      (top)    bandpass-filtered respiratory signal x(t)
      (bottom) instability metric ΔΦ(t) with threshold line

    Parameters
    ----------
    time        : time axis (s)
    filtered    : bandpass-filtered respiratory signal
    delta_phi   : instability metric ΔΦ(t)  (Eq. 5)
    threshold   : α · σ_ω decision threshold  (Eq. 6)
    title       : figure suptitle
    instability : optional boolean alarm array — shaded in red when True
    onset_time  : optional vertical line marking the event onset
    outfile     : save path; defaults to validation/figures/<safe_title>.png

    Returns
    -------
    Absolute path of the saved PNG.
    """
    _ensure_fig_dir()
    if outfile is None:
        safe = title.lower().replace(' ', '_').replace('–', '-').replace('/', '-')
        outfile = str(_FIG_DIR / f'{safe}.png')

    fig = plt.figure(figsize=(10, 5), constrained_layout=True)
    gs = gridspec.GridSpec(2, 1, figure=fig, height_ratios=[1, 1])
    ax1 = fig.add_subplot(gs[0])
    ax2 = fig.add_subplot(gs[1], sharex=ax1)

    # ── Top panel: respiratory signal ────────────────────────────────────
    ax1.plot(time, filtered, color='steelblue', linewidth=0.8,
             label='x(t) bandpass-filtered')
    if onset_time is not None:
        ax1.axvline(onset_time, color='red', linestyle='--', linewidth=1.2,
                    label='event onset')
    ax1.set_ylabel('Amplitude (a.u.)')
    ax1.set_title(title, fontsize=11, fontweight='bold')
    ax1.legend(loc='upper right', fontsize=8)
    ax1.grid(True, alpha=0.3)

    # ── Bottom panel: ΔΦ(t) ─────────────────────────────────────────────
    ax2.plot(time, delta_phi, color='darkorange', linewidth=0.8,
             label='ΔΦ(t) instability metric')
    ax2.axhline(threshold, color='crimson', linestyle='--', linewidth=1.2,
                label=f'threshold α·σ_ω = {threshold:.3f} rad/s')
    if instability is not None:
        ax2.fill_between(time, 0, delta_phi, where=instability,
                         alpha=0.25, color='red', label='instability alarm')
    if onset_time is not None:
        ax2.axvline(onset_time, color='red', linestyle='--', linewidth=1.2)
    ax2.set_xlabel('Time (s)')
    ax2.set_ylabel('ΔΦ(t) (rad/s)')
    ax2.legend(loc='upper right', fontsize=8)
    ax2.grid(True, alpha=0.3)

    plt.savefig(outfile, dpi=150, bbox_inches='tight')
    plt.close(fig)
    return outfile


def plot_stable_segment(
    time: np.ndarray,
    filtered: np.ndarray,
    delta_phi: np.ndarray,
    threshold: float,
    outfile: str = None,
) -> str:
    """
    Regime 1 — Regular breathing (control): ΔΦ(t) ≈ 0, no alarms.
    PAPER.md §5.1 row 1.
    """
    return plot_regime(
        time, filtered, delta_phi, threshold,
        title='Regime 1 – Stable Breathing (Control): ΔΦ(t) ≈ 0',
        outfile=outfile or str(_FIG_DIR / 'regime1_stable.png'),
    )


def plot_drift_segment(
    time: np.ndarray,
    filtered: np.ndarray,
    delta_phi: np.ndarray,
    threshold: float,
    instability: np.ndarray,
    onset_time: float,
    outfile: str = None,
) -> str:
    """
    Regime 2 — Frequency drift: ΔΦ(t) rises gradually.
    PAPER.md §5.1 row 2.
    """
    return plot_regime(
        time, filtered, delta_phi, threshold,
        title='Regime 2 – Frequency Drift: ΔΦ(t) rises gradually',
        instability=instability,
        onset_time=onset_time,
        outfile=outfile or str(_FIG_DIR / 'regime2_drift.png'),
    )


def plot_pause_segment(
    time: np.ndarray,
    filtered: np.ndarray,
    delta_phi: np.ndarray,
    threshold: float,
    instability: np.ndarray,
    onset_time: float,
    outfile: str = None,
) -> str:
    """
    Regime 3 — Intermittent pause: ΔΦ(t) spikes at onset.
    PAPER.md §5.1 row 3.
    """
    return plot_regime(
        time, filtered, delta_phi, threshold,
        title='Regime 3 – Intermittent Pause: ΔΦ(t) spikes at onset',
        instability=instability,
        onset_time=onset_time,
        outfile=outfile or str(_FIG_DIR / 'regime3_pause.png'),
    )


def plot_comparison(
    time: np.ndarray,
    delta_phi: np.ndarray,
    threshold: float,
    rms_env: np.ndarray,
    fft_times: np.ndarray,
    fft_freqs: np.ndarray,
    onset_time: float = None,
    outfile: str = None,
) -> str:
    """
    Three-panel comparison of ΔΦ(t) vs RMS envelope vs FFT peak frequency.
    Provides the visual basis for PAPER.md Table 1.

    Parameters
    ----------
    time        : time axis (s)
    delta_phi   : instability metric ΔΦ(t)
    threshold   : α · σ_ω threshold
    rms_env     : RMS envelope (from metrics.rms_envelope)
    fft_times   : FFT window center times (s)
    fft_freqs   : FFT dominant frequencies (Hz)
    onset_time  : optional event onset marker (s)
    outfile     : save path; defaults to validation/figures/comparison_baselines.png
    """
    _ensure_fig_dir()
    if outfile is None:
        outfile = str(_FIG_DIR / 'comparison_baselines.png')

    fig, axes = plt.subplots(3, 1, figsize=(10, 8))

    def _vline(ax):
        if onset_time is not None:
            ax.axvline(onset_time, color='gray', linestyle=':', linewidth=1.0,
                       label='event onset')

    # Panel 1: ΔΦ(t)
    axes[0].plot(time, delta_phi, color='darkorange', linewidth=0.8,
                 label='ΔΦ(t)')
    axes[0].axhline(threshold, color='crimson', linestyle='--', linewidth=1.2,
                    label=f'threshold = {threshold:.3f}')
    _vline(axes[0])
    axes[0].set_ylabel('ΔΦ(t) (rad/s)')
    axes[0].set_title('Proposed: Phase–Memory Divergence ΔΦ(t)', fontsize=10)
    axes[0].legend(fontsize=8)
    axes[0].grid(True, alpha=0.3)

    # Panel 2: RMS envelope
    axes[1].plot(time, rms_env, color='steelblue', linewidth=0.8,
                 label='RMS envelope')
    _vline(axes[1])
    axes[1].set_ylabel('RMS (a.u.)')
    axes[1].set_title('Baseline: RMS Envelope (PAPER.md §5.2)', fontsize=10)
    axes[1].legend(fontsize=8)
    axes[1].grid(True, alpha=0.3)

    # Panel 3: FFT peak frequency (convert Hz → BPM for readability)
    axes[2].plot(fft_times, fft_freqs * 60.0, color='forestgreen',
                 linewidth=0.8, marker='o', markersize=2,
                 label='FFT peak (BPM)')
    _vline(axes[2])
    axes[2].set_xlabel('Time (s)')
    axes[2].set_ylabel('Resp. rate (BPM)')
    axes[2].set_title('Baseline: FFT Peak Frequency (PAPER.md §5.2)', fontsize=10)
    axes[2].legend(fontsize=8)
    axes[2].grid(True, alpha=0.3)

    fig.suptitle(
        'Method Comparison: Proposed ΔΦ(t) vs Baselines  (PAPER.md §5.2, Table 1)',
        fontsize=11, fontweight='bold',
    )
    plt.tight_layout()
    plt.savefig(outfile, dpi=150, bbox_inches='tight')
    plt.close(fig)
    return outfile
