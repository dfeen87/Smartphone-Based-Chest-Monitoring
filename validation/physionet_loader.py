"""
PhysioNet BIDMC Dataset Loader
================================
Loads the Beth Israel Deaconess Medical Center (BIDMC) respiratory dataset
from PhysioNet using the wfdb library.

Dataset: https://physionet.org/content/bidmc/1.0.0/
  - 53 recordings, ~8 min each
  - Signals: ECG (II), PPG (PLETH), Respiratory (RESP)
  - Sampling rate: 125 Hz
  - Reference: Pimentel et al., 2017

This module provides:
  load_bidmc_record()      – download and resample a single BIDMC record
  generate_synthetic_resp() – offline fallback for CI / no-network environments
"""

from math import gcd

import numpy as np
from scipy.signal import resample_poly

try:
    import wfdb
    _WFDB_AVAILABLE = True
except ImportError:
    _WFDB_AVAILABLE = False

# Native sample rate of the BIDMC dataset (Hz)
BIDMC_FS = 125

# Target sample rate matching PAPER.md §2.2 (fₛ ∈ [50, 100] Hz)
TARGET_FS = 50

# Impedance-pneumography channel name in BIDMC records
RESP_CHANNEL = 'RESP'

# PhysioNet directory identifier for wfdb.rdrecord
BIDMC_DIR = 'bidmc/1.0.0'


def load_bidmc_record(record_id: int = 1, target_fs: int = TARGET_FS) -> dict:
    """
    Load a single BIDMC record and return the respiratory signal.

    Parameters
    ----------
    record_id : int
        Record number 1–53.
    target_fs : int
        Target sample rate in Hz (default: 50 Hz per PAPER.md §2.2).

    Returns
    -------
    dict with keys:
        signal : np.ndarray  – respiratory signal resampled to target_fs
        fs     : int         – sample rate (= target_fs)
        time   : np.ndarray  – time axis in seconds
        record : str         – PhysioNet record name (e.g. 'bidmc01')

    Raises
    ------
    ImportError  if wfdb is not installed.
    ValueError   if no RESP channel is found in the record.
    """
    if not _WFDB_AVAILABLE:
        raise ImportError(
            "wfdb package required.  Install with:\n"
            "    pip install wfdb\n"
            "or run with --synthetic to use the offline fallback."
        )

    record_name = f'bidmc{record_id:02d}'
    record = wfdb.rdrecord(record_name, pn_dir=BIDMC_DIR)

    # Locate the RESP channel (case-insensitive)
    sig_names_upper = [s.upper() for s in record.sig_name]
    if RESP_CHANNEL not in sig_names_upper:
        raise ValueError(
            f"RESP channel not found in {record_name}.  "
            f"Available channels: {record.sig_name}"
        )
    resp_idx = sig_names_upper.index(RESP_CHANNEL)
    signal = record.p_signal[:, resp_idx].astype(np.float64)

    # Replace NaN / Inf samples with linear interpolation
    bad = ~np.isfinite(signal)
    if bad.any():
        xp = np.where(~bad)[0]
        signal[bad] = np.interp(np.where(bad)[0], xp, signal[xp])

    # Resample from BIDMC native rate to target_fs
    native_fs = int(record.fs)
    if native_fs != target_fs:
        signal = _resample(signal, native_fs, target_fs)

    time = np.arange(len(signal)) / target_fs
    return dict(signal=signal, fs=target_fs, time=time, record=record_name)


def _resample(signal: np.ndarray, src_fs: int, dst_fs: int) -> np.ndarray:
    """Rational-ratio resampling via scipy.signal.resample_poly."""
    g = gcd(src_fs, dst_fs)
    return resample_poly(signal, dst_fs // g, src_fs // g)


def generate_synthetic_resp(
    duration_s: float = 120.0,
    fs: int = TARGET_FS,
    base_freq: float = 0.25,
    noise_std: float = 0.02,
    seed: int = 42,
) -> dict:
    """
    Generate a synthetic respiratory signal as an offline fallback.

    Produces three consecutive 30-second segments that match the controlled
    validation regimes from PAPER.md §5.1:

      Segment 1 (0–30 s)   – Stable breathing at base_freq
      Segment 2 (30–60 s)  – Frequency drift (base_freq → 1.8 × base_freq)
      Segment 3 (60–90 s)  – Intermittent pause (near-zero amplitude)
      Segment 4 (90–120 s) – Recovery (stable again)

    Returns the same dict format as load_bidmc_record().
    """
    rng = np.random.default_rng(seed)
    n = int(duration_s * fs)
    t = np.arange(n) / fs
    seg = n // 4

    # Instantaneous frequency for each segment
    freq = np.zeros(n)
    freq[:seg]        = base_freq                                    # stable
    freq[seg:2*seg]   = np.linspace(base_freq, base_freq * 1.8, seg) # drift
    freq[2*seg:3*seg] = base_freq                                    # (pause below)
    freq[3*seg:]      = base_freq                                    # recovery

    # Integrate frequency to get phase
    phase = 2.0 * np.pi * np.cumsum(freq) / fs
    signal = np.sin(phase)

    # Apply pause: near-zero amplitude in segment 3
    signal[2*seg:3*seg] *= 0.04

    # Add small Gaussian noise
    signal += noise_std * rng.standard_normal(n)

    return dict(signal=signal, fs=fs, time=t, record='synthetic')
