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
 * modbus_server_integration_test.go
 *
 *  Created on: 09/06/2026
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

package main

import (
	"fmt"
	"math"
	"testing"
	"time"

	"github.com/simonvetter/modbus"
)

// ---------------------------------------------------------------------------
// Server integration test helpers
// ---------------------------------------------------------------------------

// startTestServer creates and starts a GoModbusServerPort backed by a
// real modbus.ModbusServer on a free TCP port.  It returns the port instance,
// the listen address, and a cleanup function.
func startTestServer(t *testing.T, cfg *ServerPortConfig) (*GoModbusServerPort, string, func()) {
	t.Helper()

	tcpPort, err := pickFreePort()
	if err != nil {
		t.Fatalf("pickFreePort: %v", err)
	}
	listenAddr := fmt.Sprintf("127.0.0.1:%d", tcpPort)
	cfg.TCP = &ServerTCPConfig{Listen: listenAddr}

	if err := validateServerConfig(nil, cfg); err != nil {
		t.Fatalf("validateServerConfig: %v", err)
	}

	p := &GoModbusServerPort{
		name:   "test-server",
		typ:    "GoModbusServer",
		config: cfg,
	}
	p.store = newDataStore()
	p.binaryUpdateMap = make(map[uint64]serverReadTarget)
	p.analogUpdateMap = make(map[uint64]serverReadTarget)
	p.octetUpdateMap = make(map[uint64]serverReadTarget)
	p.coilWriteMap = make(map[uint16]serverWriteTarget)
	p.hrWriteMap = make(map[uint16]serverWriteTarget)
	p.buildMaps()

	srv, err := modbus.NewServer(&modbus.ServerConfiguration{
		URL: "tcp://" + listenAddr,
	}, p)
	if err != nil {
		t.Fatalf("modbus.NewServer: %v", err)
	}
	p.server = srv

	if err := srv.Start(); err != nil {
		t.Fatalf("server.Start: %v", err)
	}
	p.enabled.Store(true)

	if err := waitForPort(tcpPort, 5*time.Second); err != nil {
		srv.Stop()
		t.Fatalf("server did not come up: %v", err)
	}

	return p, listenAddr, func() {
		srv.Stop()
		p.enabled.Store(false)
	}
}

// openTestClient opens a Modbus TCP client to addr with unit ID 1.
func openTestClient(t *testing.T, addr string) *modbus.ModbusClient {
	t.Helper()
	c, err := modbus.NewClient(&modbus.ClientConfiguration{
		URL:     "tcp://" + addr,
		Timeout: 5 * time.Second,
	})
	if err != nil {
		t.Fatalf("NewClient: %v", err)
	}
	if err := c.Open(); err != nil {
		t.Fatalf("client.Open: %v", err)
	}
	c.SetUnitId(1)
	return c
}

// published events accumulator for testing — replaces odc_publish_event
// which is not available in unit tests (no ODC runtime).
// Instead, tests interact with the store directly and verify store state.

// ---------------------------------------------------------------------------
// Server integration tests
// ---------------------------------------------------------------------------

func TestServerCoilRead(t *testing.T) {
	cfg := &ServerPortConfig{
		Binaries: []PointConfig{
			{Index: 0, Modbus: ModbusConfig{Type: "Coil", Address: 0}},
			{Index: 1, Modbus: ModbusConfig{Type: "Coil", Address: 1}},
			{Index: 2, Modbus: ModbusConfig{Type: "Coil", Address: 2}},
		},
	}
	p, addr, cleanup := startTestServer(t, cfg)
	defer cleanup()

	// Set coil values directly in the store (simulating ODC events).
	p.store.mu.Lock()
	p.store.coils[0] = true
	p.store.coils[1] = false
	p.store.coils[2] = true
	p.store.mu.Unlock()

	c := openTestClient(t, addr)
	defer c.Close()

	vals, err := c.ReadCoils(0, 3)
	if err != nil {
		t.Fatal(err)
	}
	if !vals[0] || vals[1] || !vals[2] {
		t.Fatalf("unexpected coil values: %v", vals)
	}
}

