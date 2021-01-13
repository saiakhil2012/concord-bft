#!/bin/bash -e

cleanup() {
    killall -q skvbc_replica || true
    rm -rf simpleKVBTests_DB_*
    rm -rf radio_config_*
}

cleanup

../../../tools/GenerateConcordKeys -f 1 -n 4 -r 0 -o radio_config_

../TesterReplica/skvbc_replica -k radio_config_ -i 0 -p -g &
../TesterReplica/skvbc_replica -k radio_config_ -i 1 -p -g &
../TesterReplica/skvbc_replica -k radio_config_ -i 2 -p -g &
../TesterReplica/skvbc_replica -k radio_config_ -i 3 -p -g &

echo "Sleeping for 5 seconds"
sleep 5
time ../TesterClient/skvbc_client -f 1 -c 0 -p 400 -i 4

echo "Finished!"
cleanup
