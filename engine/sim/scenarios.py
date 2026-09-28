#!/usr/bin/env python3
# Writes the screen-reader scenarios, each built from what DRAWS.TXT recorded
# on the unit, with what the reader should say. Run from engine/sim; each
# directory then runs with make sim SIM=sim/NAME.
import os

WHITE, POPUP, BLACK, TITLE = '!', '~', '_', '^'

# The main screen's status fields, each from its own drawing site.
PATTERN, PAD, BUS, FIXVEL = '=80149DC7:', '=80149DD5:', '=80149DE5:', '=80149DF5:'
TOAST = '=8006BECD:'
LABEL = '=801463F7:'                    # a parameter page's labels
FXVALUE = '=80146545:'                  # an effect's values
CELL, FXNAME = '=80145EDF:', '=80146135:'   # the effects grid's cells and title
ROWLABEL, TABS = '=80148AFD:', '=80121751:' # the SYSTEM page's rows and tabs

def main_screen(t, surf, bank_pad, bus='BUS-1', big='9 4 '):
    bx = 56 if bus == 'BUS-1' else 60
    return [f'{t} s{surf} 12 2 !{PATTERN}P-2', f'{t} s{surf} 34 2 !{PAD}{bank_pad}',
            f'{t} s{surf} {bx} 2 !{BUS}{bus}', f'{t} s{surf} 84 2 !{FIXVEL}Fix',
            f'{t} s{surf} 104 2 ! DC ',
            f'{t} s{surf} {24 if big.startswith("-") else 53} 14 _{big}']

# A pad hit on the main screen: the status line and the big field wiped and
# redrawn, the pad's bus changing with it.
def pad_hit(t, surf, bank_pad, bus, big):
    return [f'{t} s{surf} FILL 0 9 128 64', f'{t} s{surf} FILL 0 0 128 8'] + \
        main_screen(t, surf, bank_pad, bus, big)

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
    d += [f'1000 s0 34 2 !{PAD}A-1', '2000 BANK 1', f'2000 s0 34 2 !{PAD}B-1']
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
    write('menu', d, ['top screen', 'B',
                      'IMPORT/EXPORT MENU | IMPORT from SD-CARD',
                      'EXPORT to SD-CARD', 'FORMAT SD-CARD', 'EXPORT to SD-CARD',
                      'EXPORT SAMPLE/project./MULTIPAD | SAMPLE', 'PROJECT', 'top screen'])

# Filter+Drive: three columns of label, value and unit, the effect's name at
# the bottom; CUTOFF turned fast with CTRL 1 and let go, then RESONANCE with
# CTRL 2. An effect is played, so its values are heard where they stop.
def params():
    d = main_screen(0, 0, 'A-13')
    d += ['1000 s0 CLEAR',
          f'1000 s0 9 3 _{LABEL}CUTOFF', '1000 s0 23 33 _Hz ', f'1000 s0 11 18 _{FXVALUE}637',
          f'1000 s0 44 3 _{LABEL}RESONANCE', '1000 s0 73 33 _ ',
          f'1000 s0 54 18 _{FXVALUE}   1',
          f'1000 s0 95 3 _{LABEL}DRIVE', '1000 s0 115 33 _ ', f'1000 s0 96 18 _{FXVALUE}   0',
          '1000 s0 53 47 _Filter+Drive']
    t = 3000
    for k, (v, x) in enumerate([('827', 11), ('1914', 7), ('4202', 7), ('8308', 7),
                                ('14035', 4), ('5183', 7), ('1194', 7), ('707', 11)]):
        d += [f'{t} CTRL 1 {40 + 10 * k}', f'{t} s0 FILL 0 18 40 30', f'{t} s0 {x} 18 _{FXVALUE}{v}']
        t += 50
    t = 6000
    for k, v in enumerate(['  22', '  36', '  58', '  81', ' 100', '  65', '  50']):
        d += [f'{t} CTRL 2 {20 + 10 * k}', f'{t} s0 FILL 41 18 80 30', f'{t} s0 52 18 _{FXVALUE}{v}']
        t += 50
    d += ['9000 VALUE']
    write('params', d, ['top screen', 'Filter+Drive', 'CUTOFF 827 hertz', '707', 'RESONANCE 22', '50'])

# The fixed velocity pop-up over the main screen: it appears, the status bar
# echoes it, it is toggled and the status bar echoes that too.
def popup():
    d = main_screen(0, 0, 'A-13')
    d += ['1000 s1 16 20 ~FIXED VELOCITY', '1000 s1 54 32 ~OFF', '1000 s1 18 84 ~',
          '1400 s0 84 2 !Vel',
          '3000 s1 57 32 ~ON', '3400 s0 84 2 !Fix',
          '5000 s1 54 32 ~OFF', '5400 s0 84 2 !Vel',
          '7000 VALUE']
    write('popup', d, ['top screen', 'FIXED VELOCITY | OFF', 'ON', 'OFF'])

# The effects grid: sixteen cells cut short, the selected one on white, the
# centred title naming it in full; the knob swept through six cells in 100 ms
# steps, each heard (a faster sweep merges steps, skipping names passed over),
# then one slow step, then the page turned.
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
            d.append(f'{t} s0 {pos[k][0]} {pos[k][1]} {"!" if k == sel else "_"}{CELL}{c}')
        name = fulls.get(names[sel], names[sel])
        d.append(f'{t} s0 {64 - 2 * len(name)} 5 _{FXNAME}{name}')
        d.append(f'{t} s0 111 5 _{n}/3')
    d += main_screen(0, 0, 'A-13')
    d.append('1000 s0 CLEAR')
    page(1000, cells, full, 6, 2)
    t = 3000
    for sel in [14, 11, 9, 8, 7, 6]:
        page(t, cells, full, sel, 2)
        t += 100
    page(5000, cells, full, 11, 2)
    page(7000, page2, full2, 4, 3)
    d.append('9000 VALUE')
    write('grid', d, ['top screen', 'Slicer', 'Equalizer', 'Crusher', 'hyper resonator',
                      'chromatic pitch shifter', 'Tremolo/Pan', 'Slicer', 'Crusher', 'D.DJFX'])

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
    write('record', d, ['top screen', 'REC | Select PAD for RECORDING', 'A 1', 'A 2', 'B 2'])

