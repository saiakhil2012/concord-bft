#!/bin/bash
for i in {0..2}
do
   base_port=8090
   port=$((base_port+i))
   docker run -it -p $port:$port -e SERVER_PORT=$port -d execution-engine:test 
done
