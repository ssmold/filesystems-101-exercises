package parhash

import (
	"context"
	"net"
	"sync"
	"sync/atomic"

	hashpb "fs101ex/pkg/gen/hashsvc"
	pb "fs101ex/pkg/gen/parhashsvc"
	"fs101ex/pkg/workgroup"

	"github.com/pkg/errors"
	"golang.org/x/sync/semaphore"
	"google.golang.org/grpc"
)

type roundRobin struct {
	backends []hashpb.HashSvcClient
	next     uint32
}

func NewRoundRobin(clients []hashpb.HashSvcClient) *roundRobin {
	return &roundRobin{
		backends: clients,
	}
}

func (r *roundRobin) Next() hashpb.HashSvcClient {
	current := atomic.AddUint32(&r.next, 1)
	return r.backends[(int(current)-1)%len(r.backends)]
}

type Config struct {
	ListenAddr   string
	BackendAddrs []string
	Concurrency  int
}

// Implement a server that responds to ParallelHash()
// as declared in /proto/parhash.proto.
//
// The implementation of ParallelHash() must not hash the content
// of buffers on its own. Instead, it must send buffers to backends
// to compute hashes. Buffers must be fanned out to backends in the
// round-robin fashion.
//
// For example, suppose that 2 backends are configured and ParallelHash()
// is called to compute hashes of 5 buffers. In this case it may assign
// buffers to backends in this way:
//
//	backend 0: buffers 0, 2, and 4,
//	backend 1: buffers 1 and 3.
//
// Requests to hash individual buffers must be issued concurrently.
// Goroutines that issue them must run within /pkg/workgroup/Wg. The
// concurrency within workgroups must be limited by Server.sem.
//
// WARNING: requests to ParallelHash() may be concurrent, too.
// Make sure that the round-robin fanout works in that case too,
// and evenly distributes the load across backends.
type Server struct {
	conf Config

	m    sync.Mutex
	sem  *semaphore.Weighted
	stop context.CancelFunc
	l    net.Listener
	wg   sync.WaitGroup
}

func New(conf Config) *Server {
	return &Server{
		conf: conf,
		sem:  semaphore.NewWeighted(int64(conf.Concurrency)),
	}
}

func (s *Server) Start(ctx context.Context) (err error) {
	defer func() { err = errors.Wrap(err, "Start()") }()

	ctx, s.stop = context.WithCancel(ctx)

	s.l, err = net.Listen("tcp", s.conf.ListenAddr)
	if err != nil {
		return err
	}

	srv := grpc.NewServer()
	pb.RegisterParallelHashSvcServer(srv, s)

	s.wg.Add(2)
	go func() {
		defer s.wg.Done()

		srv.Serve(s.l)
	}()
	go func() {
		defer s.wg.Done()

		<-ctx.Done()
		s.l.Close()
	}()

	return nil
}

func (s *Server) ListenAddr() string {
	return s.l.Addr().String()
}

func (s *Server) Stop() {
	s.stop()
	s.wg.Wait()
}

func (s *Server) ParallelHash(ctx context.Context, req *pb.ParHashReq) (res *pb.ParHashResp, err error) {
	defer func() { err = errors.Wrap(err, "ParallelHash()") }()

	wg := workgroup.New(workgroup.Config{Sem: s.sem})
	hashes := make([][]byte, len(req.Data))
	conns := make([]*grpc.ClientConn, len(s.conf.BackendAddrs))
	backends := make([]hashpb.HashSvcClient, len(s.conf.BackendAddrs))

	for i := range backends {
		if conns[i], err = grpc.Dial(s.conf.BackendAddrs[i], grpc.WithInsecure()); err != nil {
			return &pb.ParHashResp{}, err
		}
		defer conns[i].Close()

		backends[i] = hashpb.NewHashSvcClient(conns[i])
	}

	r := NewRoundRobin(backends)
	for i, bytes := range req.Data {
		data := bytes
		idx := i
		wg.Go(ctx, func(ctx context.Context) error {
			hashReq := &hashpb.HashReq{Data: data}
			s.m.Lock()
			res, err := r.Next().Hash(ctx, hashReq)
			s.m.Unlock()
			if err != nil {
				return err
			}

			s.m.Lock()
			hashes[idx] = res.Hash
			s.m.Unlock()
			return nil
		})
	}

	return &pb.ParHashResp{Hashes: hashes}, wg.Wait()
}
