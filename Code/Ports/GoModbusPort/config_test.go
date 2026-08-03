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
 * config_test.go
 *
 *  Created on: 09/06/2026
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

package main

import (
	"encoding/json"
	"testing"
)

const (
	testBinary = 1
	testAnalog = 3
)

// intPtr returns a pointer to v, used to construct *int literals in PointConfig.Bit.
func intPtr(v int) *int { return &v }

func TestParseConfigTCP(t *testing.T) {
	data := `{
		"TCP": {"Address": "192.168.1.1:502"},
		"UnitID": 1,
		"TimeoutMs": 5000,
		"PollRateMs": 1000,
		"Binaries": [{"Modbus": {"Type": "Coil", "Address": 0}}]
	}`
	var cfg PortConfig
	if err := json.Unmarshal([]byte(data), &cfg); err != nil {
		t.Fatal(err)
	}
	if cfg.TCP == nil {
		t.Fatal("expected TCP config")
	}
	if cfg.TCP.Address != "192.168.1.1:502" {
		t.Fatalf("expected address 192.168.1.1:502, got %s", cfg.TCP.Address)
	}
	if cfg.UnitID != 1 {
		t.Fatalf("expected UnitID 1, got %d", cfg.UnitID)
	}
	if len(cfg.Binaries) != 1 {
		t.Fatalf("expected 1 Binary, got %d", len(cfg.Binaries))
	}
	if cfg.Binaries[0].Modbus.Type != "Coil" {
		t.Fatalf("expected Type Coil, got %s", cfg.Binaries[0].Modbus.Type)
	}
	if cfg.Binaries[0].Modbus.Address != 0 {
		t.Fatalf("expected Address 0, got %d", cfg.Binaries[0].Modbus.Address)
	}
}

func TestParseConfigRTU(t *testing.T) {
	data := `{
		"RTU": {"Port": "/dev/ttyUSB0", "BaudRate": 9600, "DataBits": 8, "StopBits": 1, "Parity": "E"},
		"UnitID": 2,
		"TimeoutMs": 2000,
		"PollRateMs": 500
	}`
	var cfg PortConfig
	if err := json.Unmarshal([]byte(data), &cfg); err != nil {
		t.Fatal(err)
	}
	if cfg.RTU == nil {
		t.Fatal("expected RTU config")
	}
	if cfg.RTU.Port != "/dev/ttyUSB0" {
		t.Fatalf("expected port /dev/ttyUSB0, got %s", cfg.RTU.Port)
	}
	if cfg.RTU.BaudRate != 9600 {
		t.Fatalf("expected BaudRate 9600, got %d", cfg.RTU.BaudRate)
	}
}

func TestParseConfigAllPointTypes(t *testing.T) {
	data := `{
		"TCP": {"Address": "localhost:502"},
		"Binaries": [{"Modbus": {"Type": "Coil", "Address": 0}}],
		"Analogs": [{"Modbus": {"Type": "HoldingRegister", "Address": 10}, "Endian": "ABCD", "Scale": 2.0, "Offset": -1.0}],
		"BinaryControls": [{"Modbus": {"Type": "Coil", "Address": 20}, "OnAction": "Pulse", "OffAction": "Clear"}],
		"BinaryOutputStatuses": [{"Modbus": {"Type": "Coil", "Address": 20}}],
		"AnalogControls": [{"Modbus": {"Type": "HoldingRegister", "Address": 30}, "ControlType": "AnalogOutputInt32"}],
		"AnalogOutputStatuses": [{"Modbus": {"Type": "HoldingRegister", "Address": 30}}],
		"OctetStrings": [{"Modbus": {"Type": "HoldingRegister", "Address": 40, "Count": 8}}]
	}`
	var cfg PortConfig
	if err := json.Unmarshal([]byte(data), &cfg); err != nil {
		t.Fatal(err)
	}
	if len(cfg.Binaries) != 1 || len(cfg.Analogs) != 1 || len(cfg.BinaryControls) != 1 {
		t.Fatal("missing point configurations")
	}
	if cfg.Analogs[0].Scale != 2.0 {
		t.Fatalf("expected Scale 2.0, got %f", cfg.Analogs[0].Scale)
	}
	if cfg.Analogs[0].Offset != -1.0 {
		t.Fatalf("expected Offset -1.0, got %f", cfg.Analogs[0].Offset)
	}
	if cfg.Analogs[0].Endian != "ABCD" {
		t.Fatalf("expected Endian ABCD, got %s", cfg.Analogs[0].Endian)
	}
	if cfg.AnalogControls[0].ControlType != "AnalogOutputInt32" {
		t.Fatalf("expected ControlType AnalogOutputInt32, got %s", cfg.AnalogControls[0].ControlType)
	}
	// Bit absent → nil pointer
	if cfg.Binaries[0].Bit != nil {
		t.Fatalf("expected Bit nil when not configured, got %v", *cfg.Binaries[0].Bit)
	}
}

