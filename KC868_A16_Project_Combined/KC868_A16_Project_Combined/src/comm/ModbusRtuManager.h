#pragma once
/**
 * ModbusRtuManager.h
 * Modbus RTU Slave implementation for KC868-A16 (ESP32) over RS485.
 *
 * Register map: Approved consolidated map (see MODBUS register map spreadsheet).
 *
 * Notes:
 * - Uses ModbusRTU library (Alexander Emelianov).
 * - Does NOT modify existing driver behavior; it reads/writes existing globals.
 */
#include <Arduino.h>

void initModbusRtu();
void taskModbusRtu();
bool isModbusRtuRunning();