# The main screen as the last run drew it: pad hits wipe and redraw the
# status line, changing the bus and the big field; then a bank change; then
# a STOP message; then two pads whose tempos differ, the big field changing
# its digits in place, slowly and then quickly; then the SD menu, which must
# not read the old message.
def mainpads():
    d = main_screen(0, 0, 'A-13')
    d += pad_hit(1000, 0, 'A-14', 'DRY', '- - - ')
    d += pad_hit(2000, 0, 'A-13', 'BUS-1', '9 4 ')
    d += pad_hit(3000, 0, 'A-10', 'DRY', '- - - ')
    d += ['4000 BANK 1'] + pad_hit(4000, 0, 'B-10', 'DRY', '- - - ')
    d += [f'5000 s1 9 4 ~{TOAST}STOP']
    d += pad_hit(5000, 0, 'B-10', 'DRY', '- - - ')
    d += pad_hit(6000, 0, 'B-11', 'BUS-1', '9 4 ')
    d += pad_hit(7500, 0, 'B-12', 'BUS-1', '9 7 ')
    for k, t in enumerate(range(9000, 10000, 250)):
        d += pad_hit(t, 0, 'B-11' if k % 2 else 'B-12', 'BUS-1', '9 4 ' if k % 2 else '9 7 ')
    d += ['13000 s0 CLEAR', '13000 s2 5 2 ^_IMPORT/EXPORT MENU',
          '13000 s0 17 22 !IMPORT from SD-CARD', '13000 s0 17 30 EXPORT to SD-CARD',
          '14000 VALUE']
    write('mainpads', d, ['top screen', 'B', 'STOP',
                          'IMPORT/EXPORT MENU | IMPORT from SD-CARD'])

# Entering the EXPORT submenu as the unit draws it: the focused item first,
# the title from its own site a moment later, the other items after that.
def submenu():
    d = main_screen(0, 0, 'A-13')
    d += ['1000 s0 CLEAR', '1000 s0 17 22 !SAMPLE',
          '1010 s2 3 2 _=80151801:EXPORT SAMPLE/PROJ./MULTIPAD',
          '1100 s0 17 30 PROJECT', '1150 s0 17 38 MULTIPAD', '1200 s0 17 46 CANCEL',
          '2000 s0 17 22 SAMPLE', '2000 s0 17 30 !PROJECT',
          '3000 VALUE']
    write('submenu', d, ['top screen', 'EXPORT SAMPLE/project./MULTIPAD | SAMPLE', 'PROJECT'])

# The recording screen drawing its meter's scale after it appears, and the
# pad named without a dash; the doubled title of the pad link groups page.
def recscale():
    d = main_screen(0, 0, 'A-13')
    d += ['1000 s0 CLEAR', '1000 s0 21 16 _R E C',
          '1000 s1 9 4 ~=8006BECD:Select PAD\\nfor RECORDING',
          '1300 s0 121 14 _L', '1300 s0 125 14 _R', '1300 s0 118 24 _0',
          '1300 s0 114 40 _-6', '1300 s0 114 59 _dB',
          '3000 s0 110 2 !=80177A75:A14',
          '5000 s0 CLEAR', '5000 s3 14 3 ~=8013FE3D:PAD LINK GROUPS',
          '5000 s3 13 2 ~=80195269:PAD LINK GROUPS', '5000 s0 20 30 !GROUP 1',
          '7000 VALUE']
    write('recscale', d, ['top screen', 'REC | Select PAD for RECORDING', 'A 14',
                          'PAD LINK GROUPS | GROUP 1'])

# The UTILITY menu, its title on a layer never wiped, its icons on another
# surface from its labels, the main screen still under it; then the SYSTEM page:
# rows of a setting's name and its value, a tab strip, a page count. The
# menu's title must not be read again on the SYSTEM page; each value is read
# with the name on its own row; a value turned is read alone.
def system():
    d = main_screen(0, 0, 'A-13')
    items = [(20, 30, 'SYSTEM'), (51, 30, 'PAD SET'), (83, 30, 'EFX SET'),
             (20, 56, 'IMPORT'), (53, 56, 'BACKUP'), (83, 56, 'FACTORY')]
    icons = [(24, 11), (56, 11), (88, 11), (24, 37), (56, 37), (88, 37)]
    def utility(t, sel):
        out = [f'{t} s2 5 2 ^_UTILITY MENU']
        for k, (x, y, name) in enumerate(items):
            out.append(f'{t} s3 ICON {icons[k][0]} {icons[k][1]} {int(k == sel)} '
                       f'G:/xpage/work1/pages/icon{k}_pos_30')
            out.append(f'{t} s2 {x} {y} ={0x8013E92B + 0x4E * k:08X}:{name}')
        return out
    # The main screen stays on its layer under the menu, as on the unit.
    d += utility(1000, 0)
    # the knob: the selection moves to PAD SET, then EFX SET, redrawing all
    d += utility(1600, 1) + utility(2200, 2)
    rows = [('Edit Knob Mode', 93, 'Direct'), ('EFX Knob Mode', 93, 'Direct'),
            ('Load Project', 100, 'Last'), ('Sub Pad Mode', 93, 'Retrig'),
            ('Auto Trig Level', 102, '   5'), ('Scrn Saver Time', 97, '1 min')]
    # Each row as FUN_801489B8 draws it: the row, selected or not, then its
    # name and its value.
    def page(t, rows, sel):
        out = [f'{t} s0 FILL 0 9 127 60']
        for k, (name, x, value) in enumerate(rows):
            y = 13 + 8 * k
            out += [f'{t} s0 ROW {y} {int(k == sel)}',
                    f'{t} s0 6 {y} _{ROWLABEL}{name}', f'{t} s0 {x} {y} {value}']
        return out
    d += ['3000 PAGE 11', '3000 s0 CLEAR'] + page(3000, rows, 0)
    d += ['3000 s0 103 2  1/ 5']
    for k, t in enumerate(['GENERAL', 'CLICK', 'MIDI', 'GAIN', 'VERSION']):
        d.append(f'3000 s0 {5 + 25 * k} 2 {TABS}{t}')
    # The knob moves the cursor down two rows; the list does not scroll.
    d += page(5000, rows, 1) + page(6000, rows, 2)
    # CTRL 3: the next tab, a new page of settings and the page count.
    click = [('Output Assign', 109, 'ON'), ('Click Level', 115, '1'),
             ('Metronome:REC', 106, 'OFF'), ('Metronome:PTN', 109, 'ON')]
    d += page(8000, click, 0) + ['8000 s0 103 2  2/ 5']
    # A value turned on the selected row.
    d += ['9000 s0 FILL 100 12 127 19', '9000 s0 ROW 13 1', '9000 s0 109 13 OFF']
    d += ['11000 VALUE']
    write('system', d, ['top screen',
                        'UTILITY MENU | SYSTEM', 'PAD SET', 'effects SET',
                        'GENERAL | Edit Knob Mode Direct',
                        'effects Knob Mode Direct', 'Load Project Last',
                        'CLICK | Output Assign ON', 'OFF'])

