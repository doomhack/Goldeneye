#!/usr/bin/env python3
"""Run a ROM headless in mupen64plus with scripted controller input.

Drives libmupen64plus directly through ctypes. Inputs come from a script timed
in frames (frame callbacks from the core), so a run is repeatable.

Usage:
  m64run.py ROM --frames N [--script FILE] [--pads N] [--shots F,F,...]
            [--outdir DIR] [--gfx rice|none] [--map MAPFILE --watch SYMBOL]

Script lines (blank lines and # comments ignored):
  FROM TO PAD BUTTONS [STICKX STICKY]
  FROM/TO are inclusive frame numbers, PAD is 1-4, BUTTONS is '-' or names
  joined with '+': A B Z START L R CU CD CL CR DU DD DL DR.
  Example: '300 302 1 START' presses Start on pad 1 for three frames.

If no frame arrives for --stall seconds the game counts as hung: the runner saves
OUTDIR/hang-ram.bin (8 MB RDRAM) and the CPU PC. OUTDIR/result.json gives the outcome.

--watch reads a 32-bit RAM word at SYMBOL (address from the linker map) every
frame and logs each change to OUTDIR/watch.txt as 'frame value'.
"""
import argparse
import json
import ctypes as C
import os
import re
import shutil
import sys
import tempfile
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
LIBDIR = '/usr/lib/x86_64-linux-gnu'
PLUGINDIR = LIBDIR + '/mupen64plus'
DATADIR = '/usr/share/games/mupen64plus'

M64CMD_ROM_OPEN = 1
M64CMD_EXECUTE = 5
M64CMD_STOP = 6
M64CMD_SET_FRAME_CALLBACK = 15
M64CMD_TAKE_NEXT_SCREENSHOT = 16
M64CMD_CORE_STATE_SET = 17
M64CORE_SPEED_LIMITER = 5
M64PLUGIN_RSP, M64PLUGIN_GFX, M64PLUGIN_AUDIO, M64PLUGIN_INPUT = 1, 2, 3, 4
M64TYPE_INT, M64TYPE_BOOL, M64TYPE_STRING = 1, 3, 4
M64P_DBG_PTR_RDRAM = 1
M64P_DBG_PTR_VI_REG = 4
M64P_CPU_PC = 1

BUTTONS = {
    'DR': 0x0001, 'DL': 0x0002, 'DD': 0x0004, 'DU': 0x0008, 'START': 0x0010,
    'Z': 0x0020, 'B': 0x0040, 'A': 0x0080, 'CR': 0x0100, 'CL': 0x0200,
    'CD': 0x0400, 'CU': 0x0800, 'R': 0x1000, 'L': 0x2000,
}


def parse_script(path):
    events = []
    for lineno, line in enumerate(open(path), 1):
        line = line.split('#', 1)[0].split()
        if not line:
            continue
        if len(line) not in (4, 6):
            sys.exit(f'{path}:{lineno}: expected FROM TO PAD BUTTONS [STICKX STICKY]')
        start, end, pad = int(line[0]), int(line[1]), int(line[2]) - 1
        buttons = 0
        if line[3] != '-':
            for name in line[3].upper().split('+'):
                buttons |= BUTTONS[name]
        x, y = (int(line[4]), int(line[5])) if len(line) == 6 else (0, 0)
        events.append((start, end, pad, buttons, x, y))
    return events


def pad_values(events, frame):
    values = [0, 0, 0, 0]
    for start, end, pad, buttons, x, y in events:
        if start <= frame <= end:
            values[pad] |= buttons
            if x or y:
                values[pad] = (values[pad] & 0xffff) | ((x & 0xff) << 16) | ((y & 0xff) << 24)
    return values


def save_framebuffer(ram, origin, width, path):
    """Save the 16-bit framebuffer the VI is showing, e.g. the game's crash screen.

    mupen keeps RDRAM as native-endian 32-bit words, so each word is byte-swapped back first.
    """
    if not width:
        return
    height = 240
    words = (width * height * 2 + 3) // 4
    raw = bytearray()
    for i in range(words):
        raw += ram[origin + i * 4:origin + i * 4 + 4][::-1]
    try:
        from PIL import Image
    except ImportError:
        return
    img = Image.new('RGB', (width, height))
    px = img.load()
    for y in range(height):
        for x in range(width):
            v = (raw[(y * width + x) * 2] << 8) | raw[(y * width + x) * 2 + 1]
            px[x, y] = ((v >> 11 & 31) << 3, (v >> 6 & 31) << 3, (v >> 1 & 31) << 3)
    img.save(path)