func TestParseBitZero(t *testing.T) {
	// "Bit": 0 must parse as a pointer-to-zero, not as absent.
	data := `{"TCP":{"Address":"localhost:502"},
		"Binaries":[{"Index":0,"Modbus":{"Type":"HoldingRegister","Address":0},"Bit":0}]}`
	var cfg PortConfig
	if err := json.Unmarshal([]byte(data), &cfg); err != nil {
		t.Fatal(err)
	}
	if cfg.Binaries[0].Bit == nil {
		t.Fatal("Bit should be non-nil when \"Bit\": 0 is specified")
	}
	if *cfg.Binaries[0].Bit != 0 {
		t.Fatalf("expected Bit=0, got %d", *cfg.Binaries[0].Bit)
	}
}

func TestParseConfigErrors(t *testing.T) {
	tests := []struct {
		name string
		json string
	}{
		{"no connection", `{"UnitID": 1}`},
		{"both TCP and RTU", `{"TCP": {"Address": "a:1"}, "RTU": {"Port": "/dev/ttyUSB0"}}`},
		{"empty TCP address", `{"TCP": {"Address": ""}}`},
		{"empty RTU port", `{"RTU": {"Port": ""}}`},
	}
	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			var cfg PortConfig
			if err := json.Unmarshal([]byte(tc.json), &cfg); err != nil {
				t.Fatal(err)
			}
			if err := validateConfig(nil, &cfg); err == nil {
				t.Fatal("expected error, got nil")
			}
		})
	}
}

func TestConfigDefaults(t *testing.T) {
	cfg := PortConfig{TCP: &TCPConfig{Address: "localhost:502"}}
	if err := validateConfig(nil, &cfg); err != nil {
		t.Fatal(err)
	}
	if cfg.PollRateMs != 1000 {
		t.Fatalf("expected default PollRateMs 1000, got %d", cfg.PollRateMs)
	}
	if cfg.TimeoutMs != 1000 {
		t.Fatalf("expected default TimeoutMs 1000, got %d", cfg.TimeoutMs)
	}

	cfg2 := PortConfig{RTU: &RTUConfig{Port: "/dev/ttyUSB0"}}
	if err := validateConfig(nil, &cfg2); err != nil {
		t.Fatal(err)
	}
	if cfg2.RTU.BaudRate != 19200 {
		t.Fatalf("expected default BaudRate 19200, got %d", cfg2.RTU.BaudRate)
	}
	if cfg2.RTU.DataBits != 8 {
		t.Fatalf("expected default DataBits 8, got %d", cfg2.RTU.DataBits)
	}
	if cfg2.RTU.StopBits != 1 {
		t.Fatalf("expected default StopBits 1, got %d", cfg2.RTU.StopBits)
	}
	if cfg2.RTU.Parity != "N" {
		t.Fatalf("expected default Parity N, got %s", cfg2.RTU.Parity)
	}
}

