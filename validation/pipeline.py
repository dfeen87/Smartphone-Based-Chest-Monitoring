"""
Phase–Memory Operator — Python Reference Implementation
=========================================================
Mirrors the C++ PhaseMemoryOperator in core/respirosync_core.cpp.

Equations and variable names follow PAPER.md §3–4 exactly:

  z(t)   = x(t) + i·H[x(t)]            (Eq. 2)  — analytic signal
  θ(t)   = arg(z(t))                    §3.1     — instantaneous phase
  ω(t)   = dθ/dt  (2π-unwrapped)        (Eq. 3)  — phase velocity
  ω̄(t)  = rolling_mean(ω, M)            (Eq. 4)  — short-term phase memory
  ΔΦ(t) = |ω(t) − ω̄(t)|               (Eq. 5)  — instability metric

Decision logic (Eq. 6):
  instability iff ΔΦ(t) > α · σ_ω

Note on Hilbert transform:
  The C++ core uses a derivative approximation of H[x] that is valid for
  narrow-band signals.  This Python implementation uses scipy.signal.hilbert
  (FFT-based) for accuracy on real physiological signals.  Both are correct
  implementations of Eq. 2; the approximation is used in the C++ solely for
  computational efficiency on embedded hardware.
"""

import numpy as np
from scipy.signal import butter, sosfiltfilt, hilbert, detrend as sp_detrend

# ── Default operator parameters (PAPER.md §4.2 and §8) ────────────────────
DEFAULT_FS             = 50     # sample rate (Hz), PAPER.md §2.2
DEFAULT_LO_HZ          = 0.1   # bandpass lower edge (Hz), §2.4
DEFAULT_HI_HZ          = 0.5   # bandpass upper edge (Hz), §2.4
DEFAULT_MEMORY_SAMPLES = 150   # M  — phase-memory window (≈ 3 s at 50 Hz)
DEFAULT_BASELINE_SAMP  = 250   # calibration window (≈ 5 s at 50 Hz)
DEFAULT_ALPHA          = 2.0   # α sensitivity parameter ∈ [2, 3], Eq. 6


def bandpass_filter(
    signal: np.ndarray,
    fs: float = DEFAULT_FS,
    lo: float = DEFAULT_LO_HZ,
    hi: float = DEFAULT_HI_HZ,
    order: int = 2,
) -> np.ndarray:
    """
    2nd-order Butterworth bandpass filter (PAPER.md §2.4).

    Isolates the respiratory band (0.1–0.5 Hz), removing drift and motion
    artefacts.  Uses zero-phase filtering (sosfiltfilt) for offline/batch use;
    replace with a causal filter for real-time deployment.

    Parameters
    ----------
    signal : raw or detrended respiratory channel x(t)
    fs     : sample rate (Hz)
    lo, hi : passband edges (Hz)
    order  : Butterworth filter order

    Returns
    -------
    Bandpass-filtered signal of the same length.
    """
    sos = butter(order, [lo, hi], btype='bandpass', fs=fs, output='sos')
    return sosfiltfilt(sos, signal)


def phase_memory_operator(
    filtered: np.ndarray,
    fs: float = DEFAULT_FS,
    M: int = DEFAULT_MEMORY_SAMPLES,
    baseline_samples: int = DEFAULT_BASELINE_SAMP,
    alpha: float = DEFAULT_ALPHA,
) -> dict:
    """
    Compute the phase–memory instability metric ΔΦ(t) (PAPER.md §3–4).

    Parameters
    ----------
    filtered         : bandpass-filtered signal x(t)
    fs               : sample rate (Hz)
    M                : phase-memory window length in samples (Eq. 4)
    baseline_samples : number of initial samples used to estimate σ_ω (Eq. 6)
    alpha            : sensitivity parameter α (Eq. 6)

    Returns
    -------
    dict with keys:
        delta_phi   : np.ndarray – ΔΦ(t), one scalar per sample  (Eq. 5)
        instability : np.ndarray – boolean alarm per sample        (Eq. 6)
        omega       : np.ndarray – instantaneous phase velocity ω(t) (rad/s)
        omega_bar   : np.ndarray – phase memory ω̄(t)             (rad/s)
        sigma_omega : float      – baseline std-dev σ_ω           (rad/s)
        threshold   : float      – decision threshold α · σ_ω
    """
    dt = 1.0 / fs

    # ── Step 1: Analytic signal z(t) = x(t) + i·H[x(t)]  (Eq. 2) ──────────
    z = hilbert(filtered)

    # ── Step 2: Instantaneous phase θ(t) = arg(z(t))  (§3.1) ───────────────
    theta = np.angle(z)

    # ── Step 3: Phase velocity ω(t) = dθ/dt with 2π-unwrap  (Eq. 3) ────────
    theta_unwrapped = np.unwrap(theta)
    omega = np.gradient(theta_unwrapped, dt)   # rad/s

    # ── Step 4: Short-term phase memory ω̄(t)  (Eq. 4) ─────────────────────
    # Causal rolling mean over M samples — matches the C++ rolling buffer.
    omega_bar = _causal_rolling_mean(omega, M)

    # ── Step 5: Instability metric ΔΦ(t) = |ω(t) − ω̄(t)|  (Eq. 5) ────────
    delta_phi = np.abs(omega - omega_bar)

    # ── Baseline σ_ω estimation on the calibration window  (Eq. 6) ──────────
    n_cal = min(baseline_samples, len(omega))
    sigma_omega = float(np.std(omega[:n_cal]))
    if sigma_omega < 1e-6:
        sigma_omega = 1e-6       # guard against near-zero (matches C++)
    threshold = alpha * sigma_omega

    instability = delta_phi > threshold

    return dict(
        delta_phi=delta_phi,
        instability=instability,
        omega=omega,
        omega_bar=omega_bar,
        sigma_omega=sigma_omega,
        threshold=threshold,
    )


def run_pipeline(
    signal: np.ndarray,
    fs: float = DEFAULT_FS,
    **kwargs,
) -> dict:
    """
    Full end-to-end pipeline (PAPER.md §7.1):

      detrend  →  bandpass  →  Hilbert  →  θ(t)  →  ω(t)  →  ω̄(t)  →  ΔΦ(t)

    Parameters
    ----------
    signal : raw respiratory signal (e.g., from physionet_loader)
    fs     : sample rate (Hz)
    **kwargs : forwarded to phase_memory_operator (M, alpha, baseline_samples)

    Returns
    -------
    dict merging phase_memory_operator output plus:
        filtered : np.ndarray – bandpass-filtered signal
        time     : np.ndarray – time axis (s)
    """
    # Drift removal (PAPER.md §2.4)
    x = sp_detrend(signal.astype(np.float64))

    # Bandpass filter (§2.4)
    filtered = bandpass_filter(x, fs=fs)

    # Phase–memory operator (§3–4)
    result = phase_memory_operator(filtered, fs=fs, **kwargs)
    result['filtered'] = filtered
    result['time'] = np.arange(len(signal)) / fs
    return result


# ── Internal helpers ──────────────────────────────────────────────────────────

def _causal_rolling_mean(x: np.ndarray, M: int) -> np.ndarray:
    """
    Causal rolling mean over M samples.

    Matches the C++ rolling-buffer implementation exactly:
      out[n] = mean(x[max(0, n-M+1) … n])
    """
    out = np.empty_like(x)
    cumsum = np.cumsum(x)
    for n in range(len(x)):
        if n < M:
            out[n] = cumsum[n] / (n + 1)
        else:
            out[n] = (cumsum[n] - cumsum[n - M]) / M
    return out
