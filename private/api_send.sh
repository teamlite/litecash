#!/bin/sh

amount=$1
from=$2
tto=$3

curl -d '{"jsonrpc":"2.0","id":2,"method":"tx_send","params":{"value":2,"fee":0,"from":"' . $from . '", "address";"' . $tto . '"}}' -H "Content-Type: application/json" -X POST http://127.0.0.1:10002/api/wallet