def symbol_address(mapfile, symbol):
    pattern = re.compile(r'^\s+0x([0-9a-f]+)\s+' + re.escape(symbol) + r'\s*$')
    for line in open(mapfile):
        m = pattern.match(line)
        if m:
            return int(m.group(1), 16)
    sys.exit(f'{symbol} not found in {mapfile}')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('rom')
    ap.add_argument('--frames', type=int, required=True)
    ap.add_argument('--script')
    ap.add_argument('--pads', type=int, default=1)
    ap.add_argument('--shots', default='')
    ap.add_argument('--outdir', default='testrun-out')
    ap.add_argument('--gfx', choices=['rice', 'none'], default='rice')
    ap.add_argument('--map')
    ap.add_argument('--watch')
    ap.add_argument('--poke', action='append', default=[], metavar='SYMBOL=VALUE',
                    help='write a 32-bit value to a global at the first frame (needs --map)')
    ap.add_argument('--statelog', action='store_true',
                    help='turn on the game\'s state checksums (src/game/replay/testhooks.c) and write '
                         'OUTDIR/state.txt: timer exact coarse, one line per game frame')
    ap.add_argument('--demo', type=int, metavar='N',
                    help='play attract-mode demo N (0-13) and stop when it ends (needs --map)')
    ap.add_argument('--pak', choices=['none', 'controller', 'rumble'], default='none',
                    help='what is plugged into each controller (real consoles usually have a pak)')
    ap.add_argument('--dumpram', action='store_true',
                    help='save RDRAM to OUTDIR/end-ram.bin when the run ends')
    ap.add_argument('--readwatch', action='append', default=[], metavar='START:END',
                    help='log every CPU read or write of the virtual address range START..END (hex, '
                         'END exclusive) to OUTDIR/access.txt; runs the slow interpreter')
    ap.add_argument('--cachesim', metavar='FILE',
                    help='model the VR4300 data cache and write per-line and per-PC miss counts to FILE '
                         '(needs a mupen64plus core built with tools/cachesim; runs the slow interpreter)')
    ap.add_argument('--cachesim-from', type=int, default=0, metavar='N',
                    help='start counting N frames after the demo starts (or at frame N without --demo)')
    ap.add_argument('--cachesim-frames', type=int, default=600, metavar='N',
                    help='count for N frames, then save the counts and RAM and stop')
    ap.add_argument('--cachesim-watch', metavar='[*]SYMBOL:SIZE',
                    help='also count accesses and misses per 4-byte word over SIZE bytes at SYMBOL '
                         '(or, with *, at the address SYMBOL holds when counting starts); saved to FILE.words')
    ap.add_argument('--stall', type=float, default=20,
                    help='seconds without a new frame before the run counts as hung')
    args = ap.parse_args()

    os.makedirs(args.outdir, exist_ok=True)
    events = parse_script(args.script) if args.script else []
    shots = {int(s) for s in args.shots.split(',') if s}
    def addr(symbol):
        if not args.map:
            sys.exit(f'--map is needed to find {symbol}')
        return symbol_address(args.map, symbol) & 0x1fffffff

    watch_addr = addr(args.watch) if args.watch else None
    pokes = []
    for item in args.poke:
        symbol, value = item.split('=')
        pokes.append((addr(symbol), int(value, 0)))
    if args.statelog or args.demo is not None:
        pokes.append((addr('g_TestHooksEnabled'), 1))
    if args.demo is not None:
        pokes.append((addr('g_TestForceDemo'), args.demo))
        demo_flag = addr('is_ramrom_flag')
        desync_flag = addr('g_TestDemoDesynced')
    if args.statelog or args.demo is not None:
        log_count = addr('g_TestStateLogCount')
        log_base = addr('g_TestStateLog')
        state_log = open(os.path.join(args.outdir, 'state.txt'), 'w')
    else:
        state_log = None

    # mupen applies a hack to ROMs named GOLDENEYE that breaks modified builds, so rename it.
    rom = bytearray(open(args.rom, 'rb').read())
    rom[0x20:0x34] = b'GE007 TESTRUN'.ljust(20)

    configdir = tempfile.mkdtemp(prefix='m64cfg-')
    shotdir = os.path.abspath(args.outdir)
    core = C.CDLL(os.environ.get('M64RUN_CORE', LIBDIR + '/libmupen64plus.so.2'), mode=C.RTLD_GLOBAL)
    debug_cb = C.CFUNCTYPE(None, C.c_void_p, C.c_int, C.c_char_p)(
        lambda ctx, level, msg: level <= 2 and print('m64:', msg.decode(errors='replace'), file=sys.stderr))
    state_cb = C.CFUNCTYPE(None, C.c_void_p, C.c_int, C.c_int)(lambda ctx, param, value: None)
    core.CoreStartup(0x020001, configdir.encode(), DATADIR.encode(), None, debug_cb, None, state_cb)

    def config_set(section, name, kind, value):
        handle = C.c_void_p()
        core.ConfigOpenSection(section.encode(), C.byref(handle))
        if kind == M64TYPE_STRING:
            core.ConfigSetParameter(handle, name.encode(), kind, C.c_char_p(value.encode()))
        else:
            v = C.c_int(value)
            core.ConfigSetParameter(handle, name.encode(), kind, C.byref(v))

    config_set('Core', 'ScreenshotPath', M64TYPE_STRING, shotdir)
    config_set('Core', 'R4300Emulator', M64TYPE_INT, 0 if args.readwatch or args.cachesim else 2)
    if args.readwatch:
        config_set('Core', 'EnableDebugger', M64TYPE_BOOL, 1)
    config_set('Core', 'OnScreenDisplay', M64TYPE_BOOL, 0)
    config_set('Video-General', 'Fullscreen', M64TYPE_BOOL, 0)
    config_set('Video-General', 'ScreenWidth', M64TYPE_INT, 640)
    config_set('Video-General', 'ScreenHeight', M64TYPE_INT, 480)

    rombuf = C.create_string_buffer(bytes(rom), len(rom))
    if core.CoreDoCommand(M64CMD_ROM_OPEN, len(rom), rombuf) != 0:
        sys.exit('ROM open failed')

    # The core wants plugins attached in this order: video, audio, input, RSP.
    built = os.path.join(HERE, 'input-script.so')
    if not os.path.exists(built) or os.path.getmtime(built) < os.path.getmtime(os.path.join(HERE, 'input-script.c')):
        os.system(f'cc -shared -fPIC -O2 -o {built} {HERE}/input-script.c')
    plugins = []
    if args.gfx == 'rice':
        plugins.append((M64PLUGIN_GFX, PLUGINDIR + '/mupen64plus-video-rice.so'))
    plugins.append((M64PLUGIN_INPUT, built))
    plugins.append((M64PLUGIN_RSP, PLUGINDIR + '/mupen64plus-rsp-hle.so'))
    inputlib = None
    for kind, path in plugins:
        lib = C.CDLL(path)
        lib.PluginStartup(C.c_void_p(core._handle), None, debug_cb)
        if kind == M64PLUGIN_INPUT:
            inputlib = lib
            lib.ScriptSetPadCount(args.pads)
            lib.ScriptSetPak({'none': 1, 'controller': 2, 'rumble': 5}[args.pak])
        if core.CoreAttachPlugin(kind, C.c_void_p(lib._handle)) != 0:
            sys.exit(f'attaching {path} failed')

    off = C.c_int(0)
    core.CoreDoCommand(M64CMD_CORE_STATE_SET, M64CORE_SPEED_LIMITER, C.byref(off))
    core.DebugMemGetPointer.restype = C.c_void_p

    watch_log = open(os.path.join(args.outdir, 'watch.txt'), 'w') if watch_addr is not None else None

    # Memory breakpoints: the core pauses on each access and calls dbg_update, which notes
    # the access and lets it run on.
    class Breakpoint(C.Structure):
        _fields_ = [('address', C.c_uint32), ('endaddr', C.c_uint32), ('flags', C.c_uint)]

    accesses = {}

    def dbg_init():
        for item in args.readwatch:
            start, end = (int(x, 16) & 0x1fffffff for x in item.split(':'))  # breakpoints take physical addresses
            bkp = Breakpoint(start, end - 1, 0x01 | 0x02 | 0x04)  # enabled, read, write
            core.DebugBreakpointCommand(2, 0, C.byref(bkp))  # M64P_BKP_CMD_ADD_STRUCT
        core.DebugSetRunState(2)  # M64P_DBG_RUNSTATE_RUNNING

    def dbg_update(pc):
        flags, accessed = C.c_uint32(), C.c_uint32()
        core.DebugBreakpointTriggeredBy(C.byref(flags), C.byref(accessed))
        key = (pc, accessed.value, 'write' if flags.value & 0x04 else 'read')
        if key not in accesses:
            accesses[key] = progress['frame']
        core.DebugSetRunState(2)
        core.DebugStep()

    dbg_callbacks = (C.CFUNCTYPE(None)(dbg_init), C.CFUNCTYPE(None, C.c_uint)(dbg_update),
                     C.CFUNCTYPE(None)(lambda: None))
    if args.readwatch:
        core.DebugSetCallbacks(*dbg_callbacks)
    last = [None]
    rdram = [None]

    progress = {'frame': 0, 'time': time.monotonic()}

    result = {'status': 'ok'}
    seen = {'log': 0, 'demo': False}

    def word(ram, address):
        return ram[address >> 2]

    def on_frame(index):
        frame = index + 1
        progress['frame'] = frame
        progress['time'] = time.monotonic()
        if rdram[0] is None:
            rdram[0] = C.cast(core.DebugMemGetPointer(M64P_DBG_PTR_RDRAM), C.POINTER(C.c_uint32))
        ram = rdram[0]
        if frame == 1:
            for address, value in pokes:
                ram[address >> 2] = value & 0xffffffff
        if state_log:
            count = word(ram, log_count)
            for n in range(max(seen['log'], count - 64), count):
                base = log_base + (n % 64) * 12
                state_log.write(f'{word(ram, base)} {word(ram, base + 4):08x} {word(ram, base + 8):08x}\n')
            seen['log'] = count
        if args.demo is not None:
            playing = word(ram, demo_flag) != 0
            if playing:
                seen['demo'] = True
            elif seen['demo']:
                result['demo'] = 'desynced' if word(ram, desync_flag) else 'ended'
                core.CoreDoCommand(M64CMD_STOP, 0, None)
        if args.cachesim:
            cachesim_frame(frame, ram)
        if os.environ.get('M64RUN_TRACE'):
            print('frame', frame, flush=True)
        for pad, value in enumerate(pad_values(events, frame)):
            inputlib.ScriptSetPad(pad, value)
        if watch_log:
            value = ram[watch_addr >> 2]
            if value != last[0]:
                watch_log.write(f'{frame} {value:08x}\n')
                last[0] = value
        if frame in shots:
            core.CoreDoCommand(M64CMD_TAKE_NEXT_SCREENSHOT, 0, None)
        if frame >= args.frames + 2:  # +2 so a screenshot on the last frame gets written
            core.CoreDoCommand(M64CMD_STOP, 0, None)

    # Cache model: count from the start frame for --cachesim-frames frames, then save the counts,
    # the game frame count and RAM (heap pointers are needed to name the misses) and stop.
    cs = {'start': None, 'done': False}

    def cachesim_frame(frame, ram):
        if cs['done']:
            return
        if cs['start'] is None:
            began = seen['demo'] if args.demo is not None else True
            if began:
                cs['begin'] = cs.get('begin', frame)
                if frame >= cs['begin'] + args.cachesim_from:
                    # The state checksums walk every prop, guard and player each frame, which
                    # would add misses the game itself doesn't have, so they stop here.
                    if args.statelog or args.demo is not None:
                        ram[addr('g_TestHooksEnabled') >> 2] = 0
                    if args.cachesim_watch:
                        name, size = args.cachesim_watch.split(':')
                        where = addr(name.lstrip('*'))
                        if name.startswith('*'):
                            where = word(ram, where) & 0x1fffffff
                        core.cachesim_watch(C.c_uint32(where), C.c_uint32(int(size, 0)))
                    core.cachesim_reset()
                    core.cachesim_enable(1)
                    cs['start'] = frame
                    cs['tick0'] = word(ram, addr('g_LvTickCount'))
            return
        if frame >= cs['start'] + args.cachesim_frames:
            core.cachesim_enable(0)
            core.cachesim_dump(os.path.abspath(args.cachesim).encode())
            core.cachesim_dump_blocks((os.path.abspath(args.cachesim) + '.blocks').encode())
            if hasattr(core, 'cachesim_dump_usage'):
                core.cachesim_dump_usage((os.path.abspath(args.cachesim) + '.usage').encode())
            if args.cachesim_watch:
                core.cachesim_dump_watch((os.path.abspath(args.cachesim) + '.words').encode())
            open(os.path.abspath(args.cachesim) + '.ram', 'wb').write(
                C.string_at(core.DebugMemGetPointer(M64P_DBG_PTR_RDRAM), 8 << 20))
            result['cachesim'] = {'vi_frames': args.cachesim_frames, 'start_frame': cs['start'],
                                  'game_frames': word(ram, addr('g_LvTickCount')) - cs['tick0']}
            json.dump(result['cachesim'], open(os.path.abspath(args.cachesim) + '.json', 'w'))
            cs['done'] = True
            core.CoreDoCommand(M64CMD_STOP, 0, None)

    # A crashed or hung game stops producing frames, so a watchdog ends the run.

    def watchdog():
        while True:
            time.sleep(1)
            if time.monotonic() - progress['time'] > args.stall:
                core.DebugGetCPUDataPtr.restype = C.c_void_p
                pc = C.cast(core.DebugGetCPUDataPtr(M64P_CPU_PC), C.POINTER(C.c_uint32))[0]
                result.update(status='hung', frame=progress['frame'], pc=f'{pc:08x}')
                ram = C.string_at(core.DebugMemGetPointer(M64P_DBG_PTR_RDRAM), 8 << 20)
                open(os.path.join(args.outdir, 'hang-ram.bin'), 'wb').write(ram)
                vi = C.cast(core.DebugMemGetPointer(M64P_DBG_PTR_VI_REG), C.POINTER(C.c_uint32))
                save_framebuffer(ram, vi[1] & 0xffffff, vi[2] & 0xfff, os.path.join(args.outdir, 'hang-screen.png'))
                core.CoreDoCommand(M64CMD_STOP, 0, None)
                return

    if args.cachesim:
        # Note every heap allocation from boot on, so misses can be named by who allocated.
        for fn in ('mempAlloc', 'memaAlloc'):
            core.cachesim_track_alloc(C.c_uint32(symbol_address(args.map, fn)))
    frame_cb = C.CFUNCTYPE(None, C.c_uint)(on_frame)
    core.CoreDoCommand(M64CMD_SET_FRAME_CALLBACK, 0, frame_cb)
    threading.Thread(target=watchdog, daemon=True).start()
    core.CoreDoCommand(M64CMD_EXECUTE, 0, None)
    if args.dumpram and result.get('status') != 'hung':
        ram = C.string_at(core.DebugMemGetPointer(M64P_DBG_PTR_RDRAM), 8 << 20)
        open(os.path.join(args.outdir, 'end-ram.bin'), 'wb').write(ram)
    if args.readwatch:
        with open(os.path.join(args.outdir, 'access.txt'), 'w') as f:
            for (pc, address, kind), frame in sorted(accesses.items(), key=lambda kv: kv[1]):
                f.write(f'frame {frame} pc {pc:08x} {kind} {address:08x}\n')
    if watch_log:
        watch_log.close()
    if state_log:
        state_log.close()
    if args.demo is not None and 'demo' not in result and result['status'] == 'ok':
        result['demo'] = 'did not end'
    result.setdefault('frame', progress['frame'])
    json.dump(result, open(os.path.join(args.outdir, 'result.json'), 'w'))
    print('result:', json.dumps(result))
    shutil.rmtree(configdir, ignore_errors=True)
    os._exit(0)


if __name__ == '__main__':
    main()