func TestServerDiscreteInputRead(t *testing.T) {
	cfg := &ServerPortConfig{
		BinaryOutputStatuses: []PointConfig{
			{Index: 0, Modbus: ModbusConfig{Type: "DiscreteInput", Address: 5}},
			{Index: 1, Modbus: ModbusConfig{Type: "DiscreteInput", Address: 6}},
		},
	}
	p, addr, cleanup := startTestServer(t, cfg)
	defer cleanup()

	p.store.mu.Lock()
	p.store.di[5] = true
	p.store.di[6] = false
	p.store.mu.Unlock()

	c := openTestClient(t, addr)
	defer c.Close()

	vals, err := c.ReadDiscreteInputs(5, 2)
	if err != nil {
		t.Fatal(err)
	}
	if !vals[0] || vals[1] {
		t.Fatalf("unexpected DI values: %v", vals)
	}
}

func TestServerHoldingRegisterRead(t *testing.T) {
	cfg := &ServerPortConfig{
		Analogs: []AnalogPointConfig{
			{PointConfig: PointConfig{Index: 0, Modbus: ModbusConfig{Type: "HoldingRegister", Address: 10}}},
		},
	}
	p, addr, cleanup := startTestServer(t, cfg)
	defer cleanup()

	p.store.mu.Lock()
	p.store.hr[10] = 0xABCD
	p.store.mu.Unlock()

	c := openTestClient(t, addr)
	defer c.Close()

	vals, err := c.ReadRegisters(10, 1, modbus.HOLDING_REGISTER)
	if err != nil {
		t.Fatal(err)
	}
	if vals[0] != 0xABCD {
		t.Fatalf("expected 0xABCD, got 0x%04X", vals[0])
	}
}

func TestServerInputRegisterRead(t *testing.T) {
	cfg := &ServerPortConfig{
		AnalogOutputStatuses: []AnalogPointConfig{
			{PointConfig: PointConfig{Index: 0, Modbus: ModbusConfig{Type: "InputRegister", Address: 20}}},
		},
	}
	p, addr, cleanup := startTestServer(t, cfg)
	defer cleanup()

	p.store.mu.Lock()
	p.store.ir[20] = 0x1234
	p.store.mu.Unlock()

	c := openTestClient(t, addr)
	defer c.Close()

	vals, err := c.ReadRegisters(20, 1, modbus.INPUT_REGISTER)
	if err != nil {
		t.Fatal(err)
	}
	if vals[0] != 0x1234 {
		t.Fatalf("expected 0x1234, got 0x%04X", vals[0])
	}
}

func TestServerCoilWrite(t *testing.T) {
	// Track published ODC events via the coilWriteMap — we check store state
	// instead of ODC publish since the ODC host API is unavailable in tests.
	cfg := &ServerPortConfig{
		BinaryControls: []ControlConfig{
			{PointConfig: PointConfig{Index: 7, Modbus: ModbusConfig{Type: "Coil", Address: 3}}},
		},
	}
	p, addr, cleanup := startTestServer(t, cfg)
	defer cleanup()

	c := openTestClient(t, addr)
	defer c.Close()

	if err := c.WriteCoil(3, true); err != nil {
		t.Fatal(err)
	}

	// The store should be updated.
	p.store.mu.RLock()
	val := p.store.coils[3]
	p.store.mu.RUnlock()
	if !val {
		t.Fatal("expected coil 3 to be true after write")
	}

	// The coilWriteMap should have mapped addr 3 → ODC index 7.
	tgt, ok := p.coilWriteMap[3]
	if !ok {
		t.Fatal("coilWriteMap should have entry for addr 3")
	}
	if tgt.odcIndex != 7 {
		t.Fatalf("expected odcIndex 7, got %d", tgt.odcIndex)
	}
}

func TestServerHoldingRegisterWrite(t *testing.T) {
	cfg := &ServerPortConfig{
		AnalogControls: []AnalogControlConfig{
			{
				PointConfig: PointConfig{
					Index:  5,
					Modbus: ModbusConfig{Type: "HoldingRegister", Address: 100},
				},
				ControlType: "AnalogOutputInt16",
			},
		},
	}
	p, addr, cleanup := startTestServer(t, cfg)
	defer cleanup()

	c := openTestClient(t, addr)
	defer c.Close()

	if err := c.WriteRegister(100, 0x1234); err != nil {
		t.Fatal(err)
	}

	// Store should reflect the write.
	p.store.mu.RLock()
	val := p.store.hr[100]
	p.store.mu.RUnlock()
	if val != 0x1234 {
		t.Fatalf("expected HR[100]=0x1234, got 0x%04X", val)
	}
}

