#!/bin/bash

# Pure Bash test suite. 

echo "Testing msh..."
PASS=0
FAIL=0

# Helper: run msh with input, timeout after 3 seconds
run_msh() {
    echo -e "$1" | ./msh 2>/dev/null &
    PID=$!
    sleep 1
    if kill -0 $PID 2>/dev/null; then
        kill $PID 2>/dev/null
        wait $PID 2>/dev/null
        echo ""  # empty result
    else
        wait $PID 2>/dev/null
    fi
}

# Build first
gcc -o msh main.c 2>/dev/null
if [ $? -ne 0 ]; then
    echo "✗ BUILD FAILED"
    exit 1
fi
echo "✓ build passed"

# Test: pipe
result=$(run_msh "ls | wc -l\nexit")
if echo "$result" | grep -q "main.c"; then
    echo "✗ pipe FAILED (wc didn't receive pipe)"; ((FAIL++))
else
    count=$(echo "$result" | grep -oE '[0-9]+' | head -1)
    if [ -n "$count" ] && [ "$count" -gt 0 ]; then
        echo "✓ pipe"; ((PASS++))
    else
        echo "✗ pipe FAILED"; ((FAIL++))
    fi
fi

# Test: cd
result=$(run_msh "cd /tmp\npwd\nexit")
if echo "$result" | grep -q "/tmp"; then
    echo "✓ cd"; ((PASS++))
else
    echo "✗ cd FAILED"; ((FAIL++))
fi

# Test: exit
# Why this isnt using run_msh: 
# Needs to check whether the process is still alive after sending exit.
# The run_msh helper auto-kills after the timeout, can't tell if the shell quit on its own or got killed.
echo "exit" | ./msh >/dev/null 2>&1 &
PID=$!
sleep 1
if kill -0 $PID 2>/dev/null; then
    kill $PID 2>/dev/null
    echo "✗ exit FAILED (shell didn't quit)"; ((FAIL++))
else
    wait $PID
    if [ $? -eq 0 ]; then
        echo "✓ exit"; ((PASS++))
    else
        echo "✗ exit FAILED"; ((FAIL++))
    fi
fi

# Test: empty input
result=$(run_msh "\nexit")
if [ $? -eq 0 ]; then
    echo "✓ empty input"; ((PASS++))
else
    echo "✗ empty input FAILED"; ((FAIL++))
fi

# Test: external command
result=$(run_msh "echo hello\nexit")
if echo "$result" | grep -q "hello"; then
    echo "✓ external command"; ((PASS++))
else
    echo "✗ external command FAILED"; ((FAIL++))
fi

echo ""
echo "Results: $PASS passed, $FAIL failed"
