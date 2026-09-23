#!/usr/bin/env python3
"""Minimal stand-in for the mms simulator, for testing the adapter without the GUI.

Speaks the mms stdin/stdout protocol against a mazefiles .txt maze, issues a
reset after each run, and reports crashes. Usage:
    python3 fake_mms.py ../mazefiles/classic/uk2016f.txt ./mms_adapter
"""
import subprocess, sys

def load(path):
    lines = [l.rstrip('\n') for l in open(path) if l.strip()]
    walls = {}
    for y in range(16):
        top, mid, bot = lines[2*(15-y)], lines[2*(15-y)+1], lines[2*(15-y)+2]
        for x in range(16):
            walls[(x, y)] = dict(n=top[4*x+1:4*x+4] == '---', s=bot[4*x+1:4*x+4] == '---',
                                 w=mid[4*x] == '|', e=mid[4*x+4] == '|')
    return walls

DIRS = 'nesw'
DELTA = dict(n=(0, 1), e=(1, 0), s=(0, -1), w=(-1, 0))

def main():
    walls = load(sys.argv[1])
    p = subprocess.Popen(sys.argv[2:], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
    x, y, h = 0, 0, 0
    polls_since_move = 0
    runs, moves, crashes = 1, 0, 0
    def reply(s): p.stdin.write(s + '\n'); p.stdin.flush()
    while True:
        line = p.stdout.readline()
        if not line: break
        cmd = line.split()
        if not cmd: continue
        c = cmd[0]
        if c == 'mazeWidth' or c == 'mazeHeight': reply('16')
        elif c == 'wallFront': reply('true' if walls[(x, y)][DIRS[h]] else 'false')
        elif c == 'wallLeft':  reply('true' if walls[(x, y)][DIRS[(h+3) % 4]] else 'false')
        elif c == 'wallRight': reply('true' if walls[(x, y)][DIRS[(h+1) % 4]] else 'false')
        elif c == 'turnLeft':  h = (h + 3) % 4; reply('ack')
        elif c == 'turnRight': h = (h + 1) % 4; reply('ack')
        elif c == 'moveForward':
            if walls[(x, y)][DIRS[h]]: crashes += 1; reply('crash')
            else:
                dx, dy = DELTA[DIRS[h]]; x += dx; y += dy; moves += 1; polls_since_move = 0; reply('ack')
        elif c == 'wasReset':
            # The adapter polls once per cell while driving, and spins on it
            # when a run is over. Two polls with no move in between = run over:
            # grant the reset, as the user would by clicking Reset in mms.
            polls_since_move += 1
            reply('true' if polls_since_move >= 2 else 'false')
        elif c == 'ackReset':
            polls_since_move = 0; x, y, h = 0, 0, 0; runs += 1
            reply('ack')  # the real mms replies to ackReset
            if runs > 6: break
        elif c in ('setWall', 'clearWall', 'setColor', 'clearColor', 'setText', 'clearText', 'clearAllColor', 'clearAllText'):
            pass  # no response
        else:
            reply('')
    p.kill()
    print(f'runs={runs} moves={moves} crashes={crashes} final=({x},{y})')
    sys.exit(1 if crashes else 0)

if __name__ == '__main__':
    main()