# Leaving a menu for the main screen: its factory is called and it redraws
# everything unchanged on its own layer, which only the page event shows.
def pages():
    d = main_screen(0, 0, 'A-13')
    d += ['1000 PAGE 30', '1000 s1 5 2 ^_IMPORT/EXPORT MENU', '1000 s2 17 22 !IMPORT from SD-CARD',
          '1000 s2 17 30 EXPORT to SD-CARD']
    d += ['3000 PAGE 0'] + main_screen(3000, 0, 'A-13')
    d += ['5000 VALUE']
    write('pages', d, ['top screen', 'IMPORT/EXPORT MENU | IMPORT from SD-CARD', 'top screen'])

# FORMAT in the SD card menu, as the unit draws it: the menu redrawn, then a
# dialog on a surface of its own, its text on the pop-up background and its
# buttons below, CANCEL selected; OK, CANCEL again, then closing it back to
# the menu with its own CANCEL selected.
def dialog():
    top = ['IMPORT from SD-CARD', 'EXPORT to SD-CARD', 'FORMAT SD-CARD', 'CANCEL']
    def lst(t, sel):
        out = [f'{t} s0 FILL 14 20 113 52']
        for k, text in enumerate(top):
            out.append(f'{t} s0 17 {22 + 8 * k} {"!" if k == sel else ""}{text}')
        return out
    d = main_screen(0, 0, 'A-13')
    d += ['1000 s0 CLEAR', '1000 s1 5 2 ^_IMPORT/EXPORT MENU'] + lst(1000, 2)
    d += ['3000 s1 5 2 ^_IMPORT/EXPORT MENU'] + lst(3000, 2)
    d += ['3010 s3 13 10 ~Format \\nSD Card', '3010 s3 13 30 ~Are you sure?',
          '3010 s3 86 47 OK', '3010 s3 16 47 !CANCEL']
    d += ['4000 s3 86 47 !OK', '4000 s3 16 47 CANCEL',
          '5000 s3 16 47 !CANCEL', '5000 s3 86 47 OK']
    d += ['6000 s3 CLEAR'] + lst(6000, 3)
    d += ['8000 VALUE']
    write('dialog', d, ['top screen', 'IMPORT/EXPORT MENU | FORMAT SD-CARD',
                        'Format SD Card | Are you sure? | CANCEL', 'OK', 'CANCEL',
                        'CANCEL'])

# The SD card menu drawn over the main screen without wiping all of it, the
# big BPM left just above the list; into the EXPORT submenu and back out,
# the list's items all changing back at once with no title drawn.
def sdreturn():
    top = ['IMPORT from SD-CARD', 'EXPORT to SD-CARD', 'FORMAT SD-CARD', 'CANCEL']
    sub = ['SAMPLE', 'PROJECT', 'MULTIPAD', 'CANCEL']
    SEL, UNSEL = '=8016F55B:', '=8016F505:'
    def lst(t, items, sel):
        out = [f'{t} s0 FILL 14 20 113 52']
        for k, text in enumerate(items):
            out.append(f'{t} s0 17 {22 + 8 * k} {"!" + SEL if k == sel else UNSEL}{text}')
        return out
    d = main_screen(0, 0, 'A-13')
    d += ['1000 s0 FILL 0 0 128 8', '1000 s1 5 2 ^_IMPORT/EXPORT MENU'] + lst(1000, top, 0)
    d += lst(2000, top, 1)
    d += ['3000 s2 3 2 _=80151801:EXPORT SAMPLE/PROJ./MULTIPAD'] + lst(3000, sub, 0)
    d += lst(4000, sub, 3)
    d += lst(5000, top, 0)
    d += ['7000 VALUE']
    write('sdreturn', d, ['top screen', 'IMPORT/EXPORT MENU | IMPORT from SD-CARD',
                          'EXPORT to SD-CARD', 'EXPORT SAMPLE/project./MULTIPAD | SAMPLE',
                          'CANCEL', 'IMPORT from SD-CARD'])

# The speech settings menu: SHIFT + EXIT on the main screen, the VALUE knob
# to a setting, a press, the knob to change it, a press back, EXIT to save
# and close, which announces the main screen again. SHIFT and a pad go
# through to the page; the menu's own keys do not.
def settings():
    d = main_screen(0, 0, 'A-13')
    d += ['1000 KEY DOWN 2a', '1050 KEY DOWN 22', '1150 KEY UP 22', '1200 KEY UP 2a',
          '2500 KNOB 0 1',
          '4000 KEY DOWN 31', '4100 KEY UP 31',
          '5000 KNOB 0 1', '6000 KNOB 0 1',
          '7000 KEY DOWN 31', '7100 KEY UP 31',
          '8000 KEY DOWN 0c', '8100 KEY UP 0c',
          '9000 KEY DOWN 22', '9100 KEY UP 22',
          '11000 VALUE']
    write('settings', d, ['top screen', 'Speech settings | Speed 50', 'Pitch 65', '65', '70',
                          '75', 'Pitch', 'Settings saved | top screen'])

