echo "generation go code..."
protoc -I proto \
  --go_out=src/server      --go_opt=module=github.com/ffuT/p2pc4 \
  --go-grpc_out=src/server --go-grpc_opt=module=github.com/ffuT/p2pc4 \
  proto/rendezvous.proto
echo "generation finished!"