#!/usr/bin/env python3
"""
BIDMC PhysioNet Validation Script
===================================
End-to-end validation of the phase–memory operator on real respiratory data
from the PhysioNet BIDMC dataset (or a synthetic fallback).

Usage
-----
    # Full validation with PhysioNet download (requires internet):
    python validation/validate_bidmc.py

    # Offline / CI mode (no internet required):
    python validation/validate_bidmc.py --synthetic

    # Specific record (1–53):
    python validation/validate_bidmc.py --record 5

Scientific basis: PAPER.md §5 (Experimental Protocol)
Dataset:  https://physionet.org/content/bidmc/1.0.0/
"""

import argparse
import sys
from pathlib import Path

import numpy as np
from scipy.signal import resample

# Allow running from the repository root or from validation/
sys.path.insert(0, str(Path(__file__).parent))

from physionet_loader import load_bidmc_record, generate_synthetic_resp
from pipeline import run_pipeline, DEFAULT_BASELINE_SAMP, DEFAULT_MEMORY_SAMPLES
from metrics import (
    compute_all_metrics,
    detection_latency,
    false_alarm_rate,
    rms_envelope,
    fft_peak_shift,
)
from plots import (
    plot_stable_segment,
    plot_drift_segment,
    plot_pause_segment,
    plot_comparison,
)

# Minimum signal duration required for the three regimes (seconds)
_MIN_DURATION_S = 90


def _apply_drift(signal: np.ndarray, fs: float, onset_s: float) -> np.ndarray:
    """
    Semi-synthetic frequency drift (PAPER.md §5.1 row 2).

    After onset_s, compress the tail of the signal in time to simulate a
    rising respiratory rate (frequency drift).  This preserves real signal
    morphology in the stable portion while introducing a controlled perturbation.
    """
    onset = int(onset_s * fs)
    stable_part = signal[:onset].copy()
    tail = signal[onset:]
    # Resample tail to 1.6× samples → same duration but higher rate
    tail_fast = resample(tail, int(len(tail) * 1.6))[:len(tail)]
    return np.concatenate([stable_part, tail_fast])


def _apply_pause(
    signal: np.ndarray,
    fs: float,
    onset_s: float,
    duration_s: float = 8.0,
) -> np.ndarray:
    """
    Semi-synthetic intermittent pause (PAPER.md §5.1 row 3).

    Near-zeros the amplitude for duration_s seconds starting at onset_s to
    simulate a breathing pause / apnea event.
    """
    out = signal.copy()
    start = int(onset_s * fs)
    end = min(int((onset_s + duration_s) * fs), len(out))
    out[start:end] *= 0.03   # near-zero; matches §5.1 "reduced amplitude"
    return out


