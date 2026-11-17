#!/bin/sh
# Smoke tests for myshell. Run from the repository root after `make`:  sh tests/run_tests.sh
SHELL_BIN=./myshell
WORK=$(mktemp -d)
pass=0; fail=0

check() { # name, input, expected substring
    out=$(cd "$WORK" && printf '%b' "$2" | "$OLDPWD/$SHELL_BIN" 2>&1)
    if printf '%s' "$out" | grep -qF -- "$3"; then
        pass=$((pass + 1)); echo "PASS  $1"
    else
        fail=$((fail + 1)); echo "FAIL  $1"; echo "      expected: $3"; echo "      got: $out"
    fi
}

check "run a command"            'echo hello world\nquit\n'                                  'hello world'
check "pipeline"                 'printf "b\\na\\nc\\n" | sort | head -1\nquit\n'             'a'
check "redirect > and <"         'echo saved > f.txt\ncat < f.txt\nquit\n'                   'saved'
check "redirect >> appends"      'echo one > g.txt\necho two >> g.txt\nwc -l < g.txt\nquit\n' '2'
check "redirect 2>"              'ls /no/such/dir 2> err.txt\ncat err.txt\nquit\n'           'No such file'
check "status after failure"     'false\nstatus\nquit\n'                                     'hello: 1'
check "status = exit code"       'ls /no/such/dir 2> e.txt\nstatus\nquit\n'                  'hello: 2'
check "cd builtin"               'cd /tmp\npwd\nquit\n'                                      '/tmp'
check "prompt builtin"           'prompt = myprompt>\nquit\n'                                'myprompt>'
check "!! repeats last command"  'echo again\n!!\nquit\n'                                    'again'
check "many arguments (no crash)" 'echo 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15\nquit\n'         '15'
check "Ctrl-D exits cleanly"     'echo bye\n'                                                'bye'

rm -rf "$WORK"
echo "----"; echo "passed: $pass  failed: $fail"
[ "$fail" -eq 0 ]
