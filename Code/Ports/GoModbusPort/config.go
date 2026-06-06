package main

/*
#cgo CFLAGS: -I${SRCDIR}/../../../include

#include "opendatacon/odc_c_api.h"
*/
import "C"
import "encoding/json"
import "fmt"
import "unsafe"

type RangeConfig struct {
	Start uint64 `json:"Start"`
	Stop  uint64 `json:"Stop"`
}

type ModbusConfig struct {
	Type    string `json:"Type"`
	Address uint16 `json:"Address"`
	Count   uint16 `json:"Count,omitempty"`
}

type PointConfig struct {
	Index      uint64       `json:"Index"`
	Range      *RangeConfig `json:"Range,omitempty"`
	Modbus     ModbusConfig `json:"Modbus"`
	PollRateMs int          `json:"PollRateMs,omitempty"`
	Bit        int          `json:"Bit,omitempty"`
}

type AnalogPointConfig struct {
	PointConfig
	Scale  float64 `json:"Scale,omitempty"`
	Offset float64 `json:"Offset,omitempty"`
	Endian string  `json:"Endian,omitempty"`
}

type OctetStringPointConfig struct {
	PointConfig
}

type ControlConfig struct {
	PointConfig
	OnAction  string `json:"OnAction,omitempty"`
	OffAction string `json:"OffAction,omitempty"`
}

type AnalogControlConfig struct {
	PointConfig
	ControlType string `json:"ControlType,omitempty"`
}

type PortConfig struct {
	TCP                *TCPConfig `json:"TCP,omitempty"`
	RTU                *RTUConfig `json:"RTU,omitempty"`
	UnitID             uint8      `json:"UnitID"`
	TimeoutMs          int        `json:"TimeoutMs"`
	PollRateMs         int        `json:"PollRateMs"`
	MaxConcurrentPolls int        `json:"MaxConcurrentPolls,omitempty"`

	Binaries             []PointConfig             `json:"Binaries"`
	Analogs              []AnalogPointConfig        `json:"Analogs"`
	BinaryOutputStatuses []PointConfig              `json:"BinaryOutputStatuses"`
	AnalogOutputStatuses []AnalogPointConfig        `json:"AnalogOutputStatuses"`
	OctetStrings         []OctetStringPointConfig   `json:"OctetStrings"`
	BinaryControls       []ControlConfig            `json:"BinaryControls"`
	AnalogControls       []AnalogControlConfig      `json:"AnalogControls"`
}

type TCPConfig struct {
	Address string `json:"Address"`
}

type RTUConfig struct {
	Port      string `json:"Port"`
	BaudRate  int    `json:"BaudRate"`
	DataBits  int    `json:"DataBits"`
	StopBits  int    `json:"StopBits"`
	Parity    string `json:"Parity"`
}

type PolledPoint struct {
	ODCType    uint8
	ODCIndex   uint64
	ModbusType string
	ModbusAddr uint16
	Count      uint16
	PollRateMs int
	Scale      float64
	Offset     float64
	Endian     string
	Bit        int
	lastPollNs int64
}

func (p *PolledPoint) due(nowNs int64) bool {
	if p.PollRateMs <= 0 {
		return false
	}
	interval := int64(p.PollRateMs) * 1_000_000
	return (nowNs - p.lastPollNs) >= interval
}

type ControlPoint struct {
	ODCType     uint8
	ODCIndex    uint64
	ModbusType  string
	ModbusAddr  uint16
	OnAction    string
	OffAction   string
	ControlType string
	Count       uint16
}

func parseConfig(inst unsafe.Pointer) (*PortConfig, error) {
	cJSON := C.odc_GetConfigJSON(inst)
	if cJSON == nil {
		return nil, fmt.Errorf("odc_GetConfigJSON returned nil")
	}
	jsonStr := C.GoString(cJSON)
	if jsonStr == "" {
		return nil, fmt.Errorf("empty config JSON")
	}

	var cfg PortConfig
	if err := json.Unmarshal([]byte(jsonStr), &cfg); err != nil {
		return nil, fmt.Errorf("parse config JSON: %w", err)
	}
	return &cfg, nil
}