# The BPM screen (SHIFT + pad 11), page 81, as FUN_80104F68 draws it, found
# in the firmware rather than a log: the heading, PROJECT and the bank with
# an outline round the one CTRL 1 has chosen, the chosen tempo on white, a
# hint. Every change clears and redraws it all. VALUE turned; CTRL 1 to the
# bank; back; VALUE again, to the bank's tempo; CTRL 1 to the bank, whose
# tempo is now the same; the button that gives the knobs back to the
# effects, held, which swaps the heading for knob legends that are not
# said, and let go; EXIT to the main screen, on a surface of its own, while
# the BPM screen redraws itself every 40 ms, as on the unit.
def bpm():
    def screen(t, project, tempo, head=None):
        box = '10 17 55 37' if project else '74 17 118 37'
        out = [f'{t} s0 CLEAR', f'{t} s0 FILL 0 0 128 8']
        out += head or [f'{t} s0 10 2 =80105033:C1:TEMPO SEL']
        out += [f'{t} s0 BOX {box} =80105153',
                f'{t} s0 11 23 _=8010515F:PROJECT', f'{t} s0 78 23 _=8010516B:BANK A',
                f'{t} s0 FILL 10 42 125 64', f'{t} s0 FILL 46 40 82 50',
                f'{t} s0 46 42 !=801051B3:{tempo}',
                f'{t} s0 22 58 _=801051D3:SUB PAD to TAP']
        return out
    d = main_screen(0, 1, 'A-13')
    d += ['1000 PAGE 81'] + screen(1000, True, '120.00')
    d += screen(3000, True, '121.00') + screen(3500, True, '121.00')
    d += screen(5000, False, '118.50') + screen(5500, False, '118.50')
    d += screen(7000, True, '121.00')
    d += screen(9000, True, '118.50')
    d += screen(11000, False, '118.50')
    d += screen(13000, False, '118.50', ['13000 s0 10 2 =8010500D:C1:CTR1',
                                         '13000 s0 48 2 =80105019:C2:CTRL2',
                                         '13000 s0 85 2 C3:CTRL3'])
    for t in range(15000, 17000, 40):
        d += screen(t, False, '118.50')
    d += ['17000 PAGE 84'] + main_screen(17000, 1, 'A-13')
    d += ['19000 VALUE']
    write('bpm', d, ['top screen', 'control 1: TEMPO select | PROJECT | 120.00', '121.00',
                     'BANK A | 118.50', 'PROJECT | 121.00', '118.50', 'BANK A | 118.50',
                     'control 1: TEMPO select | BANK A | 118.50', 'top screen'])

# The SYSTEM page's tabs as the unit drew them (runs/52): the strip shows
# four tabs and scrolls, so the page count cannot name a tab by position;
# the tab widget says which is current. On GAIN, VALUE is turned down the
# list and on past its end, redrawing the rows unchanged; then the VERSION
# tab, which only draws its version line over them.
def tabs():
    names = ['GENERAL', 'CLICK', 'MIDI', 'GAIN', 'VERSION']
    def strip(t, cur, first, xs):
        out = [f'{t} s0 FILL 0 0 127 8', f'{t} s0 103 2  {cur + 1}/ 5']
        out += [f'{t} s0 {x} 2 {TABS}{names[first + k]}' for k, x in enumerate(xs)]
        return out + [f'{t} s0 TAB {cur} ' + ','.join(names)]
    def rows(t, rows, sel):
        out = [f'{t} s0 FILL 0 9 119 60']
        for k, (name, x, value) in enumerate(rows):
            y = 13 + 8 * k
            out += [f'{t} s0 ROW {y} {int(k == sel)}',
                    f'{t} s0 6 {y} _{ROWLABEL}{name}', f'{t} s0 {x} {y} {value}']
        return out
    general = [('Edit Knob Mode', 93, 'Direct'), ('EFX Knob Mode', 93, 'Direct'),
               ('Load Project', 100, 'Last'), ('Sub Pad Mode', 93, 'Retrig')]
    gain = [('Attenuator', 104, 'OFF'), ('Noise Gate', 96, '  OFF'), ('Line Out', 96, '  0dB'),
            ('Phones Out', 96, '  0dB'), ('USB Out', 96, '  0dB'), ('Anti Feedback', 104, 'OFF')]
    d = main_screen(0, 0, 'A-13')
    d += ['1000 PAGE 79', '1000 s0 CLEAR'] + rows(1000, general, 0)
    d += strip(1000, 0, 0, [5, 48, 76, 102, 127])
    d += rows(3000, gain, 0) + strip(3000, 3, 1, [15, 43, 69, 96])
    for k, t in enumerate(range(5000, 7000, 400)):
        d += rows(t, gain, k + 1)
    d += rows(7500, gain, 5) + rows(7900, gain, 5)
    d += ['9000 s0 10 20 _=8014B161:Version: 5.52'] + strip(9000, 4, 3, [15, 40])
    d += ['11000 VALUE']
    write('tabs', d, ['top screen', 'GENERAL | Edit Knob Mode Direct', 'GAIN | Attenuator OFF',
                      'Noise Gate OFF', 'Line Out 0 decibels', 'Phones Out 0 decibels',
                      'USB Out 0 decibels',
                      'Anti Feedback OFF', 'VERSION | Version: 5.52'])

