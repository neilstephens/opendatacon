package main

import (
	"encoding/json"
	"testing"
)

const (
	testBinary = 1
	testAnalog = 3
)

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
			if err := validateConfig(&cfg); err == nil {
				t.Fatal("expected error, got nil")
			}
		})
	}
}

func TestConfigDefaults(t *testing.T) {
	cfg := PortConfig{TCP: &TCPConfig{Address: "localhost:502"}}
	if err := validateConfig(&cfg); err != nil {
		t.Fatal(err)
	}
	if cfg.PollRateMs != 1000 {
		t.Fatalf("expected default PollRateMs 1000, got %d", cfg.PollRateMs)
	}
	if cfg.TimeoutMs != 1000 {
		t.Fatalf("expected default TimeoutMs 1000, got %d", cfg.TimeoutMs)
	}

	cfg2 := PortConfig{RTU: &RTUConfig{Port: "/dev/ttyUSB0"}}
	if err := validateConfig(&cfg2); err != nil {
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
		0, 0, "", testBinary, 1000)
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
		2.0, 0.5, "ABCD", testAnalog, 2000)
	if len(pts) != 10 {
		t.Fatalf("expected 10 points, got %d", len(pts))
	}
	for i, pt := range pts {
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
		0, 0, "", testAnalog, 1000)
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
			Bit:    3,
			Modbus: ModbusConfig{Type: "HoldingRegister", Address: 100},
		},
		0, 0, "", testBinary, 1000)
	if len(pts) != 1 {
		t.Fatalf("expected 1 point, got %d", len(pts))
	}
	if pts[0].Bit != 3 {
		t.Fatalf("expected Bit 3, got %d", pts[0].Bit)
	}
}

func TestExpandRangeWithCount(t *testing.T) {
	pts := expandPolledPoint(
		PointConfig{
			Range:  &RangeConfig{Start: 0, Stop: 3},
			Modbus: ModbusConfig{Type: "HoldingRegister", Address: 0, Count: 2},
		},
		0, 0, "", testAnalog, 1000)
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
