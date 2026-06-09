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
 * config.go
 *
 *  Created on: 09/06/2026
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

package main

/*
#cgo CFLAGS: -I${SRCDIR} -I${SRCDIR}/../../../include

#include "opendatacon/odc_c_api.h"
#include "gombus_helpers.h"
*/
import "C"
import (
	"encoding/json"
	"fmt"
	"math"
	"unsafe"
)

// ---------------------------------------------------------------------------
// Shared point config types (used by both client and server)
// ---------------------------------------------------------------------------

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

// AnalogPointConfig extends PointConfig with scaling and encoding fields.
//
// DataType controls how the raw Modbus register(s) are interpreted:
//
//	"Int16"   — signed 16-bit integer (default when Count == 0 or 1)
//	"Uint16"  — unsigned 16-bit integer
//	"Int32"   — signed 32-bit integer (default when Count == 2)
//	"Uint32"  — unsigned 32-bit integer
//	"Float32" — IEEE 754 single-precision float (Count must be 2)
//	"Float64" — IEEE 754 double-precision float (Count must be 4)
//
// Endian controls byte/word order for multi-register values:
//
//	"AB"   — big-endian single register (byte swap not applied, default)
//	"BA"   — little-endian single register (bytes within register swapped)
//	"ABCD" — big-endian two registers: reg[0]=high word, reg[1]=low word (default)
//	"CDAB" — word-swapped: reg[0]=low word, reg[1]=high word
//	"BADC" — byte-swap within each register, then big-endian combine
//	"DCBA" — complete little-endian: reg[0]=low word LE, reg[1]=high word LE
//
// For Count == 4 (Float64), big-endian register order is always used.
type AnalogPointConfig struct {
	PointConfig
	Scale    float64 `json:"Scale,omitempty"`
	Offset   float64 `json:"Offset,omitempty"`
	Endian   string  `json:"Endian,omitempty"`
	DataType string  `json:"DataType,omitempty"`
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

// ---------------------------------------------------------------------------
// Client (polling) port config
// ---------------------------------------------------------------------------

type PortConfig struct {
	TCP                *TCPConfig `json:"TCP,omitempty"`
	RTU                *RTUConfig `json:"RTU,omitempty"`
	UnitID             uint8      `json:"UnitID"`
	TimeoutMs          int        `json:"TimeoutMs"`
	PollRateMs         int        `json:"PollRateMs"`
	MaxConcurrentPolls int        `json:"MaxConcurrentPolls,omitempty"`

	Binaries             []PointConfig            `json:"Binaries"`
	Analogs              []AnalogPointConfig      `json:"Analogs"`
	BinaryOutputStatuses []PointConfig            `json:"BinaryOutputStatuses"`
	AnalogOutputStatuses []AnalogPointConfig      `json:"AnalogOutputStatuses"`
	OctetStrings         []OctetStringPointConfig `json:"OctetStrings"`
	BinaryControls       []ControlConfig          `json:"BinaryControls"`
	AnalogControls       []AnalogControlConfig    `json:"AnalogControls"`
}

// TCPConfig is the client-side TCP connection config ("connect to" address).
type TCPConfig struct {
	Address string `json:"Address"`
}

// RTUConfig is shared between client and server RTU transport.
type RTUConfig struct {
	Port     string `json:"Port"`
	BaudRate int    `json:"BaudRate"`
	DataBits int    `json:"DataBits"`
	StopBits int    `json:"StopBits"`
	Parity   string `json:"Parity"`
}

// ---------------------------------------------------------------------------
// Server port config
// ---------------------------------------------------------------------------

// ServerTCPConfig is the server-side TCP listen config.
type ServerTCPConfig struct {
	Listen string `json:"Listen"` // e.g. "0.0.0.0:502"
}

// ServerPortConfig holds the configuration for a GoModbusServer port.
//
// Point array semantics (mirrored from the client with inverted data direction):
//
//	Binaries / BinaryOutputStatuses:
//	    ODC Binary/CROB events → update coil/DI data store.
//	    Modbus clients read the stored value.
//
//	Analogs / AnalogOutputStatuses:
//	    ODC Analog events → update holding-register/input-register data store.
//	    Modbus clients read the stored value.
//
//	OctetStrings:
//	    ODC OctetString events → update a block of holding/input registers.
//	    Modbus clients read the block.
//
//	BinaryControls:
//	    Modbus client writes a Coil → server publishes ODC Binary event.
//	    Only Coil type is valid here (DiscreteInput is read-only by Modbus spec).
//
//	AnalogControls:
//	    Modbus client writes Holding Register(s) → server publishes ODC
//	    AnalogOutput* event (type controlled by ControlType field).
//	    Only HoldingRegister type is valid here (InputRegister is read-only).
//
// The same Modbus address may appear in both a readable array (e.g. Analogs)
// and a writable array (e.g. AnalogControls) to achieve two-way mapping.
type ServerPortConfig struct {
	TCP *ServerTCPConfig `json:"TCP,omitempty"`
	RTU *RTUConfig       `json:"RTU,omitempty"`

	UnitID int `json:"UnitID"` // 0 means respond to all unit IDs

	Binaries             []PointConfig            `json:"Binaries"`
	Analogs              []AnalogPointConfig      `json:"Analogs"`
	BinaryOutputStatuses []PointConfig            `json:"BinaryOutputStatuses"`
	AnalogOutputStatuses []AnalogPointConfig      `json:"AnalogOutputStatuses"`
	OctetStrings         []OctetStringPointConfig `json:"OctetStrings"`
	BinaryControls       []ControlConfig          `json:"BinaryControls"`
	AnalogControls       []AnalogControlConfig    `json:"AnalogControls"`
}

// ---------------------------------------------------------------------------
// Expanded point types (runtime, after range expansion)
// ---------------------------------------------------------------------------

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
	DataType   string
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
	Endian      string
	DataType    string
}