func TestServerHoldingRegisterWriteFloat32(t *testing.T) {
	cfg := &ServerPortConfig{
		AnalogControls: []AnalogControlConfig{
			{
				PointConfig: PointConfig{
					Index:  0,
					Modbus: ModbusConfig{Type: "HoldingRegister", Address: 0, Count: 2},
				},
				ControlType: "AnalogOutputFloat32",
			},
		},
	}
	p, addr, cleanup := startTestServer(t, cfg)
	defer cleanup()

	// The hrWriteMap should have count=2 and type=AnalogOutputFloat32.
	tgt, ok := p.hrWriteMap[0]
	if !ok {
		t.Fatal("hrWriteMap should have entry for addr 0")
	}
	if tgt.count != 2 {
		t.Fatalf("expected count 2, got %d", tgt.count)
	}

	// Write IEEE 754 float32 1.0 = 0x3F800000 as two registers ABCD.
	c := openTestClient(t, addr)
	defer c.Close()

	if err := c.WriteFloat32(0, 1.0); err != nil {
		t.Fatal(err)
	}

	p.store.mu.RLock()
	hi := p.store.hr[0]
	lo := p.store.hr[1]
	p.store.mu.RUnlock()

	raw := uint32(hi)<<16 | uint32(lo)
	got := math.Float32frombits(raw)
	if got != 1.0 {
		t.Fatalf("expected 1.0, got %v (raw 0x%08X)", got, raw)
	}
}

func TestServerOctetStringRead(t *testing.T) {
	cfg := &ServerPortConfig{
		OctetStrings: []OctetStringPointConfig{
			{PointConfig: PointConfig{Index: 0, Modbus: ModbusConfig{Type: "HoldingRegister", Address: 200, Count: 4}}},
		},
	}
	p, addr, cleanup := startTestServer(t, cfg)
	defer cleanup()

	// Store 8 bytes: "HELLO!!!" as 4 holding registers.
	p.store.mu.Lock()
	p.store.hr[200] = 0x4845 // HE
	p.store.hr[201] = 0x4C4C // LL
	p.store.hr[202] = 0x4F21 // O!
	p.store.hr[203] = 0x2121 // !!
	p.store.mu.Unlock()

	c := openTestClient(t, addr)
	defer c.Close()

	vals, err := c.ReadRegisters(200, 4, modbus.HOLDING_REGISTER)
	if err != nil {
		t.Fatal(err)
	}
	if vals[0] != 0x4845 || vals[1] != 0x4C4C || vals[2] != 0x4F21 || vals[3] != 0x2121 {
		t.Fatalf("unexpected register values: %v", vals)
	}
}

func TestServerMultipleClients(t *testing.T) {
	cfg := &ServerPortConfig{
		Analogs: []AnalogPointConfig{
			{PointConfig: PointConfig{Index: 0, Modbus: ModbusConfig{Type: "HoldingRegister", Address: 0}}},
		},
	}
	p, addr, cleanup := startTestServer(t, cfg)
	defer cleanup()

	p.store.mu.Lock()
	p.store.hr[0] = 0xBEEF
	p.store.mu.Unlock()

	c1 := openTestClient(t, addr)
	defer c1.Close()
	c2 := openTestClient(t, addr)
	defer c2.Close()

	for _, c := range []*modbus.ModbusClient{c1, c2} {
		vals, err := c.ReadRegisters(0, 1, modbus.HOLDING_REGISTER)
		if err != nil {
			t.Fatal(err)
		}
		if vals[0] != 0xBEEF {
			t.Fatalf("expected 0xBEEF, got 0x%04X", vals[0])
		}
	}
}

