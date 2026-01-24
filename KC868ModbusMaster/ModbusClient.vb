Option Strict On
Option Explicit On

Imports System
Imports System.IO.Ports
Imports System.Threading
Imports Modbus.Device

Namespace KC868ModbusMaster
    Public Class ModbusClient
        Implements IDisposable

        Private _port As SerialPort
        Private _master As IModbusSerialMaster
        Private ReadOnly _sync As New Object()

        Public ReadOnly Property IsConnected As Boolean
            Get
                Return _port IsNot Nothing AndAlso _port.IsOpen AndAlso _master IsNot Nothing
            End Get
        End Property

        Public Sub Connect(portName As String, baud As Integer, dataBits As Integer, parity As Parity, stopBits As StopBits)
            SyncLock _sync
                Disconnect()

                ' SerialPort timeouts here are for the Windows driver layer.
                ' Modbus timeouts are set on _master.Transport below.
                _port = New SerialPort(portName, baud, parity, dataBits, stopBits) With {
                    .ReadTimeout = 2000,
                    .WriteTimeout = 2000,
                    .Handshake = Handshake.None,
                    .DtrEnable = True,
                    .RtsEnable = False
                }
                _port.Open()

                _master = ModbusSerialMaster.CreateRtu(_port)
                ' Give the ESP32 slave enough time to respond and allow retry on occasional CRC/frame issues.
                _master.Transport.Retries = 2
                _master.Transport.ReadTimeout = 2000
                _master.Transport.WriteTimeout = 2000
            End SyncLock
        End Sub

        Public Sub Disconnect()
            SyncLock _sync
                If _master IsNot Nothing Then
                    _master.Dispose()
                    _master = Nothing
                End If
                If _port IsNot Nothing Then
                    If _port.IsOpen Then
                        Try
                            _port.Close()
                        Catch
                        End Try
                    End If
                    _port.Dispose()
                    _port = Nothing
                End If
            End SyncLock
        End Sub

        Public Function ReadCoils(slaveId As Byte, startAddress As UShort, count As UShort) As Boolean()
            SyncLock _sync
                Return _master.ReadCoils(slaveId, startAddress, count)
            End SyncLock
        End Function

        Public Function ReadInputs(slaveId As Byte, startAddress As UShort, count As UShort) As Boolean()
            SyncLock _sync
                Return _master.ReadInputs(slaveId, startAddress, count)
            End SyncLock
        End Function

        Public Function ReadInputRegisters(slaveId As Byte, startAddress As UShort, count As UShort) As UShort()
            SyncLock _sync
                Return _master.ReadInputRegisters(slaveId, startAddress, count)
            End SyncLock
        End Function

        Public Function ReadHoldingRegisters(slaveId As Byte, startAddress As UShort, count As UShort) As UShort()
            SyncLock _sync
                Return _master.ReadHoldingRegisters(slaveId, startAddress, count)
            End SyncLock
        End Function

        Public Sub WriteSingleCoil(slaveId As Byte, address As UShort, value As Boolean)
            SyncLock _sync
                _master.WriteSingleCoil(slaveId, address, value)
            End SyncLock
        End Sub

        Public Sub WriteMultipleCoils(slaveId As Byte, startAddress As UShort, values As Boolean())
            SyncLock _sync
                _master.WriteMultipleCoils(slaveId, startAddress, values)
            End SyncLock
        End Sub

        Public Sub WriteSingleRegister(slaveId As Byte, address As UShort, value As UShort)
            SyncLock _sync
                _master.WriteSingleRegister(slaveId, address, value)
            End SyncLock
        End Sub

        Public Sub WriteMultipleRegisters(slaveId As Byte, startAddress As UShort, values As UShort())
            SyncLock _sync
                _master.WriteMultipleRegisters(slaveId, startAddress, values)
            End SyncLock
        End Sub

        Public Sub Dispose() Implements IDisposable.Dispose
            Disconnect()
        End Sub
    End Class
End Namespace
