# slotmap.py REC.wav: which of the 64 SAI slots reach the SP's main outputs.
#
# The slot probe puts a sine at 300 + 41*k Hz in slot k (line k // 16, word
# k % 16) for six seconds. The recording is all eighteen XR18 inputs; the two
# channels carrying the SP are picked by energy, and each slot's frequency is
# measured on both against the noise floor around it.
import sys, wave
import numpy as np

w = wave.open(sys.argv[1], 'rb')
ch, rate, width = w.getnchannels(), w.getframerate(), w.getsampwidth()
raw = w.readframes(w.getnframes())
x = np.frombuffer(raw, dtype={2: np.int16, 4: np.int32}[width]).reshape(-1, ch).astype(np.float64)
x /= float(2 ** (8 * width - 1))

# The probe's six seconds are where the energy between 280 Hz and 2.9 kHz
# jumps; find them per 100 ms block on the loudest channels.
rms = np.sqrt((x ** 2).mean(axis=0))
pair = [int(c) for c in sys.argv[2].split(",")] if len(sys.argv) > 2 else sorted(np.argsort(rms)[-2:])
print('channels by energy: %s; using %s' % (
    ', '.join('%d:%.4f' % (i, rms[i]) for i in np.argsort(rms)[::-1][:4]), pair))
blk = rate // 10
env = np.array([np.sqrt((x[i:i + blk, pair] ** 2).mean()) for i in range(0, len(x) - blk, blk)])
on = np.where(env > env.max() * 0.3)[0]
if len(on) == 0:
    sys.exit('no probe found in the recording')
start, stop = on[0] * blk + blk * 3, on[-1] * blk - blk * 3
print('probe from %.2f s to %.2f s' % (start / rate, stop / rate))
seg = x[start:stop, :]
n = len(seg)
win = np.hanning(n)
freqs = np.fft.rfftfreq(n, 1 / rate)
for side, c in zip(('left', 'right'), pair):
    spec = np.abs(np.fft.rfft(seg[:, c] * win)) / (win.sum() / 2)
    print('\n%s (channel %d): slots present, level in dBFS and above the floor' % (side, c))
    found = 0
    for k in range(64):
        f = 300 + 41 * k
        i = int(round(f * n / rate))
        peak = spec[i - 2:i + 3].max()
        floor = np.median(np.r_[spec[i - 40:i - 8], spec[i + 8:i + 40]])
        snr = 20 * np.log10(peak / floor)
        if snr > 20:
            found += 1
            print('  line %d word %2d  %4d Hz  %6.1f dBFS  %5.1f dB' %
                  (k // 16, k % 16, f, 20 * np.log10(peak), snr))
    if not found:
        print('  none')
