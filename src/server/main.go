package main

import (
	"bufio"
	"context"
	"crypto/rand"
	"flag"
	"fmt"
	"log"
	"net"
	"os"
	"os/exec"
	"runtime"
	"strconv"
	"strings"
	"sync"

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
	fin   uint8
	p1    *Player
	p2    *Player
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
	if _, ok := peer.FromContext(ctx); ok {
		log.Printf("new client connected")
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
		if value.p1 != nil && value.p2 != nil {
			log.Printf("room: %s is full", req.Room)
			s.mu.Unlock()
			return nil, fmt.Errorf("room: %s is full", req.Room)
		}
		value.p2 = &Player{token: string(tok), addr: nil}
		s.rooms[req.Room] = value
		log.Printf("room: %s is full", req.Room)
	} else { // room doesnt exist
		s.rooms[req.Room] = Room{p1: &Player{token: string(tok), addr: nil}, p2: nil, ready: make(chan struct{})}
		log.Printf("created room: %s", req.Room)
	}
	s.mu.Unlock()

	return &pb.JoinAck{Token: tok}, nil
}

func (s *server) WaitPeer(ctx context.Context, req *pb.WaitReq) (*pb.PeerInfo, error) {
	var player1 bool
	var ready chan struct{}
	var matchKey string
	s.mu.Lock()
	for key, room := range s.rooms {
		matchKey = key
		if room.p1.token == string(req.Token) {
			ready = room.ready
			player1 = true
			break
		} else if room.p2.token == string(req.Token) {
			player1 = false
			ready = room.ready
			break
		}
		s.mu.Unlock()
		return nil, fmt.Errorf("invalid token!")
	}
	s.mu.Unlock()

	select {
	case <-ready:
	case <-ctx.Done():
		log.Print("client disconnected")
		s.mu.Lock()
		if player1 { // delete p1 and put p2 into p1
			if s.rooms[matchKey].p2 != nil {
				s.rooms[matchKey] = Room{
					p1:    s.rooms[matchKey].p2,
					p2:    nil,
					ready: s.rooms[matchKey].ready}
				log.Printf("player 1 left room: %s", matchKey)
			} else {
				delete(s.rooms, matchKey)
				log.Printf("deleted room: %s", matchKey)
			}
		} else { // delete player 2
			s.rooms[matchKey] = Room{p1: s.rooms[matchKey].p1, p2: nil, ready: s.rooms[matchKey].ready}
		}
		s.mu.Unlock()
		return nil, fmt.Errorf("client Disconnected")
	}
	defer s.cleanRoom(matchKey)
	info := s.rooms[matchKey]
	if player1 {
		return &pb.PeerInfo{Ip: info.p2.addr.IP.String(), Port: uint32(info.p2.addr.Port), Start: true}, nil
	} else {
		return &pb.PeerInfo{Ip: info.p1.addr.IP.String(), Port: uint32(info.p1.addr.Port), Start: false}, nil
	}
}

func (s *server) cleanRoom(key string) {
	s.mu.Lock()
	defer s.mu.Unlock()
	r := s.rooms[key]
	r.fin++
	s.rooms[key] = r
	if s.rooms[key].fin >= 2 {
		log.Printf("Game: %s started!", key)
		delete(s.rooms, key)
	}
}

func udpLoop(conn *net.UDPConn, s *server) {
	buf := make([]byte, 1500)
	for {
		n, addr, err := conn.ReadFromUDP(buf)
		if err != nil {
			log.Printf("udp read: %v", err)
			continue
		}
		token := string(buf[:n])
		log.Printf("udp from %s (%d bytes)", addr, n)
		s.mu.Lock()
		for k, room := range s.rooms {
			if room.p1.token == token {
				room.p1.addr = addr
			} else if room.p2 != nil && room.p2.token == token {
				room.p2.addr = addr
			}
			if room.p1 != nil && room.p2 != nil {
				if room.p1.addr != nil && room.p2.addr != nil {
					log.Printf("Room: %s is ready!", k)
					close(room.ready)
				}
			}
		}
		s.mu.Unlock()

		conn.WriteToUDP([]byte("OK"), addr)
	}
}

func commands() {
	var scanner = bufio.NewScanner(os.Stdin)
	for {
		scanner.Scan()
		err := scanner.Err()
		if err != nil {
			log.Printf("Scanner Broke: %v", err)
		}

		args := strings.Split(scanner.Text(), " ")
		switch args[0] {
		case "exit", "quit", "shutdown":
			log.Printf("Server Shutting Down!")
			os.Exit(0)
		case "list":
			if len(args) <= 1 {
				log.Printf("usage: list [what]")
				continue
			}
			switch args[1] {
			case "players":

			case "rooms":
			}
		case "clear":
			var cmd *exec.Cmd
			if runtime.GOOS == "windows" {
				cmd = exec.Command("cmd", "/c", "cls")
			} else {
				cmd = exec.Command("clear")
			}
			cmd.Stdout = os.Stdout
			cmd.Run()

		default:
			log.Printf("Command Not Found: %s", args[0])
		}
	}
}

func main() {
	tcpPort := flag.Int("tcp", 50051, "tcp port")
	updPort := flag.Int("udp", 50052, "udp port")
	flag.Parse()

	lis, err := net.Listen("tcp", ":"+strconv.Itoa(*tcpPort))
	if err != nil {
		log.Fatal(err)
	}
	fmt.Printf("server listening tcp on :%d...\n", *tcpPort)
	udpConn, err := net.ListenUDP("udp", &net.UDPAddr{Port: *updPort})
	if err != nil {
		log.Fatal(err)
	}
	fmt.Printf("server listening udp on :%d...\n", *updPort)

	s := GetServer()
	go udpLoop(udpConn, s)
	go commands()

	g := grpc.NewServer()
	pb.RegisterRendezvousServer(g, s)
	log.Fatal(g.Serve(lis))
}
