#!/bin/bash
# Run fake_mms over every maze in parallel; one result line per maze.
cd "$(dirname "$0")"
ls ../mazefiles/classic/*.txt | xargs -P 8 -I{} sh -c 'r=$(python3 fake_mms.py "{}" ./mms_adapter 2>/dev/null); echo "$(basename {}) $r"' > sweep_results.txt
echo "SWEEP DONE: $(wc -l < sweep_results.txt) mazes"
grep -vc 'crashes=0' sweep_results.txt | sed 's/^/non-zero-crash lines: /'
