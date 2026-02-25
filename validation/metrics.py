"""
Quantitative Metrics for Respiratory Instability Validation
=============================================================
Implements the primary outcomes from PAPER.md §5.3:

  - Detection latency   — onset-to-alarm time
  - False alarm rate    — alarm rate in the control (stable) regime
  - RMS envelope        — windowed RMS amplitude baseline  (PAPER.md §5.2)
  - FFT peak shift      — dominant spectral frequency baseline (PAPER.md §5.2)
"""

import numpy as np
from scipy.ndimage import uniform_filter1d
from scipy.signal import welch


def detection_latency(
    delta_phi: np.ndarray,
    threshold: float,
    onset_sample: int,
    fs: float,
) -> float:
    """
    Time from instability onset to first threshold crossing (PAPER.md §5.3).

    Parameters
    ----------
    delta_phi    : ΔΦ(t) array (output of pipeline.phase_memory_operator)
    threshold    : α · σ_ω decision threshold
    onset_sample : sample index at which the instability begins
    fs           : sample rate (Hz)

    Returns
    -------
    Latency in seconds.  Returns np.inf if no alarm fires after onset.
    """
    tail = delta_phi[onset_sample:]
    alarm_indices = np.where(tail > threshold)[0]
    if alarm_indices.size == 0:
        return np.inf
    return float(alarm_indices[0]) / fs


def false_alarm_rate(
    delta_phi: np.ndarray,
    threshold: float,
    fs: float,
) -> float:
    """
    Alarm rate in a stable (control) segment (PAPER.md §5.3).

    Alarms are counted as rising-edge transitions (False → True), not
    individual samples, to avoid inflating the rate during sustained events.

    Parameters
    ----------
    delta_phi : ΔΦ(t) array for the stable/control region only
    threshold : α · σ_ω decision threshold
    fs        : sample rate (Hz)

    Returns
    -------
    False alarms per minute.
    """
    above = delta_phi > threshold
    transitions = np.diff(above.astype(np.int8))
    n_alarms = int(np.sum(transitions == 1))
    duration_min = len(delta_phi) / fs / 60.0
    if duration_min < 1e-9:
        return 0.0
    return n_alarms / duration_min


def rms_envelope(signal: np.ndarray, window_samples: int = 150) -> np.ndarray:
    """
    Windowed RMS amplitude baseline (PAPER.md §5.2).

    Parameters
    ----------
    signal         : bandpass-filtered respiratory signal
    window_samples : RMS window length (default matches phase-memory M = 150)

    Returns
    -------
    RMS envelope array with the same length as signal.
    """
    mean_sq = uniform_filter1d(signal ** 2, size=window_samples, mode='nearest')
    return np.sqrt(np.maximum(mean_sq, 0.0))


def fft_peak_shift(
    signal: np.ndarray,
    fs: float,
    window_samples: int = 256,
    step_samples: int = 25,
    lo: float = 0.1,
    hi: float = 0.5,
) -> tuple:
    """
    Track the dominant spectral peak in the respiratory band (PAPER.md §5.2).

    Uses overlapping Welch-style windows to estimate the instantaneous
    respiratory frequency over time.

    Parameters
    ----------
    signal         : bandpass-filtered respiratory signal
    fs             : sample rate (Hz)
    window_samples : FFT window length in samples
    step_samples   : hop size between successive windows
    lo, hi         : respiratory band limits (Hz)

    Returns
    -------
    times      : np.ndarray – center time of each window (s)
    peak_freqs : np.ndarray – dominant frequency in [lo, hi] Hz
    """
    times, peak_freqs = [], []
    half = window_samples // 2
    for start in range(0, len(signal) - window_samples + 1, step_samples):
        seg = signal[start:start + window_samples]
        freqs_w, psd = welch(seg, fs=fs, nperseg=len(seg))
        mask = (freqs_w >= lo) & (freqs_w <= hi)
        if mask.any():
            peak_freqs.append(freqs_w[mask][np.argmax(psd[mask])])
        else:
            peak_freqs.append(float('nan'))
        times.append((start + half) / fs)
    return np.array(times), np.array(peak_freqs)


def compute_all_metrics(
    result: dict,
    onset_sample: int,
    stable_end: int,
    fs: float,
) -> dict:
    """
    Compute all primary outcomes from PAPER.md §5.3 given a pipeline result.

    Parameters
    ----------
    result       : dict returned by pipeline.run_pipeline()
    onset_sample : sample index where the instability begins
    stable_end   : last sample index of the stable (control) region
    fs           : sample rate (Hz)

    Returns
    -------
    dict with keys:
        latency_s    – detection latency (s)
        far_per_min  – false alarm rate (alarms/min) in the stable region
        rms_env      – RMS envelope array
        fft_times    – center times for FFT windows (s)
        fft_freqs    – dominant frequencies for FFT windows (Hz)
    """
    delta_phi = result['delta_phi']
    threshold = result['threshold']
    filtered  = result['filtered']

    latency  = detection_latency(delta_phi, threshold, onset_sample, fs)
    far      = false_alarm_rate(delta_phi[:stable_end], threshold, fs)
    rms_env  = rms_envelope(filtered)
    fft_t, fft_f = fft_peak_shift(filtered, fs)

    return dict(
        latency_s=latency,
        far_per_min=far,
        rms_env=rms_env,
        fft_times=fft_t,
        fft_freqs=fft_f,
    )