func TestExpandSinglePoint(t *testing.T) {
	pts := expandPolledPoint(
		PointConfig{
			Index:  5,
			Modbus: ModbusConfig{Type: "Coil", Address: 42},
		},
		0, 0, "", "", testBinary, 1000)
	if len(pts) != 1 {
		t.Fatalf("expected 1 point, got %d", len(pts))
	}
	if pts[0].ODCIndex != 5 {
		t.Fatalf("expected ODCIndex 5, got %d", pts[0].ODCIndex)
	}
	if pts[0].ModbusAddr != 42 {
		t.Fatalf("expected ModbusAddr 42, got %d", pts[0].ModbusAddr)
	}
	if pts[0].ModbusType != "Coil" {
		t.Fatalf("expected ModbusType Coil, got %s", pts[0].ModbusType)
	}
	if pts[0].PollRateMs != 1000 {
		t.Fatalf("expected PollRateMs 1000, got %d", pts[0].PollRateMs)
	}
}

func TestExpandRange(t *testing.T) {
	pts := expandPolledPoint(
		PointConfig{
			Range:  &RangeConfig{Start: 10, Stop: 19},
			Modbus: ModbusConfig{Type: "HoldingRegister", Address: 100},
		},
		2.0, 0.5, "ABCD", "Float32", testAnalog, 2000)
	if len(pts) != 10 {
		t.Fatalf("expected 10 points, got %d", len(pts))
	}
	for i := range pts {
		pt := &pts[i]
		if pt.ODCIndex != uint64(10+i) {
			t.Fatalf("point %d: expected ODCIndex %d, got %d", i, 10+i, pt.ODCIndex)
		}
		if pt.ModbusAddr != uint16(100+i) {
			t.Fatalf("point %d: expected ModbusAddr %d, got %d", i, 100+i, pt.ModbusAddr)
		}
		if pt.Scale != 2.0 {
			t.Fatalf("point %d: expected Scale 2.0, got %f", i, pt.Scale)
		}
		if pt.Offset != 0.5 {
			t.Fatalf("point %d: expected Offset 0.5, got %f", i, pt.Offset)
		}
		if pt.Endian != "ABCD" {
			t.Fatalf("point %d: expected Endian ABCD, got %s", i, pt.Endian)
		}
		if pt.DataType != "Float32" {
			t.Fatalf("point %d: expected DataType Float32, got %s", i, pt.DataType)
		}
		if pt.PollRateMs != 2000 {
			t.Fatalf("point %d: expected PollRateMs 2000, got %d", i, pt.PollRateMs)
		}
	}
}

func TestExpandCount(t *testing.T) {
	pts := expandPolledPoint(
		PointConfig{
			Index:  0,
			Modbus: ModbusConfig{Type: "HoldingRegister", Address: 0, Count: 4},
		},
		0, 0, "", "", testAnalog, 1000)
	if len(pts) != 1 {
		t.Fatalf("expected 1 point, got %d", len(pts))
	}
	if pts[0].Count != 4 {
		t.Fatalf("expected Count 4, got %d", pts[0].Count)
	}
}

func TestExpandBit(t *testing.T) {
	pts := expandPolledPoint(
		PointConfig{
			Index:  0,
			Bit:    intPtr(3),
			Modbus: ModbusConfig{Type: "HoldingRegister", Address: 100},
		},
		0, 0, "", "", testBinary, 1000)
	if len(pts) != 1 {
		t.Fatalf("expected 1 point, got %d", len(pts))
	}
	if pts[0].Bit != 3 {
		t.Fatalf("expected Bit 3, got %d", pts[0].Bit)
	}
}

func TestExpandBitZero(t *testing.T) {
	// Bit=0 (the LSB) must be usable; it was previously treated as "not set".
	pts := expandPolledPoint(
		PointConfig{
			Index:  0,
			Bit:    intPtr(0),
			Modbus: ModbusConfig{Type: "HoldingRegister", Address: 100},
		},
		0, 0, "", "", testBinary, 1000)
	if len(pts) != 1 {
		t.Fatalf("expected 1 point, got %d", len(pts))
	}
	if pts[0].Bit != 0 {
		t.Fatalf("expected Bit 0 (LSB), got %d", pts[0].Bit)
	}
}