// ---------------------------------------------------------------------------
// Parse / validate — client
// ---------------------------------------------------------------------------

func parseConfig(inst unsafe.Pointer) (*PortConfig, error) {
	cJSON := C.odc_get_config_json(inst)
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
		if err := applyRTUDefaults(cfg.RTU); err != nil {
			return err
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

// ---------------------------------------------------------------------------
// Parse / validate — server
// ---------------------------------------------------------------------------

func parseServerConfig(inst unsafe.Pointer) (*ServerPortConfig, error) {
	cJSON := C.odc_get_config_json(inst)
	if cJSON == nil {
		return nil, fmt.Errorf("odc_GetConfigJSON returned nil")
	}
	jsonStr := C.GoString(cJSON)
	if jsonStr == "" {
		return nil, fmt.Errorf("empty config JSON")
	}

	var cfg ServerPortConfig
	if err := json.Unmarshal([]byte(jsonStr), &cfg); err != nil {
		return nil, fmt.Errorf("parse server config JSON: %w", err)
	}
	return &cfg, nil
}

func validateServerConfig(cfg *ServerPortConfig) error {
	if cfg.TCP == nil && cfg.RTU == nil {
		return fmt.Errorf("either TCP or RTU listen config is required")
	}
	if cfg.TCP != nil && cfg.RTU != nil {
		return fmt.Errorf("specify only one of TCP or RTU, not both")
	}
	if cfg.TCP != nil && cfg.TCP.Listen == "" {
		return fmt.Errorf("TCP listen address is empty")
	}
	if cfg.RTU != nil {
		if err := applyRTUDefaults(cfg.RTU); err != nil {
			return err
		}
	}
	// Validate that BinaryControls only reference Coil (master-writable).
	for _, bc := range cfg.BinaryControls {
		if bc.Modbus.Type != "Coil" {
			return fmt.Errorf("BinaryControls entry has Modbus.Type %q; only \"Coil\" is writable by a Modbus client", bc.Modbus.Type)
		}
	}
	// Validate that AnalogControls only reference HoldingRegister (master-writable).
	for _, ac := range cfg.AnalogControls {
		if ac.Modbus.Type != "HoldingRegister" {
			return fmt.Errorf("AnalogControls entry has Modbus.Type %q; only \"HoldingRegister\" is writable by a Modbus client", ac.Modbus.Type)
		}
	}
	return nil
}

// applyRTUDefaults fills in zero-value RTU fields with sensible defaults.
func applyRTUDefaults(cfg *RTUConfig) error {
	if cfg.Port == "" {
		return fmt.Errorf("RTU port is empty")
	}
	if cfg.BaudRate == 0 {
		cfg.BaudRate = 19200
	}
	if cfg.DataBits == 0 {
		cfg.DataBits = 8
	}
	if cfg.StopBits == 0 {
		cfg.StopBits = 1
	}
	if cfg.Parity == "" {
		cfg.Parity = "N"
	}
	return nil
}

// ---------------------------------------------------------------------------
// Point expansion helpers
// ---------------------------------------------------------------------------

// expandPolledPoint expands a single PointConfig (potentially a range) into
// one or more PolledPoints.  scale, offset, endian, dataType are carried over
// from AnalogPointConfig callers; binary/octet callers pass zero values.
func expandPolledPoint(cfg PointConfig, scale float64, offset float64, endian string, dataType string, odcType uint8, defaultRate int) []PolledPoint {
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
				DataType:   dataType,
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
		DataType:   dataType,
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

// expandServerAnalogControlsFull is like expandAnalogControls but also
// carries Endian and DataType for server-side HR write decoding.
func expandServerAnalogControlsFull(cfg AnalogControlConfig, endian, dataType string) []ControlPoint {
	pts := expandAnalogControls(cfg)
	for i := range pts {
		pts[i].Endian = endian
		pts[i].DataType = dataType
	}
	return pts
}

// ---------------------------------------------------------------------------
// Endian / DataType codec helpers
// ---------------------------------------------------------------------------

// byteSwap16 swaps the two bytes of a uint16.
func byteSwap16(v uint16) uint16 {
	return (v >> 8) | (v << 8)
}

// regsToUint32 combines two Modbus registers into a uint32 according to the
// Endian notation.  See AnalogPointConfig for a full description of each value.
func regsToUint32(r0, r1 uint16, endian string) uint32 {
	switch endian {
	case "CDAB": // word-swapped: reg[0]=low word, reg[1]=high word
		return uint32(r1)<<16 | uint32(r0)
	case "BADC": // byte-swap within each register, then ABCD combine
		return uint32(byteSwap16(r0))<<16 | uint32(byteSwap16(r1))
	case "DCBA": // complete little-endian
		return uint32(byteSwap16(r1))<<16 | uint32(byteSwap16(r0))
	default: // "ABCD" or ""
		return uint32(r0)<<16 | uint32(r1)
	}
}

// uint32ToRegs splits a uint32 into two Modbus registers according to the
// Endian notation.  This is the inverse of regsToUint32.
func uint32ToRegs(v uint32, endian string) [2]uint16 {
	hi := uint16(v >> 16)
	lo := uint16(v)
	switch endian {
	case "CDAB":
		return [2]uint16{lo, hi}
	case "BADC":
		return [2]uint16{byteSwap16(hi), byteSwap16(lo)}
	case "DCBA":
		return [2]uint16{byteSwap16(lo), byteSwap16(hi)}
	default: // "ABCD" or ""
		return [2]uint16{hi, lo}
	}
}

// decodeAnalogRegs converts raw Modbus register(s) to a float64 value,
// applying the Endian and DataType settings.
func decodeAnalogRegs(regs []uint16, count uint16, endian, dataType string) float64 {
	switch {
	case count <= 1:
		r := regs[0]
		if endian == "BA" {
			r = byteSwap16(r)
		}
		switch dataType {
		case "Uint16":
			return float64(r)
		default: // "Int16" or ""
			return float64(int16(r))
		}

	case count == 2:
		if len(regs) < 2 {
			// Insufficient registers: fall back to 16-bit decode.
			return decodeAnalogRegs(regs, 1, endian, dataType)
		}
		raw := regsToUint32(regs[0], regs[1], endian)
		switch dataType {
		case "Uint32":
			return float64(raw)
		case "Float32":
			return float64(math.Float32frombits(raw))
		default: // "Int32" or ""
			return float64(int32(raw))
		}

	case count >= 4:
		if len(regs) < 4 {
			return decodeAnalogRegs(regs, 2, endian, dataType)
		}
		// Float64: 4 registers, big-endian word order.
		raw := uint64(regs[0])<<48 | uint64(regs[1])<<32 |
			uint64(regs[2])<<16 | uint64(regs[3])
		return math.Float64frombits(raw)

	default:
		return float64(int16(regs[0]))
	}
}

// encodeAnalogRegs converts a float64 ODC value to raw Modbus register(s),
// applying the Endian and DataType settings.  This is the inverse of
// decodeAnalogRegs and is used by the server when updating its data store.
func encodeAnalogRegs(val float64, count uint16, endian, dataType string) []uint16 {
	switch {
	case count <= 1:
		var r uint16
		switch dataType {
		case "Uint16":
			if val < 0 {
				val = 0
			} else if val > 65535 {
				val = 65535
			}
			r = uint16(val)
		default: // "Int16" or ""
			if val < -32768 {
				val = -32768
			} else if val > 32767 {
				val = 32767
			}
			r = uint16(int16(val))
		}
		if endian == "BA" {
			r = byteSwap16(r)
		}
		return []uint16{r}

	case count == 2:
		var raw uint32
		switch dataType {
		case "Uint32":
			if val < 0 {
				val = 0
			}
			raw = uint32(val)
		case "Float32":
			raw = math.Float32bits(float32(val))
		default: // "Int32" or ""
			raw = uint32(int32(val))
		}
		pair := uint32ToRegs(raw, endian)
		return pair[:]

	case count >= 4:
		// Float64: 4 registers, big-endian word order.
		raw := math.Float64bits(val)
		return []uint16{
			uint16(raw >> 48),
			uint16(raw >> 32),
			uint16(raw >> 16),
			uint16(raw),
		}

	default:
		return []uint16{uint16(int16(val))}
	}
}
