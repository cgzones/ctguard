#!/bin/sh

set -eu

BIN_INTERVENTION=../../../src/intervention/ctguard-intervention
if ! [ -e ${BIN_INTERVENTION} ]; then
    echo "Could not find binary at '${BIN_INTERVENTION}'!"
    exit 1
fi

BIN_RESEARCH=../../../src/research/ctguard-research
if ! [ -e ${BIN_RESEARCH} ]; then
    echo "Could not find binary at '${BIN_RESEARCH}'!"
    exit 1
fi

BIN_LOGSCAN=../../../src/logscan/ctguard-logscan
if ! [ -e ${BIN_LOGSCAN} ]; then
    echo "Could not find binary at '${BIN_LOGSCAN}'!"
    exit 1
fi

cleanup () {
    rm -f logscan.state input.log alerts.log research.sock intervention_action.log intervention.sock
}

cleanup

chmod 640 intervention.conf logscan.conf research.conf rules.xml
touch input.log intervention_action.log

${BIN_INTERVENTION} --cfg-file intervention.conf -f -x &
intervention_pid=$!
echo "intervention daemon running with pid ${intervention_pid}."

sleep 1

${BIN_RESEARCH} --cfg-file research.conf -f -x &
research_pid=$!
echo "research daemon running with pid ${research_pid}."

sleep 1

${BIN_LOGSCAN} --cfg-file logscan.conf -f -x &
logscan_pid=$!
echo "logscan daemon running with pid ${logscan_pid}."

trap "kill -9 ${intervention_pid}; kill -9 ${research_pid}; kill -9 ${logscan_pid}" 0 2

sleep 1

echo "Sep 24 12:10:03 desktopdebian unix_chkpwd[12490]: password check failed for user (xy?)" >> input.log
echo "Sep 24 12:10:03 desktopdebian unix_chkpwd[12490]: password check failed for user (whitelist)" >> input.log
echo "Sep 24 12:10:03 desktopdebian unix_chkpwd[12490]: password check failed for user (christian)" >> input.log

sleep 4

if ! ps -p ${logscan_pid} > /dev/null; then
    echo "logscan daemon with pid ${logscan_pid} not running anymore!\nFAILURE!"
    exit 1
fi

kill -INT ${logscan_pid}

if ! ps -p ${research_pid} > /dev/null; then
    echo "research daemon with pid ${research_pid} not running anymore!\nFAILURE!"
    exit 1
fi

kill -INT ${research_pid}

if ! ps -p ${intervention_pid} > /dev/null; then
    echo "intervention daemon with pid ${intervention_pid} not running anymore!\nFAILURE!"
    exit 1
fi

kill -INT ${intervention_pid}
trap - 0 2

sleep 2

echo "Comapring expected vs actual alert output:"
diff -u test.output.expected alerts.log

echo "Comparing expected vs actual intervention action:"
diff -u intervention_action.expected intervention_action.log

cleanup

echo "SUCCESS!"
