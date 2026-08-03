/*	opendatacon
 *
 *	Copyright (c) 2014:
 *
 *		DCrip3fJguWgVCLrZFfA7sIGgvx1Ou3fHfCxnrz4svAi
 *		yxeOtDhDCXf1Z4ApgXvX5ahqQmzRfJ2DoX8S05SqHA==
 *
 *	Licensed under the Apache License, Version 2.0 (the "License");
 *	you may not use this file except in compliance with the License.
 *	You may obtain a copy of the License at
 *
 *		http://www.apache.org/licenses/LICENSE-2.0
 *
 *	Unless required by applicable law or agreed to in writing, software
 *	distributed under the License is distributed on an "AS IS" BASIS,
 *	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *	See the License for the specific language governing permissions and
 *	limitations under the License.
 */
/*
 * modbus_integration_test.go
 *
 *  Created on: 09/06/2026
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

package main

import (
	"fmt"
	"net"
	"sync"
	"testing"
	"time"

	"github.com/simonvetter/modbus"
)

type testModbusHandler struct {
	lock     sync.RWMutex
	coils    [256]bool
	discrete [256]bool
	holding  [256]uint16
	input    [256]uint16
}

func (h *testModbusHandler) HandleCoils(req *modbus.CoilsRequest) ([]bool, error) {
	if req.UnitId != 1 {
		return nil, modbus.ErrIllegalFunction
	}
	h.lock.Lock()
	defer h.lock.Unlock()
	if int(req.Addr)+int(req.Quantity) > len(h.coils) {
		return nil, modbus.ErrIllegalDataAddress
	}
	res := make([]bool, req.Quantity)
	for i := 0; i < int(req.Quantity); i++ {
		addr := int(req.Addr) + i
		if req.IsWrite {
			h.coils[addr] = req.Args[i]
		}
		res[i] = h.coils[addr]
	}
	return res, nil
}

func (h *testModbusHandler) HandleDiscreteInputs(req *modbus.DiscreteInputsRequest) ([]bool, error) {
	if req.UnitId != 1 {
		return nil, modbus.ErrIllegalFunction
	}
	h.lock.RLock()
	defer h.lock.RUnlock()
	if int(req.Addr)+int(req.Quantity) > len(h.discrete) {
		return nil, modbus.ErrIllegalDataAddress
	}
	res := make([]bool, req.Quantity)
	for i := 0; i < int(req.Quantity); i++ {
		res[i] = h.discrete[int(req.Addr)+i]
	}
	return res, nil
}

func (h *testModbusHandler) HandleHoldingRegisters(req *modbus.HoldingRegistersRequest) ([]uint16, error) {
	if req.UnitId != 1 {
		return nil, modbus.ErrIllegalFunction
	}
	h.lock.Lock()
	defer h.lock.Unlock()
	if int(req.Addr)+int(req.Quantity) > len(h.holding) {
		return nil, modbus.ErrIllegalDataAddress
	}
	res := make([]uint16, req.Quantity)
	for i := 0; i < int(req.Quantity); i++ {
		addr := int(req.Addr) + i
		if req.IsWrite {
			h.holding[addr] = req.Args[i]
		}
		res[i] = h.holding[addr]
	}
	return res, nil
}

func (h *testModbusHandler) HandleInputRegisters(req *modbus.InputRegistersRequest) ([]uint16, error) {
	if req.UnitId != 1 {
		return nil, modbus.ErrIllegalFunction
	}
	h.lock.RLock()
	defer h.lock.RUnlock()
	if int(req.Addr)+int(req.Quantity) > len(h.input) {
		return nil, modbus.ErrIllegalDataAddress
	}
	res := make([]uint16, req.Quantity)
	for i := 0; i < int(req.Quantity); i++ {
		res[i] = h.input[int(req.Addr)+i]
	}
	return res, nil
}

func pickFreePort() (int, error) {
	l, err := net.Listen("tcp", "localhost:0")
	if err != nil {
		return 0, err
	}
	port := l.Addr().(*net.TCPAddr).Port
	l.Close()
	return port, nil
}

func waitForPort(port int, timeout time.Duration) error {
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		conn, err := net.DialTimeout("tcp", fmt.Sprintf("localhost:%d", port), 100*time.Millisecond)
		if err == nil {
			conn.Close()
			return nil
		}
		time.Sleep(50 * time.Millisecond)
	}
	return fmt.Errorf("port %d not reachable within %v", port, timeout)
}

func TestModbusCoilReadWrite(t *testing.T) {
	port, err := pickFreePort()
	if err != nil {
		t.Fatal(err)
	}
	handler := &testModbusHandler{}
	handler.coils[0] = true
	handler.coils[7] = true

	server, err := modbus.NewServer(&modbus.ServerConfiguration{
		URL:     fmt.Sprintf("tcp://localhost:%d", port),
		Timeout: 5 * time.Second,
	}, handler)
	if err != nil {
		t.Fatal(err)
	}
	if err := server.Start(); err != nil {
		t.Fatal(err)
	}
	defer server.Stop()

	if err := waitForPort(port, 5*time.Second); err != nil {
		t.Fatal(err)
	}

	client, err := modbus.NewClient(&modbus.ClientConfiguration{
		URL:     fmt.Sprintf("tcp://localhost:%d", port),
		Timeout: 5 * time.Second,
	})
	if err != nil {
		t.Fatal(err)
	}
	if err := client.Open(); err != nil {
		t.Fatal(err)
	}
	defer client.Close()
	client.SetUnitId(1)

	vals, err := client.ReadCoils(0, 10)
	if err != nil {
		t.Fatal(err)
	}
	if len(vals) != 10 {
		t.Fatalf("expected 10 coil values, got %d", len(vals))
	}
	if !vals[0] {
		t.Fatal("expected coil 0 to be true")
	}
	if vals[1] {
		t.Fatal("expected coil 1 to be false")
	}
	if !vals[7] {
		t.Fatal("expected coil 7 to be true")
	}

	if err := client.WriteCoil(0, false); err != nil {
		t.Fatal(err)
	}
	if err := client.WriteCoil(5, true); err != nil {
		t.Fatal(err)
	}

	vals, err = client.ReadCoils(0, 6)
	if err != nil {
		t.Fatal(err)
	}
	if vals[0] {
		t.Fatal("expected coil 0 to be false after write")
	}
	if !vals[5] {
		t.Fatal("expected coil 5 to be true after write")
	}
}

func TestModbusDiscreteInputRead(t *testing.T) {
	port, err := pickFreePort()
	if err != nil {
		t.Fatal(err)
	}
	handler := &testModbusHandler{}
	handler.discrete[3] = true
	handler.discrete[4] = true

	server, err := modbus.NewServer(&modbus.ServerConfiguration{
		URL:     fmt.Sprintf("tcp://localhost:%d", port),
		Timeout: 5 * time.Second,
	}, handler)
	if err != nil {
		t.Fatal(err)
	}
	if err := server.Start(); err != nil {
		t.Fatal(err)
	}
	defer server.Stop()

	if err := waitForPort(port, 5*time.Second); err != nil {
		t.Fatal(err)
	}

	client, err := modbus.NewClient(&modbus.ClientConfiguration{
		URL:     fmt.Sprintf("tcp://localhost:%d", port),
		Timeout: 5 * time.Second,
	})
	if err != nil {
		t.Fatal(err)
	}
	if err := client.Open(); err != nil {
		t.Fatal(err)
	}
	defer client.Close()
	client.SetUnitId(1)

	vals, err := client.ReadDiscreteInputs(0, 6)
	if err != nil {
		t.Fatal(err)
	}
	if len(vals) != 6 {
		t.Fatalf("expected 6 discrete input values, got %d", len(vals))
	}
	if vals[3] != true || vals[4] != true {
		t.Fatal("expected discrete inputs 3 and 4 to be true")
	}
	if vals[0] || vals[1] || vals[2] || vals[5] {
		t.Fatal("expected discrete inputs 0-2,5 to be false")
	}
}

func TestModbusHoldingRegisterReadWrite(t *testing.T) {
	port, err := pickFreePort()
	if err != nil {
		t.Fatal(err)
	}
	handler := &testModbusHandler{}
	handler.holding[100] = 0xABCD

	server, err := modbus.NewServer(&modbus.ServerConfiguration{
		URL:     fmt.Sprintf("tcp://localhost:%d", port),
		Timeout: 5 * time.Second,
	}, handler)
	if err != nil {
		t.Fatal(err)
	}
	if err := server.Start(); err != nil {
		t.Fatal(err)
	}
	defer server.Stop()

	if err := waitForPort(port, 5*time.Second); err != nil {
		t.Fatal(err)
	}

	client, err := modbus.NewClient(&modbus.ClientConfiguration{
		URL:     fmt.Sprintf("tcp://localhost:%d", port),
		Timeout: 5 * time.Second,
	})
	if err != nil {
		t.Fatal(err)
	}
	if err := client.Open(); err != nil {
		t.Fatal(err)
	}
	defer client.Close()
	client.SetUnitId(1)

	vals, err := client.ReadRegisters(100, 2, modbus.HOLDING_REGISTER)
	if err != nil {
		t.Fatal(err)
	}
	if len(vals) != 2 {
		t.Fatalf("expected 2 register values, got %d", len(vals))
	}
	if vals[0] != 0xABCD {
		t.Fatalf("expected register 100 to be 0xABCD, got 0x%04X", vals[0])
	}

	if err := client.WriteRegister(100, 0x1234); err != nil {
		t.Fatal(err)
	}

	val, err := client.ReadRegisters(100, 1, modbus.HOLDING_REGISTER)
	if err != nil {
		t.Fatal(err)
	}
	if val[0] != 0x1234 {
		t.Fatalf("expected register 100 to be 0x1234 after write, got 0x%04X", val[0])
	}
}

func TestModbusInputRegisterRead(t *testing.T) {
	port, err := pickFreePort()
	if err != nil {
		t.Fatal(err)
	}
	handler := &testModbusHandler{}
	handler.input[50] = 0xDEAD
	handler.input[51] = 0xBEEF

	server, err := modbus.NewServer(&modbus.ServerConfiguration{
		URL:     fmt.Sprintf("tcp://localhost:%d", port),
		Timeout: 5 * time.Second,
	}, handler)
	if err != nil {
		t.Fatal(err)
	}
	if err := server.Start(); err != nil {
		t.Fatal(err)
	}
	defer server.Stop()

	if err := waitForPort(port, 5*time.Second); err != nil {
		t.Fatal(err)
	}

	client, err := modbus.NewClient(&modbus.ClientConfiguration{
		URL:     fmt.Sprintf("tcp://localhost:%d", port),
		Timeout: 5 * time.Second,
	})
	if err != nil {
		t.Fatal(err)
	}
	if err := client.Open(); err != nil {
		t.Fatal(err)
	}
	defer client.Close()
	client.SetUnitId(1)

	vals, err := client.ReadRegisters(50, 2, modbus.INPUT_REGISTER)
	if err != nil {
		t.Fatal(err)
	}
	if len(vals) != 2 {
		t.Fatalf("expected 2 input register values, got %d", len(vals))
	}
	if vals[0] != 0xDEAD {
		t.Fatalf("expected input register 50 to be 0xDEAD, got 0x%04X", vals[0])
	}
	if vals[1] != 0xBEEF {
		t.Fatalf("expected input register 51 to be 0xBEEF, got 0x%04X", vals[1])
	}
}

func TestModbusConnectionRefused(t *testing.T) {
	client, err := modbus.NewClient(&modbus.ClientConfiguration{
		URL:     "tcp://localhost:1",
		Timeout: time.Second,
	})
	if err != nil {
		t.Fatal(err)
	}
	err = client.Open()
	if err == nil {
		client.Close()
		t.Fatal("expected connection error, got nil")
	}
}

func TestModbusServerStop(t *testing.T) {
	port, err := pickFreePort()
	if err != nil {
		t.Fatal(err)
	}
	handler := &testModbusHandler{}
	server, err := modbus.NewServer(&modbus.ServerConfiguration{
		URL:     fmt.Sprintf("tcp://localhost:%d", port),
		Timeout: time.Second,
	}, handler)
	if err != nil {
		t.Fatal(err)
	}
	if err := server.Start(); err != nil {
		t.Fatal(err)
	}

	if err := waitForPort(port, 5*time.Second); err != nil {
		t.Fatal(err)
	}

	client, err := modbus.NewClient(&modbus.ClientConfiguration{
		URL:     fmt.Sprintf("tcp://localhost:%d", port),
		Timeout: time.Second,
	})
	if err != nil {
		t.Fatal(err)
	}
	if err := client.Open(); err != nil {
		t.Fatal(err)
	}
	client.SetUnitId(1)

	if _, err := client.ReadCoils(0, 1); err != nil {
		t.Fatal(err)
	}

	server.Stop()

	// After server stop, subsequent reads should fail
	_, err = client.ReadCoils(0, 1)
	if err == nil {
		t.Fatal("expected error after server stop")
	}

	client.Close()
}
