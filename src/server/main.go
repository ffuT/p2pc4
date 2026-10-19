package main

import (
	"context"
	"fmt"
	"log"
	"net"

	pb "github.com/ffuT/p2pc4/gen/rendezvousv1"
	"google.golang.org/grpc"
)

type server struct {
	pb.UnimplementedRendezvousServer
}

func (s *server) Join(ctx context.Context, req *pb.JoinReq) (*pb.JoinAck, error) {
	fmt.Printf("client at ip: %s connected\n", "idk")
	return &pb.JoinAck{Token: []byte{54, 55}}, nil
}

func main() {
	lis, err := net.Listen("tcp", ":50051")
	if err != nil {
		log.Fatal(err)
	}
	fmt.Printf("server listening on :50051...\n")

	g := grpc.NewServer()
	pb.RegisterRendezvousServer(g, &server{})
	log.Fatal(g.Serve(lis))
}
