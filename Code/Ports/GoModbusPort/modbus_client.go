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
 * modbus_client.go
 *
 *  Created on: 09/06/2026
 *      Author: Neil Stephens <dearknarl@gmail.com>
 */

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
