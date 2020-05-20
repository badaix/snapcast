# Example HTTP requests

## Command line requests

Requires `curl` and `jq`

### Get all client names

```bash
curl --header "Content-Type: application/json" --request POST --data '{"id":1, "jsonrpc":"2.0", "method": "Server.GetStatus"}' http://127.0.0.1:1780/jsonrpc | jq .result.server.groups[].clients[].config.name
```
