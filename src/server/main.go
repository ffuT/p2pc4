package main

import (
	"context"
	"log"
	"net"

	pb "github.com/ffuT/p2pc4/gen/rendezvousv1"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
)

type server struct {
	pb.UnimplementedRendezvousServer
}

func (s *server) Join(ctx context.Context, req *pb.JoinReq) (*pb.JoinAck, error) {
	return nil, status.Error(codes.Unimplemented, "todo")
}

func main() {
	lis, err := net.Listen("tcp", ":50051")
	if err != nil {
		log.Fatal(err)
	}
	g := grpc.NewServer()
	pb.RegisterRendezvousServer(g, &server{})
	log.Fatal(g.Serve(lis))
}
