curl -d '{"jsonrpc":"2.0","id":1,"method":"create_address","params":{"expiration":"never","comment": ""}}' -H "Content-Type: application/json" -X POST http://127.0.0.1:10002/api/wallet