def run_validation(use_synthetic: bool = False, record_id: int = 1) -> dict:
    """
    Run the complete validation pipeline and print the PAPER.md Table 1 summary.

    Returns a dict of computed metrics for programmatic use / testing.
    """
    print("=" * 62)
    print("RespiroSync — PhysioNet BIDMC Validation  (PAPER.md §5)")
    print("=" * 62)

    # ── 1. Load data ─────────────────────────────────────────────────────────
    if use_synthetic:
        print("[INFO] Using synthetic signal (offline mode).")
        data = generate_synthetic_resp(duration_s=120)
    else:
        try:
            print(f"[INFO] Downloading BIDMC record {record_id:02d} from PhysioNet …")
            data = load_bidmc_record(record_id)
            print(f"       {len(data['signal'])} samples at {data['fs']} Hz "
                  f"({len(data['signal'])/data['fs']:.0f} s)  — record: {data['record']}")
        except Exception as exc:
            print(f"[WARN] PhysioNet download failed: {exc}")
            print("[INFO] Falling back to synthetic signal.")
            data = generate_synthetic_resp(duration_s=120)

    signal, fs = data['signal'], data['fs']

    # Ensure minimum duration
    min_n = int(_MIN_DURATION_S * fs)
    if len(signal) < min_n:
        reps = int(np.ceil(min_n / len(signal)))
        signal = np.tile(signal, reps)
    signal = signal[:min_n]

    onset_s  = 30.0                 # perturbation onset at 30 s
    n_stable = int(onset_s * fs)    # samples in the stable (control) segment
    seg_len  = n_stable * 2         # total samples per per-regime signal

    # ── 2. Stable segment (Regime 1) ─────────────────────────────────────────
    print("\n[1/3] Stable segment (Regime 1 — control) …")
    stable_sig    = signal[:n_stable]
    result_stable = run_pipeline(stable_sig, fs=fs)
    fig1 = plot_stable_segment(
        result_stable['time'],
        result_stable['filtered'],
        result_stable['delta_phi'],
        result_stable['threshold'],
    )
    print(f"      → {fig1}")
    far_stable = false_alarm_rate(
        # Skip the baseline calibration window at the start and M samples at
        # the end (boundary effects from the Hilbert transform).
        # FAR is measured on the interior post-calibration steady-state portion.
        result_stable['delta_phi'][DEFAULT_BASELINE_SAMP:-DEFAULT_MEMORY_SAMPLES],
        result_stable['threshold'],
        fs,
    )

    # ── 3. Drift segment (Regime 2) ──────────────────────────────────────────
    print("\n[2/3] Frequency-drift segment (Regime 2) …")
    drift_sig    = _apply_drift(signal, fs, onset_s=onset_s)[:seg_len]
    result_drift = run_pipeline(drift_sig, fs=fs)
    fig2 = plot_drift_segment(
        result_drift['time'],
        result_drift['filtered'],
        result_drift['delta_phi'],
        result_drift['threshold'],
        result_drift['instability'],
        onset_time=onset_s,
    )
    print(f"      → {fig2}")
    lat_drift = detection_latency(
        result_drift['delta_phi'],
        result_drift['threshold'],
        n_stable,
        fs,
    )

    # ── 4. Pause segment (Regime 3) ──────────────────────────────────────────
    print("\n[3/3] Pause segment (Regime 3) …")
    pause_sig    = _apply_pause(signal, fs, onset_s=onset_s)[:seg_len]
    result_pause = run_pipeline(pause_sig, fs=fs)
    fig3 = plot_pause_segment(
        result_pause['time'],
        result_pause['filtered'],
        result_pause['delta_phi'],
        result_pause['threshold'],
        result_pause['instability'],
        onset_time=onset_s,
    )
    print(f"      → {fig3}")
    lat_pause = detection_latency(
        result_pause['delta_phi'],
        result_pause['threshold'],
        n_stable,
        fs,
    )

    # ── 5. Baseline comparison plot ───────────────────────────────────────────
    print("\n[4/4] Baseline comparison plot …")
    full_result = run_pipeline(signal, fs=fs)
    rms_env       = rms_envelope(full_result['filtered'])
    fft_times, fft_freqs = fft_peak_shift(full_result['filtered'], fs)
    fig4 = plot_comparison(
        full_result['time'],
        full_result['delta_phi'],
        full_result['threshold'],
        rms_env,
        fft_times,
        fft_freqs,
        onset_time=onset_s,
    )
    print(f"      → {fig4}")

    # ── 6. PAPER.md Table 1 summary ──────────────────────────────────────────
    print()
    print("=" * 62)
    print("PAPER.md Table 1 — Quantitative Results")
    print("=" * 62)
    print(f"{'Metric':<38} {'Value':>20}")
    print("-" * 60)
    print(f"{'False alarm rate — stable (alarms/min)':<38} "
          f"{far_stable:>18.3f}")
    if lat_drift == np.inf:
        print(f"{'Detection latency — drift (s)':<38} {'no alarm':>20}")
    else:
        print(f"{'Detection latency — drift (s)':<38} {lat_drift:>18.3f}")
    if lat_pause == np.inf:
        print(f"{'Detection latency — pause (s)':<38} {'no alarm':>20}")
    else:
        print(f"{'Detection latency — pause (s)':<38} {lat_pause:>18.3f}")
    print(f"{'σ_ω  baseline std-dev (rad/s)':<38} "
          f"{result_stable['sigma_omega']:>18.4f}")
    print(f"{'α·σ_ω  decision threshold (rad/s)':<38} "
          f"{result_stable['threshold']:>18.4f}")
    print("=" * 62)
    print(f"\nFigures saved to: {Path(fig1).parent}")

    return dict(
        far_stable=far_stable,
        lat_drift=lat_drift,
        lat_pause=lat_pause,
        sigma_omega=result_stable['sigma_omega'],
        threshold=result_stable['threshold'],
    )


def main() -> None:
    parser = argparse.ArgumentParser(
        description='BIDMC PhysioNet validation for the phase–memory operator',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        '--synthetic', action='store_true',
        help='Use synthetic fallback signal (no internet required)',
    )
    parser.add_argument(
        '--record', type=int, default=1, metavar='N',
        help='BIDMC record number 1–53 (default: 1)',
    )
    args = parser.parse_args()
    run_validation(use_synthetic=args.synthetic, record_id=args.record)


if __name__ == '__main__':
    main()
