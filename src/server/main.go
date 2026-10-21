package main

import (
	"context"
	"crypto/rand"
	"fmt"
	"log"
	"net"
	"sync"
	"time"

	pb "github.com/ffuT/p2pc4/gen/rendezvousv1"
	"google.golang.org/grpc"
	"google.golang.org/grpc/peer"
)

const version uint32 = (1 << 16) | (0 << 8) | 0 // 1.0.0

func checkVersion(client uint32) bool {
	if client>>16 != version>>16 {
		return true
	}
	return false
}

type Player struct {
	token string
	addr  *net.UDPAddr
}

type Room struct {
	p1    Player
	p2    Player
	ready chan struct{}
}

type server struct {
	pb.UnimplementedRendezvousServer
	rooms map[string]Room
	mu    sync.Mutex
}

func GetServer() *server {
	return &server{
		rooms: make(map[string]Room),
	}
}

func (s *server) Join(ctx context.Context, req *pb.JoinReq) (*pb.JoinAck, error) {
	if p, ok := peer.FromContext(ctx); ok {
		log.Printf("client at ip: %s connected\n", p.Addr)
	}
	if checkVersion(req.Version) {
		sv := fmt.Sprintf("%d.%d.%d", uint8(version>>16&0xFF), uint8(version>>8&0xFF), uint8(version&0xFF))
		cv := fmt.Sprintf("%d.%d.%d", uint8(req.Version>>16&0xFF), uint8(req.Version>>8&0xFF), uint8(req.Version&0xFF))
		log.Printf("version mismatch server:%s user:%s", sv, cv)
		return nil, fmt.Errorf("outdated client version:%s, required:%s", cv, sv)
	}
	tok := make([]byte, 16)
	rand.Read(tok)
	s.mu.Lock()
	if value, ok := s.rooms[req.Room]; ok { // room exists
		value.p2 = Player{token: string(tok), addr: nil}
		s.rooms[req.Room] = value
		log.Printf("room: %s is full", req.Room)
	} else { // room doesnt exist
		s.rooms[req.Room] = Room{p1: Player{token: string(tok), addr: nil}, ready: make(chan struct{})}
		log.Printf("created room: %s", req.Room)
	}
	s.mu.Unlock()

	return &pb.JoinAck{Token: tok}, nil
}

func (s *server) WaitPeer(ctx context.Context, req *pb.WaitReq) (*pb.PeerInfo, error) {

	var player1 bool
	var ready chan struct{}
	var matchRoom Room
	s.mu.Lock()
	for _, room := range s.rooms {
		if room.p1.token == string(req.Token) {
			ready = room.ready
			player1 = true
			matchRoom = room
			break
		} else if room.p2.token == string(req.Token) {
			player1 = false
			ready = room.ready
			matchRoom = room
			break
		}
	}
	s.mu.Unlock()

	select {
	case <-ready:
	case <-ctx.Done():
		log.Print("client disconnected")
		// delete player
		return nil, fmt.Errorf("client Disconnected")
	}

	if player1 {
		return &pb.PeerInfo{Ip: string(matchRoom.p2.addr.IP), Port: uint32(matchRoom.p2.addr.Port), Start: true}, nil
	} else {
		return &pb.PeerInfo{Ip: string(matchRoom.p1.addr.IP), Port: uint32(matchRoom.p1.addr.Port), Start: true}, nil
	}
}

func udpLoop(conn *net.UDPConn, s *server) {
	buf := make([]byte, 1500)
	for {
		time.Sleep(10 * time.Millisecond)
		n, addr, err := conn.ReadFromUDP(buf)
		if err != nil {
			log.Printf("udp read: %v", err)
			continue
		}
		//token := string(buf[:n])
		log.Printf("udp from %s (%d bytes)", addr, n)

		// todo find player by token and connect

		conn.WriteToUDP([]byte("OK"), addr)
	}
}

func main() {
	lis, err := net.Listen("tcp", ":50051")
	if err != nil {
		log.Fatal(err)
	}
	fmt.Printf("server listening tcp on :50051...\n")
	udpConn, err := net.ListenUDP("udp", &net.UDPAddr{Port: 50052})
	if err != nil {
		log.Fatal(err)
	}
	fmt.Printf("server listening udp on :50052...\n")

	s := GetServer()
	go udpLoop(udpConn, s)

	g := grpc.NewServer()
	pb.RegisterRendezvousServer(g, s)
	log.Fatal(g.Serve(lis))
}
