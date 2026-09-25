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
CELL, FXNAME = '=80145EDF:', '=80146135:'   # the effects grid's cells and title
ROWLABEL, TABS = '=80148AFD:', '=80121751:' # the SYSTEM page's rows and tabs

def main_screen(t, surf, bank_pad, bus='BUS-1', big='9 4 '):
    bx = 56 if bus == 'BUS-1' else 60
    return [f'{t} s{surf} 12 2 !{PATTERN}P-2', f'{t} s{surf} 34 2 !{PAD}{bank_pad}',
            f'{t} s{surf} {bx} 2 !{BUS}{bus}', f'{t} s{surf} 84 2 !{FIXVEL}Fix',
            f'{t} s{surf} 104 2 ! DC ',
            f'{t} s{surf} {53 if big == "9 4 " else 24} 14 _{big}']

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
          f'1000 s0 9 3 _{LABEL}CUTOFF', '1000 s0 23 33 _Hz ', '1000 s0 11 18 _637',
          f'1000 s0 44 3 _{LABEL}RESONANCE', '1000 s0 73 33 _ ', '1000 s0 54 18 _   1',
          f'1000 s0 95 3 _{LABEL}DRIVE', '1000 s0 115 33 _ ', '1000 s0 96 18 _   0',
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

# The main screen as the last run drew it: pad hits wipe and redraw the
# status line, changing the bus and the big field; then a bank change; then
# a STOP message; then the SD menu, which must not read the old message.
def mainpads():
    d = main_screen(0, 0, 'A-13')
    d += pad_hit(1000, 0, 'A-14', 'DRY', '- - - ')
    d += pad_hit(2000, 0, 'A-13', 'BUS-1', '9 4 ')
    d += pad_hit(3000, 0, 'A-10', 'DRY', '- - - ')
    d += pad_hit(4000, 0, 'B-10', 'DRY', '- - - ')
    d += [f'5000 s1 9 4 ~{TOAST}STOP']
    d += pad_hit(5000, 0, 'B-10', 'DRY', '- - - ')
    d += ['9000 s0 CLEAR', '9000 s2 5 2 ^_IMPORT/EXPORT MENU',
          '9000 s0 17 22 !IMPORT from SD-CARD', '9000 s0 17 30 EXPORT to SD-CARD',
          '10000 VALUE']
    write('mainpads', d, ['A 13', 'B 10', 'STOP',
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
    write('submenu', d, ['A 13', 'EXPORT SAMPLE/PROJ./MULTIPAD | SAMPLE', 'PROJECT'])

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
    write('recscale', d, ['A 13', 'Select PAD for RECORDING', 'A 14',
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
    write('system', d, ['A 13',
                        'UTILITY MENU | SYSTEM', 'PAD SET', 'EFX SET',
                        'GENERAL | Edit Knob Mode Direct',
                        'EFX Knob Mode Direct', 'Load Project Last',
                        'CLICK | Output Assign ON', 'OFF'])

# Leaving a menu for the main screen: its factory is called and it redraws
# everything unchanged on its own layer, which only the page event shows.
def pages():
    d = main_screen(0, 0, 'A-13')
    d += ['1000 PAGE 30', '1000 s1 5 2 ^_IMPORT/EXPORT MENU', '1000 s2 17 22 !IMPORT from SD-CARD',
          '1000 s2 17 30 EXPORT to SD-CARD']
    d += ['3000 PAGE 0'] + main_screen(3000, 0, 'A-13')
    d += ['5000 VALUE']
    write('pages', d, ['A 13', 'IMPORT/EXPORT MENU | IMPORT from SD-CARD', 'A 13'])

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
    write('dialog', d, ['A 13', 'IMPORT/EXPORT MENU | FORMAT SD-CARD',
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
    write('sdreturn', d, ['A 13', 'IMPORT/EXPORT MENU | IMPORT from SD-CARD',
                          'EXPORT to SD-CARD', 'EXPORT SAMPLE/PROJ./MULTIPAD | SAMPLE',
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
    write('settings', d, ['A 13', 'Speech settings | Speed 50', 'Pitch 65', '65', '70',
                          '75', 'Pitch', 'Settings saved | A 13'])

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
