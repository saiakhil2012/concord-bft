bash -x build.sh

java -jar ../execution-engine/target/execution-engine-0.1.jar > /dev/null  2>&1 &

java -jar ../simplekvbc/target/simplekvbc-0.1.jar > /dev/null 2>&1 &
