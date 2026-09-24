#!/usr/bin/env python3
# Writes the screen-reader scenarios, each built from what DRAWS.TXT recorded
# on the unit, with what the reader should say. Run from engine/sim; each
# directory then runs with make sim SIM=sim/NAME.
import os

WHITE, POPUP, BLACK, TITLE = '!', '~', '_', '^'

def main_screen(t, surf, bank_pad):
    return [f'{t} s{surf} 12 2 !P-2', f'{t} s{surf} 34 2 !{bank_pad}',
            f'{t} s{surf} 56 2 !BUS-1', f'{t} s{surf} 84 2 !Fix',
            f'{t} s{surf} 104 2 ! DC ', f'{t} s{surf} 53 14 _9 4 ']

def write(name, draws, expect, say='#mode changed\n'):
    os.makedirs(name, exist_ok=True)
    open(f'{name}/sim_say.txt', 'w').write(say)
    open(f'{name}/sim_draws.txt', 'w').write('\n'.join(draws) + '\n')
    open(f'{name}/sim_expect.txt', 'w').write('\n'.join(expect) + '\n')

# The main screen, then the SD card menu (SHIFT + pad 14): its title on a
# layer of its own, drawn by the title setter; the menu cleared and redrawn
# every 50 ms whether or not anything changed; the highlight moved; the
# EXPORT submenu entered; EXIT back to the main screen on a new surface.
def menu():
    d = ['# The main screen; a pad hit; a bank change.']
    d += main_screen(0, 0, 'A-13')
    d += ['1000 s0 34 2 !A-1', '2000 s0 34 2 !B-1']
    d += ['# The SD card menu, redrawn every 50 ms.']
    top = ['IMPORT from SD-CARD', 'EXPORT to SD-CARD', 'FORMAT SD-CARD', 'CANCEL']
    sub = ['SAMPLE', 'PROJECT', 'MULTIPAD', 'CANCEL']
    d += ['3000 s1 CLEAR', '3000 s1 5 2 ^_IMPORT/EXPORT MENU']
    for t in range(3000, 8000, 50):
        items, sel = (top, 0 if t < 4000 else 1 if t < 5000 else 2 if t < 5500 else 1) \
            if t < 6000 else (sub, 0 if t < 7000 else 1)
        d.append(f'{t} s0 CLEAR')
        for k, text in enumerate(items):
            d.append(f'{t} s0 17 {22 + 8 * k} {"!" if k == sel else ""}{text}')
        if t == 6000:
            d += ['6000 s1 CLEAR', '6000 s2 3 2 ^_EXPORT SAMPLE/PROJ./MULTIPAD']
    d += ['# EXIT to the main screen.', '8000 s0 CLEAR', '8000 s2 CLEAR']
    d += main_screen(8000, 3, 'B-1')
    d += ['9000 VALUE']
    write('menu', d, ['A 13', 'B 1',
                      'IMPORT/EXPORT MENU | IMPORT from SD-CARD',
                      'EXPORT to SD-CARD', 'FORMAT SD-CARD', 'EXPORT to SD-CARD',
                      'EXPORT SAMPLE/PROJ./MULTIPAD | SAMPLE', 'PROJECT', 'B 1'])

# Filter+Drive: three columns of label, value and unit, the effect's name at
# the bottom; CUTOFF turned fast and let go, then RESONANCE.
def params():
    d = main_screen(0, 0, 'A-13')
    d += ['1000 s0 CLEAR',
          '1000 s0 9 3 _CUTOFF', '1000 s0 23 33 _Hz ', '1000 s0 11 18 _637',
          '1000 s0 44 3 _RESONANCE', '1000 s0 73 33 _ ', '1000 s0 54 18 _   1',
          '1000 s0 95 3 _DRIVE', '1000 s0 115 33 _ ', '1000 s0 96 18 _   0',
          '1000 s0 53 47 _Filter+Drive']
    t = 3000
    for v, x in [('827', 11), ('1914', 7), ('4202', 7), ('8308', 7), ('14035', 4),
                 ('5183', 7), ('1194', 7), ('707', 11)]:
        d += [f'{t} s0 FILL 0 18 40 30', f'{t} s0 {x} 18 _{v}']
        t += 50
    t = 6000
    for v in ['  22', '  36', '  58', '  81', ' 100', '  65', '  50']:
        d += [f'{t} s0 FILL 41 18 80 30', f'{t} s0 52 18 _{v}']
        t += 50
    d += ['9000 VALUE']
    write('params', d, ['A 13', 'CUTOFF 637 Hz | RESONANCE 1 | DRIVE 0 | Filter+Drive',
                        'CUTOFF 827 Hz', '707', 'RESONANCE 22', '50'])