func validateConfig(cfg *PortConfig) error {
	if cfg.TCP == nil && cfg.RTU == nil {
		return fmt.Errorf("either TCP or RTU connection config is required")
	}
	if cfg.TCP != nil && cfg.RTU != nil {
		return fmt.Errorf("specify only one of TCP or RTU, not both")
	}
	if cfg.TCP != nil && cfg.TCP.Address == "" {
		return fmt.Errorf("TCP address is empty")
	}
	if cfg.RTU != nil {
		if cfg.RTU.Port == "" {
			return fmt.Errorf("RTU port is empty")
		}
		if cfg.RTU.BaudRate == 0 {
			cfg.RTU.BaudRate = 19200
		}
		if cfg.RTU.DataBits == 0 {
			cfg.RTU.DataBits = 8
		}
		if cfg.RTU.StopBits == 0 {
			cfg.RTU.StopBits = 1
		}
		if cfg.RTU.Parity == "" {
			cfg.RTU.Parity = "N"
		}
	}
	if cfg.PollRateMs <= 0 {
		cfg.PollRateMs = 1000
	}
	if cfg.TimeoutMs <= 0 {
		cfg.TimeoutMs = 1000
	}
	if cfg.MaxConcurrentPolls <= 0 {
		cfg.MaxConcurrentPolls = 1
	}
	return nil
}

func expandPolledPoint(cfg PointConfig, scale float64, offset float64, endian string, odcType uint8, defaultRate int) []PolledPoint {
	count := cfg.Modbus.Count
	if count == 0 {
		count = 1
	}
	bit := cfg.Bit
	if bit == 0 && cfg.Bit == 0 {
		bit = -1
	}

	if cfg.Range != nil {
		var pts []PolledPoint
		for i := cfg.Range.Start; i <= cfg.Range.Stop; i++ {
			addrOff := uint16((i - cfg.Range.Start) * uint64(count))
			rate := cfg.PollRateMs
			if rate <= 0 {
				rate = defaultRate
			}
			pts = append(pts, PolledPoint{
				ODCType:    odcType,
				ODCIndex:   i,
				ModbusType: cfg.Modbus.Type,
				ModbusAddr: cfg.Modbus.Address + addrOff,
				Count:      count,
				PollRateMs: rate,
				Scale:      scale,
				Offset:     offset,
				Endian:     endian,
				Bit:        bit,
			})
		}
		return pts
	}

	rate := cfg.PollRateMs
	if rate <= 0 {
		rate = defaultRate
	}
	return []PolledPoint{{
		ODCType:    odcType,
		ODCIndex:   cfg.Index,
		ModbusType: cfg.Modbus.Type,
		ModbusAddr: cfg.Modbus.Address,
		Count:      count,
		PollRateMs: rate,
		Scale:      scale,
		Offset:     offset,
		Endian:     endian,
		Bit:        bit,
	}}
}

func expandControls(cfg ControlConfig, odcType uint8) []ControlPoint {
	onAction := cfg.OnAction
	if onAction == "" {
		onAction = "On"
	}
	offAction := cfg.OffAction
	if offAction == "" {
		offAction = "Off"
	}
	count := cfg.Modbus.Count
	if count == 0 {
		count = 1
	}

	if cfg.Range != nil {
		var pts []ControlPoint
		for i := cfg.Range.Start; i <= cfg.Range.Stop; i++ {
			addrOff := uint16(i - cfg.Range.Start)
			pts = append(pts, ControlPoint{
				ODCType:    odcType,
				ODCIndex:   i,
				ModbusType: cfg.Modbus.Type,
				ModbusAddr: cfg.Modbus.Address + addrOff,
				OnAction:   onAction,
				OffAction:  offAction,
				Count:      count,
			})
		}
		return pts
	}

	return []ControlPoint{{
		ODCType:    odcType,
		ODCIndex:   cfg.Index,
		ModbusType: cfg.Modbus.Type,
		ModbusAddr: cfg.Modbus.Address,
		OnAction:   onAction,
		OffAction:  offAction,
		Count:      count,
	}}
}

func expandAnalogControls(cfg AnalogControlConfig) []ControlPoint {
	ct := cfg.ControlType
	if ct == "" {
		ct = "AnalogOutputInt16"
	}
	var odcType uint8
	switch ct {
	case "AnalogOutputInt16":
		odcType = C.C_EventType_AnalogOutputInt16
	case "AnalogOutputInt32":
		odcType = C.C_EventType_AnalogOutputInt32
	case "AnalogOutputFloat32":
		odcType = C.C_EventType_AnalogOutputFloat32
	case "AnalogOutputDouble64":
		odcType = C.C_EventType_AnalogOutputDouble64
	default:
		odcType = C.C_EventType_AnalogOutputInt16
	}
	pts := expandControls(ControlConfig{PointConfig: cfg.PointConfig}, odcType)
	for i := range pts {
		pts[i].ControlType = ct
	}
	return pts
}