# A settings page laid out as the pitch and speed screen in runs/31, three
# columns of label, value and unit like an effect's but drawn by other code,
# with a meter in the status bar changing on its own all the while, drawn
# from the sites runs/58 recorded. Arriving says nothing of the knobs. SPEED
# turned with CTRL 1 and PITCH with VALUE, a step every frame: each step is
# said as it comes, cutting off the one before. The meter is never said,
# though it changes in the same frames as the turned values.
def turned():
    d = main_screen(0, 0, 'A-13')
    L, V = '=801709AB:', '=80170A15:'
    d += ['1000 s0 CLEAR', '1000 s0 89 2 !LEVEL:120',
          f'1000 s0 11 3 _{L}SPEED', f'1000 s0 12 18 _{V}1.00', '1000 s0 15 33 _ ',
          f'1000 s0 55 3 _{L}PITCH', f'1000 s0 56 18 _{V}  0', '1000 s0 56 33 _SEMI',
          f'1000 s0 93 3 _{L}VOLUME', f'1000 s0 97 18 _{V}127', '1000 s0 97 33 _ ']
    for k, t in enumerate(range(1100, 8000, 100)):
        d.append(f'{t} s0 89 2 !LEVEL:{100 + k % 27}')
    for k, v in enumerate(['1.10', '1.20', '1.30', '1.40']):
        t = 3000 + 53 * k
        d += [f'{t} CTRL 1 {70 + 3 * k}', f'{t} s0 FILL 0 18 40 30', f'{t} s0 12 18 _{V}{v}']
    for k, v in enumerate(['  1', '  2', '  3']):
        t = 5000 + 53 * k
        d += [f'{t} KNOB 0 1', f'{t} s0 FILL 41 18 80 30', f'{t} s0 56 18 _{V}{v}']
    d += ['9000 VALUE']
    write('turned', d, ['top screen', 'SPEED 1.10', '1.20', '1.30', '1.40', 'PITCH 1 semitones', '2',
                        '3'])

# The pattern recording settings, page 62, as runs/57 drew them: three
# columns, each a name at the top and a line at the bottom, BPM's tempo in
# the middle over its unit, BPM again; below, on a surface of its own, a
# hint and the quantise grid on white, the mode and the pattern. Entering
# reads what the knobs do not set; CTRL 1 turns the tempo, CTRL 2 the
# length and CTRL 3 the strength, each named on its first step only.
def patrec():
    NAME, LOW, TEMPO = '=801709AB:', '=801709EF:', '=80170A15:'
    d = main_screen(0, 0, 'A-13')
    d += ['1000 PAGE 62', '1000 s1 83 46 !=8016B061:GRID 16',
          '1000 s1 5 45 !=8016BAB7:SHIFT:OTHER', '1000 s1 83 54 _=8016BB11:A-1',
          '1000 s1 5 54 _=8016BB45:MODE : Real-Time', '1000 s1 65 46 _=8016BB55:QTZ:',
          '1000 s0 CLEAR',
          f'1000 s0 15 4 _{NAME}BPM', f'1000 s0 15 34 _{LOW}BPM', f'1000 s0 9 19 _{TEMPO}90.0',
          f'1000 s0 53 4 _{NAME}LENGTH', f'1000 s0 53 34 _{LOW}2 Bars',
          f'1000 s0 86 4 _{NAME} STRENGTH ', f'1000 s0 102 34 _{LOW}0%']
    for k, (x, v) in enumerate([(9, '93.0'), (9, '97.0'), (6, '105.0'), (6, '111.0')]):
        t = 3000 + 50 * k
        d += [f'{t} CTRL 1 {43 + 4 * k}', f'{t} s0 {x} 19 _{TEMPO}{v}']
    for k, v in enumerate(['38 Bars', '41 Bars']):
        t = 5000 + 50 * k
        d += [f'{t} CTRL 2 {75 + 4 * k}', f'{t} s0 51 34 _{LOW}{v}']
    for k, v in enumerate(['43%', '49%']):
        t = 7000 + 50 * k
        d += [f'{t} CTRL 3 {53 + 4 * k}', f'{t} s0 100 34 _{LOW}{v}']
    d += ['9000 VALUE']
    write('patrec', d, ['top screen', 'quantize: GRID 16 | MODE : Real-Time | A 1',
                        'BPM 93.0', '97.0', '105.0', '111.0', 'LENGTH 38 Bars', '41 Bars',
                        'STRENGTH 43%', '49%'])

# Exporting samples, page 85, as FUN_801518B0 draws it once SAMPLE is
# chosen, found in the firmware rather than a log: its heading, PLEASE
# SELECT SMPL and ENTER:EX. The pads show what is chosen only by their
# lights, and every press redraws the page unchanged. Pads chosen and let
# go in two banks; then choosing a project, then a pattern, the same one
# twice; then a mode that chooses nothing.
def export():
    def status(t):
        return [f'{t} s0 FILL 0 0 127 63', f'{t} s0 3 2 _=80151927:EXPORT SAMPLE/PROJ./MULTIPAD',
                f'{t} s0 19 20 _=80151995:PLEASE SELECT SMPL', f'{t} s0 73 48 =800EE779:ENTER:EX']
    d = main_screen(0, 0, 'A-13')
    d += ['1000 PAGE 85'] + status(1000)
    for t, pad in [(3000, 5), (4000, 5), (5500, 1), (6500, 16)]:
        if t == 5500:
            d.append('5400 BANK 1')
        d += [f'{t} PAD {pad}'] + status(t + 40)
    d += ['8000 MODE 1', '8500 PAD 3', '9500 MODE 2', '9600 BANK 0', '10000 PAD 4',
          '11000 PAD 4', '12000 MODE 3', '12500 PAD 2', '14000 VALUE']
    write('export', d, ['top screen', 'EXPORT SAMPLE/project./MULTIPAD | PLEASE SELECT sample',
                        'A 5 selected', 'A 5 deselected', 'B', 'B 1 selected',
                        'B 16 selected', 'project 03 selected', 'A', 'pattern A 4'])