func TestExpandBitAbsent(t *testing.T) {
	// When Bit is absent from the config, the internal sentinel -1 must be used
	// (not 0, which would mean "extract the LSB").
	pts := expandPolledPoint(
		PointConfig{
			Index:  0,
			Modbus: ModbusConfig{Type: "HoldingRegister", Address: 100},
		},
		0, 0, "", "", testBinary, 1000)
	if len(pts) != 1 {
		t.Fatalf("expected 1 point, got %d", len(pts))
	}
	if pts[0].Bit != -1 {
		t.Fatalf("expected Bit sentinel -1 (not set), got %d", pts[0].Bit)
	}
}

func TestExpandRangeWithCount(t *testing.T) {
	pts := expandPolledPoint(
		PointConfig{
			Range:  &RangeConfig{Start: 0, Stop: 3},
			Modbus: ModbusConfig{Type: "HoldingRegister", Address: 0, Count: 2},
		},
		0, 0, "", "", testAnalog, 1000)
	if len(pts) != 4 {
		t.Fatalf("expected 4 points, got %d", len(pts))
	}
	if pts[0].ModbusAddr != 0 || pts[1].ModbusAddr != 2 ||
		pts[2].ModbusAddr != 4 || pts[3].ModbusAddr != 6 {
		t.Fatalf("range+count: unexpected addresses: %d %d %d %d",
			pts[0].ModbusAddr, pts[1].ModbusAddr, pts[2].ModbusAddr, pts[3].ModbusAddr)
	}
}

func TestExpandAnalogControls(t *testing.T) {
	pts := expandAnalogControls(AnalogControlConfig{
		PointConfig: PointConfig{
			Index:  0,
			Modbus: ModbusConfig{Type: "HoldingRegister", Address: 200},
		},
		ControlType: "AnalogOutputFloat32",
	})
	if len(pts) != 1 {
		t.Fatalf("expected 1 control point, got %d", len(pts))
	}
	if pts[0].ControlType != "AnalogOutputFloat32" {
		t.Fatalf("expected ControlType AnalogOutputFloat32, got %s", pts[0].ControlType)
	}
}

func TestExpandServerAnalogControlsEndianDataType(t *testing.T) {
	// expandServerAnalogControls must propagate Endian and DataType into the
	// resulting ControlPoints so the server handler can decode HR writes correctly.
	pts := expandServerAnalogControls(AnalogControlConfig{
		PointConfig: PointConfig{
			Index:  5,
			Modbus: ModbusConfig{Type: "HoldingRegister", Address: 10, Count: 2},
		},
		ControlType: "AnalogOutputFloat32",
		Endian:      "DCBA",
		DataType:    "Float32",
	})
	if len(pts) != 1 {
		t.Fatalf("expected 1 control point, got %d", len(pts))
	}
	if pts[0].Endian != "DCBA" {
		t.Fatalf("expected Endian DCBA, got %q", pts[0].Endian)
	}
	if pts[0].DataType != "Float32" {
		t.Fatalf("expected DataType Float32, got %q", pts[0].DataType)
	}
}

func TestAnalogControlConfigEndianDataTypeJSON(t *testing.T) {
	// Verify that Endian and DataType round-trip through JSON.
	data := `{
		"TCP": {"Address": "localhost:502"},
		"AnalogControls": [{
			"Index": 3,
			"Modbus": {"Type": "HoldingRegister", "Address": 22, "Count": 2},
			"ControlType": "AnalogOutputFloat32",
			"Endian": "CDAB",
			"DataType": "Float32"
		}]
	}`
	var cfg PortConfig
	if err := json.Unmarshal([]byte(data), &cfg); err != nil {
		t.Fatal(err)
	}
	if len(cfg.AnalogControls) != 1 {
		t.Fatalf("expected 1 AnalogControl, got %d", len(cfg.AnalogControls))
	}
	ac := cfg.AnalogControls[0]
	if ac.Endian != "CDAB" {
		t.Fatalf("expected Endian CDAB, got %q", ac.Endian)
	}
	if ac.DataType != "Float32" {
		t.Fatalf("expected DataType Float32, got %q", ac.DataType)
	}
}

