package main

import (
	"fmt"
	"time"

	"github.com/simonvetter/modbus"
)

func newModbusClient(cfg *PortConfig) (*modbus.ModbusClient, error) {
	url := ""
	if cfg.TCP != nil {
		url = "tcp://" + cfg.TCP.Address
	} else if cfg.RTU != nil {
		url = fmt.Sprintf("rtu://%s", cfg.RTU.Port)
	} else {
		return nil, fmt.Errorf("no TCP or RTU connection config")
	}

	clientConf := &modbus.ClientConfiguration{
		URL:     url,
		Timeout: time.Duration(cfg.TimeoutMs) * time.Millisecond,
	}

	if cfg.RTU != nil {
		clientConf.Speed = uint(cfg.RTU.BaudRate)
		clientConf.DataBits = uint(cfg.RTU.DataBits)
		clientConf.Parity = modbusParityFromString(cfg.RTU.Parity)
		clientConf.StopBits = uint(cfg.RTU.StopBits)
	}

	client, err := modbus.NewClient(clientConf)
	if err != nil {
		return nil, fmt.Errorf("modbus.NewClient: %w", err)
	}

	return client, nil
}

func openModbusClient(client *modbus.ModbusClient, unitID uint8) error {
	if err := client.SetUnitId(unitID); err != nil {
		return fmt.Errorf("SetUnitId: %w", err)
	}
	return client.Open()
}

// modbusParityFromString converts the config parity string ("E", "O", "N") to
// the modbus library constant.  Used by both client and server.
func modbusParityFromString(parity string) uint {
	switch parity {
	case "E", "e":
		return modbus.PARITY_EVEN
	case "O", "o":
		return modbus.PARITY_ODD
	default:
		return modbus.PARITY_NONE
	}
}
