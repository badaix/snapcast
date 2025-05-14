# Example HTTP requests

## Command line requests

Requires `curl` and `jq`

### Get all client names

```bash
curl --header "Content-Type: application/json" --request POST --data '{"id":1, "jsonrpc":"2.0", "method": "Server.GetStatus"}' http://127.0.0.1:1780/jsonrpc | jq .result.server.groups[].clients[].config.name
```

### Set latency

```bash
curl --header "Content-Type: application/json" --request POST --data '{"id":7,"jsonrpc":"2.0","method":"Client.SetLatency","params":{"id":"713aefd7-e6cb-4c3b-9f5e-91bbcf9bcbc2","latency":10}}' http://127.0.0.1:1780/jsonrpc
```

### Remove all disconnected clients
```bash
curl -s  -H "Content-Type: application/json" -d '{"id":1, "jsonrpc":"2.0", "method": "Server.GetStatus"}' http://127.0.0.1:1780/jsonrpc | jq '.result.server.groups[].clients[] | select(.connected==false) .id' | while read ln; do curl -s -H  "Content-Type: application/json" -d '{"id":1, "jsonrpc":"2.0", "method": "Server.DeleteClient", "params": {"id":'$ln'}}' http://127.0.0.1:1780/jsonrpc; done
```
