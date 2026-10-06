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
                      'EXPORT SAMPLE/project/MULTIPAD | SAMPLE', 'PROJECT', 'top screen'])

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
    write('submenu', d, ['top screen', 'EXPORT SAMPLE/project/MULTIPAD | SAMPLE', 'PROJECT'])

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
                          'EXPORT to SD-CARD', 'EXPORT SAMPLE/project/MULTIPAD | SAMPLE',
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
# names it RECORD SETTING, as the manual does, and reads what the knobs do
# not set, MODE : Real-Time closed up so the colon is not read out; CTRL 1
# turns the tempo, CTRL 2 the
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
    write('patrec', d, ['top screen', 'RECORD SETTING | quantize: GRID 16 | MODE: Real-Time | A 1',
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
    write('export', d, ['top screen', 'EXPORT SAMPLE/project/MULTIPAD | PLEASE SELECT sample',
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
    # One pad chosen and DEL pressed: the pads are deleted and the top
    # screen comes back with the message, which is said first, as runs/41
    # drew it.
    d += ['7000 PAD 1', '7048 s0 28 4 !=8015604B:TOT SELECTED PADS:1',
          '8000 KEY DOWN 1a', '8000 PAGE 84', '8000 s0 FILL 0 0 127 63']
    d += main_screen(8000, 0, 'A-1')
    d += ['8008 s3 18 20 ~=8001F4BD:Operation\\nCompleted!', '10000 VALUE']
    write('delete', d, ['top screen', 'delete | SELECT PAD', 'A 1 selected', 'A 2 selected',
                        'A 1 deselected', 'A 2 deselected', 'A 1 selected',
                        'Operation Completed! | top screen'])

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
    write('padset', d, ['top screen', 'PITCH/SPEED'])

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
    write('sampedit', d, ['top screen', 'START/END', 'VALUE: ZOOM (2x)', 'VALUE: ZOOM (3x)',
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
    write('automark', d, ['top screen', 'CHOP', 'TIME DIVISION',
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

# The effects grid, shown while MFX is held, which says nothing arriving;
# then turned to the next page, showing none of its effects selected, as
# runs/59 drew it: MFX LIST 17-32 in the title's place, the page count, and
# sixteen names cut short. That says the heading and the page; the names wait for
# VALUE to select one.
def fxgrid():
    CELL = '=80145EDF:'
    cells = ['Scatt..', 'Down..', 'Ha-Dou', 'Ko-Da..', 'Zan-Z..', 'To-Gu..', 'SBF',
             'Stopp..', 'Tape ..', 'Time..', 'Super..', 'WrmS..', '303 V..', '404 V..',
             'Casse..', 'Lo-fi']
    d = main_screen(0, 0, 'D-1')
    d += ['500 KEY DOWN 30', '500 s0 FILL 0 8 127 63']
    d += [f'500 s0 {7 + 30 * (k % 4)} {16 + 11 * (k // 4)} {"!" if k == 0 else "_"}{CELL}{c}'
          for k, c in enumerate(cells)]
    d += ['500 s0 49 5 _=80146135:Scatter', '500 s0 111 5 _=800EE779:1/3',
          '1000 KNOB 0 1', '1000 s0 FILL 0 8 127 63']
    d += [f'1000 s0 {7 + 30 * (k % 4)} {16 + 11 * (k // 4)} _{CELL}{c}'
          for k, c in enumerate(cells)]
    d += ['1000 s0 26 5 _=80146175: MFX LIST 17-32 ', '1000 s0 111 5 _=800EE779:2/3',
          '3000 VALUE']
    write('fxgrid', d, ['top screen', 'MFX LIST 17-32 | 2 of 3'])

# MFX as runs/63 pressed it: held, the grid with the current effect
# selected, which says nothing whether the press turns the effect on or
# off; let go, the effect's page if it is now on, which names it, or the top
# screen if off, which says nothing; pressed quickly on and off and on
# again, the name each time it comes on. Then held while pads choose
# effects, each said; let go, the chosen effect's page, its name just said,
# and AGE's unit, Years, not read with it.
def fxspam():
    CELL = '=80145EDF:'
    names = ['Scatt..', 'Down..', 'Ha-Dou', 'Ko-Da..', 'Zan-Z..', 'To-Gu..', 'SBF', 'Stopp..',
             'Tape ..', 'Time..', 'Super..', 'WrmS..', '303 V..', '404 V..', 'Casse..', 'Lo-fi']
    full = {'Scatt..': 'Scatter', '303 V..': '303 VinylSim', '404 V..': '404 VinylSim',
            'Casse..': 'Cassette Sim'}
    def grid(t, sel):
        out = [f'{t} s0 FILL 0 8 127 63']
        out += [f'{t} s0 {7 + 30 * (k % 4)} {16 + 11 * (k // 4)} {"!" if k == sel else "_"}{CELL}{c}'
                for k, c in enumerate(names)]
        return out + [f'{t} s0 49 5 _=80146135:{full[names[sel]]}', f'{t} s0 111 5 _=800EE779:1/3']
    def page(t, name, cols):
        out = [f'{t} s0 FILL 0 0 127 63']
        for k, (label, value, unit) in enumerate(cols):
            x = 12 + 42 * k
            out += [f'{t} s0 {x} 3 _=801463F7:{label}', f'{t} s0 {x} 18 _=80146545:{value}',
                    f'{t} s0 {x} 33 _=800EE779:{unit}']
        return out + [f'{t} s0 50 47 _=800EE779:{name}']
    scatter = [('TYPE', '  5', ' '), ('DEPTH', '  50', '  '), ('SCATTER', 'ON', '  ')]
    d = main_screen(0, 0, 'A-13')
    for t, on in [(1000, True), (2000, False), (3000, True)]:
        d += [f'{t} KEY DOWN 30'] + grid(t + 30, 0) + [f'{t + 110} KEY UP 30']
        d += page(t + 150, 'Scatter', scatter) if on else \
            [f'{t + 150} s0 FILL 0 0 127 63'] + main_screen(t + 150, 0, 'A-13')
    d += ['5000 KEY DOWN 30'] + grid(5030, 0)
    for t, pad in [(6000, 14), (6500, 13), (7000, 15)]:
        d += [f'{t} PAD {pad}'] + grid(t + 20, pad - 1)
    d += ['7500 KEY UP 30']
    d += page(7550, 'Cassette Sim', [('TONE', ' 100', ' '), ('HISS', '  27', ' '),
                                     ('AGE', '  9', 'Years ')])
    d += ['9000 VALUE']
    write('fxspam', d, ['top screen', 'Scatter', 'Scatter', '404 Vinyl Sim', '303 Vinyl Sim',
                        'Cassette Sim'])


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
    write('export2', d, ['top screen', 'EXPORT SAMPLE/project/MULTIPAD | SAMPLE',
                         'PLEASE SELECT sample', 'A 10 selected'])

# EXT SOURCE pressed and let go twice, as runs/62 drew it: the pad field
# shows EXT only while it is held, but each press flips the input, which is
# what is said, on and then off. Then the power source changing at the end
# of the status bar, which says nothing, until the batteries run low.
def extpower():
    d = main_screen(0, 0, 'A-13')
    for t in (2000, 3000):
        d += [f'{t} KEY DOWN 12', f'{t + 30} s0 36 2 !=80149DD5:EXT', f'{t + 110} KEY UP 12',
              f'{t + 150} s0 34 2 !=80149DD5:A-13']
    d += ['4000 s0 104 2 !=800EE779: BAT ', '6000 s0 104 2 !=800EE779: LOW!', '8000 VALUE']
    write('extpower', d, ['top screen', 'external SOURCE ON', 'external SOURCE OFF', 'LOW!'])

# COPY on the pad operations page, 67 in its mode 0, as runs/60 drew it:
# COPY PAD in the status bar, the project it copies into on white, the
# source and destination below, A13 >> B1. Arriving says the heading and
# the project; a pad names the source, "source A13"; a bank key, which the
# screen does not show, is said; a pad in the new bank names the
# destination, "destination B1", in the manual's words: OpenEVV reads >> as
# "greater than" twice.
def copy():
    d = main_screen(0, 0, 'A-13')
    d += ['1000 KEY DOWN 23', '1000 PAGE 67', '1000 MODE 0', '1000 s0 FILL 0 0 127 63',
          '1000 s0 49 4 !=8015592D:COPY PAD', '1000 s0 81 22 !=8015598F:P-01',
          '1000 s0 35 32 _=801561D5:-- >> --',
          '3000 PAD 13', '3040 s0 35 32 _=801561D5:A13 >> --',
          '4000 KEY DOWN 26', '4000 BANK 1',
          '5000 PAD 1', '5040 s0 35 32 _=801561D5:A13 >> B1', '7000 VALUE']
    write('copy', d, ['top screen', 'COPY PAD | P-01', 'source A13', 'B', 'destination B1'])

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

# The playback buttons on the top screen, keys 0x1D to 0x20, each saying
# its name and what it now is for the current pad, as the firmware's own
# BANK A GATE ON does: GATE on and off, LOOP on and off, SHIFT and LOOP the
# ping pong loop, REVERSE, BPM SYNC; SHIFT and GATE, the whole bank, says
# only the firmware's own message; a pad played, then GATE for it.
def buttons():
    d = main_screen(0, 0, 'A-13')
    d += ['1000 PAD 3']
    for t, key in [(2000, '1e'), (3000, '1e'), (4000, '1f'), (5000, '1f')]:
        d += [f'{t} KEY DOWN {key}', f'{t + 100} KEY UP {key}']
    d += ['6000 KEY DOWN 2a', '6050 KEY DOWN 1f', '6100 KEY UP 1f', '6150 KEY UP 2a',
          '7000 KEY DOWN 1f', '7100 KEY UP 1f', '8000 KEY DOWN 20', '8100 KEY UP 20',
          '9000 KEY DOWN 1d', '9100 KEY UP 1d',
          '10000 KEY DOWN 2a', '10050 KEY DOWN 1e', '10080 s1 9 4 ~=8006BECD:BANK A GATE ON ',
          '10100 KEY UP 1e', '10150 KEY UP 2a',
          '14000 PAD 5', '14500 KEY DOWN 1e', '14600 KEY UP 1e', '16000 VALUE']
    write('buttons', d, ['top screen', 'GATE ON', 'GATE OFF', 'LOOP ON', 'LOOP OFF',
                         'ping pong loop ON', 'LOOP OFF', 'REVERSE ON', 'BPM SYNC ON',
                         'BANK A GATE ON', 'GATE ON'])

# The pattern screen, page 60, as runs/64 and runs/65 drew it: the status
# line's SELECT and tempo, the big P T N, which names it. A bank key moves
# the pattern store's bank and not the sample bank, and says its letter and
# not the tempo it brings. A pad starts C1, drawing STOP and
# then PLAY 83 ms later, which is said as it settles, PLAY; pressed again
# and again, quickly, each state is said as it comes; the big field, the
# bar and beat running and the time signature are not. C1 is the pattern
# C1 and not the CTRL 1 knob.
def patterns():
    ST, BIG = '=8015F85F:', '=800EE779:'
    d = main_screen(0, 0, 'A-13')
    d += ['1000 KEY DOWN 14', '1000 PAGE 60', '1000 PMODE 1', '1000 s0 FILL 0 0 127 63',
          f'1000 s0 76 2 !{ST}SELECT', '1000 s0 8 2 !=80156727:BPM 103.0',
          f'1000 s0 28 14 _{BIG}P T N',
          '3000 KEY DOWN 27', '3015 s0 8 2 !=80156727:BPM  90.0',
          '4000 PAD 1', f'4020 s0 66 2 !{ST}STOP-PTN C1', '4020 s0 FILL 0 9 127 40',
          f'4020 s0 60 14 _{BIG}C 1', f'4083 s0 66 2 !{ST}PLAY-PTN C1',
          f'4085 s0 112 59 _{BIG}4.4', '4300 s0 FILL 0 9 127 40', f'4300 s0 45 14 _{BIG}1.1.']
    for k, t in enumerate(range(4950, 7000, 650)):
        d.append(f'{t} s0 45 14 _{BIG}1.{k + 2}.')
    d += ['7000 PAD 1', f'7020 s0 66 2 !{ST}STOP-PTN C1', '7020 s0 FILL 0 9 127 63',
          f'7020 s0 60 14 _{BIG}C 1']
    for k, t in enumerate(range(8000, 9600, 400)):
        state = 'PLAY' if k % 2 == 0 else 'STOP'
        d += ['%d PAD 1' % t, f'{t + 20} s0 66 2 !{ST}{state}-PTN C1']
    d += ['11000 VALUE']
    write('patterns', d, ['top screen', 'pattern', 'C', 'PLAY-pattern C1', 'STOP-pattern C1',
                          'PLAY-pattern C1', 'STOP-pattern C1', 'PLAY-pattern C1',
                          'STOP-pattern C1'])

# COPY BANK PAD, the pad operations page in its mode 3, as runs/66 drew it,
# opened by holding COPY and pressing EXIT: its title, its warning, the
# project on white, and the banks it copies from and to either side of >>,
# none chosen. Arriving says the title, the warning, the project and where
# the cursor is. A bank key sets the bank the cursor is on, pressed again
# the other bank of its pair, each said with its side in the manual's
# words; VALUE moves the cursor, said as the side it lands on, the screen
# showing it only by a bar.
def copybank():
    d = main_screen(0, 0, 'A-13')
    d += ['1000 KEY DOWN 23', '1000 PAGE 67', '1000 MODE 0', '1000 s0 FILL 0 0 127 63',
          '1000 s0 49 4 !=8015592D:COPY PAD', '1000 s0 81 22 !=8015598F:P-01',
          '1000 s0 35 32 _=801561D5:-- >> --',
          '2000 KEY DOWN 22', '2000 PAGE 67', '2000 MODE 3', '2000 s0 FILL 0 0 127 63',
          '2000 s0 39 4 !=80155C61:COPY BANK PAD',
          '2000 s0 13 12 _=80155C7F:(PAD will be overwritten)',
          '2000 s0 77 22 !=80155CE1:P-01', '2000 s0 39 32 _=8015608D:-',
          '2000 s0 55 32 _=8015609B:>>', '2000 s0 81 32 _=801560A9:-',
          '3000 KEY DOWN 26', '3000 s0 39 32 _=8015608D:B',
          '3500 KEY DOWN 26', '3500 s0 39 32 _=8015608D:G',
          '4000 KNOB 0 1', '5000 KEY DOWN 25', '5000 s0 80 32 _=801560A9:A',
          '6000 KNOB 0 -1', '8000 VALUE']
    write('copybank', d, ['top screen', 'COPY PAD | P-01',
                          'COPY BANK PAD | (PAD will be overwritten) | P-01 | source',
                          'source B', 'source G', 'destination', 'destination A', 'source'])

# The pattern screen's status line and body, as FUN_8015F4A8 and FUN_80148B50
# draw them, found in the firmware rather than a log: its mode or state
# centred in the status bar beside the tempo, and the body wiped and drawn
# again whole whenever anything changes.
PST, PBIG = '=8015F85F:', '=800EE779:'

def pattern_status(t, state, bpm='103.0'):
    return [f'{t} s0 FILL 44 0 128 8', f'{t} s0 {88 - 2 * len(state)} 2 !{PST}{state}',
            f'{t} s0 8 2 !=80156727:BPM {bpm:>5}']

def pattern_screen(t, state='SELECT', big='P T N', bpm='103.0'):
    return ['%d s0 FILL 0 0 127 63' % t] + pattern_status(t, state, bpm) + \
        [f'{t} s0 {28 + 17 * (5 - len(big))} 14 _{PBIG}{big}']

# COPY on the pattern screen, its mode 8: its headings on white, PATTERN
# and PATTERN, or SAMPLE once PATTERN SELECT makes it a bounce, the project
# under the second, and the source and destination between >>, drawn as
# text: FUN_801350B8 takes the pads. Arriving says the mode, the headings
# and the project; each pad is said as the side of the line it sets,
# source or destination; a bank key, which moves the destination's bank
# once there is a source, says its letter, which nothing shows; PATTERN
# SELECT says SAMPLE, the destination it clears unsaid. Pressed as quickly
# as a hand does, each change of the line is said at once.
def ptncopy():
    def body(t, line, right='PATTERN', hint=None):
        d = [f'{t} s0 FILL 0 9 128 64', f'{t} s0 35 30 _=80149303:{line}',
             f'{t} s0 19 12 !=80149361:PATTERN', f'{t} s0 {94 - 2 * len(right)} 12 !=801493ED:{right}']
        if right == 'PATTERN':
            d.append(f'{t} s0 81 22 !=80149449:P-01')
        if hint:
            d.append(f'{t} s0 20 54 _=80149869:{hint}')
        return d
    hint = 'REMAIN: Samples to copy'
    d = main_screen(0, 0, 'A-13')
    d += ['1000 KEY DOWN 14', '1000 PAGE 60', '1000 PMODE 1'] + pattern_screen(1000)
    d += ['3000 KEY DOWN 23', '3000 PMODE 8'] + pattern_status(3010, 'COPY') + body(3010, '-- >> --')
    d += ['3100 KEY UP 23', '5000 PAD 1'] + pattern_status(5010, 'COPY') + body(5010, 'A1 >> --', hint=hint)
    d += ['6000 KEY DOWN 26'] + pattern_status(6010, 'COPY') + body(6010, 'A1 >> --', hint=hint)
    d += ['6500 PAD 3'] + pattern_status(6510, 'COPY') + body(6510, 'A1 >> B3', hint=hint)
    d += ['7000 KEY DOWN 14'] + pattern_status(7010, 'COPY') + \
        body(7010, 'A1 >> --', right='SAMPLE', hint=hint)
    d += ['7600 PAD 5'] + pattern_status(7610, 'COPY') + body(7610, 'A1 >> B5', right='SAMPLE', hint=hint)
    d += ['9500 VALUE']
    write('ptncopy', d, ['top screen', 'pattern', 'COPY | PATTERN | P-01',
                         'source A1 | REMAIN: Samples to copy', 'B', 'destination B3',
                         'SAMPLE', 'destination B5'])

# DELETE on the pattern screen, its mode 6: the big D E L, which names it,
# and DELETE in the status bar. The pads choose patterns shown only by their
# lights, flipping a word each in the pattern store, from the bank a bank
# key moves, as FUN_801350B8 does; each is said chosen or let go. The bank
# key brings the new bank's tempo to the status bar, as runs/68 drew it,
# which is not said.
def ptndelete():
    d = main_screen(0, 0, 'A-13')
    d += ['1000 KEY DOWN 14', '1000 PAGE 60', '1000 PMODE 1'] + pattern_screen(1000)
    d += ['3000 KEY DOWN 1a', '3000 PMODE 6'] + pattern_screen(3010, 'DELETE', 'D E L')
    d += ['5000 KEY DOWN 26'] + pattern_screen(5010, 'DELETE', 'D E L', '90.0')
    for k, pad in enumerate([3, 3, 4]):
        t = 6000 + 1000 * k
        d += [f'{t} PAD {pad}'] + pattern_screen(t + 10, 'DELETE', 'D E L', '90.0')
    d += ['10000 VALUE']
    write('ptndelete', d, ['top screen', 'pattern', 'delete', 'B', 'B 3 selected',
                           'B 3 deselected', 'B 4 selected'])

# COPY BANK on the pattern screen, its mode 9, held COPY and EXIT: as COPY
# BANK PAD, its warning, the project on white, and the banks either side of
# >>, which the bank keys set on the side the cursor is on and VALUE moves
# it between, as FUN_80084258 does.
def ptncopybank():
    def body(t, src, dst):
        return [f'{t} s0 FILL 0 9 128 64', f'{t} s0 25 10 _=801496DD:PATTERN will be\\noverwritten',
                f'{t} s0 81 22 !=80149739:P-01', f'{t} s0 39 30 _=8014989F:{src}',
                f'{t} s0 57 30 _=801498AB:>>', f'{t} s0 81 30 _=801498B7:{dst}']
    d = main_screen(0, 0, 'A-13')
    d += ['1000 KEY DOWN 14', '1000 PAGE 60', '1000 PMODE 1'] + pattern_screen(1000)
    d += ['3000 KEY DOWN 23', '3050 KEY DOWN 22', '3050 PMODE 9'] + \
        pattern_status(3060, 'COPY BANK') + body(3060, '-', '-')
    d += ['4000 KEY DOWN 26'] + pattern_status(4010, 'COPY BANK') + body(4010, 'B', '-')
    d += ['4500 KEY DOWN 26'] + pattern_status(4510, 'COPY BANK') + body(4510, 'G', '-')
    d += ['5000 KNOB 0 1'] + pattern_status(5010, 'COPY BANK') + body(5010, 'G', '-')
    d += ['6000 KEY DOWN 25'] + pattern_status(6010, 'COPY BANK') + body(6010, 'G', 'A')
    d += ['7000 KNOB 0 -1'] + pattern_status(7010, 'COPY BANK') + body(7010, 'G', 'A')
    d += ['9000 VALUE']
    write('ptncopybank', d, ['top screen', 'pattern',
                             'COPY BANK | PATTERN will be overwritten | P-01 | source',
                             'source B', 'source G', 'destination', 'destination A', 'source'])

# REC on the pattern screen, as runs/57 drew it: Select PAD for RECORDING,
# STAND BY and the big R E C. A bank key moves the pattern store's bank; a
# pad then chooses the pattern to record, moving the sample bank to its
# bank, which is not said, and the recording settings follow, naming it.
def ptnrec():
    NAME, LOW, TEMPO = '=801709AB:', '=801709EF:', '=80170A15:'
    d = main_screen(0, 0, 'A-13')
    d += ['1000 KEY DOWN 14', '1000 PAGE 60', '1000 PMODE 1'] + pattern_screen(1000)
    d += ['3000 KEY DOWN 1b', '3010 s1 9 4 ~=8006BECD:Select PAD\\nfor RECORDING'] + \
        pattern_status(3040, 'STAND BY') + [f'3040 s0 28 14 _{PBIG}R E C', '3100 KEY UP 1b']
    d += ['5000 KEY DOWN 27', '5100 KEY UP 27']
    d += ['7000 PAD 2', '7000 BANK 2', '7030 PAGE 62', '7030 s1 83 46 !=8016B061:GRID 16',
          '7030 s1 5 45 !=8016BAB7:SHIFT:OTHER', '7030 s1 83 54 _=8016BB11:C-2',
          '7030 s1 5 54 _=8016BB45:MODE : Real-Time', '7030 s1 65 46 _=8016BB55:QTZ:',
          '7030 s0 CLEAR',
          f'7030 s0 15 4 _{NAME}BPM', f'7030 s0 15 34 _{LOW}BPM', f'7030 s0 9 19 _{TEMPO}90.0',
          f'7030 s0 53 4 _{NAME}LENGTH', f'7030 s0 53 34 _{LOW}2 Bars',
          f'7030 s0 86 4 _{NAME} STRENGTH ', f'7030 s0 102 34 _{LOW}0%']
    d += ['9000 VALUE']
    write('ptnrec', d, ['top screen', 'pattern', 'REC | Select PAD for RECORDING', 'C',
                        'RECORD SETTING | quantize: GRID 16 | MODE: Real-Time | C 2'])


# EXCHANGE on the pattern screen, its mode 10, SHIFT and pad 5: EXCHANGE in
# the status bar and the pads it swaps between <>, which FUN_801350B8 sets
# as COPY's, nothing said until one is chosen, then each pad alone. The pad
# that opens it is not a choice.
def ptnexchange():
    def body(t, line):
        return [f'{t} s0 FILL 0 9 128 64', f'{t} s0 35 30 _=80149303:{line}']
    d = main_screen(0, 0, 'A-13')
    d += ['1000 KEY DOWN 14', '1000 PAGE 60', '1000 PMODE 1'] + pattern_screen(1000)
    d += ['3000 KEY DOWN 2a', '3050 PAD 5', '3050 PMODE 10'] + pattern_status(3060, 'EXCHANGE') + \
        body(3060, '-- <> --') + ['3200 KEY UP 2a']
    d += ['5000 PAD 2'] + pattern_status(5010, 'EXCHANGE') + body(5010, 'A2 <> --')
    d += ['5600 KEY DOWN 27'] + pattern_status(5610, 'EXCHANGE') + body(5610, 'A2 <> --')
    d += ['6100 PAD 7'] + pattern_status(6110, 'EXCHANGE') + body(6110, 'A2 <> C7')
    d += ['8000 VALUE']
    write('ptnexchange', d, ['top screen', 'pattern', 'EXCHANGE', 'A2', 'C', 'C7'])

# DELETE BANK on the pattern screen, its mode 7, held DEL and EXIT: the bank
# in the big field, Bn:A, which a bank key moves; its letter is said, and
# the field drawn again for it is not.
def ptndeletebank():
    d = main_screen(0, 0, 'A-13')
    d += ['1000 KEY DOWN 14', '1000 PAGE 60', '1000 PMODE 1'] + pattern_screen(1000)
    d += ['3000 KEY DOWN 1a', '3050 KEY DOWN 22', '3050 PMODE 7'] + \
        pattern_screen(3060, 'DELETE BANK', 'Bn:A')
    d += ['5000 KEY DOWN 26'] + pattern_screen(5010, 'DELETE BANK', 'Bn:B')
    d += ['7000 VALUE']
    write('ptndeletebank', d, ['top screen', 'pattern', 'DELETE BANK | bank: A', 'B'])

# COPY on the pattern screen keeping only some of the pattern's samples:
# with a source chosen, REMAIN shows Select Samples and the pads choose
# samples from the source's bank, shown only by their lights, a word each
# in the pattern store as FUN_801350B8 flips them; each is said chosen or
# let go. Both lines under the pads come from one call, at x 20 and x 32,
# and REMAIN pressed a second after the source changes it at once.
def ptnkeep():
    def body(t, line, hint=None):
        d = [f'{t} s0 FILL 0 9 128 64', f'{t} s0 35 30 _=80149303:{line}',
             f'{t} s0 19 12 !=80149361:PATTERN', f'{t} s0 80 12 !=801493ED:PATTERN',
             f'{t} s0 81 22 !=80149449:P-01']
        if hint:
            d.append(f'{t} s0 {32 if hint.startswith("Select") else 20} 54 _=80149869:{hint}')
        return d
    d = main_screen(0, 0, 'A-13')
    d += ['1000 KEY DOWN 14', '1000 PAGE 60', '1000 PMODE 1'] + pattern_screen(1000)
    d += ['3000 KEY DOWN 23', '3000 PMODE 8'] + pattern_status(3010, 'COPY') + \
        body(3010, '-- >> --')
    d += ['5000 PAD 1'] + pattern_status(5010, 'COPY') + body(5010, 'A1 >> --', 'REMAIN: Samples to copy')
    d += ['6000 KEY DOWN 24', '6000 PKEEP 1'] + pattern_status(6010, 'COPY') + \
        body(6010, 'A1 >> --', 'Select Samples')
    for k, pad in enumerate([2, 3, 2]):
        t = 7000 + 600 * k
        d += [f'{t} PAD {pad}'] + pattern_status(t + 10, 'COPY') + body(t + 10, 'A1 >> --', 'Select Samples')
    d += ['10000 VALUE']
    write('ptnkeep', d, ['top screen', 'pattern', 'COPY | PATTERN | P-01',
                         'source A1 | REMAIN: Samples to copy', 'Select Samples', 'A 2 selected',
                         'A 3 selected', 'A 2 deselected'])

# The message box, FUN_8001F4xx, as runs/68 drew it deleting a pattern:
# Working, then Operation Completed! as the pattern screen goes back to
# choosing patterns. The message leads, before the screen it leaves you on.
def msgfirst():
    MSG = '=8001F4BD:'
    d = main_screen(0, 0, 'A-13')
    d += ['1000 KEY DOWN 14', '1000 PAGE 60', '1000 PMODE 1'] + pattern_screen(1000)
    d += ['3000 KEY DOWN 1a', '3000 PMODE 6'] + pattern_screen(3010, 'DELETE', 'D E L')
    d += ['4000 PAD 1'] + pattern_screen(4010, 'DELETE', 'D E L')
    d += ['5000 KEY DOWN 1a', f'5000 s1 18 20 ~{MSG}Working', '5000 s1 18 40 ~=8001F6B5: ',
          '5010 PMODE 1'] + pattern_screen(5010) + \
        [f'5010 s1 18 20 ~{MSG}Operation\\nCompleted!', '5010 s1 18 40 ~=8001F6B5:']
    d += ['7000 VALUE']
    write('msgfirst', d, ['top screen', 'pattern', 'delete', 'A 1 selected',
                          'Operation Completed! | pattern'])

# COUNT-IN on the pattern screen, SHIFT and pad 10, as runs/68 drew it:
# each press brings its pop-up a moment later, and presses come within a
# second of each other; each is said as it comes.
def countin():
    TOAST = '=8006BECD:'
    d = main_screen(0, 0, 'A-13')
    d += ['1000 KEY DOWN 14', '1000 PAGE 60', '1000 PMODE 1'] + pattern_screen(1000)
    d += ['3000 KEY DOWN 2a', '3100 PAD 10', f'3128 s1 9 4 ~{TOAST}COUNT-IN 2MEAS',
          '3810 PAD 10', f'3838 s1 10 4 ~{TOAST}COUNT-IN WAIT',
          '4340 PAD 10', f'4368 s1 10 4 ~{TOAST}COUNT-IN OFF', '4500 KEY UP 2a', '7000 VALUE']
    write('countin', d, ['top screen', 'pattern', 'COUNT-IN 2 measures', 'COUNT-IN WAIT',
                         'COUNT-IN OFF'])

# The recording settings, page 62, as runs/57 and runs/68 drew them.
def record_setting(t, pattern):
    NAME, LOW, TEMPO = '=801709AB:', '=801709EF:', '=80170A15:'
    return [f'{t} PAGE 62', f'{t} s1 83 46 !=8016B061:GRID 16',
            f'{t} s1 5 45 !=8016BAB7:SHIFT:OTHER', f'{t} s1 83 54 _=8016BB11:{pattern}',
            f'{t} s1 5 54 _=8016BB45:MODE : Real-Time', f'{t} s1 65 46 _=8016BB55:QTZ:',
            f'{t} s0 CLEAR',
            f'{t} s0 15 4 _{NAME}BPM', f'{t} s0 15 34 _{LOW}BPM', f'{t} s0 9 19 _{TEMPO}95.0',
            f'{t} s0 53 4 _{NAME}LENGTH', f'{t} s0 53 34 _{LOW}2 Bars',
            f'{t} s0 86 4 _{NAME} STRENGTH ', f'{t} s0 98 34 _{LOW}100%']

# Recording a pattern in real time, as runs/68 drew it: REC on the
# recording settings builds the pattern screen, STAND BY, WAIT NOTE... and
# COUNT IN in the status bar a moment apart, which says COUNT IN; the
# count-in's bars, -1.1. on, are not said. Recording begins with the
# metronome's and the quantise's settings in the status bar and the pattern
# and 2.4 in the corners, none of it said. EXIT ends it with the pattern
# playing, PLAY-PTN G1, and EXIT again stops it.
def ptnrecord():
    BAR = f'{PBIG}'
    d = main_screen(0, 0, 'A-13')
    d += ['1000 KEY DOWN 14', '1000 PAGE 60', '1000 PMODE 1'] + pattern_screen(1000)
    d += ['2000 KEY DOWN 1b', '2000 s1 9 4 ~=8006BECD:Select PAD\\nfor RECORDING'] + \
        pattern_status(2040, 'STAND BY') + ['2040 s0 FILL 0 9 128 64', f'2040 s0 28 14 _{PBIG}R E C']
    d += ['3000 PAD 1'] + record_setting(3010, 'G-1')
    d += ['5000 KEY DOWN 1b', '5010 PAGE 60', '5010 s0 CLEAR'] + pattern_status(5010, 'STAND BY-PTN G1') + \
        ['5010 s0 FILL 0 9 128 64', f'5010 s0 28 14 _{PBIG}R E C'] + \
        pattern_status(5026, 'WAIT NOTE...-PTN G1') + pattern_status(5055, 'COUNT IN') + \
        ['5055 s0 FILL 0 9 128 64', f'5055 s0 21 14 _{BAR}-1.1.']
    for k, t in enumerate([5690, 6325, 6910]):
        d += [f'{t} s0 FILL 0 9 128 64', f'{t} s0 21 14 _{BAR}-1.{k + 2}.']
    d += ['7540 s0 FILL 44 0 128 8', '7540 s0 51 2 !=8015F7D3:METRO 5',
          '7540 s0 86 2 !=8015F7E1:GRID 100%', '7540 s0 8 2 !=80156727:BPM 103.0',
          '7540 s0 FILL 0 9 128 64', f'7540 s0 45 14 _{BAR}1.1.', f'7540 s0 8 53 _{PBIG}G1',
          f'7540 s0 112 59 _{PBIG}2.4']
    for k, t in enumerate([8175, 8810, 9445, 10080]):
        d += [f'{t} s0 FILL 0 9 128 64', f'{t} s0 45 14 _{BAR}{1 + (k + 1) // 4}.{(k + 1) % 4 + 1}.',
              f'{t} s0 8 53 _{PBIG}G1', f'{t} s0 112 59 _{PBIG}2.4']
    d += ['10500 KEY DOWN 22'] + pattern_status(10533, 'PLAY-PTN G1') + \
        ['10585 s0 FILL 0 9 128 64', f'10585 s0 45 14 _{BAR}2.2.', f'10585 s0 112 59 _{PBIG}2.4']
    d += ['12000 KEY DOWN 22'] + pattern_screen(12050)
    d += ['14000 VALUE']
    write('ptnrecord', d, ['top screen', 'pattern', 'REC | Select PAD for RECORDING',
                           'RECORD SETTING | quantize: GRID 16 | MODE: Real-Time | G 1',
                           'COUNT IN', 'PLAY-pattern G1', 'pattern'])

# TR-REC, as runs/68 drew it: REC on the recording settings in its TR-REC
# mode builds the pattern screen with the CTRL knobs' columns, SUBSTEP,
# PITCH over its mode CHROMATIC, VELOCITY, and below the pattern, the bar
# and the sample to input. Arriving says TR-REC, which the empty status bar
# does not, and what the knobs do not set. A pad sets its step or clears
# it, shown only by the pads' lights, a moment after it is pressed, as the
# sequencer takes the note: each is said, "step 5 on". SUB PAD and a pad
# choose the sample, said, and set no step; VALUE chooses the bar. PATTERN EDIT and a pad open the
# Microscope, its knobs' legends in the status bar and the note's step and
# sample over its timing, on white, its pitch and its velocity: arriving
# names it and the note; the timing is said bare as VALUE moves it, the
# pitch names itself, the velocity is named by its legend, VELO. EXIT goes
# back to TR-REC, and EXIT again leaves it with the pattern playing.
def trrec():
    NAME, MID, LOW, VAL = '=801709AB:', '=801709C7:', '=801709EF:', '=80170A15:'
    def screen(t, bar, sample):
        return [f'{t} PAGE 60', f'{t} s0 CLEAR', f'{t} s0 88 2 !=8015F85F:',
                f'{t} s0 8 4 _{NAME}SUBSTEP', f'{t} s0 55 4 _{NAME}PITCH', f'{t} s0 46 10 _{MID}CHROMATIC',
                f'{t} s0 65 34 _{LOW}', f'{t} s0 58 19 _{VAL}+0', f'{t} s0 90 4 _{NAME}VELOCITY',
                f'{t} s0 99 19 _{VAL}90', f'{t} s0 5 43 !=8016C5CD:SHIFT:OTHER',
                f'{t} s0 58 43 _=8016C6C5:Ptn:G1', f'{t} s0 92 43 _=8016C6D9:BAR:{bar}',
                f'{t} s0 5 51 _=8016C75F:{sample}', f'{t} s0 112 59 _{PBIG}2.4']
    def micro(t):
        return ['%d PAGE 80' % t, f'{t} s1 CLEAR', f'{t} s1 21 22 !=8015F007:0',
                f'{t} s1 50 22 _=8015F09B:CHROM:0', f'{t} s1 102 22 _=8015F0BD:90',
                f'{t} s1 9 2 !=80172FF5:C1:ITEM', f'{t} s1 49 2 !=8017301B:C2:PITCH',
                f'{t} s1 93 2 !=8017303D:C3:VELO', f'{t} s1 10 12 _=801730D3:STEP: 7  BAR:2/2',
                f'{t} s1 104 12 _{PBIG}G-13']
    d = main_screen(0, 0, 'A-13')
    d += ['1000 KEY DOWN 14', '1000 PAGE 60', '1000 PMODE 1'] + pattern_screen(1000)
    d += record_setting(2000, 'G-1') + ['2000 s1 5 54 _=8016BB45:MODE : TR-REC']
    d += ['4000 KEY DOWN 1b', '4000 TRREC 143'] + screen(4010, '1/2', 'G-1 : TRIG')
    d += ['4600 PAD 1', '5000 PAD 5', '5400 PAD 1']
    d += ['6000 KEY DOWN 13', '6400 PAD 13', '6435 s0 5 51 _=8016C75F:G-13 : TRIG', '6600 KEY UP 13']
    d += ['7500 KNOB 0 1', '7540 s0 92 43 _=8016C6D9:BAR:2/2', '8200 PAD 13']
    d += ['9000 KEY DOWN 15', '9400 PAD 7'] + micro(9410) + ['9700 KEY UP 15']
    for k in range(3):
        t = 11000 + 130 * k
        d += [f'{t} KNOB 0 1', f'{t + 13} s1 21 22 !=8015F007:{k + 1}']
    for k, v in enumerate(['-10', '-8', '-6']):
        t = 13000 + 55 * k
        d += [f'{t} CTRL 2 {5 - k}', f'{t + 50} s1 {46 if k == 0 else 48} 22 _=8015F09B:CHROM:{v}']
    for k, v in enumerate(['68', '74', '77']):
        t = 15000 + 55 * k
        d += [f'{t} CTRL 3 {67 + 3 * k}', f'{t + 20} s1 102 22 _=8015F0BD:{v}']
    d += ['17000 KEY DOWN 22'] + screen(17000, '2/2', 'G-13 : TRIG')
    d += ['19000 KEY DOWN 22', '19000 TRREC 0', '19000 PAGE 60', '19000 s0 CLEAR', '19000 s0 88 2 !=8015F85F:',
          '19000 s0 8 2 !=80156727:BPM 103.0', f'19000 s0 45 14 _{PBIG}2.2.',
          f'19000 s0 112 59 _{PBIG}2.4'] + pattern_status(19066, 'PLAY-PTN G1')
    d += ['21000 VALUE']
    write('trrec', d, ['top screen', 'pattern',
                       'RECORD SETTING | quantize: GRID 16 | MODE: TR-REC | G 1',
                       'TR-REC | pattern: G1 | BAR: 1 of 2 | G-1: TRIG', 'step 1 on',
                       'step 5 on', 'step 1 off', 'G-13: TRIG', 'BAR: 2 of 2', 'step 13 on',
                       'Microscope | STEP: 7 BAR: 2 of 2 | G 13', '1', '2', '3',
                       'CHROM: -10', 'CHROM: -8', 'CHROM: -6', 'velocity 68', '74', '77',
                       'TR-REC | pattern: G1 | BAR: 2 of 2 | G-13: TRIG', 'PLAY-pattern G1'])

# The pattern chain, page 57, as runs/68 drew it: key 0x11 on the pattern
# screen asks for a chain in the message box, and a pad opens it: its
# heading, sixteen slots and REPEAT. Each pad adds its pattern to the next
# slot, the first highlighted, the heading gaining (*), which is not said;
# each is said, a pattern the highlighted slot holds too. DEL takes the last
# off, said as what it held. SUB PAD plays the chain: its position and the
# 2.4 over the slots are not said, nor the highlight moving from slot to
# slot with it. REMAIN changes REPEAT, said at once.
def chain():
    CELL, LIT, POS = '=8008D11B:', '=8008D19B:', '=8016EBDB:'
    xs, ys = [17, 38, 59, 80], [21, 32, 43, 54]
    d = main_screen(0, 0, 'A-13')
    d += ['1000 KEY DOWN 14', '1000 PAGE 60', '1000 PMODE 1'] + pattern_screen(1000)
    d += ['3000 KEY DOWN 11', '3045 s1 18 20 ~=8001F4BD:Please Select\\nChain 1-16',
          '3045 s1 18 40 ~=8001F6B5:', '3500 KEY UP 11']
    d += ['4800 PAD 1', '4810 PAGE 57', '4810 s0 CLEAR']
    d += [f'4810 s0 {x} {y} _{CELL} ' for y in ys for x in xs]
    d += [f'4810 s0 106 22 _{POS}-', '4810 s0 23 2 _=80141D3F:PATTERN CHAIN [1]',
          '4810 s0 97 40 _=800FE92F:REPEAT', '4810 s0 103 48 _=800FE983:All']
    d += ['7000 PAD 1', f'7010 s0 14 21 !{LIT}G.1', f'7010 s0 100 22 _{POS}1.1',
          '7010 s0 14 2 _=80141D3F:PATTERN CHAIN [1](*)']
    for k, (pad, x, y) in enumerate([(1, 35, 21), (1, 56, 21), (2, 77, 21)]):
        t = 9000 + 2000 * k
        d += [f'{t} PAD {pad}', f'{t + 10} s0 {x} {y} _{CELL}G.{pad}']
    d += ['16000 KEY DOWN 1a', '16010 s0 80 21 _=8008D11B: ', '16100 KEY UP 1a']
    d += ['18000 KEY DOWN 13', f'18050 s0 72 11 _{PBIG}2.4', '18100 KEY UP 13']
    for k in range(8):
        d.append(f'{18700 + 650 * k} s0 100 22 _{POS}{1 + (k + 1) // 4}.{(k + 1) % 4 + 1}')
    d += ['23900 s0 14 21 _=8008D11B:G.1', '23900 s0 35 21 !=8008D19B:G.1',
          f'23900 s0 100 22 _{POS}1.1']
    d += ['25000 KEY DOWN 24', '25050 s0 101 48 _=800FE983:Current', '25100 KEY UP 24',
          '25500 KEY DOWN 24', '25550 s0 103 48 _=800FE983:Off', '25600 KEY UP 24']
    d += ['27000 VALUE']
    write('chain', d, ['top screen', 'pattern', 'Please Select Chain 1-16',
                       'PATTERN CHAIN [1] | REPEAT All', 'G.1', 'G.1', 'G.1', 'G.2',
                       'delete, G.2', 'Current', 'Off'])

# REMAIN held on the top screen, as runs/68 drew it: page 49, a list of
# names and values, one on white; let go, the top screen again. Each line
# is said, name and value together; letting go says nothing.
def remain():
    L = '=8008C665:'
    d = main_screen(0, 0, 'A-13')
    d += ['2000 KEY DOWN 24', '2000 PAGE 49', '2000 s1 CLEAR', '2000 s1 5 2 _^',
          '2000 s1 3 9 _=80194097:STORAGE AVAILABLE', f'2000 s1 93 9 _{PBIG} 14.36GB',
          f'2000 s1 1 18 !{L} PROJECT  1', f'2000 s1 81 18 !{PBIG}PROJECT_01',
          f'2000 s1 1 27 _{L} NAME', f'2000 s1 76 27 _{PBIG}New Sample',
          f'2000 s1 1 36 _{L} Color', f'2000 s1 99 36 _{PBIG}White',
          f'2000 s1 1 45 _{L} Remaining Time', f'2000 s1 99 45 _{PBIG}-00:05',
          f'2000 s1 1 54 _{L} STEREO/MONO', f'2000 s1 97 54 _{PBIG}STEREO']
    d += ['3500 KEY UP 24', '3500 PAGE 84'] + main_screen(3500, 0, 'A-13')
    d += ['5000 VALUE']
    write('remain', d, ['top screen',
                        'STORAGE AVAILABLE 14.36GB | PROJECT 1 PROJECT_01 | NAME New Sample | '
                        'Color White | Remaining Time -00:05 | STEREO/MONO STEREO'])

# SELECT PROJECT, SHIFT and SUB PAD, page 59, as runs/69 drew it: its
# title in the status bar, SEL. PROJECT(INT)-CURR:02(INT), which OpenEVV
# spelled out a letter at a time and read a dash in, and the big S E L.
# Arriving says the title, its brackets and the hyphen read as pauses and
# the stop after SEL dropped, and not S E L's "select" again. A pad loads
# its project: the bank goes back to A, not said; the message box says
# Working and Load Project 01 while the title is drawn again for the new
# project, which is not said; then the top screen.
def project():
    d = main_screen(0, 0, 'A-13')
    d += ['800 KEY DOWN 26', '800 BANK 1', '840 s0 34 2 !=80149DD5:B-1']
    d += ['2000 KEY DOWN 2a', '2300 KEY DOWN 13', '2300 PAGE 59', '2300 s0 CLEAR',
          '2300 s0 6 4 !=80156139:SEL. PROJECT(INT)-CURR:02(INT)',
          f'2300 s0 28 18 _{PBIG}S E L', '2500 KEY UP 13', '2600 KEY UP 2a']
    d += ['5000 PAD 1', '5284 BANK 0', '5288 s1 18 20 ~=8001F4BD:Working',
          '5288 s1 18 31 ~=8001F677:Load Project 01', '5288 s1 18 40 ~=8001F6B5: ',
          '5288 s0 6 4 !=80156139:SEL. PROJECT(INT)-CURR:01(INT)',
          '5290 s1 72 20 _=8006B7F9:. ', '5500 s1 72 20 _=8006B7F9:.. ']
    d += ['6025 PAGE 84'] + main_screen(6025, 0, 'A-1', big='- - - ')
    d += ['8000 VALUE']
    write('project', d, ['top screen', 'B',
                         'select PROJECT (internal), current: 02 (internal)',
                         'Working | Load Project 01', 'top screen'])

# IMPORT SAMPLE's file list, page 86, as runs/60 drew it after its menu: a
# row every 7 pixels from y 11, files at x 5, the focused one on white, the
# path at the top and DEST:PRESS PAD at the bottom, redrawn every 50 ms. A
# name too long for its column scrolls as FUN_801554E0 scrolls it: the
# focused row drawn a character further every ten draws and whole again
# once the rest fits, carrying on from where it was when the list comes
# back to the same row. The whole name is said on arriving and on coming
# back to it, and nothing as it scrolls.
def importlist():
    LIST = '=80155187:'
    files = ['file_name_example_140bpm_gmaj_whow.wav', 'kick.wav', 'snare_01.wav']
    state = {'row': -1, 'count': 0}

    def fits(name):
        return 5 * len(name) < 0x73

    def shown(row):
        name = files[row]
        if fits(name):
            return 0
        state['count'] = state['count'] + 1 if state['row'] == row else 0
        state['row'] = row
        if fits(name[state['count'] // 10:]):
            state['count'] = 0
        return state['count'] // 10

    def frame(t, sel):
        f = [f'{t} s0 FILL 1 10 126 52']
        for k, name in enumerate(files):
            f.append(f'{t} s0 SCROLL {shown(k) if k == sel else 0} 5 {11 + 7 * k} '
                     f'{"!" if k == sel else ""}{LIST}{name}')
        return f + [f'{t} s0 8 58 !=80161895:DEST:PRESS PAD']

    def menu(t):
        return [f'{t} PAGE 86', f'{t} s0 CLEAR', f'{t} s1 3 2 _=801522A7:IMPORT SAMPLE / PROJECT',
                f'{t} s0 17 22 !=8016F55B:SAMPLE', f'{t} s0 17 30 =8016F505:PROJECT',
                f'{t} s0 17 38 =8016F505:PROJECT(SX)', f'{t} s0 17 46 =8016F505:CANCEL']

    def arrive(t):
        return [f'{t} KEY DOWN 31', f'{t} s0 FILL 0 9 127 63', f'{t} s0 5 0 _=8014EAE9:SDCARD:/',
                f'{t} s0 8 58 !=80161895:DEST:PRESS PAD', f'{t + 70} KEY UP 31']

    d = main_screen(0, 0, 'A-13') + menu(1000) + arrive(3000)
    sel = 0
    for t in range(3000, 22000, 50):
        if t == 9000:
            d.append('9000 KNOB 0 1')
            sel = 1
        if t == 12000:
            d.append('12000 KNOB 0 -1')
            sel = 0
        if 16000 <= t < 17000:
            if t == 16000:
                d += ['16000 KEY DOWN 22'] + menu(16000) + ['16100 KEY UP 22']
            continue
        if t == 17000:
            d += arrive(17000)
        d += frame(t, sel)
    d.append('22000 VALUE')
    write('importlist', d, ['top screen', 'SAMPLE', '(stop)',
                            'file_name_example_140bpm_gmaj_whow.wav | destination: PRESS PAD',
                            '(stop)', 'kick.wav', '(stop)', 'file_name_example_140bpm_gmaj_whow.wav',
                            'SAMPLE', '(stop)',
                            'file_name_example_140bpm_gmaj_whow.wav | destination: PRESS PAD'])

# Folders in IMPORT SAMPLE's file list: each drawn at x 15 behind its icon,
# whose outline comes first; VALUE pressed on one goes into it, the path at
# the top naming it, with the focus on the first row, its "..", which the
# list draws as ^ and a press on goes back up, the focus then on the folder
# left, as FUN_80152D60 and FUN_800BA280 do. Only the focused row is said,
# the .. as parent folder; the path is not.
def importfolder():
    LIST, ICON = '=80155187:', '=801551CF'
    top = [('Splice_Pack', True), ('kick.wav', False)]
    inner = [('..', True), ('snare_01.wav', False), ('hat_02.wav', False)]

    def frame(t, rows, sel, path):
        f = [f'{t} s0 FILL 1 10 126 52', f'{t} s0 FILL 0 0 127 8', f'{t} s0 5 0 _=8014EAE9:{path}']
        for k, (name, folder) in enumerate(rows):
            y = 11 + 7 * k
            lit = '!' if k == sel else ''
            if folder:
                f.append(f'{t} s0 BOX 9 {y} 10 {y} {ICON}')
            f.append(f'{t} s0 SCROLL 0 {15 if folder else 5} {y} {lit}{LIST}{name}')
        return f + [f'{t} s0 8 58 !=80161895:DEST:PRESS PAD']

    d = main_screen(0, 0, 'A-13')
    d += ['1000 PAGE 86', '1000 s0 CLEAR', '1000 s1 3 2 _=801522A7:IMPORT SAMPLE / PROJECT',
          '1000 s0 17 22 !=8016F55B:SAMPLE', '1000 s0 17 30 =8016F505:PROJECT',
          '1000 s0 17 38 =8016F505:PROJECT(SX)', '1000 s0 17 46 =8016F505:CANCEL']
    d += ['3000 KEY DOWN 31', '3000 s0 FILL 0 9 127 63', '3000 s0 8 58 !=80161895:DEST:PRESS PAD',
          '3070 KEY UP 31']
    d += frame(3000, top, 0, 'SDCARD:/')
    d += ['5000 KEY DOWN 31', '5070 KEY UP 31'] + frame(5000, inner, 0, 'SDCARD:/Splice_Pack')
    d += ['6000 KNOB 0 1'] + frame(6000, inner, 1, 'SDCARD:/Splice_Pack')
    d += ['9000 KNOB 0 -1'] + frame(9000, inner, 0, 'SDCARD:/Splice_Pack')
    d += ['10000 KEY DOWN 31', '10070 KEY UP 31'] + frame(10000, top, 0, 'SDCARD:/')
    d.append('11500 VALUE')
    write('importfolder', d, ['top screen', 'SAMPLE', 'Splice_Pack | destination: PRESS PAD',
                              'parent folder', '(stop)', 'snare_01.wav', 'parent folder',
                              'Splice_Pack'])

# A folder of samples scrolled through quickly, to hear them: the list plays
# each sample the focus lands on, so a sample's name waits for the focus to
# rest two seconds, and what was being said stops as it lands on one; a
# folder is said at once. Moves 250 ms apart for a second and a half say
# nothing, the second's limit on waiting not applying to samples, and nor
# does a pause of a second; a sample whose name is longer than the reader
# keeps, losing its .wav, is still a sample. The destination line, redrawn
# with the list, is said on arriving and not again going into the folder
# or scrolling the list a page.
def importscroll():
    LIST, ICON = '=80155187:', '=801551CF'
    top = [('Drums', True), ('kick.wav', False)]
    inner = [('..', True), ('hit_01.wav', False),
             ('063_SOULSURPLUS_elevador_percussion_drum_loop_bossa_nova_jazz_main.wav', False),
             ('hit_03.wav', False), ('Loops', True), ('tail.wav', False), ('tail_2.wav', False),
             ('tail_3.wav', False)]

    def frame(t, rows, sel, path):
        top = max(0, sel - 5)
        f = [f'{t} s0 FILL 1 10 126 52', f'{t} s0 FILL 0 0 127 8', f'{t} s0 5 0 _=8014EAE9:{path}']
        for k, (name, folder) in enumerate(rows[top:top + 6]):
            y = 11 + 7 * k
            lit = '!' if top + k == sel else ''
            if folder:
                f.append(f'{t} s0 BOX 9 {y} 10 {y} {ICON}')
            f.append(f'{t} s0 SCROLL 0 {15 if folder else 5} {y} {lit}{LIST}{name}')
        return f + [f'{t} s0 8 58 !=80161895:DEST:PRESS PAD']

    d = main_screen(0, 0, 'A-13')
    d += ['1000 PAGE 86', '1000 s0 CLEAR', '1000 s1 3 2 _=801522A7:IMPORT SAMPLE / PROJECT',
          '1000 s0 17 22 !=8016F55B:SAMPLE', '1000 s0 17 30 =8016F505:PROJECT',
          '1000 s0 17 38 =8016F505:PROJECT(SX)', '1000 s0 17 46 =8016F505:CANCEL']
    d += ['3000 KEY DOWN 31', '3000 s0 FILL 0 9 127 63', '3000 s0 8 58 !=80161895:DEST:PRESS PAD',
          '3070 KEY UP 31']
    d += frame(3000, top, 0, 'SDCARD:/')
    path = 'SDCARD:/Drums'
    d += ['5000 KEY DOWN 31', '5070 KEY UP 31'] + frame(5000, inner, 0, path)
    sel = 0
    for t, step in [(6000, 1), (6250, 1), (6500, 1), (6750, -1), (7000, -1), (7250, 1), (7500, 1),
                    (8500, -1), (11500, 1), (11700, 1), (11900, 1), (14500, 1)]:
        sel += step
        d += [f'{t} KNOB 0 {step}'] + frame(t, inner, sel, path)
    d.append('17500 VALUE')
    write('importscroll', d, ['top screen', 'SAMPLE', 'Drums | destination: PRESS PAD',
                              'parent folder', '(stop)',
                              '063_SOULSURPLUS_elevador_percussion_drum_loop_bossa_nova_jazz_m',
                              '(stop)', 'Loops', '(stop)', 'tail.wav', '(stop)', 'tail_2.wav'])

# A sample imported to a pad, as runs/74 drew it: a pad pressed sets the
# destination line, VALUE brings the Import SMPL pop-up, and the destination
# line shows the progress, 100, 70, 40 and 90 per cent, before Operation
# Completed! and the line back at PRESS PAD. The progress is said once,
# without its number.
def importprogress():
    LIST, MSG = '=80155187:', '=8001F4BD:'
    files = ['kick.wav', 'snare.wav']
    dest = {'text': 'DEST:PRESS PAD'}

    def frame(t):
        f = [f'{t} s0 FILL 1 10 126 52']
        for k, name in enumerate(files):
            f.append(f'{t} s0 SCROLL 0 5 {11 + 7 * k} {"!" if k == 0 else ""}{LIST}{name}')
        return f + [f'{t} s0 8 58 !=80161895:{dest["text"]}']

    d = main_screen(0, 0, 'A-13')
    d += ['1000 PAGE 86', '1000 s0 CLEAR', '1000 s1 3 2 _=801522A7:IMPORT SAMPLE / PROJECT',
          '1000 s0 17 22 !=8016F55B:SAMPLE', '1000 s0 17 30 =8016F505:PROJECT',
          '1000 s0 17 38 =8016F505:PROJECT(SX)', '1000 s0 17 46 =8016F505:CANCEL']
    d += ['3000 KEY DOWN 31', '3000 s0 FILL 0 9 127 63', '3070 KEY UP 31']
    steps = {6000: 'DEST:PAD B14', 9000: 'WAIT: IMPORTING 100%', 9300: 'WAIT: IMPORTING 70%',
             9450: 'WAIT: IMPORTING 40%', 9650: 'WAIT: IMPORTING 90%', 10300: 'DEST:PRESS PAD'}
    for t in range(3000, 13000, 50):
        if t in steps:
            dest['text'] = steps[t]
        if t == 9000:
            d += ['9000 KEY DOWN 31', '9000 s2 BOX 1 1 90 14', '9000 s2 10 4 ~=8006BECD:Import SMPL',
                  '9010 KEY UP 31']
        if t == 9800:
            d += ['9800 s2 CLEAR', f'9800 s3 18 20 ~{MSG}Operation\\nCompleted!',
                  '9800 s3 18 40 ~=8001F6B5:']
        if t == 10300:
            d += ['10300 s3 CLEAR']
        d += frame(t)
    d.append('13000 VALUE')
    write('importprogress', d, ['top screen', 'SAMPLE', '(stop)', 'kick.wav | destination: PRESS PAD',
                                'destination: PAD B14', 'Import sample | WAIT: IMPORTING',
                                'Operation Completed!', 'destination: PRESS PAD'])

# REMAIN with a sample's name too long for its line: FUN_8008C4A0 draws it
# from x 56, not right-aligned, a character further every draw and whole
# again once the rest fits, the line wiped and drawn every 50 ms. The line
# says the whole name, once.
def remainname():
    L, NAME = '=8008C665:', '=8008C701:'
    name = 'file_name_example_140bpm_gmaj_whow'
    d = main_screen(0, 0, 'A-13')
    d += ['2000 KEY DOWN 24', '2000 PAGE 49', '2000 s1 CLEAR', '2000 s1 5 2 _^',
          '2000 s1 3 9 _=80194097:STORAGE AVAILABLE', f'2000 s1 93 9 _{PBIG} 14.36GB',
          f'2000 s1 1 18 !{L} PROJECT  1', f'2000 s1 81 18 !{PBIG}PROJECT_01',
          f'2000 s1 1 36 _{L} Color', f'2000 s1 99 36 _{PBIG}White',
          f'2000 s1 1 45 _{L} Remaining Time', f'2000 s1 99 45 _{PBIG}-00:05',
          f'2000 s1 1 54 _{L} STEREO/MONO', f'2000 s1 97 54 _{PBIG}STEREO']
    k = 0
    for t in range(2000, 7000, 50):
        d += [f'{t} s1 FILL 0 27 127 35', f'{t} s1 1 27 _{L} NAME',
              f'{t} s1 SCROLL {k} 56 27 _{NAME}{name}']
        k = k + 1 if 5 * len(name[k:]) > 0x79 - 0x3C else 0
    d += ['7000 KEY UP 24', '7000 PAGE 84'] + main_screen(7000, 0, 'A-13')
    d += ['8500 VALUE']
    write('remainname', d, ['top screen',
                            'STORAGE AVAILABLE 14.36GB | PROJECT 1 PROJECT_01 | '
                            'NAME file_name_example_140bpm_gmaj_whow | Color White | '
                            'Remaining Time -00:05 | STEREO/MONO STEREO'])

# The card swapped for another while the log is on, as for samples on a
# second card: the run's card taken out, a bank changed, another card in,
# a bank changed, the run's card back, a bank changed, and the other card
# in again for the end; ten seconds or more each way, so a write is tried
# with the card out and with the other in, and the last write, as the
# engine finishes, with the other in. Nothing goes to the other card: what
# comes while the run's card is away goes to the internal storage, is moved
# to the card once it is back, and is said to be; the last stays there.
def cardswap():
    d = main_screen(0, 0, 'A-13')
    d += ['2000 CARD OUT', '4000 BANK 1', f'4000 s0 34 2 !{PAD}B-1',
          '14000 CARD OTHER', '16000 BANK 2', f'16000 s0 34 2 !{PAD}C-1',
          '28000 CARD OUT', '29000 CARD IN', '32000 BANK 3', f'32000 s0 34 2 !{PAD}D-1',
          '41000 CARD OUT', '42000 CARD OTHER', '47000 VALUE']
    write('cardswap', d, ['top screen', 'B', 'C', 'D',
                          '@owns ready | logging to SD card, log 1 | '
                          'SD card out, logging to internal storage | '
                          'another SD card, logging to internal storage | '
                          'SD card out, logging to internal storage | '
                          'log card in, moving 1 log file from internal storage | '
                          'moved 1 log file to SD card | '
                          'SD card out, logging to internal storage | '
                          'another SD card, logging to internal storage | speech off',
                          '@has sim_LOG001_DRAWS000.TXT # the card out, at',
                          '@has sim_LOG001_DRAWS000.TXT # another card, at',
                          "@has sim_LOG001_DRAWS000.TXT # the log's card back, at",
                          '@has sim_LOG001_DRAWS000.TXT |B-1|', '@has sim_LOG001_DRAWS000.TXT |C-1|',
                          '@has sim_LOG001_DRAWS000.TXT |D-1|',
                          '@order sim_LOG001_DRAWS000.TXT |C-1| >> # the card out, at 2',
                          "@order sim_LOG001_DRAWS000.TXT # another card, at >> # the log's card back",
                          '@lacks sim_LOG001_DRAWS000.TXT # items:',
                          '@has sim_emmc_LOG001_DRAWS000.TXT # items:',
                          '@absent sim_other_LOG001_DRAWS000.TXT'],
          say='#mode changed\n#log on\n')

# The run's card out for long enough that the draw log passes a part, 48
# KB, the pads played meanwhile, which are logged and not said: both parts
# go to the internal storage as they grow, and both are moved to the card
# once it is back, before anything more is written there.
def cardlong():
    d = main_screen(0, 0, 'A-13') + ['2000 CARD OUT']
    for k, t in enumerate(range(3000, 55000, 50)):
        d.append(f'{t} s0 34 2 !{PAD}A-{k % 16 + 1}')
    d += ['56000 CARD IN', '62000 BANK 1', f'62000 s0 34 2 !{PAD}B-1', '72000 VALUE']
    write('cardlong', d, ['top screen', 'B',
                          '@owns ready | logging to SD card, log 1 | '
                          'SD card out, logging to internal storage | '
                          'log card in, moving 2 log files from internal storage | '
                          'moved 2 log files to SD card | speech off',
                          '@has sim_LOG001_DRAWS000.TXT # the card out, at',
                          '@has sim_LOG001_DRAWS000.TXT # items:',
                          '@has sim_LOG001_DRAWS001.TXT # part 1, from',
                          "@order sim_LOG001_DRAWS001.TXT # part 1, from >> # the log's card back, at",
                          '@has sim_LOG001_DRAWS001.TXT |B-1|', '@lacks sim_LOG001_DRAWS000.TXT |B-1|',
                          '@absent sim_emmc_LOG001_DRAWS000.TXT',
                          '@absent sim_emmc_LOG001_DRAWS001.TXT'],
          say='#mode changed\n#log on\n')

# A write to the run's card cut short by its coming out: nothing more is
# written to that card, what follows goes to the internal storage and
# stays there, and the card, once back, is said to need checking.
def cardcut():
    d = main_screen(0, 0, 'A-13')
    d += ['1500 BANK 1', f'1500 s0 34 2 !{PAD}B-1', '12000 CARD CUT',
          '13000 BANK 2', f'13000 s0 34 2 !{PAD}C-1', '25000 CARD IN',
          '33000 BANK 3', f'33000 s0 34 2 !{PAD}D-1', '45000 VALUE']
    write('cardcut', d, ['top screen', 'B', 'C', 'D',
                         '@owns ready | logging to SD card, log 1 | '
                         'SD card out, logging to internal storage | '
                         'log card needs checking | speech off',
                         '@lacks sim_LOG001_DRAWS000.TXT |D-1|',
                         '@has sim_emmc_LOG001_DRAWS000.TXT |D-1|'],
          say='#mode changed\n#log on\n')

# The run's card back, and known to be, just as a write falls due, a long
# message being said across both, with what came while the card was out
# still on the internal storage: that write goes there too, and is moved
# after the rest, so the card's part keeps its order.
def cardback():
    d = main_screen(0, 0, 'A-13')
    d += ['2000 CARD OUT', '4000 BANK 1', f'4000 s0 34 2 !{PAD}B-1',
          '14900 CARD IN', '15800 s3 18 20 ~=8001F4BD:Operation\\nCompleted! '
          'The samples are now on the pads of bank C', '30000 VALUE']
    write('cardback', d, ['top screen', 'B',
                          'Operation Completed! The samples are now on the pads of bank C',
                          '@owns ready | logging to SD card, log 1 | '
                          'SD card out, logging to internal storage | '
                          'log card in, moving 1 log file from internal storage | '
                          'moved 1 log file to SD card | speech off',
                          '@order sim_LOG001_DRAWS000.TXT |B-1| >> pads of bank C|',
                          '@absent sim_emmc_LOG001_DRAWS000.TXT'],
          say='#mode changed\n#log on\n')

# A start after sessions that ended with the log's card away: their files
# on the internal storage are moved to their folders on the card first, the
# part the card already holds added to in order, and the new session takes
# the next number neither the card nor the internal storage holds, here 4,
# 3 being free on the card but left on the internal storage. And the file
# list's steady messages, counted rather than logged each time.
def cardleft():
    d = ['@0 SEED B:/TALLFREE/LOG001/DRAWS003.TXT leftover three|',
         '@0 SEED B:/TALLFREE/LOG001/DRAWS004.TXT leftover four|',
         '@0 SEED B:/TALLFREE/LOG001/LOG003.TXT system three|',
         '@0 SEED B:/TALLFREE/LOG003/DRAWS000.TXT leftover of log 3|',
         '@0 SEED A:/TALLFREE/LOG001/DRAWS003.TXT on the card three|',
         '@0 SEED A:/TALLFREE/LOG002/LOG000.TXT an earlier session|']
    d += main_screen(0, 0, 'A-13')
    d += ['3000 PAGE 86'] + [f'{t} PAGE 86' for t in range(3040, 3400, 40)] + ['3500 PAGE 84']
    d += ['6000 BANK 1', f'6000 s0 34 2 !{PAD}B-1', '20000 VALUE']
    write('cardleft', d, ['top screen', 'B',
                          '@owns ready | logging to SD card, log 4 | '
                          'moving 3 log files of log 1 from internal storage | '
                          'moved 3 log files to SD card | '
                          'moving 1 log file of log 3 from internal storage | '
                          'moved 1 log file to SD card | speech off',
                          '@has sim_LOG001_DRAWS003.TXT on the card three|leftover three|',
                          '@has sim_LOG001_DRAWS004.TXT leftover four|',
                          '@has sim_LOG001_LOG003.TXT system three|',
                          '@absent sim_emmc_LOG001_DRAWS003.TXT',
                          '@absent sim_emmc_LOG001_DRAWS004.TXT',
                          '@absent sim_emmc_LOG001_LOG003.TXT',
                          '@has sim_LOG003_DRAWS000.TXT leftover of log 3|',
                          '@absent sim_emmc_LOG003_DRAWS000.TXT',
                          '@has sim_LOG004_DRAWS000.TXT |B-1|',
                          '@has sim_LOG004_DRAWS000.TXT page 86 message 1',
                          '@once sim_LOG004_DRAWS000.TXT page 86 message 3',
                          '@has sim_LOG004_DRAWS000.TXT # items:'],
          say='#mode changed\n#log on\n')

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
buttons()
fxspam()
patterns()
copybank()
ptncopy()
ptndelete()
ptncopybank()
ptnrec()
ptnexchange()
ptndeletebank()
ptnkeep()
msgfirst()
countin()
ptnrecord()
trrec()
chain()
remain()
project()
importlist()
remainname()
importfolder()
importscroll()
importprogress()
cardswap()
cardlong()
cardcut()
cardback()
cardleft()