func TestExpandControlDefaultActions(t *testing.T) {
	pts := expandControls(
		ControlConfig{PointConfig: PointConfig{
			Index: 0,
			Modbus: ModbusConfig{Type: "Coil", Address: 0},
		}},
		testBinary)
	if len(pts) != 1 {
		t.Fatalf("expected 1 control point, got %d", len(pts))
	}
	if pts[0].OnAction != "On" {
		t.Fatalf("expected default OnAction 'On', got %s", pts[0].OnAction)
	}
	if pts[0].OffAction != "Off" {
		t.Fatalf("expected default OffAction 'Off', got %s", pts[0].OffAction)
	}
}

func TestExpandControlCustomActions(t *testing.T) {
	pts := expandControls(
		ControlConfig{
			PointConfig: PointConfig{
				Index: 0,
				Modbus: ModbusConfig{Type: "Coil", Address: 0},
			},
			OnAction:  "Pulse",
			OffAction: "Clear",
		},
		testBinary)
	if len(pts) != 1 {
		t.Fatalf("expected 1 control point, got %d", len(pts))
	}
	if pts[0].OnAction != "Pulse" || pts[0].OffAction != "Clear" {
		t.Fatalf("unexpected actions: %s / %s", pts[0].OnAction, pts[0].OffAction)
	}
}

// ---------------------------------------------------------------------------
// Endian / DataType codec tests
// ---------------------------------------------------------------------------

func TestDecodeAnalogRegsInt16(t *testing.T) {
	tests := []struct {
		name    string
		regs    []uint16
		endian  string
		want    float64
	}{
		{"pos default", []uint16{0x0064}, "", 100},
		{"neg default", []uint16{0xFF9C}, "", -100},
		{"BA byte-swap", []uint16{0x6400}, "BA", 100},
		{"BA byte-swap neg", []uint16{0x9CFF}, "BA", -100},
	}
	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			got := decodeAnalogRegs(tc.regs, 1, tc.endian, "Int16")
			if got != tc.want {
				t.Fatalf("want %v, got %v", tc.want, got)
			}
		})
	}
}

func TestDecodeAnalogRegsUint16(t *testing.T) {
	got := decodeAnalogRegs([]uint16{0xFFFF}, 1, "", "Uint16")
	if got != 65535 {
		t.Fatalf("expected 65535, got %v", got)
	}
}

func TestDecodeAnalogRegsInt32(t *testing.T) {
	// 0x00010002 = 65538
	got := decodeAnalogRegs([]uint16{0x0001, 0x0002}, 2, "ABCD", "Int32")
	if got != 65538 {
		t.Fatalf("expected 65538, got %v", got)
	}
}

func TestDecodeAnalogRegsInt32CDAB(t *testing.T) {
	// CDAB: reg[0]=low word, reg[1]=high word
	// reg[0]=0x0002 (low), reg[1]=0x0001 (high) → 0x00010002 = 65538
	got := decodeAnalogRegs([]uint16{0x0002, 0x0001}, 2, "CDAB", "Int32")
	if got != 65538 {
		t.Fatalf("expected 65538, got %v", got)
	}
}

func TestDecodeAnalogRegsFloat32ABCD(t *testing.T) {
	// IEEE 754 float32: 1.0 = 0x3F800000
	got := decodeAnalogRegs([]uint16{0x3F80, 0x0000}, 2, "ABCD", "Float32")
	if got != 1.0 {
		t.Fatalf("expected 1.0, got %v", got)
	}
}