func TestServerScaleOffset(t *testing.T) {
	// Scale=2.0, Offset=-1.0
	// ODC value 5.0 → raw = (5.0 - (-1.0)) / 2.0 = 3.0 → HR=3
	cfg := &ServerPortConfig{
		Analogs: []AnalogPointConfig{
			{
				PointConfig: PointConfig{Index: 0, Modbus: ModbusConfig{Type: "HoldingRegister", Address: 50}},
				Scale:       2.0,
				Offset:      -1.0,
				DataType:    "Int16",
			},
		},
	}
	p, _, cleanup := startTestServer(t, cfg)
	defer cleanup()

	tgt := p.analogUpdateMap[0]
	if tgt.scale != 2.0 || tgt.offset != -1.0 {
		t.Fatalf("unexpected scale/offset in map: %+v", tgt)
	}

	// Simulate an ODC event updating the store.
	// ODC value = 5.0, inverse: raw = (5.0 - (-1.0)) / 2.0 = 3
	rawVal := (5.0 - tgt.offset) / tgt.scale
	regs := encodeAnalogRegs(rawVal, tgt.count, tgt.endian, tgt.dataType)
	p.store.mu.Lock()
	p.store.hr[50] = regs[0]
	p.store.mu.Unlock()

	p.store.mu.RLock()
	hr50 := p.store.hr[50]
	p.store.mu.RUnlock()

	if hr50 != 3 {
		t.Fatalf("expected HR[50]=3, got %d", hr50)
	}
}

func TestServerEndianFloat32DCBA(t *testing.T) {
	// Verify that a float32 stored in DCBA order is correctly decoded.
	// float32(3.14) ≈ 0x4048F5C3
	// DCBA encoding: pair = uint32ToRegs(0x4048F5C3, "DCBA")
	//   hi=0x4048, lo=0xF5C3
	//   DCBA: {byteSwap(lo), byteSwap(hi)} = {byteSwap(0xF5C3), byteSwap(0x4048)}
	//                                       = {0xC3F5,           0x4840}  ← wrong?
	// Let me trace through uint32ToRegs for DCBA:
	//   v = 0x4048F5C3, hi = 0x4048, lo = 0xF5C3
	//   DCBA case: {byteSwap(lo), byteSwap(hi)} = {0xC3F5, 0x4840}
	// And decoding DCBA: byteSwap(r1)<<16 | byteSwap(r0) with r0=0xC3F5, r1=0x4840
	//   = byteSwap(0x4840)<<16 | byteSwap(0xC3F5)
	//   = 0x4048<<16 | 0xF5C3 = 0x4048F5C3 ✓
	want := float32(3.14)
	raw := math.Float32bits(want)
	regs := uint32ToRegs(raw, "DCBA")
	got32 := decodeAnalogRegs(regs[:], 2, "DCBA", "Float32")
	if float32(got32) != want {
		t.Fatalf("DCBA round-trip: want %v, got %v (regs=%v)", want, float32(got32), regs)
	}
}

func TestServerEnableDisable(t *testing.T) {
	port, err := pickFreePort()
	if err != nil {
		t.Fatal(err)
	}
	listenAddr := fmt.Sprintf("127.0.0.1:%d", port)

	cfg := &ServerPortConfig{
		TCP: &ServerTCPConfig{Listen: listenAddr},
	}
	if err := validateServerConfig(nil, cfg); err != nil {
		t.Fatal(err)
	}

	p := &GoModbusServerPort{
		name:            "test-ed",
		typ:             "GoModbusServer",
		config:          cfg,
		store:           newDataStore(),
		binaryUpdateMap: make(map[uint64]serverReadTarget),
		analogUpdateMap: make(map[uint64]serverReadTarget),
		octetUpdateMap:  make(map[uint64]serverReadTarget),
		coilWriteMap:    make(map[uint16]serverWriteTarget),
		hrWriteMap:      make(map[uint16]serverWriteTarget),
	}

	srv, err := modbus.NewServer(&modbus.ServerConfiguration{URL: "tcp://" + listenAddr}, p)
	if err != nil {
		t.Fatal(err)
	}
	p.server = srv
	p.built = true

	// Enable: server starts.
	if err := srv.Start(); err != nil {
		t.Fatal(err)
	}
	p.enabled.Store(true)

	if err := waitForPort(port, 5*time.Second); err != nil {
		t.Fatal(err)
	}

	// Connect and verify working.
	c, err := modbus.NewClient(&modbus.ClientConfiguration{
		URL:     "tcp://" + listenAddr,
		Timeout: 2 * time.Second,
	})
	if err != nil {
		t.Fatal(err)
	}
	if err := c.Open(); err != nil {
		t.Fatal(err)
	}
	c.SetUnitId(1)
	if _, err := c.ReadCoils(0, 1); err != nil {
		t.Fatal(err)
	}

	// Disable: server stops.
	srv.Stop()
	p.enabled.Store(false)

	// Subsequent reads should fail.
	_, err = c.ReadCoils(0, 1)
	if err == nil {
		t.Fatal("expected error after server stop")
	}
	c.Close()
}