# Deleting pads, page 67 in its mode 1, as runs/41 drew it: SELECT PAD in
# the status bar and the big DEL; each pad pressed redraws the count, TOT
# SELECTED PADS, a frame later, and SELECT PAD again at none. The pad is
# said, not the count.
def delete():
    d = main_screen(0, 0, 'A-13')
    d += ['1000 PAGE 67', '1000 MODE 1', '1000 s0 FILL 0 0 127 63',
          '1000 s0 45 4 !=8015604B:SELECT PAD', '1000 s0 28 18 _=800EE779:D E L']
    for t, pad, count in [(3000, 1, 1), (4000, 2, 2), (5000, 1, 1), (6000, 2, 0)]:
        text = f'28 4 !=8015604B:TOT SELECTED PADS:{count}' if count else '45 4 !=8015604B:SELECT PAD'
        d += [f'{t} PAD {pad}', f'{t + 48} s0 {text}']
    d += ['8000 VALUE']
    write('delete', d, ['top screen', 'delete | SELECT PAD', 'A 1 selected', 'A 2 selected',
                        'A 1 deselected', 'A 2 deselected'])

# The pitch and speed screen, page 83, as runs/58 drew it on arriving: three
# columns like the pattern settings', SPEED 100.0% over the pad's BPM:90.00,
# PITCH 0.00 over VINYL, VOLUME 127; below, SHIFT:FINE on white, BPM SET
# with its value MANU on white and the tempo, the pad and the fixed velocity
# indicator. It has no title, so its button names it; neither the hint nor
# MANU is the focus, the knobs' columns wait to be turned, and nor is the
# indicator read.
def padset():
    NAME, LOW, MID = '=801709AB:', '=801709EF:', '=80170A15:'
    d = main_screen(0, 0, 'D-1')
    d += ['1000 PAGE 83', '1000 s0 CLEAR',
          '1000 s1 7 45 !=800F7EE1:SHIFT:FINE', '1000 s1 64 46 _=800F7683:BPM SET',
          '1000 s1 64 55 _=800F769F:VALUE',
          f'1000 s0 93 3 _{NAME}VOLUME', f'1000 s0 96 18 _{MID}127',
          f'1000 s0 11 3 _{NAME}SPEED', f'1000 s0 4 33 _{LOW}BPM:90.00', f'1000 s0 0 18 _{MID}100.0%',
          f'1000 s0 55 3 _{NAME}PITCH', f'1000 s0 55 33 _{LOW}VINYL', f'1000 s0 52 18 _{MID}0.00',
          '1000 s2 101 46 !=80145629:MANU', '1000 s2 101 55 _=8014572B:90.00',
          '1000 s2 7 55 _=80145351:   D-1', '1000 s2 44 55 _=800EE779:Vel']
    # A pad plays the next sample, and its strip and knobs redraw for it,
    # none of it said.
    d += ['3000 PAD 2', '3030 s2 CLEAR', '3030 s2 101 46 !=80145629:MANU',
          '3030 s2 101 55 _=8014572B:120.00', '3030 s2 7 55 _=80145351:   D-2',
          '3030 s2 44 55 _=800EE779:Vel', f'3030 s0 0 18 _{MID}110.0%', f'3030 s0 96 18 _{MID} 90',
          '5000 VALUE']
    write('padset', d, ['top screen', 'PITCH/SPEED | BPM SET | manual | D 1 | 90.00'])

# Sample edit, page 90, as runs/58 drew it: the knobs' legends along the
# top, the pad, the MARK button's legend, MENU and the zoom along the
# bottom, S and E on the waveform. Arriving says its button's name,
# START/END, and the pad; the zoom the page
# sets itself straight after arriving is not said, nor the markers as CTRL 1
# and CTRL 3 move them; VALUE zooming is said, and so is the MARK button's
# legend moving on to the end point.
def sampedit():
    d = main_screen(0, 0, 'D-1')
    d += ['1000 KEY DOWN 17', '1000 PAGE 90', '1000 s0 CLEAR', '1000 s1 CLEAR',
          '1000 s0 8 2 _=80107AA5:C1:START', '1000 s0 95 2 _=80107AC7:C3:END',
          '1000 s1 46 2 _=80107C07:C2:OFF', '1000 s1 58 53 _=80107C83:D-1',
          '1000 s1 76 53 _=80107CF5:M:[S]', '1000 s1 103 53 !=80107D6D:MENU',
          '1000 s1 4 53 !=801078A9:ENC:ZOOM(0x)',
          '1057 s1 4 53 !=801078A9:ENC:ZOOM(1x)',
          '1067 s0 2 42 _=8014BBB3:S', '1067 s0 122 42 _=8014BCA5:E']
    for k, x in enumerate([5, 9, 14, 20]):
        t = 3000 + 53 * k
        d += [f'{t} CTRL 1 {10 + 5 * k}', f'{t} s0 FILL 0 40 127 50',
              f'{t} s0 {x} 42 _=8014BBB3:S', f'{t} s0 122 42 _=8014BCA5:E']
    for k, z in enumerate(['2x', '3x']):
        t = 5000 + 400 * k
        d += [f'{t} KNOB 0 1', f'{t} s1 4 53 !=801078A9:ENC:ZOOM({z})']
    d += ['7000 KEY DOWN 19', '7000 s1 76 53 _=80107CF5:M:[E]']
    # Pads choose the sample to edit by playing it, and are not said.
    d += ['8000 PAD 2', '8030 s1 58 53 _=80107C83:D-2', '8600 PAD 5',
          '8630 s1 58 53 _=80107C83:D-5', '10000 VALUE']
    write('sampedit', d, ['top screen', 'START/END | D 1', 'VALUE: ZOOM(2x)', 'VALUE: ZOOM(3x)',
                          'mark: [end]'])