func TestDecodeAnalogRegsFloat32DCBA(t *testing.T) {
	// DCBA = complete little-endian byte order.
	// float32 1.0 bytes in big-endian (ABCD): A=0x3F, B=0x80, C=0x00, D=0x00
	// DCBA wire order: [D, C, B, A] → reg[0]=D<<8|C=0x0000, reg[1]=B<<8|A=0x803F
	// Decode: byteSwap(reg[1])<<16|byteSwap(reg[0]) = 0x3F80<<16|0x0000 = 0x3F800000 = 1.0
	got := decodeAnalogRegs([]uint16{0x0000, 0x803F}, 2, "DCBA", "Float32")
	if got != 1.0 {
		t.Fatalf("expected 1.0, got %v", got)
	}
}

func TestEncodeDecodeRoundTrip(t *testing.T) {
	cases := []struct {
		val      float64
		count    uint16
		endian   string
		dataType string
	}{
		{42.0, 1, "", "Int16"},
		{-42.0, 1, "", "Int16"},
		{100.0, 1, "BA", "Int16"},
		{65535, 1, "", "Uint16"},
		{123456, 2, "ABCD", "Int32"},
		{123456, 2, "CDAB", "Int32"},
		{123456, 2, "BADC", "Int32"},
		{123456, 2, "DCBA", "Int32"},
		{3.14159, 2, "ABCD", "Float32"},
		{3.14159, 2, "DCBA", "Float32"},
		{-273.15, 2, "CDAB", "Float32"},
	}
	for _, tc := range cases {
		regs := encodeAnalogRegs(tc.val, tc.count, tc.endian, tc.dataType)
		got := decodeAnalogRegs(regs, tc.count, tc.endian, tc.dataType)
		// For float32, allow small rounding error from float64↔float32 conversion.
		diff := got - tc.val
		if diff < 0 {
			diff = -diff
		}
		tol := 0.001 * (tc.val + 1)
		if tol < 0 {
			tol = -tol
		}
		if diff > tol+0.001 {
			t.Errorf("val=%v count=%d endian=%s type=%s: encode→decode got %v (diff %v)",
				tc.val, tc.count, tc.endian, tc.dataType, got, diff)
		}
	}
}

// ---------------------------------------------------------------------------
// Server config tests
// ---------------------------------------------------------------------------

func TestParseServerConfigTCP(t *testing.T) {
	data := `{
		"TCP": {"Listen": "0.0.0.0:502"},
		"UnitID": 1,
		"Binaries": [{"Modbus": {"Type": "Coil", "Address": 0}}],
		"BinaryControls": [{"Modbus": {"Type": "Coil", "Address": 10}}]
	}`
	var cfg ServerPortConfig
	if err := json.Unmarshal([]byte(data), &cfg); err != nil {
		t.Fatal(err)
	}
	if cfg.TCP == nil {
		t.Fatal("expected TCP config")
	}
	if cfg.TCP.Listen != "0.0.0.0:502" {
		t.Fatalf("expected listen 0.0.0.0:502, got %s", cfg.TCP.Listen)
	}
	if cfg.UnitID != 1 {
		t.Fatalf("expected UnitID 1, got %d", cfg.UnitID)
	}
	if len(cfg.Binaries) != 1 {
		t.Fatalf("expected 1 Binary, got %d", len(cfg.Binaries))
	}
	if len(cfg.BinaryControls) != 1 {
		t.Fatalf("expected 1 BinaryControl, got %d", len(cfg.BinaryControls))
	}
}

func TestParseServerConfigRTU(t *testing.T) {
	data := `{
		"RTU": {"Port": "/dev/ttyUSB0", "BaudRate": 9600},
		"UnitID": 5
	}`
	var cfg ServerPortConfig
	if err := json.Unmarshal([]byte(data), &cfg); err != nil {
		t.Fatal(err)
	}
	if cfg.RTU == nil {
		t.Fatal("expected RTU config")
	}
	if cfg.RTU.Port != "/dev/ttyUSB0" {
		t.Fatalf("expected port /dev/ttyUSB0, got %s", cfg.RTU.Port)
	}
}

