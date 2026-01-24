Option Strict On
Option Explicit On

Imports System
Imports System.Collections.Generic
Imports System.Text

Namespace KC868ModbusMaster
    Public Module RegisterMap
        Public ReadOnly Property Map As List(Of RegisterDef)
            Get
                If _map Is Nothing Then
                    _map = Build()
                End If
                Return _map
            End Get
        End Property
        Private _map As List(Of RegisterDef)

        Private Function Build() As List(Of RegisterDef)
            Dim m As New List(Of RegisterDef)()

            ' ---- Coils (0xxxx) ----
            For i As Integer = 0 To 15
                m.Add(New RegisterDef(ModbusTable.Coils, 1 + i, i, 1, "R/W", RegDataType.Bool, "", $"DO{1 + i} MOSFET output"))
            Next
            m.Add(New RegisterDef(ModbusTable.Coils, 19, 18, 1, "R/W", RegDataType.Bool, "", "Outputs Master Enable (0=force all OFF)"))
            m.Add(New RegisterDef(ModbusTable.Coils, 20, 19, 1, "R/W", RegDataType.Bool, "", "Night Mode Enable"))

            ' ---- Discrete Inputs (1xxxx) ----
            For i As Integer = 0 To 15
                m.Add(New RegisterDef(ModbusTable.DiscreteInputs, 10001 + i, i, 1, "R", RegDataType.Bool, "", $"DI{1 + i} (main input)"))
            Next
            For i As Integer = 0 To 2
                m.Add(New RegisterDef(ModbusTable.DiscreteInputs, 10017 + i, 16 + i, 1, "R", RegDataType.Bool, "", $"Direct input {1 + i} (GPIO)"))
            Next
            m.Add(New RegisterDef(ModbusTable.DiscreteInputs, 10020, 19, 1, "R", RegDataType.Bool, "", "Ethernet connected"))
            m.Add(New RegisterDef(ModbusTable.DiscreteInputs, 10021, 20, 1, "R", RegDataType.Bool, "", "WiFi connected"))
            m.Add(New RegisterDef(ModbusTable.DiscreteInputs, 10022, 21, 1, "R", RegDataType.Bool, "", "AP mode active"))
            m.Add(New RegisterDef(ModbusTable.DiscreteInputs, 10023, 22, 1, "R", RegDataType.Bool, "", "Restart required flag"))
            m.Add(New RegisterDef(ModbusTable.DiscreteInputs, 10024, 23, 1, "R", RegDataType.Bool, "", "Modbus RTU active"))

            ' ---- Input Registers (3xxxx) ----
            m.Add(New RegisterDef(ModbusTable.InputRegisters, 30001, 0, 1, "R", RegDataType.U16, "", "Register map version (0x0100=v1.0)"))
            m.Add(New RegisterDef(ModbusTable.InputRegisters, 30003, 2, 1, "R", RegDataType.U16, "", "Heartbeat (increments every 1s)"))
            m.Add(New RegisterDef(ModbusTable.InputRegisters, 30004, 3, 2, "R", RegDataType.U32, "s", "Uptime seconds (U32)"))
            m.Add(New RegisterDef(ModbusTable.InputRegisters, 30006, 5, 1, "R", RegDataType.U16, "", "Change sequence counter"))
            m.Add(New RegisterDef(ModbusTable.InputRegisters, 30010, 9, 2, "R", RegDataType.U32, "bytes", "Free heap (U32)"))
            m.Add(New RegisterDef(ModbusTable.InputRegisters, 30012, 11, 1, "R", RegDataType.U16, "MHz", "CPU frequency"))

            For ch As Integer = 0 To 3
                m.Add(New RegisterDef(ModbusTable.InputRegisters, 30013 + ch, 12 + ch, 1, "R", RegDataType.U16, "", $"AI{1 + ch} raw (0..4095)"))
                m.Add(New RegisterDef(ModbusTable.InputRegisters, 30017 + ch, 16 + ch, 1, "R", RegDataType.U16, "mV", $"AI{1 + ch} millivolts (0..5000)"))
            Next

            m.Add(New RegisterDef(ModbusTable.InputRegisters, 30021, 20, 1, "R", RegDataType.S16, "0.1°C", "DHT1 temp x10"))
            m.Add(New RegisterDef(ModbusTable.InputRegisters, 30022, 21, 1, "R", RegDataType.U16, "0.1%RH", "DHT1 humidity x10"))
            m.Add(New RegisterDef(ModbusTable.InputRegisters, 30023, 22, 1, "R", RegDataType.S16, "0.1°C", "DHT2 temp x10"))
            m.Add(New RegisterDef(ModbusTable.InputRegisters, 30024, 23, 1, "R", RegDataType.U16, "0.1%RH", "DHT2 humidity x10"))
            m.Add(New RegisterDef(ModbusTable.InputRegisters, 30025, 24, 1, "R", RegDataType.S16, "0.1°C", "DS18B20 temp x10"))
            m.Add(New RegisterDef(ModbusTable.InputRegisters, 30026, 25, 1, "R", RegDataType.BitmaskU16, "", "Sensor status bitfield (bit0=DHT1 ok, bit1=DHT2 ok, bit2=DS18 ok)"))

            m.Add(New RegisterDef(ModbusTable.InputRegisters, 30027, 26, 2, "R", RegDataType.U32, "unix_s", "RTC UNIX time seconds (U32)"))
            m.Add(New RegisterDef(ModbusTable.InputRegisters, 30029, 28, 1, "R", RegDataType.U16, "", "RTC year"))
            m.Add(New RegisterDef(ModbusTable.InputRegisters, 30030, 29, 1, "R", RegDataType.U16, "", "RTC month"))
            m.Add(New RegisterDef(ModbusTable.InputRegisters, 30031, 30, 1, "R", RegDataType.U16, "", "RTC day"))
            m.Add(New RegisterDef(ModbusTable.InputRegisters, 30032, 31, 1, "R", RegDataType.U16, "", "RTC hour"))
            m.Add(New RegisterDef(ModbusTable.InputRegisters, 30033, 32, 1, "R", RegDataType.U16, "", "RTC minute"))
            m.Add(New RegisterDef(ModbusTable.InputRegisters, 30034, 33, 1, "R", RegDataType.U16, "", "RTC second"))

            m.Add(New RegisterDef(ModbusTable.InputRegisters, 30040, 39, 1, "R", RegDataType.BitmaskU16, "", "Outputs bitmask (CH1..CH16)"))
            m.Add(New RegisterDef(ModbusTable.InputRegisters, 30041, 40, 1, "R", RegDataType.BitmaskU16, "", "Inputs bitmask (IN1..IN16)"))
            m.Add(New RegisterDef(ModbusTable.InputRegisters, 30042, 41, 1, "R", RegDataType.BitmaskU16, "", "Direct inputs bitmask (bit0..2)"))
            m.Add(New RegisterDef(ModbusTable.InputRegisters, 30043, 42, 1, "R", RegDataType.BitmaskU16, "", "System status flags (bit0=ETH,1=WiFi,2=AP,3=MasterEn,4=RestartReq,5=ModbusRun)"))

            ' ---- Holding Registers (4xxxx) ----
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40001, 0, 1, "R", RegDataType.U16, "", "Register map version"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40002, 1, 1, "R", RegDataType.U16, "", "Model ID"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40003, 2, 1, "R", RegDataType.U16, "", "Firmware version packed (MMmm)"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40004, 3, 1, "R", RegDataType.U16, "", "Hardware version packed (MMmm)"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40005, 4, 1, "R", RegDataType.U16, "", "Year of development"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40006, 5, 1, "R", RegDataType.BitmaskU16, "", "Capabilities bitmask"))

            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40011, 10, 16, "R/W", RegDataType.StringAscii, "", "Board Name (STRING(32))"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40027, 26, 16, "R/W", RegDataType.StringAscii, "", "Serial Number (STRING(32))"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40043, 42, 16, "R", RegDataType.StringAscii, "", "Manufacturer (STRING(32))"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40059, 58, 8, "R", RegDataType.StringAscii, "", "Firmware version string (STRING(16))"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40067, 66, 8, "R", RegDataType.StringAscii, "", "Hardware version string (STRING(16))"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40075, 74, 16, "R/W", RegDataType.StringAscii, "", "Device Name (STRING(32))"))

            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40101, 100, 6, "R", RegDataType.BytesU16, "", "EFUSE MAC (6 bytes)"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40107, 106, 6, "R/W", RegDataType.BytesU16, "", "Ethernet MAC config (6 bytes)"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40113, 112, 6, "R/W", RegDataType.BytesU16, "", "WiFi STA MAC config (6 bytes)"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40119, 118, 6, "R/W", RegDataType.BytesU16, "", "WiFi AP MAC config (6 bytes)"))

            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40201, 200, 1, "R/W", RegDataType.U16, "", "RS485 protocol (0=custom,1=ModbusRTU)"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40202, 201, 1, "R/W", RegDataType.U16, "", "Modbus Slave ID (1..247)"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40203, 202, 1, "R/W", RegDataType.U16, "", "Baud rate"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40204, 203, 1, "R/W", RegDataType.U16, "", "Data bits (7/8)"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40205, 204, 1, "R/W", RegDataType.U16, "", "Parity (0=N,1=O,2=E)"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40206, 205, 1, "R/W", RegDataType.U16, "", "Stop bits (1/2)"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40207, 206, 1, "R/W", RegDataType.U16, "ms", "Frame gap ms"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40208, 207, 1, "R/W", RegDataType.U16, "ms", "Internal Modbus timeout ms"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40209, 208, 1, "W", RegDataType.U16, "", "APPLY+SAVE serial settings: write 0xA55A"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40210, 209, 1, "R", RegDataType.U16, "", "Apply status (0=idle,1=ok,2=err)"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40211, 210, 1, "R/W", RegDataType.U16, "", "USB baud rate"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40212, 211, 1, "R/W", RegDataType.U16, "", "USB data bits"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40213, 212, 1, "R/W", RegDataType.U16, "", "USB parity"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40214, 213, 1, "R/W", RegDataType.U16, "", "USB stop bits"))

            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40301, 300, 1, "R/W", RegDataType.BitmaskU16, "", "Outputs bitmask write"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40302, 301, 1, "R/W", RegDataType.BitmaskU16, "", "Outputs write mask"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40303, 302, 1, "R", RegDataType.BitmaskU16, "", "Current outputs bitmask (mirror)"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40304, 303, 1, "R", RegDataType.BitmaskU16, "", "Current inputs bitmask (mirror)"))

            For ch As Integer = 0 To 3
                m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40311 + ch * 2, 310 + ch * 2, 2, "R/W", RegDataType.Float32, "", $"Analog scale factor CH{1 + ch} (FLOAT32)"))
                m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40319 + ch * 2, 318 + ch * 2, 2, "R/W", RegDataType.Float32, "", $"Analog offset CH{1 + ch} (FLOAT32)"))
            Next

            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40610, 609, 1, "W", RegDataType.U16, "", "CMD_ARM write 0xA55A"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40611, 610, 1, "W", RegDataType.U16, "", "CMD_CODE"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40612, 611, 1, "W", RegDataType.U16, "", "CMD_ARG0"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40613, 612, 1, "W", RegDataType.U16, "", "CMD_ARG1"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40614, 613, 1, "W", RegDataType.U16, "", "CMD_CONFIRM write 0x5AA5"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40615, 614, 1, "R", RegDataType.U16, "", "CMD_STATUS"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40616, 615, 1, "R", RegDataType.U16, "", "CMD_RESULT"))
            m.Add(New RegisterDef(ModbusTable.HoldingRegisters, 40617, 616, 1, "R", RegDataType.U16, "", "CMD_LAST_ERROR"))

            Return m
        End Function

        ' ---- Encoding/decoding helpers ----
        Public Function DecodeU32(words As UShort(), start As Integer) As UInteger
            Dim lo As UInteger = words(start)
            Dim hi As UInteger = words(start + 1)
            Return lo Or (hi << 16)
        End Function

        Public Function DecodeS16(word As UShort) As Short
            Return CType(word, Short)
        End Function

        Public Function DecodeFloat32(words As UShort(), start As Integer) As Single
            Dim lo As UInteger = words(start)
            Dim hi As UInteger = words(start + 1)
            Dim u As UInteger = lo Or (hi << 16)
            Return BitConverter.ToSingle(BitConverter.GetBytes(u), 0)
        End Function

        Public Function DecodeAscii(words As UShort(), start As Integer, countRegs As Integer) As String
            Dim bytes As New List(Of Byte)(countRegs * 2)
            For i As Integer = 0 To countRegs - 1
                Dim w As UShort = words(start + i)
                bytes.Add(CByte((w >> 8) And &HFFUS))
                bytes.Add(CByte(w And &HFFUS))
            Next
            Dim s As String = Encoding.ASCII.GetString(bytes.ToArray())
            Dim nul As Integer = s.IndexOf(ChrW(0))
            If nul >= 0 Then s = s.Substring(0, nul)
            Return s.Trim()
        End Function

        Public Function EncodeAscii(value As String, countRegs As Integer) As UShort()
            Dim maxChars As Integer = countRegs * 2
            Dim s As String = If(value, "").Trim()
            If s.Length > maxChars Then s = s.Substring(0, maxChars)
            Dim b() As Byte = Encoding.ASCII.GetBytes(s)
            Dim padded(maxChars - 1) As Byte
            Array.Clear(padded, 0, padded.Length)
            Array.Copy(b, padded, b.Length)
            Dim words(countRegs - 1) As UShort
            For i As Integer = 0 To countRegs - 1
                Dim c1 As UShort = padded(i * 2)
                Dim c2 As UShort = padded(i * 2 + 1)
                words(i) = CUShort((c1 << 8) Or c2)
            Next
            Return words
        End Function

        Public Function EncodeFloat32(value As Single) As UShort()
            Dim u As UInteger = BitConverter.ToUInt32(BitConverter.GetBytes(value), 0)
            Dim lo As UShort = CUShort(u And &HFFFFUI)
            Dim hi As UShort = CUShort((u >> 16) And &HFFFFUI)
            Return New UShort() {lo, hi}
        End Function
    End Module
End Namespace
