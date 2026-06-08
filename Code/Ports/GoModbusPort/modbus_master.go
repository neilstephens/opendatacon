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
		var parity uint
		switch cfg.RTU.Parity {
		case "E", "e":
			parity = modbus.PARITY_EVEN
		case "O", "o":
			parity = modbus.PARITY_ODD
		default:
			parity = modbus.PARITY_NONE
		}
		clientConf.Speed = uint(cfg.RTU.BaudRate)
		clientConf.DataBits = uint(cfg.RTU.DataBits)
		clientConf.Parity = parity
		clientConf.StopBits = uint(cfg.RTU.StopBits)
	}

	client, err := modbus.NewClient(clientConf)
	if err != nil {
		return nil, fmt.Errorf("modbus.NewClient: %w", err)
	}

	return client, nil
}

func openClient(client *modbus.ModbusClient, unitID uint8) error {
	if err := client.SetUnitId(unitID); err != nil {
		return fmt.Errorf("SetUnitId: %w", err)
	}
	return client.Open()
}