# Auto mark, page 88, as runs/58 drew it: the knobs' legends, the pad, MENU
# and the zoom, the marks along the waveform, the selected one on white.
# Its AUTO MARK menu is drawn every frame and at once drawn over by the
# Auto Mark Mode page's own settings, which must neither be spoken nor
# fill the log; the pads step through the marks, which says nothing; the
# page's "Current Marks will be lost." dialog, shown, cancelled and shown
# again on the layer it used before, is read both times.
def automark():
    MENU = [('AUTO MARK', '!=8016F55B:'), ('ASSIGN TO PAD', '=8016F505:'),
            ('DELETE ALL MARKS', '=8016F505:'), ('CANCEL', '=8016F505:')]
    def marks(t, sel):
        out = [f'{t} s0 FILL 0 40 127 50']
        for k, x in enumerate([7, 16, 25, 34, 43]):
            out.append(f'{t} s0 {x} 42 ' + ('!=8014B237:M' if k == sel else '_=8014B4E1:M'))
        return out
    def mode_page(t):
        return [f'{t} s2 FILL 0 8 127 60', f'{t} s2 35 10 =8016BC8D:[C2]',
                f'{t} s2 13 19 =8016BCA1:Auto Mark Mode', f'{t} s2 98 10 =8016BCB5:[C3]',
                f'{t} s2 95 19 =8016BCC9:Param', f'{t} s2 4 33 !=8016BE39:TIME DIVISION',
                f'{t} s2 103 33 =8016BE7D:8', f'{t} s2 85 54 !=8016BD2D:ENTER:EXE',
                f'{t} s2 8 54 !=800EE779:EXIT:BACK']
    def dialog(t):
        return [f'{t} s3 13 10 ~=8001F4BD:Current Marks will be lost.',
                f'{t} s3 13 32 ~=8001F6B5:Continue?', f'{t} s3 86 47 =800907B1:OK',
                f'{t} s3 16 47 !=800907B1:CANCEL']
    d = main_screen(0, 0, 'D-1')
    d += ['1000 KEY DOWN 1b', '1000 PAGE 88', '1000 s0 CLEAR', '1000 s1 CLEAR',
          '1000 s0 4 2 _=80106265:C1:MOV(C)', '1000 s0 45 2 _=80106289:C2:MOV(M)',
          '1000 s0 87 2 _=801062AD:C3:SEL(M)', '1000 s1 58 53 _=80106431:D-1',
          '1000 s1 102 53 !=8010647D:MENU', '1000 s1 4 53 !=80105B41:ENC:ZOOM(1x)']
    d += marks(1000, 0)
    d += ['3000 KEY DOWN 31']
    for t in range(3000, 5000, 40):
        d += [f'{t} s2 {28} {20 + 10 * k} {flag}{text}' for k, (text, flag) in enumerate(MENU)]
        d += mode_page(t + 3)
    for k, t in enumerate([6000, 6600, 7200]):
        d += [f'{t} PAD {k + 2}'] + marks(t + 30, k + 1)
    d += ['8000 KEY DOWN 31'] + dialog(8000)
    d += ['9000 KEY DOWN 22', '9000 s3 CLEAR'] + mode_page(9000)
    d += ['14000 KEY DOWN 31'] + dialog(14000)
    d += ['16000 VALUE']
    write('automark', d, ['top screen', 'CHOP | D 1', 'TIME DIVISION',
                          'Current Marks will be lost. | Continue? | CANCEL',
                          'Current Marks will be lost. | Continue? | CANCEL'])

# An effect over the main screen as runs/58 drew it: TimeCtrlDly's three
# columns on the main screen's own surface, which redraws its status line
# and its big tempo in the same frames, the tempo sitting between FEEDBACK
# and its value. Arriving says the effect's name alone, the knobs' values
# waiting to be turned; FEEDBACK turned with CTRL 2, named; the display timing out back
# to the main screen, which says nothing, the bank and pad being the same;
# then a bank change, which is said.
def fxreturn():
    NAME, VAL, UNIT = '=801463F7:', '=80146545:', '=800EE779:'
    def fx(t, feedback):
        return main_screen(t, 0, 'A-13', big='9 7 ') + [
            f'{t} s0 12 3 _{NAME}TIME', f'{t} s0 13 33 _{UNIT}msec ', f'{t} s0 11 18 _{VAL}100',
            f'{t} s0 47 3 _{NAME}FEEDBACK', f'{t} s0 69 33 _{UNIT}% ',
            f'{t} s0 54 18 _{VAL}{feedback}',
            f'{t} s0 95 3 _{NAME}LEVEL', f'{t} s0 115 33 _{UNIT} ', f'{t} s0 93 18 _{VAL} 100',
            f'{t} s0 57 47 _{UNIT}TimeCtrlDly']
    d = main_screen(0, 0, 'A-13', big='9 7 ')
    d += ['1000 CTRL 1 64', '1000 s0 FILL 0 0 127 63'] + fx(1000, ' 49')
    for k, v in enumerate([' 52', ' 55', ' 58']):
        t = 3000 + 53 * k
        d += [f'{t} CTRL 2 {60 + 3 * k}', f'{t} s0 FILL 41 18 80 30', f'{t} s0 54 18 _{VAL}{v}']
    d += ['6000 s0 FILL 0 0 127 63'] + main_screen(6000, 0, 'A-13', big='9 7 ')
    d += ['8000 KEY DOWN 26', '8000 BANK 1', '8000 s0 34 2 !=80149DD5:B-13']
    # CTRL 1 touched again, as runs/59 drew it: the display drawn back over
    # the main screen without wiping it, and then the value moving.
    d += ['10000 CTRL 1 70'] + fx(10000, ' 58')[6:]
    d += ['10053 CTRL 1 72', '10053 s0 FILL 0 18 40 30', f'10053 s0 11 18 _{VAL}120',
          '12000 VALUE']
    write('fxreturn', d, ['top screen', 'time control delay', 'FEEDBACK 52', '58', 'B',
                          'TIME 120 milliseconds'])