# The fixed velocity pop-up over the main screen: it appears, the status bar
# echoes it, it is toggled and the status bar echoes that too.
def popup():
    d = main_screen(0, 0, 'A-13')
    d += ['1000 s1 16 20 ~FIXED VELOCITY', '1000 s1 54 32 ~OFF', '1000 s1 18 84 ~',
          '1400 s0 84 2 !Vel',
          '3000 s1 57 32 ~ON', '3400 s0 84 2 !Fix',
          '5000 s1 54 32 ~OFF', '5400 s0 84 2 !Vel',
          '7000 VALUE']
    write('popup', d, ['A 13', 'FIXED VELOCITY | OFF', 'ON', 'OFF'])

# The effects grid: sixteen cells cut short, the selected one on white, the
# centred title naming it in full; the knob swept through six cells in 50 ms
# steps, then one slow step, then the page turned.
def grid():
    cells = ['Reverb', 'Chorus', 'JUNO ..', 'Flang..', 'Phaser', 'Wah', 'Slicer',
             'Trem..', 'Chro..', 'Hyper..', 'Ring ..', 'Crush..', 'Overd..',
             'Disto..', 'Equal..', 'Comp..']
    full = {'Crush..': 'Crusher', 'Hyper..': 'Hyper-Reso', 'Chro..': 'Chromatic PS',
            'Trem..': 'Tremolo/Pan', 'Equal..': 'Equalizer', 'Disto..': 'Distortion'}
    page2 = ['SX Re..', 'SX De..', 'Cloud..', 'Back ..', 'D.DJFX', 'Filte..', 'Reson..',
             'Sync ..', 'Isola..', 'DJFX ..', '-', '-', '-', '-', '-', '-']
    full2 = {'D.DJFX': 'D.DJFX'}
    pos = [(7 + 30 * (k % 4), 16 + 11 * (k // 4)) for k in range(16)]
    d = []
    def page(t, names, fulls, sel, n):
        d.append(f'{t} s0 FILL 0 5 127 60')
        for k, c in enumerate(names):
            d.append(f'{t} s0 {pos[k][0]} {pos[k][1]} {"!" if k == sel else "_"}{c}')
        name = fulls.get(names[sel], names[sel])
        d.append(f'{t} s0 {64 - 2 * len(name)} 5 _{name}')
        d.append(f'{t} s0 111 5 _{n}/3')
    d += main_screen(0, 0, 'A-13')
    d.append('1000 s0 CLEAR')
    page(1000, cells, full, 6, 2)
    t = 3000
    for sel in [14, 11, 9, 8, 7, 6]:
        page(t, cells, full, sel, 2)
        t += 50
    page(5000, cells, full, 11, 2)
    page(7000, page2, full2, 4, 3)
    d.append('9000 VALUE')
    write('grid', d, ['A 13', 'Slicer', 'Equalizer', 'Crusher', 'Hyper-Reso',
                      'Chromatic PS', 'Tremolo/Pan', 'Slicer', 'Crusher', 'D.DJFX'])

# Sampling: the recording screen with its pop-up asking for a pad, a level
# meter in the status bar changing all the time, then pads hit, each named.
def record():
    d = main_screen(0, 0, 'A-13')
    d += ['1000 s0 CLEAR', '1000 s0 8 2 !BPM  94', '1000 s0 44 2 !M:oo',
          '1000 s0 89 2 !LEVEL:120', '1000 s0 21 16 _R E C',
          '1000 s1 9 4 ~Select PAD\\nfor RECORDING']
    for k, t in enumerate(range(1100, 6000, 100)):
        d.append(f'{t} s0 89 2 !LEVEL:{100 + k % 27}')
    d += ['3000 s0 110 2 !A 1', '4000 s0 110 2 !A 2', '4500 s0 110 2 !B 2',
          '7000 VALUE']
    write('record', d, ['A 13', 'Select PAD for RECORDING', 'A 1', 'A 2', 'B 2'])

os.chdir(os.path.dirname(os.path.abspath(__file__)))
menu()
params()
popup()
grid()
record()