func TestServerConfigValidation(t *testing.T) {
	tests := []struct {
		name string
		cfg  ServerPortConfig
	}{
		{
			"no connection",
			ServerPortConfig{},
		},
		{
			"both TCP and RTU",
			ServerPortConfig{
				TCP: &ServerTCPConfig{Listen: "0.0.0.0:502"},
				RTU: &RTUConfig{Port: "/dev/ttyUSB0"},
			},
		},
		{
			"empty listen address",
			ServerPortConfig{TCP: &ServerTCPConfig{Listen: ""}},
		},
		{
			"BinaryControls with DiscreteInput",
			ServerPortConfig{
				TCP: &ServerTCPConfig{Listen: ":502"},
				BinaryControls: []ControlConfig{
					{PointConfig: PointConfig{Modbus: ModbusConfig{Type: "DiscreteInput", Address: 0}}},
				},
			},
		},
		{
			"AnalogControls with InputRegister",
			ServerPortConfig{
				TCP: &ServerTCPConfig{Listen: ":502"},
				AnalogControls: []AnalogControlConfig{
					{PointConfig: PointConfig{Modbus: ModbusConfig{Type: "InputRegister", Address: 0}}},
				},
			},
		},
	}
	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			if err := validateServerConfig(nil, &tc.cfg); err == nil {
				t.Fatal("expected error, got nil")
			}
		})
	}
}

func TestServerConfigDefaults(t *testing.T) {
	cfg := ServerPortConfig{
		TCP: &ServerTCPConfig{Listen: ":502"},
		RTU: nil,
	}
	if err := validateServerConfig(nil, &cfg); err != nil {
		t.Fatal(err)
	}
	// UnitID 0 means "respond to all"
	if cfg.UnitID != 0 {
		t.Fatalf("expected UnitID 0, got %d", cfg.UnitID)
	}
}

func TestServerConfigAllPointTypes(t *testing.T) {
	data := `{
		"TCP": {"Listen": ":502"},
		"Binaries": [{"Modbus": {"Type": "Coil", "Address": 0}}],
		"Analogs": [{"Modbus": {"Type": "HoldingRegister", "Address": 10, "Count": 2}, "Endian": "ABCD", "DataType": "Float32", "Scale": 2.0, "Offset": -1.0}],
		"BinaryControls": [{"Modbus": {"Type": "Coil", "Address": 20}}],
		"AnalogControls": [{"Modbus": {"Type": "HoldingRegister", "Address": 30, "Count": 2}, "ControlType": "AnalogOutputFloat32"}],
		"BinaryOutputStatuses": [{"Modbus": {"Type": "DiscreteInput", "Address": 40}}],
		"AnalogOutputStatuses": [{"Modbus": {"Type": "InputRegister", "Address": 50}}],
		"OctetStrings": [{"Modbus": {"Type": "HoldingRegister", "Address": 60, "Count": 8}}]
	}`
	var cfg ServerPortConfig
	if err := json.Unmarshal([]byte(data), &cfg); err != nil {
		t.Fatal(err)
	}
	if err := validateServerConfig(nil, &cfg); err != nil {
		t.Fatal(err)
	}
	if len(cfg.Binaries) != 1 || len(cfg.Analogs) != 1 ||
		len(cfg.BinaryControls) != 1 || len(cfg.AnalogControls) != 1 {
		t.Fatal("missing point configs")
	}
	if cfg.Analogs[0].DataType != "Float32" {
		t.Fatalf("expected DataType Float32, got %s", cfg.Analogs[0].DataType)
	}
	if cfg.Analogs[0].Endian != "ABCD" {
		t.Fatalf("expected Endian ABCD, got %s", cfg.Analogs[0].Endian)
	}
	if cfg.Analogs[0].Scale != 2.0 {
		t.Fatalf("expected Scale 2.0, got %v", cfg.Analogs[0].Scale)
	}
}