# The effects grid on a page showing none of its effects selected, as
# runs/59 drew it: MFX LIST 1-16 in the title's place, the page count, and
# sixteen names cut short. Arriving says the heading and the page; the
# names wait for VALUE to select one.
def fxgrid():
    CELL = '=80145EDF:'
    cells = ['Scatt..', 'Down..', 'Ha-Dou', 'Ko-Da..', 'Zan-Z..', 'To-Gu..', 'SBF',
             'Stopp..', 'Tape ..', 'Time..', 'Super..', 'WrmS..', '303 V..', '404 V..',
             'Casse..', 'Lo-fi']
    d = main_screen(0, 0, 'D-1')
    d += ['1000 KEY DOWN 30', '1000 s0 FILL 0 8 127 63']
    d += [f'1000 s0 {7 + 30 * (k % 4)} {16 + 11 * (k // 4)} _{CELL}{c}'
          for k, c in enumerate(cells)]
    d += ['1000 s0 30 5 _=80146175: MFX LIST 1-16 ', '1000 s0 111 5 _=800EE779:1/3',
          '3000 VALUE']
    write('fxgrid', d, ['top screen', 'MFX LIST 1-16 | 1 of 3'])

# Exporting samples as runs/58 drew it: the heading on a layer of its own,
# the SAMPLE, PROJECT, MULTIPAD, CANCEL list on another; pressing VALUE on
# SAMPLE leaves the list where it is and draws PLEASE SELECT SMPL and
# ENTER:EXE on the heading's layer, and the prompt is said; then a pad.
def export2():
    SEL, UNSEL = '!=8016F55B:', '=8016F505:'
    d = main_screen(0, 0, 'A-13')
    d += ['1000 KEY DOWN 31', '1000 PAGE 85', '1000 s1 3 2 _=80151801:EXPORT SAMPLE/PROJ./MULTIPAD',
          f'1000 s2 17 22 {SEL}SAMPLE', f'1000 s2 17 30 {UNSEL}PROJECT',
          f'1000 s2 17 38 {UNSEL}MULTIPAD', f'1000 s2 17 46 {UNSEL}CANCEL',
          '3000 KEY DOWN 31', '3000 s1 26 20 _=80151995:PLEASE SELECT\\nSMPL',
          '3000 s1 61 48 !=800EE779:ENTER:EXE',
          '5000 PAD 10', '7000 VALUE']
    write('export2', d, ['top screen', 'EXPORT SAMPLE/project./MULTIPAD | SAMPLE',
                         'PLEASE SELECT sample', 'A 10 selected'])

# The main screen's pad field showing EXT for EXT SOURCE, said in full; the
# power source changing at the end of the status bar, which says nothing,
# until the batteries run low.
def extpower():
    d = main_screen(0, 0, 'A-13')
    d += ['2000 KEY DOWN 12', '2000 s0 36 2 !=80149DD5:EXT',
          '4000 s0 104 2 !=800EE779: BAT ', '6000 s0 104 2 !=800EE779: LOW!', '8000 VALUE']
    write('extpower', d, ['top screen', 'external', 'LOW!'])

# COPY on the pad operations page, 67 in its mode 0, as runs/60 drew it:
# COPY PAD in the status bar, the project it copies into on white, the
# source and destination below. Arriving says the heading and the project;
# a pad names the source; a bank key, which the screen does not show, is
# said; a pad in the new bank names the destination.
def copy():
    d = main_screen(0, 0, 'A-13')
    d += ['1000 KEY DOWN 23', '1000 PAGE 67', '1000 MODE 0', '1000 s0 FILL 0 0 127 63',
          '1000 s0 49 4 !=8015592D:COPY PAD', '1000 s0 81 22 !=8015598F:P-01',
          '1000 s0 35 32 _=801561D5:-- >> --',
          '3000 PAD 13', '3040 s0 35 32 _=801561D5:A13 >> --',
          '4000 KEY DOWN 26', '4000 BANK 1',
          '5000 PAD 1', '5040 s0 35 32 _=801561D5:A13 >> B1', '7000 VALUE']
    write('copy', d, ['top screen', 'COPY PAD | P-01', 'A13 >> --', 'B', 'A13 >> B1'])

# An effect's button pressed, as runs/60 drew it: the grid, its cell for the
# effect on white and the effect's name as its title, for a tenth of a
# second; then the effect's page, which names it again and is not said.
def fxbutton():
    CELL = '=80145EDF:'
    cells = ['Scatt..', 'Down..', 'Ha-Dou', 'Ko-Da..']
    d = main_screen(0, 0, 'A-13')
    d += ['1000 KEY DOWN 30', '1000 s0 FILL 0 8 127 63']
    d += [f'1000 s0 {7 + 30 * k} 16 {"!" if k == 0 else "_"}{CELL}{c}' for k, c in enumerate(cells)]
    d += ['1000 s0 49 5 _=80146135:Scatter', '1000 s0 111 5 _=800EE779:1/3',
          '1110 s0 FILL 0 0 127 63',
          '1110 s0 13 3 _=801463F7:TYPE', '1110 s0 14 18 _=80146545:  5',
          '1110 s0 53 3 _=801463F7:DEPTH', '1110 s0 52 18 _=80146545:  50',
          '1110 s0 91 3 _=801463F7:SCATTER', '1110 s0 98 18 _=80146545:ON',
          '1110 s0 76 47 _=800EE779:Scatter', '3000 VALUE']
    write('fxbutton', d, ['top screen', 'Scatter'])

# Powering on: the reader starts knowing nothing, and the first it sees is
# the main screen redrawn for a pad hit, which says nothing; then a bank
# key, which says the bank and pad; then a pad again, nothing.
def poweron():
    d = ['0 KEY DOWN 0c'] + main_screen(0, 0, 'A-13')
    d += ['2000 KEY DOWN 26', '2000 BANK 1', '2000 s0 34 2 !=80149DD5:B-13',
          '3000 KEY DOWN 0d'] + pad_hit(3000, 0, 'B-14', 'DRY', '- - - ')
    d += ['5000 VALUE']
    write('poweron', d, ['B'])

os.chdir(os.path.dirname(os.path.abspath(__file__)))
menu()
params()
popup()
grid()
record()
mainpads()
submenu()
recscale()
system()
sdreturn()
dialog()
pages()
settings()
bpm()
tabs()
turned()
patrec()
export()
delete()
padset()
sampedit()
automark()
fxreturn()
extpower()
export2()
fxgrid()
copy()
fxbutton()
poweron()
