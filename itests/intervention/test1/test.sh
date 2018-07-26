#!/bin/sh

set -eu

cleanup () {
  rm -f logscan.state input.log alerts.log research.sock intervention_action.log
}

cleanup

touch input.log intervention_action.log

../../../ctguard-intervention --cfg-file intervention.conf -f -x &
intervention_pid=$!
echo "intervention daemon running with pid ${intervention_pid}\n"

sleep 1

../../../ctguard-research --cfg-file research.conf -f -x &
research_pid=$!
echo "research daemon running with pid ${research_pid}\n"

sleep 1

../../../ctguard-logscan --cfg-file logscan.conf -f -x &
logscan_pid=$!
echo "logscan daemon running with pid ${logscan_pid}\n"

sleep 1

echo "Sep 24 12:10:03 desktopdebian unix_chkpwd[12490]: password check failed for user (xy?)" >> input.log
echo "Sep 24 12:10:03 desktopdebian unix_chkpwd[12490]: password check failed for user (whitelist)" >> input.log
echo "Sep 24 12:10:03 desktopdebian unix_chkpwd[12490]: password check failed for user (christian)" >> input.log

sleep 4

logscan_pid2=`pidof ctguard-logscan`
if [ "X${logscan_pid}" != "X${logscan_pid2}" ]; then
  echo "logscan daemon not running anymore!!\n'${logscan_pid}' vs '${logscan_pid2}'\nFAILURE!!\n\n";
  exit 1
fi

kill -INT ${logscan_pid}

research_pid2=`pidof ctguard-research`
if [ "X${research_pid}" != "X${research_pid2}" ]; then
  echo "research daemon not running anymore!!\n'${research_pid}' vs '${research_pid2}'\nFAILURE!!\n\n";
  exit 1
fi

kill -INT ${research_pid}

intervention_pid2=`pidof ctguard-intervention`
if [ "X${intervention_pid}" != "X${intervention_pid2}" ]; then
  echo "intervention daemon not running anymore!!\n'${intervention_pid}' vs '${intervention_pid2}'\nFAILURE!!\n\n";
  exit 1
fi

kill -INT ${intervention_pid}

sleep 2

echo "Comapring expected vs actual alert output:"
diff -u test.output.expected alerts.log

echo "Comparing expected vs actual intervention action:"
diff -u intervention_action.expected intervention_action.log
cleanup

echo "\n\n\nsuccess!!!\n\n"
