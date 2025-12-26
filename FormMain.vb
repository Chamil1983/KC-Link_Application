Imports System.Drawing
Imports System.Globalization
' NEW: Serial support
Imports System.IO.Ports
Imports System.Net.Sockets
Imports System.Text
Imports System.Windows.Forms
'Imports Modbus.Device
Imports NModbus
Imports NModbus.Serial



' ... keep your existing FormMain code ...

Partial Public Class FormMain

    ' -------------------------
    ' Shared baud table (INDEXED)
    ' Must match Arduino BAUD_TABLE[]
    ' -------------------------
    Private Shared ReadOnly BaudTable As Integer() = New Integer() {
        1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600
    }

    Private Shared Function BaudFromIndex(idx As Integer, fallback As Integer) As Integer
        If idx >= 0 AndAlso idx < BaudTable.Length Then Return BaudTable(idx)
        Return fallback
    End Function


    ' -------------------------
    ' Modbus constants (match Config.h + MODBUS_Test.ino)
    ' -------------------------
    Private Const MB_SLAVE_ID_DEFAULT As Integer = 1

    Private Const MB_REG_INPUTS_START As Integer = 0      ' Discrete Inputs + Input registers (DI state)
    Private Const MB_REG_RELAYS_START As Integer = 10     ' Coils + Holding (relay state)
    Private Const MB_REG_ANALOG_START As Integer = 20     ' Input regs (raw+scaled pairs)
    Private Const MB_REG_TEMP_START As Integer = 30       ' Input regs (temp*100)
    Private Const MB_REG_HUM_START As Integer = 40        ' Input regs (hum*100)
    Private Const MB_REG_DS18B20_START As Integer = 50    ' Input regs (temp encoding in test sketch)
    Private Const MB_REG_DAC_START As Integer = 70        ' Holding regs
    Private Const MB_REG_RTC_START As Integer = 80        ' Holding regs

    ' ===== NEW HOLDING REG BLOCKS =====
    Private Const MB_REG_SYSINFO_START As Integer = 100   ' 100..157
    Private Const MB_REG_NETINFO_START As Integer = 170   ' 170..219 + 202..203 in middle
    Private Const MB_REG_MBSET_START As Integer = 230     ' 230..234
    Private Const MB_REG_SERSET_START As Integer = 240    ' 240..243
    Private Const MB_REG_BUZZ_START As Integer = 250      ' 250..255
    ' ===== NEW: MAC info block =====
    Private Const MB_REG_MACINFO_START As Integer = 290   ' 290..316
    Private Const MB_MACINFO_REGS As Integer = 27

    Private Const NUM_DIGITAL_INPUTS As Integer = 8
    Private Const NUM_RELAY_OUTPUTS As Integer = 6
    Private Const NUM_ANALOG_CHANNELS As Integer = 2
    Private Const NUM_CURRENT_CHANNELS As Integer = 2
    Private Const NUM_DHT_SENSORS As Integer = 2
    Private Const MAX_DS18B20_SENSORS As Integer = 8


    ' ---------- New HR map constants ----------
    Private Const MB_REG_SYS_BOARDINFO_START As UShort = 100US
    Private Const MB_REG_SYS_BOARDINFO_END As UShort = 160US

    Private Const MB_REG_SYS_NETINFO_START As UShort = 170US
    Private Const MB_REG_SYS_NETINFO_END As UShort = 219US

    Private Const MB_REG_SYS_MBSET_START As UShort = 230US   ' 230..234
    Private Const MB_REG_SYS_SERSET_START As UShort = 240US  ' 240..243

    Private Const MB_REG_SYS_BUZZ_START As UShort = 250US    ' 250..255

    Private Const HR_ETH_MAC_START As UShort = 290US      ' 9 regs (17 chars)
    Private Const HR_WIFI_STA_MAC_START As UShort = 299US ' 9 regs
    Private Const HR_WIFI_AP_MAC_START As UShort = 308US  ' 9 regs

    ' BoardInfo layout (HR / FC03)
    Private Const HR_BOARD_NAME_START As UShort = 100US ' 100..115 (32 chars => 16 regs)
    Private Const HR_BOARD_SERIAL_START As UShort = 116US ' 116..131 (32 chars => 16 regs)
    Private Const HR_MANUFACTURER_START As UShort = 132US ' 132..139 (16 chars => 8 regs)
    Private Const HR_MAC_START As UShort = 140US ' 140..148 (17 chars => 9 regs)
    Private Const HR_FW_START As UShort = 149US ' 159..152 (8 chars => 4 regs)
    Private Const HR_HW_START As UShort = 153US ' 153..156 (8 chars => 4 regs)
    Private Const HR_YEAR As UShort = 157US ' uint16

    ' NetworkInfo layout (HR / FC03)
    Private Const HR_IP_START As UShort = 170US ' 16 chars => 8 regs
    Private Const HR_MASK_START As UShort = 178US
    Private Const HR_GW_START As UShort = 186US
    Private Const HR_DNS_START As UShort = 194US
    Private Const HR_DHCP As UShort = 202US ' 0/1
    Private Const HR_TCPPORT As UShort = 203US ' uint16
    Private Const HR_APSSID_START As UShort = 204US ' 16 chars => 8 regs
    Private Const HR_APPASS_START As UShort = 212US ' 16 chars => 8 regs

    ' Modbus settings block (HR / FC03) 230..234
    Private Const HR_MB_SLAVEID As UShort = 230US
    Private Const HR_MB_BAUD As UShort = 231US
    Private Const HR_MB_DATABITS As UShort = 232US
    Private Const HR_MB_PARITY As UShort = 233US
    Private Const HR_MB_STOPBITS As UShort = 234US

    ' Serial settings block (HR / FC03) 240..243
    Private Const HR_SER_BAUD As UShort = 240US
    Private Const HR_SER_DATABITS As UShort = 241US
    Private Const HR_SER_PARITY As UShort = 242US
    Private Const HR_SER_STOPBITS As UShort = 243US

    ' Buzzer control block (HR / FC03) 250..255
    Private Const HR_BUZ_FREQ As UShort = 250US
    Private Const HR_BUZ_MS As UShort = 251US
    Private Const HR_BUZ_PATTERN_ON As UShort = 252US
    Private Const HR_BUZ_PATTERN_OFF As UShort = 253US
    Private Const HR_BUZ_PATTERN_REP As UShort = 254US
    Private Const HR_BUZ_CMD As UShort = 255US

    ' Analog block size from MODBUS_Test.ino:
    ' (NUM_ANALOG_CHANNELS + NUM_CURRENT_CHANNELS) * 2 = 8 registers
    ' Layout (by MODBUS_Test.ino updateSensorData):
    '   V1: raw, scaled(V*100)
    '   V2: raw, scaled(V*100)
    '   I1: raw, scaled(mA*100)
    '   I2: raw, scaled(mA*100)
    Private Const MB_ANALOG_BLOCK_REGS As Integer = (NUM_ANALOG_CHANNELS + NUM_CURRENT_CHANNELS) * 2 ' 8

    ' -------------------------
    ' Modbus runtime state
    ' -------------------------
    Private _mbSerial As SerialPort = Nothing
    Private _mbIsConnected As Boolean = False
    Private _mbPollTimer As System.Windows.Forms.Timer = Nothing
    Private _mbPollBusy As Boolean = False

    Private _mbSlaveId As Integer = MB_SLAVE_ID_DEFAULT

    ' Cache for registers we read (address -> value)
    Private _mbCache As New Dictionary(Of Integer, UShort)()

    ' =========================
    ' MODBUS (NEW TAB)
    ' =========================
    Private mbSerial As SerialPort = Nothing
    Private mbMaster As IModbusSerialMaster = Nothing
    Private mbFactory As New ModbusFactory()

    Private mbPollTimer As Timer
    Private mbConnected As Boolean = False
    Private mbSlaveId As Byte = 1

    ' Prevent event feedback loops (when updating UI from polling)
    Private _mbUiUpdating As Boolean = False

    ' Register map row model
    Private Class MbRow
        Public Property Section As String
        Public Property Kind As String ' "DI", "COIL", "IR", "HR"
        Public Property Address As UShort
        Public Property Name As String
        Public Property Format As String
        Public Property ValueText As String
    End Class

    Private mbRows As New List(Of MbRow)()

    ' ===== NEW: Virtual grid live value storage (prevents UI freeze) =====
    Private _mbLiveValues As String() = Array.Empty(Of String)()
    Private _mbGridBuilt As Boolean = False

    ' Call this inside FormMain_Load after your existing init:
    Private Sub InitModbusUi()
        ' Fill defaults
        cboMbBaud.Items.Clear()
        cboMbBaud.Items.AddRange(New Object() {"9600", "19200", "38400", "57600", "115200"})
        cboMbBaud.SelectedItem = "9600"

        cboMbDataBits.Items.Clear()
        cboMbDataBits.Items.AddRange(New Object() {"8", "7"})
        cboMbDataBits.SelectedItem = "8"

        cboMbParity.Items.Clear()
        cboMbParity.Items.AddRange(New Object() {"None", "Even", "Odd"})
        cboMbParity.SelectedItem = "None"

        cboMbStopBits.Items.Clear()
        cboMbStopBits.Items.AddRange(New Object() {"One", "Two"})
        cboMbStopBits.SelectedItem = "One"

        txtMbSlaveId.Text = "1"
        txtMbPollMs.Text = "500"
        chkMbAutoPoll.Checked = True

        AddHandler btnMbRefreshPorts.Click, AddressOf BtnMbRefreshPorts_Click
        AddHandler btnMbConnect.Click, AddressOf BtnMbConnect_Click
        AddHandler btnMbDisconnect.Click, AddressOf BtnMbDisconnect_Click
        AddHandler btnMbReadOnce.Click, AddressOf BtnMbReadOnce_Click

        AddHandler chkMbRel1.CheckedChanged, AddressOf MbRelay_CheckedChanged
        AddHandler chkMbRel2.CheckedChanged, AddressOf MbRelay_CheckedChanged
        AddHandler chkMbRel3.CheckedChanged, AddressOf MbRelay_CheckedChanged
        AddHandler chkMbRel4.CheckedChanged, AddressOf MbRelay_CheckedChanged
        AddHandler chkMbRel5.CheckedChanged, AddressOf MbRelay_CheckedChanged
        AddHandler chkMbRel6.CheckedChanged, AddressOf MbRelay_CheckedChanged

        AddHandler btnMbApplyDac.Click, AddressOf BtnMbApplyDac_Click
        AddHandler chkMbAutoPoll.CheckedChanged, AddressOf ChkMbAutoPoll_CheckedChanged

        mbPollTimer = New Timer()
        mbPollTimer.Interval = 500
        AddHandler mbPollTimer.Tick, AddressOf MbPollTimer_Tick

        RefreshMbPortList()
        BuildModbusMapTableVirtual()
        UpdateMbUi(False, "Modbus: Disconnected")
    End Sub

    ' IMPORTANT: in your FormMain_Load add:
    ' InitModbusUi()

    Private Sub BtnMbRefreshPorts_Click(sender As Object, e As EventArgs)
        RefreshMbPortList()
    End Sub

    Private Sub RefreshMbPortList()
        Try
            Dim ports = SerialPort.GetPortNames().OrderBy(Function(p) p).ToArray()
            cboMbComPort.Items.Clear()
            cboMbComPort.Items.AddRange(ports)
            If ports.Length > 0 Then cboMbComPort.SelectedIndex = 0
        Catch ex As Exception
            LogLine("Modbus COM refresh error: " & ex.Message)
        End Try
    End Sub

    Private Sub BtnMbConnect_Click(sender As Object, e As EventArgs)
        MbConnect()
    End Sub

    Private Sub BtnMbDisconnect_Click(sender As Object, e As EventArgs)
        MbDisconnect()
    End Sub

    Private Sub BtnMbReadOnce_Click(sender As Object, e As EventArgs)
        MbPollOnce()
    End Sub

    Private Sub ChkMbAutoPoll_CheckedChanged(sender As Object, e As EventArgs)
        If Not mbConnected Then Return
        If chkMbAutoPoll.Checked Then
            mbPollTimer.Start()
        Else
            mbPollTimer.Stop()
        End If
    End Sub

    Private Sub MbConnect()
        If mbConnected Then Return

        Dim portName As String = If(cboMbComPort.SelectedItem, "").ToString()
        If String.IsNullOrWhiteSpace(portName) Then
            MessageBox.Show("Select a COM port.", "Modbus", MessageBoxButtons.OK, MessageBoxIcon.Warning)
            Return
        End If

        mbSlaveId = CByte(ToInt(txtMbSlaveId.Text.Trim(), 1))
        If mbSlaveId < 1 OrElse mbSlaveId > 247 Then mbSlaveId = 1

        Dim baud = ToInt(If(cboMbBaud.SelectedItem, "9600").ToString(), 9600)
        Dim dataBits = ToInt(If(cboMbDataBits.SelectedItem, "8").ToString(), 8)

        Dim parity As Parity = Parity.None
        Select Case If(cboMbParity.SelectedItem, "None").ToString().Trim().ToUpperInvariant()
            Case "EVEN" : parity = Parity.Even
            Case "ODD" : parity = Parity.Odd
            Case Else : parity = Parity.None
        End Select

        Dim stopBits As StopBits = StopBits.One
        Select Case If(cboMbStopBits.SelectedItem, "One").ToString().Trim().ToUpperInvariant()
            Case "TWO" : stopBits = StopBits.Two
            Case Else : stopBits = StopBits.One
        End Select

        Try
            ' IMPORTANT: do not allow Modbus if Settings-tab Serial is open
            If IsSerialOpen() Then
                MessageBox.Show("Close the Serial Port in Settings tab before using Modbus.", "Modbus",
                                MessageBoxButtons.OK, MessageBoxIcon.Warning)
                Return
            End If

            mbSerial = New SerialPort(portName, baud, parity, dataBits, stopBits)
            mbSerial.ReadTimeout = 1000
            mbSerial.WriteTimeout = 1000
            mbSerial.Open()

            Dim adapter = New SerialPortAdapter(mbSerial)
            mbMaster = mbFactory.CreateRtuMaster(adapter)

            mbMaster.Transport.Retries = 0
            mbMaster.Transport.ReadTimeout = 1000
            mbMaster.Transport.WaitToRetryMilliseconds = 50

            mbConnected = True
            UpdateMbUi(True, $"Modbus: Connected ({portName} {baud})")

            Dim pollMs = ToInt(txtMbPollMs.Text.Trim(), 500)
            If pollMs < 150 Then pollMs = 150
            mbPollTimer.Interval = pollMs
            If chkMbAutoPoll.Checked Then mbPollTimer.Start()

            MbPollOnce()

        Catch ex As Exception
            MbDisconnect()
            MessageBox.Show("Modbus connect failed: " & ex.Message, "Modbus", MessageBoxButtons.OK, MessageBoxIcon.Error)
        End Try
    End Sub

    Private Sub MbDisconnect()
        Try : mbPollTimer.Stop() : Catch : End Try
        Try : mbMaster = Nothing : Catch : End Try

        Try
            If mbSerial IsNot Nothing Then
                If mbSerial.IsOpen Then mbSerial.Close()
                mbSerial.Dispose()
            End If
        Catch
        End Try

        mbSerial = Nothing
        mbConnected = False
        UpdateMbUi(False, "Modbus: Disconnected")
    End Sub

    Private Sub UpdateMbUi(isOpen As Boolean, status As String)
        If lblMbStatus.InvokeRequired Then
            lblMbStatus.Invoke(Sub() UpdateMbUi(isOpen, status))
            Return
        End If

        lblMbStatus.Text = status
        lblMbStatus.ForeColor = If(isOpen, Color.DarkGreen, Color.DarkRed)

        btnMbConnect.Enabled = Not isOpen
        btnMbDisconnect.Enabled = isOpen
        btnMbReadOnce.Enabled = isOpen
        grpModbusIO.Enabled = isOpen
    End Sub

    Private Sub MbPollTimer_Tick(sender As Object, e As EventArgs)
        MbPollOnce()
    End Sub

    Private Sub MbPollOnce()
        If Not mbConnected OrElse mbMaster Is Nothing Then Return
        If _mbUiUpdating Then Return

        Try
            _mbUiUpdating = True

            ' Existing blocks
            Dim di = mbMaster.ReadInputs(mbSlaveId, MB_REG_INPUTS_START, NUM_DIGITAL_INPUTS)
            Dim coils = mbMaster.ReadCoils(mbSlaveId, MB_REG_RELAYS_START, NUM_RELAY_OUTPUTS)
            Dim ai = mbMaster.ReadInputRegisters(mbSlaveId, MB_REG_ANALOG_START, MB_ANALOG_BLOCK_REGS)
            Dim temps = mbMaster.ReadInputRegisters(mbSlaveId, MB_REG_TEMP_START, NUM_DHT_SENSORS)
            Dim hums = mbMaster.ReadInputRegisters(mbSlaveId, MB_REG_HUM_START, NUM_DHT_SENSORS)
            Dim ds = mbMaster.ReadInputRegisters(mbSlaveId, MB_REG_DS18B20_START, MAX_DS18B20_SENSORS)
            Dim dac = mbMaster.ReadHoldingRegisters(mbSlaveId, MB_REG_DAC_START, 4)
            Dim rtc = mbMaster.ReadHoldingRegisters(mbSlaveId, MB_REG_RTC_START, 8)

            ' NEW blocks (Holding)
            Dim sysInfo = mbMaster.ReadHoldingRegisters(mbSlaveId, MB_REG_SYSINFO_START, CUShort(58)) ' 100..158 inclusive = 58 regs
            Dim netInfo = mbMaster.ReadHoldingRegisters(mbSlaveId, MB_REG_NETINFO_START, CUShort(50)) ' 170..219 inclusive = 50 regs
            Dim mbSet = mbMaster.ReadHoldingRegisters(mbSlaveId, MB_REG_MBSET_START, CUShort(5))     ' 230..234
            Dim serSet = mbMaster.ReadHoldingRegisters(mbSlaveId, MB_REG_SERSET_START, CUShort(4))   ' 240..243
            Dim buz = mbMaster.ReadHoldingRegisters(mbSlaveId, MB_REG_BUZZ_START, CUShort(6))        ' 250..255

            ' ===== NEW: MAC info block 290..316 =====
            Dim macInfo = mbMaster.ReadHoldingRegisters(mbSlaveId, MB_REG_MACINFO_START, CUShort(MB_MACINFO_REGS))

            ApplyModbusToDashboard(di, coils, ai, temps, hums, ds, dac, rtc)

            ' NEW: update virtual grid values (fast)
            UpdateModbusMapLiveValuesVirtual(di, coils, ai, temps, hums, ds, dac, rtc, sysInfo, netInfo, mbSet, serSet, buz, macInfo)

        Catch ex As Exception
            LogLine("Modbus poll error: " & ex.Message)
            If chkMbAutoPoll.Checked Then MbDisconnect()
        Finally
            _mbUiUpdating = False
        End Try
    End Sub


    ' ==========================================================
    ' Virtualized Modbus Map Table (NO FREEZE)
    ' ==========================================================
    Private Sub BuildModbusMapTableVirtual()
        mbRows.Clear()

        ' --- Existing blocks ---
        For i As Integer = 0 To 7
            mbRows.Add(New MbRow With {.Section = "Hardware / Digital Inputs", .Kind = "DI", .Address = CUShort(MB_REG_INPUTS_START + i), .Name = $"DI{i + 1}", .Format = "0/1"})
        Next

        For i As Integer = 0 To 5
            mbRows.Add(New MbRow With {.Section = "Hardware / Relays (Coils)", .Kind = "COIL", .Address = CUShort(MB_REG_RELAYS_START + i), .Name = $"Relay{i + 1}", .Format = "ON/OFF"})
        Next

        mbRows.Add(New MbRow With {.Section = "Analog Inputs", .Kind = "IR", .Address = 20, .Name = "V1 Raw", .Format = "0..4095"})
        mbRows.Add(New MbRow With {.Section = "Analog Inputs", .Kind = "IR", .Address = 21, .Name = "V1 Scaled", .Format = "V*100"})
        mbRows.Add(New MbRow With {.Section = "Analog Inputs", .Kind = "IR", .Address = 22, .Name = "V2 Raw", .Format = "0..4095"})
        mbRows.Add(New MbRow With {.Section = "Analog Inputs", .Kind = "IR", .Address = 23, .Name = "V2 Scaled", .Format = "V*100"})
        mbRows.Add(New MbRow With {.Section = "Analog Inputs", .Kind = "IR", .Address = 24, .Name = "I1 Raw", .Format = "0..4095"})
        mbRows.Add(New MbRow With {.Section = "Analog Inputs", .Kind = "IR", .Address = 25, .Name = "I1 Scaled", .Format = "mA*100"})
        mbRows.Add(New MbRow With {.Section = "Analog Inputs", .Kind = "IR", .Address = 26, .Name = "I2 Raw", .Format = "0..4095"})
        mbRows.Add(New MbRow With {.Section = "Analog Inputs", .Kind = "IR", .Address = 27, .Name = "I2 Scaled", .Format = "mA*100"})

        mbRows.Add(New MbRow With {.Section = "DAC Outputs", .Kind = "HR", .Address = 70, .Name = "DAC0 Raw", .Format = "0..32767"})
        mbRows.Add(New MbRow With {.Section = "DAC Outputs", .Kind = "HR", .Address = 71, .Name = "DAC1 Raw", .Format = "0..32767"})
        mbRows.Add(New MbRow With {.Section = "DAC Outputs", .Kind = "HR", .Address = 72, .Name = "DAC0 V", .Format = "V*100"})
        mbRows.Add(New MbRow With {.Section = "DAC Outputs", .Kind = "HR", .Address = 73, .Name = "DAC1 V", .Format = "V*100"})

        mbRows.Add(New MbRow With {.Section = "Sensors (DHT)", .Kind = "IR", .Address = 30, .Name = "DHT1 Temp", .Format = "C*100"})
        mbRows.Add(New MbRow With {.Section = "Sensors (DHT)", .Kind = "IR", .Address = 31, .Name = "DHT2 Temp", .Format = "C*100"})
        mbRows.Add(New MbRow With {.Section = "Sensors (DHT)", .Kind = "IR", .Address = 40, .Name = "DHT1 Hum", .Format = "%*100"})
        mbRows.Add(New MbRow With {.Section = "Sensors (DHT)", .Kind = "IR", .Address = 41, .Name = "DHT2 Hum", .Format = "%*100"})

        For i As Integer = 0 To 7
            mbRows.Add(New MbRow With {.Section = "Sensors (DS18B20)", .Kind = "IR", .Address = CUShort(50 + i), .Name = $"DS18[{i}]", .Format = "(C+100)*100 (0=invalid)"})
        Next

        mbRows.Add(New MbRow With {.Section = "RTC", .Kind = "HR", .Address = 80, .Name = "Year", .Format = "YYYY"})
        mbRows.Add(New MbRow With {.Section = "RTC", .Kind = "HR", .Address = 81, .Name = "Month", .Format = "1..12"})
        mbRows.Add(New MbRow With {.Section = "RTC", .Kind = "HR", .Address = 82, .Name = "Day", .Format = "1..31"})
        mbRows.Add(New MbRow With {.Section = "RTC", .Kind = "HR", .Address = 83, .Name = "Hour", .Format = "0..23"})
        mbRows.Add(New MbRow With {.Section = "RTC", .Kind = "HR", .Address = 84, .Name = "Minute", .Format = "0..59"})
        mbRows.Add(New MbRow With {.Section = "RTC", .Kind = "HR", .Address = 85, .Name = "Second", .Format = "0..59 (write triggers RTC update)"})

        ' --- NEW blocks per request ---
        mbRows.Add(New MbRow With {.Section = "System / BoardInfo (HR packed ASCII)", .Kind = "HR", .Address = 100, .Name = "BOARD_NAME", .Format = "32 chars (100..115)"})
        mbRows.Add(New MbRow With {.Section = "System / BoardInfo (HR packed ASCII)", .Kind = "HR", .Address = 116, .Name = "BOARD_SERIAL", .Format = "32 chars (116..131)"})
        mbRows.Add(New MbRow With {.Section = "System / BoardInfo (HR packed ASCII)", .Kind = "HR", .Address = 132, .Name = "MANUFACTURER", .Format = "16 chars (132..139)"})
        mbRows.Add(New MbRow With {.Section = "System / BoardInfo (HR packed ASCII)", .Kind = "HR", .Address = 140, .Name = "MAC", .Format = "17 chars (140..148)"})
        mbRows.Add(New MbRow With {.Section = "System / BoardInfo (HR packed ASCII)", .Kind = "HR", .Address = 149, .Name = "FW_VERSION", .Format = "8 chars (149..152)"})
        mbRows.Add(New MbRow With {.Section = "System / BoardInfo (HR packed ASCII)", .Kind = "HR", .Address = 153, .Name = "HW_VERSION", .Format = "8 chars (153..156)"})
        mbRows.Add(New MbRow With {.Section = "System / BoardInfo", .Kind = "HR", .Address = 157, .Name = "MAKE_YEAR", .Format = "uint16"})

        mbRows.Add(New MbRow With {.Section = "System / NetworkInfo (HR packed ASCII)", .Kind = "HR", .Address = 170, .Name = "IP", .Format = "16 chars (170..177)"})
        mbRows.Add(New MbRow With {.Section = "System / NetworkInfo (HR packed ASCII)", .Kind = "HR", .Address = 178, .Name = "MASK", .Format = "16 chars (178..185)"})
        mbRows.Add(New MbRow With {.Section = "System / NetworkInfo (HR packed ASCII)", .Kind = "HR", .Address = 186, .Name = "GW", .Format = "16 chars (186..193)"})
        mbRows.Add(New MbRow With {.Section = "System / NetworkInfo (HR packed ASCII)", .Kind = "HR", .Address = 194, .Name = "DNS", .Format = "16 chars (194..201)"})
        mbRows.Add(New MbRow With {.Section = "System / NetworkInfo", .Kind = "HR", .Address = 202, .Name = "DHCP", .Format = "0/1"})
        mbRows.Add(New MbRow With {.Section = "System / NetworkInfo", .Kind = "HR", .Address = 203, .Name = "TCPPORT", .Format = "uint16"})
        mbRows.Add(New MbRow With {.Section = "System / NetworkInfo (HR packed ASCII)", .Kind = "HR", .Address = 204, .Name = "AP_SSID", .Format = "16 chars (204..211)"})
        mbRows.Add(New MbRow With {.Section = "System / NetworkInfo (HR packed ASCII)", .Kind = "HR", .Address = 212, .Name = "AP_PASS", .Format = "16 chars (212..219)"})

        mbRows.Add(New MbRow With {.Section = "System / Modbus Settings", .Kind = "HR", .Address = 230, .Name = "MB_SLAVEID", .Format = "1..247"})
        mbRows.Add(New MbRow With {.Section = "System / Modbus Settings", .Kind = "HR", .Address = 231, .Name = "MB_BAUD", .Format = "uint16"})
        mbRows.Add(New MbRow With {.Section = "System / Modbus Settings", .Kind = "HR", .Address = 232, .Name = "MB_DATABITS", .Format = "7/8"})
        mbRows.Add(New MbRow With {.Section = "System / Modbus Settings", .Kind = "HR", .Address = 233, .Name = "MB_PARITY", .Format = "0=None 1=Odd 2=Even"})
        mbRows.Add(New MbRow With {.Section = "System / Modbus Settings", .Kind = "HR", .Address = 234, .Name = "MB_STOPBITS", .Format = "1/2"})

        mbRows.Add(New MbRow With {.Section = "System / Serial Settings", .Kind = "HR", .Address = 240, .Name = "COM_SER_BAUD", .Format = "uint16"})
        mbRows.Add(New MbRow With {.Section = "System / Serial Settings", .Kind = "HR", .Address = 241, .Name = "COM_SER_DATABITS", .Format = "7/8"})
        mbRows.Add(New MbRow With {.Section = "System / Serial Settings", .Kind = "HR", .Address = 242, .Name = "COM_SER_PARITY", .Format = "0=None 1=Odd 2=Even"})
        mbRows.Add(New MbRow With {.Section = "System / Serial Settings", .Kind = "HR", .Address = 243, .Name = "COM_SER_STOPBITS", .Format = "1/2"})

        mbRows.Add(New MbRow With {.Section = "Buzzer Control", .Kind = "HR", .Address = 250, .Name = "BUZ_FREQ", .Format = "Hz"})
        mbRows.Add(New MbRow With {.Section = "Buzzer Control", .Kind = "HR", .Address = 251, .Name = "BUZ_MS", .Format = "ms (single beep)"})
        mbRows.Add(New MbRow With {.Section = "Buzzer Control", .Kind = "HR", .Address = 252, .Name = "BUZ_PATTERN_ON_MS", .Format = "ms"})
        mbRows.Add(New MbRow With {.Section = "Buzzer Control", .Kind = "HR", .Address = 253, .Name = "BUZ_PATTERN_OFF_MS", .Format = "ms"})
        mbRows.Add(New MbRow With {.Section = "Buzzer Control", .Kind = "HR", .Address = 254, .Name = "BUZ_PATTERN_REPEATS", .Format = "count"})
        mbRows.Add(New MbRow With {.Section = "Buzzer Control", .Kind = "HR", .Address = 255, .Name = "BUZ_CMD", .Format = "write 1=beep 2=pattern 0=stop"})

        ' ===== NEW: MAC info rows =====
        mbRows.Add(New MbRow With {.Section = "System / MAC Info (HR packed ASCII)", .Kind = "HR", .Address = 290, .Name = "ETH_MAC", .Format = "17 chars (290..298)"})
        mbRows.Add(New MbRow With {.Section = "System / MAC Info (HR packed ASCII)", .Kind = "HR", .Address = 299, .Name = "WIFI_STA_MAC", .Format = "17 chars (299..307)"})
        mbRows.Add(New MbRow With {.Section = "System / MAC Info (HR packed ASCII)", .Kind = "HR", .Address = 308, .Name = "WIFI_AP_MAC", .Format = "17 chars (308..316)"})

        ' live values array
        _mbLiveValues = New String(mbRows.Count - 1) {}

        ' Grid config (virtual)
        dgvModbusMap.SuspendLayout()
        Try
            dgvModbusMap.Columns.Clear()
            dgvModbusMap.AutoGenerateColumns = False
            dgvModbusMap.AllowUserToAddRows = False
            dgvModbusMap.AllowUserToDeleteRows = False
            dgvModbusMap.ReadOnly = True
            dgvModbusMap.RowHeadersVisible = False
            dgvModbusMap.AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.None ' IMPORTANT (prevents freeze)
            dgvModbusMap.VirtualMode = True
            dgvModbusMap.ScrollBars = ScrollBars.Both

            dgvModbusMap.Columns.Add(New DataGridViewTextBoxColumn() With {.Name = "colSection", .HeaderText = "Section", .Width = 200})
            dgvModbusMap.Columns.Add(New DataGridViewTextBoxColumn() With {.Name = "colKind", .HeaderText = "Type", .Width = 55})
            dgvModbusMap.Columns.Add(New DataGridViewTextBoxColumn() With {.Name = "colAddr", .HeaderText = "Addr", .Width = 55})
            dgvModbusMap.Columns.Add(New DataGridViewTextBoxColumn() With {.Name = "colName", .HeaderText = "Name", .Width = 170})
            dgvModbusMap.Columns.Add(New DataGridViewTextBoxColumn() With {.Name = "colFmt", .HeaderText = "Format", .Width = 220})
            dgvModbusMap.Columns.Add(New DataGridViewTextBoxColumn() With {.Name = "colVal", .HeaderText = "Live Value", .Width = 180})



            dgvModbusMap.RowCount = mbRows.Count

            RemoveHandler dgvModbusMap.CellValueNeeded, AddressOf DgvModbusMap_CellValueNeeded
            AddHandler dgvModbusMap.CellValueNeeded, AddressOf DgvModbusMap_CellValueNeeded

            _mbGridBuilt = True
        Finally
            dgvModbusMap.ResumeLayout()
        End Try
    End Sub

    Private Sub DataGridView1_RowPrePaint(
    sender As Object,
    e As DataGridViewRowPrePaintEventArgs
) Handles dgvModbusMap.RowPrePaint

        Dim rowIndex As Integer = e.RowIndex

        ' Section 1 (Rows 0–4)
        If rowIndex >= 0 AndAlso rowIndex <= 7 Then
            dgvModbusMap.Rows(rowIndex).DefaultCellStyle.BackColor = Color.FromArgb(174, 228, 249)

            ' Section 2 (Rows 5–9)
        ElseIf rowIndex >= 8 AndAlso rowIndex <= 13 Then
            dgvModbusMap.Rows(rowIndex).DefaultCellStyle.BackColor = Color.FromArgb(157, 255, 157)

            ' Section 3 (Rows 10–14)
        ElseIf rowIndex >= 14 AndAlso rowIndex <= 21 Then
            dgvModbusMap.Rows(rowIndex).DefaultCellStyle.BackColor = Color.LightYellow

        ElseIf rowIndex >= 22 AndAlso rowIndex <= 25 Then
            dgvModbusMap.Rows(rowIndex).DefaultCellStyle.BackColor = Color.FromArgb(247, 193, 157)

        ElseIf rowIndex >= 26 AndAlso rowIndex <= 29 Then
            dgvModbusMap.Rows(rowIndex).DefaultCellStyle.BackColor = Color.FromArgb(198, 198, 255)

        ElseIf rowIndex >= 30 AndAlso rowIndex <= 37 Then
            dgvModbusMap.Rows(rowIndex).DefaultCellStyle.BackColor = Color.FromArgb(181, 155, 106)

        ElseIf rowIndex >= 38 AndAlso rowIndex <= 43 Then
            dgvModbusMap.Rows(rowIndex).DefaultCellStyle.BackColor = Color.FromArgb(255, 145, 200)

        ElseIf rowIndex >= 44 AndAlso rowIndex <= 50 Then
            dgvModbusMap.Rows(rowIndex).DefaultCellStyle.BackColor = Color.FromArgb(116, 215, 252)

        ElseIf rowIndex >= 51 AndAlso rowIndex <= 58 Then
            dgvModbusMap.Rows(rowIndex).DefaultCellStyle.BackColor = Color.FromArgb(182, 148, 252)

        ElseIf rowIndex >= 59 AndAlso rowIndex <= 63 Then
            dgvModbusMap.Rows(rowIndex).DefaultCellStyle.BackColor = Color.FromArgb(132, 255, 193)

        ElseIf rowIndex >= 64 AndAlso rowIndex <= 67 Then
            dgvModbusMap.Rows(rowIndex).DefaultCellStyle.BackColor = Color.FromArgb(255, 255, 149)

        ElseIf rowIndex >= 68 AndAlso rowIndex <= 73 Then
            dgvModbusMap.Rows(rowIndex).DefaultCellStyle.BackColor = Color.FromArgb(85, 255, 85)

        ElseIf rowIndex >= 74 AndAlso rowIndex <= 76 Then
            dgvModbusMap.Rows(rowIndex).DefaultCellStyle.BackColor = Color.FromArgb(252, 159, 133)

        End If


    End Sub



    Private Sub DgvModbusMap_CellValueNeeded(sender As Object, e As DataGridViewCellValueEventArgs)
        If Not _mbGridBuilt Then Return
        If e.RowIndex < 0 OrElse e.RowIndex >= mbRows.Count Then Return
        Dim r = mbRows(e.RowIndex)

        Select Case e.ColumnIndex
            Case 0 : e.Value = r.Section
            Case 1 : e.Value = r.Kind
            Case 2 : e.Value = r.Address.ToString()
            Case 3 : e.Value = r.Name
            Case 4 : e.Value = r.Format
            Case 5
                If _mbLiveValues IsNot Nothing AndAlso e.RowIndex < _mbLiveValues.Length Then
                    e.Value = _mbLiveValues(e.RowIndex)
                Else
                    e.Value = ""
                End If
        End Select
    End Sub

    ' ===== NEW: fast, minimal UI updates =====
    Private Sub UpdateModbusMapLiveValuesVirtual(di() As Boolean,
                                                 coils() As Boolean,
                                                 ai() As UShort,
                                                 temps() As UShort,
                                                 hums() As UShort,
                                                 ds() As UShort,
                                                 dac() As UShort,
                                                 rtc() As UShort,
                                                 sysInfo() As UShort,
                                                 netInfo() As UShort,
                                                 mbSet() As UShort,
                                                 serSet() As UShort,
                                                 buz() As UShort,
                                                 macInfo() As UShort)

        If Not _mbGridBuilt Then Return

        For i As Integer = 0 To mbRows.Count - 1
            Dim row = mbRows(i)
            Dim val As String = ""

            Select Case row.Kind
                Case "DI"
                    Dim idx = CInt(row.Address) - MB_REG_INPUTS_START
                    If idx >= 0 AndAlso idx < di.Length Then val = If(di(idx), "1", "0")

                Case "COIL"
                    Dim idx = CInt(row.Address) - MB_REG_RELAYS_START
                    If idx >= 0 AndAlso idx < coils.Length Then val = If(coils(idx), "ON", "OFF")

                Case "IR"
                    Dim a = CInt(row.Address)
                    If a >= MB_REG_ANALOG_START AndAlso a < MB_REG_ANALOG_START + MB_ANALOG_BLOCK_REGS Then
                        Dim j = a - MB_REG_ANALOG_START
                        If (j Mod 2) = 0 Then val = ai(j).ToString()
                        val = (ai(j) / 100.0).ToString("0.00")
                    ElseIf a >= MB_REG_TEMP_START AndAlso a < MB_REG_TEMP_START + NUM_DHT_SENSORS Then
                        val = (temps(a - MB_REG_TEMP_START) / 100.0).ToString("0.00")
                    ElseIf a >= MB_REG_HUM_START AndAlso a < MB_REG_HUM_START + NUM_DHT_SENSORS Then
                        val = (hums(a - MB_REG_HUM_START) / 100.0).ToString("0.00")
                    ElseIf a >= MB_REG_DS18B20_START AndAlso a < MB_REG_DS18B20_START + MAX_DS18B20_SENSORS Then
                        Dim j = a - MB_REG_DS18B20_START
                        If ds(j) = 0 Then val = "invalid" Else val = ((ds(j) / 100.0) - 100.0).ToString("0.00")
                    End If

                Case "HR"
                    Dim a = CInt(row.Address)

                    ' Existing DAC/RTC
                    If a >= MB_REG_DAC_START AndAlso a < MB_REG_DAC_START + 4 Then
                        Dim j = a - MB_REG_DAC_START
                        If j <= 1 Then val = dac(j).ToString() Else val = (dac(j) / 100.0).ToString("0.00")
                    ElseIf a >= MB_REG_RTC_START AndAlso a < MB_REG_RTC_START + 6 Then
                        val = rtc(a - MB_REG_RTC_START).ToString()

                        ' NEW: BoardInfo / NetworkInfo / settings / buzzer
                    ElseIf a >= MB_REG_SYSINFO_START AndAlso a <= 157 Then
                        val = DecodeSystemInfoField(a, sysInfo)

                    ElseIf a >= MB_REG_NETINFO_START AndAlso a <= 219 Then
                        val = DecodeNetworkInfoField(a, netInfo)

                    ElseIf a >= MB_REG_MBSET_START AndAlso a <= 234 Then
                        val = DecodeSimpleHrField(a, mbSet, MB_REG_MBSET_START)

                    ElseIf a >= MB_REG_SERSET_START AndAlso a <= 243 Then
                        val = DecodeSimpleHrField(a, serSet, MB_REG_SERSET_START)

                    ElseIf a >= MB_REG_BUZZ_START AndAlso a <= 255 Then
                        val = DecodeSimpleHrField(a, buz, MB_REG_BUZZ_START)
                    ElseIf a >= MB_REG_MACINFO_START AndAlso a <= 316 Then
                        val = DecodeMacInfoField(a, macInfo)
                    End If

            End Select

            _mbLiveValues(i) = val
        Next

        ' Just repaint visible area; no row/cell modifications
        dgvModbusMap.Invalidate()
    End Sub


    Private Function DecodeSimpleHrField(addr As Integer, regs() As UShort, baseAddr As Integer) As String
        Dim idx = addr - baseAddr
        If idx < 0 OrElse idx >= regs.Length Then Return ""

        ' SPECIAL: baud registers are INDEX, not raw baud
        If baseAddr = MB_REG_MBSET_START AndAlso addr = 231 Then
            Dim baudIdx As Integer = regs(idx)
            Return $"{baudIdx} => {BaudFromIndex(baudIdx, 9600)}"
        End If

        If baseAddr = MB_REG_SERSET_START AndAlso addr = 240 Then
            Dim baudIdx As Integer = regs(idx)
            Return $"{baudIdx} => {BaudFromIndex(baudIdx, 115200)}"
        End If

        Return regs(idx).ToString()
    End Function

    Private Function DecodeMacInfoField(addr As Integer, macInfo() As UShort) As String
        If macInfo Is Nothing OrElse macInfo.Length < MB_MACINFO_REGS Then Return ""

        If addr >= 290 AndAlso addr <= 298 Then
            Return DecodeAsciiBlock(macInfo, 0, 9) ' ETH_MAC
        ElseIf addr >= 299 AndAlso addr <= 307 Then
            Return DecodeAsciiBlock(macInfo, 9, 9) ' WIFI_STA_MAC
        ElseIf addr >= 308 AndAlso addr <= 316 Then
            Return DecodeAsciiBlock(macInfo, 18, 9) ' WIFI_AP_MAC
        End If

        Return ""
    End Function

    ' Packed ASCII: 2 chars per 16-bit register (HiByte then LoByte)
    Private Function DecodeAsciiBlock(regs() As UShort, regOffset As Integer, regCount As Integer) As String
        Dim bytes As New List(Of Byte)(regCount * 2)
        For i As Integer = 0 To regCount - 1
            Dim w As UShort = regs(regOffset + i)
            bytes.Add(CByte((w >> 8) And &HFFUS))
            bytes.Add(CByte(w And &HFFUS))
        Next
        Dim s = Encoding.ASCII.GetString(bytes.ToArray())
        s = s.TrimEnd(ChrW(0), " "c)
        Return s
    End Function

    Private Function DecodeSystemInfoField(addr As Integer, sysInfo() As UShort) As String
        If sysInfo Is Nothing OrElse sysInfo.Length < 58 Then Return ""

        If addr >= 100 AndAlso addr <= 115 Then
            Return DecodeAsciiBlock(sysInfo, 0, 16)
        ElseIf addr >= 116 AndAlso addr <= 131 Then
            Return DecodeAsciiBlock(sysInfo, 16, 16)
        ElseIf addr >= 132 AndAlso addr <= 139 Then
            ' manufacturer is 16 chars max; firmware enforces this now
            Return DecodeAsciiBlock(sysInfo, 32, 8)
        ElseIf addr >= 140 AndAlso addr <= 148 Then
            Return DecodeAsciiBlock(sysInfo, 40, 9)
        ElseIf addr >= 149 AndAlso addr <= 152 Then
            Return DecodeAsciiBlock(sysInfo, 49, 4)
        ElseIf addr >= 153 AndAlso addr <= 156 Then
            Return DecodeAsciiBlock(sysInfo, 53, 4)
        ElseIf addr = 157 Then
            Return sysInfo(57).ToString()
        End If

        Return ""
    End Function

    Private Function DecodeNetworkInfoField(addr As Integer, netInfo() As UShort) As String
        ' netInfo corresponds to 170..219 (50 regs)
        Dim offset = addr - MB_REG_NETINFO_START
        If offset < 0 OrElse offset >= netInfo.Length Then Return ""

        If addr >= 170 AndAlso addr <= 177 Then
            Return DecodeAsciiBlock(netInfo, 0, 8)
        ElseIf addr >= 178 AndAlso addr <= 185 Then
            Return DecodeAsciiBlock(netInfo, 8, 8)
        ElseIf addr >= 186 AndAlso addr <= 193 Then
            Return DecodeAsciiBlock(netInfo, 16, 8)
        ElseIf addr >= 194 AndAlso addr <= 201 Then
            Return DecodeAsciiBlock(netInfo, 24, 8)
        ElseIf addr = 202 Then
            Return netInfo(32).ToString()
        ElseIf addr = 203 Then
            Return netInfo(33).ToString()
        ElseIf addr >= 204 AndAlso addr <= 211 Then
            Return DecodeAsciiBlock(netInfo, 34, 8)
        ElseIf addr >= 212 AndAlso addr <= 219 Then
            Return DecodeAsciiBlock(netInfo, 42, 8)
        End If

        Return ""
    End Function

    Private Sub ApplyModbusToDashboard(di() As Boolean,
                                      coils() As Boolean,
                                      ai() As UShort,
                                      temps() As UShort,
                                      hums() As UShort,
                                      ds() As UShort,
                                      dac() As UShort,
                                      rtc() As UShort)

        ' Update Modbus tab relay checkboxes (coil true=ON)
        chkMbRel1.Checked = coils(0)
        chkMbRel2.Checked = coils(1)
        chkMbRel3.Checked = coils(2)
        chkMbRel4.Checked = coils(3)
        chkMbRel5.Checked = coils(4)
        chkMbRel6.Checked = coils(5)

        ' Also sync your Dashboard relays if connected via TCP (optional):
        ' (NOT forcing TCP here; Modbus tab is independent.)

        ' Digital Inputs -> reflect on Dashboard DI indicators too
        chkDI1.Checked = di(0)
        chkDI2.Checked = di(1)
        chkDI3.Checked = di(2)
        chkDI4.Checked = di(3)
        chkDI5.Checked = di(4)
        chkDI6.Checked = di(5)
        chkDI7.Checked = di(6)
        chkDI8.Checked = di(7)

        ' Analog mapping
        ' ai regs:
        ' 20 raw V1, 21 V1*100
        ' 22 raw V2, 23 V2*100
        ' 24 raw I1, 25 I1*100
        ' 26 raw I2, 27 I2*100
        Dim v1 = ai(1) / 100.0
        Dim v2 = ai(3) / 100.0
        Dim i1 = ai(5) / 100.0
        Dim i2 = ai(7) / 100.0

        txtV1.Text = v1.ToString("0.000")
        txtV2.Text = v2.ToString("0.000")
        txtI1.Text = i1.ToString("0.000")
        txtI2.Text = i2.ToString("0.000")

        ' DAC: holding regs [0]=raw0, [1]=raw1, [2]=V*100 ch0, [3]=V*100 ch1
        txtMbDac1mV.Text = CInt(dac(2) * 10).ToString() ' V*100 => mV ~ *10
        txtMbDac2mV.Text = CInt(dac(3) * 10).ToString()
        txtDAC1.Text = txtMbDac1mV.Text
        txtDAC2.Text = txtMbDac2mV.Text

        ' Temps/Humidity
        Dim t1 = temps(0) / 100.0
        Dim t2 = temps(1) / 100.0
        Dim h1 = hums(0) / 100.0
        Dim h2 = hums(1) / 100.0
        txtT1.Text = t1.ToString("0.0")
        txtT2.Text = t2.ToString("0.0")
        txtH1.Text = h1.ToString("0.0")
        txtH2.Text = h2.ToString("0.0")

        ' DS18B20 decode: (t + 100) * 100
        txtDsCount.Text = ds.Count(Function(x) x <> 0).ToString()
        If ds(0) <> 0 Then
            Dim ds0 = (ds(0) / 100.0) - 100.0
            txtDs0.Text = ds0.ToString("0.0")
        Else
            txtDs0.Text = "NaN"
        End If

        ' RTC
        If rtc.Length >= 6 Then
            Dim y = rtc(0)
            Dim mo = rtc(1)
            Dim d = rtc(2)
            Dim hh = rtc(3)
            Dim mm = rtc(4)
            Dim ss = rtc(5)
            txtRtcCurrent.Text = $"{y:0000}-{mo:00}-{d:00} {hh:00}:{mm:00}:{ss:00}"
        End If
    End Sub

    Private Sub MbRelay_CheckedChanged(sender As Object, e As EventArgs)
        If Not mbConnected OrElse mbMaster Is Nothing Then Return
        If _mbUiUpdating Then Return

        Dim chk = CType(sender, CheckBox)
        Dim relayIdx As Integer = 0
        If chk Is chkMbRel1 Then relayIdx = 0
        If chk Is chkMbRel2 Then relayIdx = 1
        If chk Is chkMbRel3 Then relayIdx = 2
        If chk Is chkMbRel4 Then relayIdx = 3
        If chk Is chkMbRel5 Then relayIdx = 4
        If chk Is chkMbRel6 Then relayIdx = 5

        Dim coilAddr As UShort = CUShort(MB_REG_RELAYS_START + relayIdx)
        Try
            mbMaster.WriteSingleCoil(mbSlaveId, coilAddr, chk.Checked)
        Catch ex As Exception
            LogLine("Modbus relay write error: " & ex.Message)
        End Try
    End Sub

    Private Sub BtnMbApplyDac_Click(sender As Object, e As EventArgs)
        If Not mbConnected OrElse mbMaster Is Nothing Then Return
        Dim mv1 = ToInt(txtMbDac1mV.Text.Trim(), 0)
        Dim mv2 = ToInt(txtMbDac2mV.Text.Trim(), 0)

        ' Firmware DAC regs 72/73 store V*100.
        ' Convert mV -> V*100: (mV/1000)*100 = mV/10
        Dim v100_1 As UShort = CUShort(Math.Max(0, Math.Min(65535, mv1 \ 10)))
        Dim v100_2 As UShort = CUShort(Math.Max(0, Math.Min(65535, mv2 \ 10)))

        Try
            mbMaster.WriteSingleRegister(mbSlaveId, CUShort(MB_REG_DAC_START + 2), v100_1)
            mbMaster.WriteSingleRegister(mbSlaveId, CUShort(MB_REG_DAC_START + 3), v100_2)
        Catch ex As Exception
            LogLine("Modbus DAC write error: " & ex.Message)
        End Try
    End Sub

    ' ==========================================================
    ' Register Map Table (sections + live values)
    ' ==========================================================
    Private Sub BuildModbusMapTable()
        mbRows.Clear()

        ' Board Information / Network info are not exposed via Modbus in current firmware map.
        ' (Those are available via TCP commands BOARDINFO/NETCFG.)
        ' Here we document what Modbus DOES expose, per MODBUS_Test and your Config.h.

        mbRows.Add(New MbRow With {.Section = "Hardware / Digital Inputs", .Kind = "DI", .Address = 0, .Name = "DI1..DI8", .Format = "Discrete Inputs FC02 (0..7)", .ValueText = ""})

        For i As Integer = 0 To 7
            mbRows.Add(New MbRow With {.Section = "Hardware / Digital Inputs", .Kind = "DI", .Address = CUShort(MB_REG_INPUTS_START + i), .Name = $"DI{i + 1}", .Format = "0=LOW,1=HIGH", .ValueText = ""})
        Next

        For i As Integer = 0 To 5
            mbRows.Add(New MbRow With {.Section = "Hardware / Relays (Coils)", .Kind = "COIL", .Address = CUShort(MB_REG_RELAYS_START + i), .Name = $"Relay{i + 1}", .Format = "Coil FC01/FC05", .ValueText = ""})
        Next

        ' Analog block
        mbRows.Add(New MbRow With {.Section = "Analog Inputs", .Kind = "IR", .Address = 20, .Name = "V1 Raw", .Format = "0..4095", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "Analog Inputs", .Kind = "IR", .Address = 21, .Name = "V1 Scaled", .Format = "V*100", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "Analog Inputs", .Kind = "IR", .Address = 22, .Name = "V2 Raw", .Format = "0..4095", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "Analog Inputs", .Kind = "IR", .Address = 23, .Name = "V2 Scaled", .Format = "V*100", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "Analog Inputs", .Kind = "IR", .Address = 24, .Name = "I1 Raw", .Format = "0..4095", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "Analog Inputs", .Kind = "IR", .Address = 25, .Name = "I1 Scaled", .Format = "mA*100", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "Analog Inputs", .Kind = "IR", .Address = 26, .Name = "I2 Raw", .Format = "0..4095", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "Analog Inputs", .Kind = "IR", .Address = 27, .Name = "I2 Scaled", .Format = "mA*100", .ValueText = ""})

        ' DAC
        mbRows.Add(New MbRow With {.Section = "DAC Outputs", .Kind = "HR", .Address = 70, .Name = "DAC0 Raw", .Format = "0..32767", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "DAC Outputs", .Kind = "HR", .Address = 71, .Name = "DAC1 Raw", .Format = "0..32767", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "DAC Outputs", .Kind = "HR", .Address = 72, .Name = "DAC0 V", .Format = "V*100", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "DAC Outputs", .Kind = "HR", .Address = 73, .Name = "DAC1 V", .Format = "V*100", .ValueText = ""})

        ' Temps/Humidity
        mbRows.Add(New MbRow With {.Section = "Sensors (DHT)", .Kind = "IR", .Address = 30, .Name = "DHT1 Temp", .Format = "C*100", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "Sensors (DHT)", .Kind = "IR", .Address = 31, .Name = "DHT2 Temp", .Format = "C*100", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "Sensors (DHT)", .Kind = "IR", .Address = 40, .Name = "DHT1 Hum", .Format = "%*100", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "Sensors (DHT)", .Kind = "IR", .Address = 41, .Name = "DHT2 Hum", .Format = "%*100", .ValueText = ""})

        ' DS18B20
        For i As Integer = 0 To 7
            mbRows.Add(New MbRow With {.Section = "Sensors (DS18B20)", .Kind = "IR", .Address = CUShort(50 + i), .Name = $"DS18[{i}]", .Format = "(C+100)*100 (0=invalid)", .ValueText = ""})
        Next

        ' RTC
        mbRows.Add(New MbRow With {.Section = "RTC", .Kind = "HR", .Address = 80, .Name = "Year", .Format = "YYYY", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "RTC", .Kind = "HR", .Address = 81, .Name = "Month", .Format = "1..12", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "RTC", .Kind = "HR", .Address = 82, .Name = "Day", .Format = "1..31", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "RTC", .Kind = "HR", .Address = 83, .Name = "Hour", .Format = "0..23", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "RTC", .Kind = "HR", .Address = 84, .Name = "Minute", .Format = "0..59", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "RTC", .Kind = "HR", .Address = 85, .Name = "Second", .Format = "0..59 (write triggers RTC update)", .ValueText = ""})

        BuildModbusMapTable_AddSystemBlocks()


        dgvModbusMap.Columns.Clear()
        dgvModbusMap.Columns.Add("colSection", "Section")
        dgvModbusMap.Columns.Add("colKind", "Type")
        dgvModbusMap.Columns.Add("colAddr", "Addr")
        dgvModbusMap.Columns.Add("colName", "Name")
        dgvModbusMap.Columns.Add("colFmt", "Format")
        dgvModbusMap.Columns.Add("colVal", "Live Value")

        dgvModbusMap.Rows.Clear()
        For Each r In mbRows
            dgvModbusMap.Rows.Add(r.Section, r.Kind, r.Address.ToString(), r.Name, r.Format, r.ValueText)
        Next
    End Sub

    Private Sub UpdateModbusMapLiveValues(di() As Boolean,
                                         coils() As Boolean,
                                         ai() As UShort,
                                         temps() As UShort,
                                         hums() As UShort,
                                         ds() As UShort,
                                         dac() As UShort,
                                         rtc() As UShort)
        ' Rebuild “Live Value” by scanning rows and mapping
        For i As Integer = 0 To mbRows.Count - 1
            Dim row = mbRows(i)
            Dim val As String = ""

            Select Case row.Kind
                Case "DI"
                    If row.Address >= MB_REG_INPUTS_START AndAlso row.Address < MB_REG_INPUTS_START + NUM_DIGITAL_INPUTS Then
                        Dim idx = row.Address - MB_REG_INPUTS_START
                        val = If(di(idx), "1", "0")
                    End If
                Case "COIL"
                    If row.Address >= MB_REG_RELAYS_START AndAlso row.Address < MB_REG_RELAYS_START + NUM_RELAY_OUTPUTS Then
                        Dim idx = row.Address - MB_REG_RELAYS_START
                        val = If(coils(idx), "ON", "OFF")
                    End If
                Case "IR"
                    If row.Address >= MB_REG_ANALOG_START AndAlso row.Address < MB_REG_ANALOG_START + 8 Then
                        Dim idx = row.Address - MB_REG_ANALOG_START
                        Dim raw = ai(idx)
                        If (idx Mod 2) = 0 Then
                            val = raw.ToString()
                        Else
                            val = (raw / 100.0).ToString("0.00")
                        End If
                    ElseIf row.Address >= MB_REG_TEMP_START AndAlso row.Address < MB_REG_TEMP_START + NUM_DHT_SENSORS Then
                        Dim idx = row.Address - MB_REG_TEMP_START
                        val = (temps(idx) / 100.0).ToString("0.00")
                    ElseIf row.Address >= MB_REG_HUM_START AndAlso row.Address < MB_REG_HUM_START + NUM_DHT_SENSORS Then
                        Dim idx = row.Address - MB_REG_HUM_START
                        val = (hums(idx) / 100.0).ToString("0.00")
                    ElseIf row.Address >= MB_REG_DS18B20_START AndAlso row.Address < MB_REG_DS18B20_START + MAX_DS18B20_SENSORS Then
                        Dim idx = row.Address - MB_REG_DS18B20_START
                        If ds(idx) = 0 Then val = "invalid" Else val = ((ds(idx) / 100.0) - 100.0).ToString("0.00")
                    End If
                Case "HR"
                    If row.Address >= MB_REG_DAC_START AndAlso row.Address < MB_REG_DAC_START + 4 Then
                        Dim idx = row.Address - MB_REG_DAC_START
                        If idx <= 1 Then val = dac(idx).ToString() Else val = (dac(idx) / 100.0).ToString("0.00")
                    ElseIf row.Address >= MB_REG_RTC_START AndAlso row.Address < MB_REG_RTC_START + 6 Then
                        Dim idx = row.Address - MB_REG_RTC_START
                        val = rtc(idx).ToString()
                    End If
            End Select

            dgvModbusMap.Rows(i).Cells(5).Value = val
        Next
    End Sub

    ' ==========================================================
    ' ASCII packed string helpers (2 chars per register)
    ' HiByte then LoByte. Null (0) terminates; remaining padded.
    ' ==========================================================

    Private Function DecodeAsciiFromHoldingRegs(regs As UShort(), startIndex As Integer, regCount As Integer, Optional maxChars As Integer = -1) As String
        If regs Is Nothing Then Return ""
        If startIndex < 0 OrElse startIndex >= regs.Length Then Return ""
        Dim endIdx = Math.Min(regs.Length, startIndex + regCount)

        Dim bytes As New List(Of Byte)(regCount * 2)
        For i = startIndex To endIdx - 1
            Dim w As UShort = regs(i)
            Dim hi As Byte = CByte((w >> 8) And &HFFUS)
            Dim lo As Byte = CByte(w And &HFFUS)
            bytes.Add(hi)
            bytes.Add(lo)
        Next

        ' Trim at first NULL
        Dim cut = bytes.IndexOf(0)
        If cut >= 0 Then bytes = bytes.Take(cut).ToList()

        If maxChars > 0 AndAlso bytes.Count > maxChars Then
            bytes = bytes.Take(maxChars).ToList()
        End If

        Return Encoding.ASCII.GetString(bytes.ToArray()).Trim()
    End Function

    Private Function EncodeAsciiToHoldingRegs(value As String, regCount As Integer) As UShort()
        If value Is Nothing Then value = ""
        Dim bytes = Encoding.ASCII.GetBytes(value)

        ' We will produce exactly regCount registers => regCount*2 bytes
        Dim outBytes(regCount * 2 - 1) As Byte
        Array.Clear(outBytes, 0, outBytes.Length)

        Dim n = Math.Min(bytes.Length, outBytes.Length)
        Array.Copy(bytes, 0, outBytes, 0, n)

        Dim regs(regCount - 1) As UShort
        For i = 0 To regCount - 1
            Dim hi As UShort = outBytes(i * 2)
            Dim lo As UShort = outBytes(i * 2 + 1)
            regs(i) = CUShort((hi << 8) Or lo)
        Next
        Return regs
    End Function

    ' Utility: convert an absolute Modbus HR address into index inside a block read
    Private Function HrIndex(baseAddr As UShort, addr As UShort) As Integer
        Return CInt(addr - baseAddr)
    End Function

    ' ==========================================================
    ' Polling additions: read new HR blocks during MbPollOnce()
    ' ==========================================================

    ' Call this inside MbPollOnce() AFTER you confirmed mbConnected and mbMaster not Nothing.
    ' It reads FC03 blocks and updates the map table live values.
    Private Sub MbPollSystemBlocks()
        If Not mbConnected OrElse mbMaster Is Nothing Then Return

        ' Read BoardInfo: 100..157 => 58 registers
        Dim boardRegs = mbMaster.ReadHoldingRegisters(mbSlaveId, MB_REG_SYS_BOARDINFO_START, CUShort(MB_REG_SYS_BOARDINFO_END - MB_REG_SYS_BOARDINFO_START + 1))

        ' Read NetworkInfo: 170..219 => 50 registers
        Dim netRegs = mbMaster.ReadHoldingRegisters(mbSlaveId, MB_REG_SYS_NETINFO_START, CUShort(MB_REG_SYS_NETINFO_END - MB_REG_SYS_NETINFO_START + 1))

        ' Read Modbus settings: 230..234 => 5 registers
        Dim mbSetRegs = mbMaster.ReadHoldingRegisters(mbSlaveId, MB_REG_SYS_MBSET_START, 5)

        ' Read Serial settings: 240..243 => 4 registers
        Dim serSetRegs = mbMaster.ReadHoldingRegisters(mbSlaveId, MB_REG_SYS_SERSET_START, 4)

        ' Read Buzzer control: 250..255 => 6 registers
        Dim buzRegs = mbMaster.ReadHoldingRegisters(mbSlaveId, MB_REG_SYS_BUZZ_START, 6)

        ' Update Map grid live values (and optionally any UI labels/textboxes you add later)
        UpdateModbusMapSystemLiveValues(boardRegs, netRegs, mbSetRegs, serSetRegs, buzRegs)
    End Sub

    ' ==========================================================
    ' Update the Modbus Map table with decoded system blocks
    ' ==========================================================
    Private Sub UpdateModbusMapSystemLiveValues(boardRegs() As UShort,
                                           netRegs() As UShort,
                                           mbSetRegs() As UShort,
                                           serSetRegs() As UShort,
                                           buzRegs() As UShort)

        ' We only update dgvModbusMap live column based on the row.Address
        ' This matches your existing UpdateModbusMapLiveValues style.

        For i As Integer = 0 To mbRows.Count - 1
            Dim row = mbRows(i)
            If row Is Nothing Then Continue For
            Dim val As String = Nothing

            If row.Kind = "HR" Then
                Select Case row.Address

                ' -------- BoardInfo strings --------
                    Case HR_BOARD_NAME_START
                        val = DecodeAsciiFromHoldingRegs(boardRegs, HrIndex(MB_REG_SYS_BOARDINFO_START, HR_BOARD_NAME_START), 16, 32)
                    Case HR_BOARD_SERIAL_START
                        val = DecodeAsciiFromHoldingRegs(boardRegs, HrIndex(MB_REG_SYS_BOARDINFO_START, HR_BOARD_SERIAL_START), 16, 32)
                    Case HR_MANUFACTURER_START
                        val = DecodeAsciiFromHoldingRegs(boardRegs, HrIndex(MB_REG_SYS_BOARDINFO_START, HR_MANUFACTURER_START), 8, 16)
                    Case HR_MAC_START
                        val = DecodeAsciiFromHoldingRegs(boardRegs, HrIndex(MB_REG_SYS_BOARDINFO_START, HR_MAC_START), 9, 17)
                    Case HR_FW_START
                        val = DecodeAsciiFromHoldingRegs(boardRegs, HrIndex(MB_REG_SYS_BOARDINFO_START, HR_FW_START), 4, 8)
                    Case HR_HW_START
                        val = DecodeAsciiFromHoldingRegs(boardRegs, HrIndex(MB_REG_SYS_BOARDINFO_START, HR_HW_START), 4, 8)
                    Case HR_YEAR
                        val = boardRegs(HrIndex(MB_REG_SYS_BOARDINFO_START, HR_YEAR)).ToString()

                ' -------- NetworkInfo strings --------
                    Case HR_IP_START
                        val = DecodeAsciiFromHoldingRegs(netRegs, HrIndex(MB_REG_SYS_NETINFO_START, HR_IP_START), 8, 16)
                    Case HR_MASK_START
                        val = DecodeAsciiFromHoldingRegs(netRegs, HrIndex(MB_REG_SYS_NETINFO_START, HR_MASK_START), 8, 16)
                    Case HR_GW_START
                        val = DecodeAsciiFromHoldingRegs(netRegs, HrIndex(MB_REG_SYS_NETINFO_START, HR_GW_START), 8, 16)
                    Case HR_DNS_START
                        val = DecodeAsciiFromHoldingRegs(netRegs, HrIndex(MB_REG_SYS_NETINFO_START, HR_DNS_START), 8, 16)
                    Case HR_DHCP
                        val = netRegs(HrIndex(MB_REG_SYS_NETINFO_START, HR_DHCP)).ToString()
                    Case HR_TCPPORT
                        val = netRegs(HrIndex(MB_REG_SYS_NETINFO_START, HR_TCPPORT)).ToString()
                    Case HR_APSSID_START
                        val = DecodeAsciiFromHoldingRegs(netRegs, HrIndex(MB_REG_SYS_NETINFO_START, HR_APSSID_START), 8, 16)
                    Case HR_APPASS_START
                        val = DecodeAsciiFromHoldingRegs(netRegs, HrIndex(MB_REG_SYS_NETINFO_START, HR_APPASS_START), 8, 16)

                ' -------- Modbus settings --------
                    Case HR_MB_SLAVEID
                        val = mbSetRegs(0).ToString()
                    Case HR_MB_BAUD
                        val = mbSetRegs(1).ToString()
                    Case HR_MB_DATABITS
                        val = mbSetRegs(2).ToString()
                    Case HR_MB_PARITY
                        val = mbSetRegs(3).ToString()
                    Case HR_MB_STOPBITS
                        val = mbSetRegs(4).ToString()

                ' -------- Serial settings --------
                    Case HR_SER_BAUD
                        val = serSetRegs(0).ToString()
                    Case HR_SER_DATABITS
                        val = serSetRegs(1).ToString()
                    Case HR_SER_PARITY
                        val = serSetRegs(2).ToString()
                    Case HR_SER_STOPBITS
                        val = serSetRegs(3).ToString()

                ' -------- Buzzer control --------
                    Case HR_BUZ_FREQ
                        val = buzRegs(0).ToString()
                    Case HR_BUZ_MS
                        val = buzRegs(1).ToString()
                    Case HR_BUZ_PATTERN_ON
                        val = buzRegs(2).ToString()
                    Case HR_BUZ_PATTERN_OFF
                        val = buzRegs(3).ToString()
                    Case HR_BUZ_PATTERN_REP
                        val = buzRegs(4).ToString()
                    Case HR_BUZ_CMD
                        val = buzRegs(5).ToString()

                End Select
            End If

            If val IsNot Nothing Then
                dgvModbusMap.Rows(i).Cells(5).Value = val
            End If
        Next
    End Sub

    ' ==========================================================
    ' Extend your BuildModbusMapTable() with new rows
    ' ==========================================================
    Private Sub BuildModbusMapTable_AddSystemBlocks()
        ' Call this at the end of your existing BuildModbusMapTable() after you add current rows.

        ' ---- BoardInfo ----
        mbRows.Add(New MbRow With {.Section = "System / BoardInfo", .Kind = "HR", .Address = HR_BOARD_NAME_START, .Name = "BOARD_NAME", .Format = "ASCII packed (32 chars, HR100..115)", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "System / BoardInfo", .Kind = "HR", .Address = HR_BOARD_SERIAL_START, .Name = "BOARD_SERIAL", .Format = "ASCII packed (32 chars, HR116..131)", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "System / BoardInfo", .Kind = "HR", .Address = HR_MANUFACTURER_START, .Name = "MANUFACTURER", .Format = "ASCII packed (16 chars, HR132..139)", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "System / BoardInfo", .Kind = "HR", .Address = HR_MAC_START, .Name = "MAC", .Format = "ASCII packed (17 chars, HR140..148)", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "System / BoardInfo", .Kind = "HR", .Address = HR_FW_START, .Name = "FW_VERSION", .Format = "ASCII packed (8 chars, HR149..152)", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "System / BoardInfo", .Kind = "HR", .Address = HR_HW_START, .Name = "HW_VERSION", .Format = "ASCII packed (8 chars, HR153..156)", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "System / BoardInfo", .Kind = "HR", .Address = HR_YEAR, .Name = "MAKE_YEAR", .Format = "uint16 (HR157)", .ValueText = ""})

        ' ---- NetworkInfo ----
        mbRows.Add(New MbRow With {.Section = "System / NetworkInfo", .Kind = "HR", .Address = HR_IP_START, .Name = "IP", .Format = "ASCII packed (16 chars, HR170..177)", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "System / NetworkInfo", .Kind = "HR", .Address = HR_MASK_START, .Name = "MASK", .Format = "ASCII packed (16 chars, HR178..185)", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "System / NetworkInfo", .Kind = "HR", .Address = HR_GW_START, .Name = "GW", .Format = "ASCII packed (16 chars, HR186..193)", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "System / NetworkInfo", .Kind = "HR", .Address = HR_DNS_START, .Name = "DNS", .Format = "ASCII packed (16 chars, HR194..201)", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "System / NetworkInfo", .Kind = "HR", .Address = HR_DHCP, .Name = "DHCP", .Format = "0/1 (HR202)", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "System / NetworkInfo", .Kind = "HR", .Address = HR_TCPPORT, .Name = "TCPPORT", .Format = "uint16 (HR203)", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "System / NetworkInfo", .Kind = "HR", .Address = HR_APSSID_START, .Name = "AP_SSID", .Format = "ASCII packed (16 chars, HR204..211)", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "System / NetworkInfo", .Kind = "HR", .Address = HR_APPASS_START, .Name = "AP_PASS", .Format = "ASCII packed (16 chars, HR212..219)", .ValueText = ""})

        ' ---- Modbus Settings ----
        mbRows.Add(New MbRow With {.Section = "System / Modbus Settings", .Kind = "HR", .Address = HR_MB_SLAVEID, .Name = "MB_SLAVEID", .Format = "uint16 (HR230)", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "System / Modbus Settings", .Kind = "HR", .Address = HR_MB_BAUD, .Name = "MB_BAUD", .Format = "uint16 (HR231)", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "System / Modbus Settings", .Kind = "HR", .Address = HR_MB_DATABITS, .Name = "MB_DATABITS", .Format = "7/8 (HR232)", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "System / Modbus Settings", .Kind = "HR", .Address = HR_MB_PARITY, .Name = "MB_PARITY", .Format = "0=None,1=Odd,2=Even (HR233)", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "System / Modbus Settings", .Kind = "HR", .Address = HR_MB_STOPBITS, .Name = "MB_STOPBITS", .Format = "1/2 (HR234)", .ValueText = ""})

        ' ---- Serial Settings ----
        mbRows.Add(New MbRow With {.Section = "System / Serial Settings", .Kind = "HR", .Address = HR_SER_BAUD, .Name = "COM_SER_BAUD", .Format = "uint16 (HR240)", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "System / Serial Settings", .Kind = "HR", .Address = HR_SER_DATABITS, .Name = "COM_SER_DATABITS", .Format = "7/8 (HR241)", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "System / Serial Settings", .Kind = "HR", .Address = HR_SER_PARITY, .Name = "COM_SER_PARITY", .Format = "0=None,1=Odd,2=Even (HR242)", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "System / Serial Settings", .Kind = "HR", .Address = HR_SER_STOPBITS, .Name = "COM_SER_STOPBITS", .Format = "1/2 (HR243)", .ValueText = ""})

        ' ---- Buzzer Control ----
        mbRows.Add(New MbRow With {.Section = "System / Buzzer Control", .Kind = "HR", .Address = HR_BUZ_FREQ, .Name = "BUZ_FREQ", .Format = "Hz (HR250)", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "System / Buzzer Control", .Kind = "HR", .Address = HR_BUZ_MS, .Name = "BUZ_MS", .Format = "Beep duration ms (HR251)", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "System / Buzzer Control", .Kind = "HR", .Address = HR_BUZ_PATTERN_ON, .Name = "BUZ_PATTERN_ON_MS", .Format = "ms (HR252)", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "System / Buzzer Control", .Kind = "HR", .Address = HR_BUZ_PATTERN_OFF, .Name = "BUZ_PATTERN_OFF_MS", .Format = "ms (HR253)", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "System / Buzzer Control", .Kind = "HR", .Address = HR_BUZ_PATTERN_REP, .Name = "BUZ_PATTERN_REPEATS", .Format = "count (HR254)", .ValueText = ""})
        mbRows.Add(New MbRow With {.Section = "System / Buzzer Control", .Kind = "HR", .Address = HR_BUZ_CMD, .Name = "BUZ_CMD", .Format = "write: 1=beep, 2=pattern, 0=stop (HR255)", .ValueText = ""})

    End Sub

    ' ==========================================================
    ' OPTIONAL: simple write helpers (FC06)
    ' ==========================================================
    Private Sub MbWriteU16(hrAddr As UShort, value As UShort)
        If Not mbConnected OrElse mbMaster Is Nothing Then Return
        Try
            mbMaster.WriteSingleRegister(mbSlaveId, hrAddr, value)
        Catch ex As Exception
            LogLine("Modbus HR write error @" & hrAddr & ": " & ex.Message)
        End Try
    End Sub

    Private Sub MbBuzzerBeepOnce(freqHz As Integer, ms As Integer)
        If Not mbConnected OrElse mbMaster Is Nothing Then Return
        MbWriteU16(HR_BUZ_FREQ, CUShort(Math.Max(0, Math.Min(65535, freqHz))))
        MbWriteU16(HR_BUZ_MS, CUShort(Math.Max(0, Math.Min(65535, ms))))
        MbWriteU16(HR_BUZ_CMD, 1US)
    End Sub

    Private Sub MbBuzzerStop()
        MbWriteU16(HR_BUZ_CMD, 0US)
    End Sub

    Private Sub MbBuzzerStartPattern(freqHz As Integer, onMs As Integer, offMs As Integer, repeats As Integer)
        MbWriteU16(HR_BUZ_FREQ, CUShort(Math.Max(0, Math.Min(65535, freqHz))))
        MbWriteU16(HR_BUZ_PATTERN_ON, CUShort(Math.Max(0, Math.Min(65535, onMs))))
        MbWriteU16(HR_BUZ_PATTERN_OFF, CUShort(Math.Max(0, Math.Min(65535, offMs))))
        MbWriteU16(HR_BUZ_PATTERN_REP, CUShort(Math.Max(0, Math.Min(65535, repeats))))
        MbWriteU16(HR_BUZ_CMD, 2US)
    End Sub

End Class

Public Class FormMain

    ' =========================
    ' Connection + IO state
    ' =========================
    Private client As TcpClient
    Private stream As NetworkStream
    Private recvBuffer As New StringBuilder()

    ' Read-loop safety (prevents NullReference during reboot/disconnect)
    Private _readToken As Integer = 0                 ' increments each time we (re)connect/disconnect
    Private _closing As Boolean = False               ' true while we are intentionally closing
    Private _connecting As Boolean = False            ' true during async connect attempts

    Private lastSnapshotJson As String = ""
    Private lastSnapshotTime As DateTime = DateTime.MinValue

    Private reconnectTimer As Timer
    Private pollTimer As Timer

    ' Reconnect scheme
    Private _reconnectPlan As ReconnectPlan = ReconnectPlan.Idle
    Private _rebootReconnectUntil As DateTime = DateTime.MinValue
    Private _lastReconnectAttempt As DateTime = DateTime.MinValue
    Private Const RECONNECT_MIN_GAP_MS As Integer = 1200

    ' DHCP refresh / discovery
    Private _dhcpDiscoveryUntil As DateTime = DateTime.MinValue
    Private _dhcpProbeStep As Integer = 0

    ' prevent log spam & UI loops
    Private _suppressDhcpUiEvent As Boolean = False

    Private Enum ReconnectPlan
        Idle = 0
        Normal = 1          ' normal auto-reconnect or after unexpected drop
        AfterReboot = 2      ' after Save & Reboot
        DhcpDiscover = 3     ' after switching to DHCP or reboot with DHCP
    End Enum

    ' --- Settings: last device config (from NETCFG GET response) ---
    Private Class DeviceNetConfig
        Public Property EthDhcp As Boolean
        Public Property EthMac As String
        Public Property EthIp As String              ' stored static (may be empty in DHCP)
        Public Property EthMask As String
        Public Property EthGw As String
        Public Property EthDns As String
        Public Property TcpPort As Integer
        Public Property ApSsid As String
        Public Property ApPass As String

        ' Runtime values (from NETCFG GET: CLIENTIP / LINK)
        Public Property ClientIp As String
        Public Property LinkUp As Boolean
    End Class

    Private lastDeviceCfg As New DeviceNetConfig()

    ' ==== Schedule list model ====
    Private Class ScheduleItem
        Public Property Id As Integer
        Public Property Enabled As Boolean
        Public Property Mode As String ' DI / RTC / COMBINED / ANALOG / ANALOG_DI / ANALOG_RTC / ANALOG_DI_RTC
        Public Property DaysMask As Integer ' bit0=Mon..bit6=Sun
        Public Property TriggerDI As Integer
        Public Property Edge As String ' RISING / FALLING / BOTH
        Public Property StartHHMM As String
        Public Property EndHHMM As String
        Public Property Recurring As Boolean
        Public Property RelayMask As Integer
        Public Property RelayAction As String ' ON/OFF/TOGGLE
        Public Property Dac1mV As Integer
        Public Property Dac2mV As Integer
        Public Property Dac1R As Integer ' 5/10
        Public Property Dac2R As Integer ' 5/10
        Public Property BuzzEnable As Boolean
        Public Property BuzzFreq As Integer
        Public Property BuzzOn As Integer
        Public Property BuzzOff As Integer
        Public Property BuzzRep As Integer

        ' NEW: Analog Trigger
        Public Property AnalogEnable As Boolean
        Public Property AnalogType As String ' VOLT / CURR
        Public Property AnalogCh As Integer ' 1..2
        Public Property AnalogOp As String ' ABOVE/BELOW/EQUAL/IN_RANGE
        Public Property AnalogV1 As Integer ' mV or mA
        Public Property AnalogV2 As Integer ' mV or mA
        Public Property AnalogHys As Integer ' mV or mA
        Public Property AnalogDbMs As Integer
        Public Property AnalogTol As Integer
    End Class

    Private schedules As New Dictionary(Of Integer, ScheduleItem)()

    ' =========================
    ' NEW: Serial port support (Settings tab)
    ' =========================
    Private serial As SerialPort = Nothing
    Private serialRxBuffer As New StringBuilder()
    Private _serialUiTimer As Timer
    Private _serialBytesPending As Long = 0
    Private _serialLastPieceAt As DateTime = DateTime.MinValue
    Private _serialOpenToken As Integer = 0

    ' =========================
    ' Form init
    ' =========================
    Private Sub FormMain_Load(sender As Object, e As EventArgs) Handles MyBase.Load
        Text = "C-Link A8R-M Configurator"

        txtIp.Text = "169.254.85.149"
        txtPort.Text = "5000"

        cboTempUnit.Items.AddRange(New Object() {"C", "F"})
        cboTempUnit.SelectedIndex = 0

        ' Schedules UI
        cboSchedMode.Items.Clear()
        cboSchedMode.Items.AddRange(New Object() {
            "DI",
            "RTC",
            "DI_RTC",
            "ANALOG",
            "ANALOG_DI",
            "ANALOG_RTC",
            "ANALOG_DI_RTC"
        })
        cboSchedMode.SelectedIndex = 0

        ' Add after your other schedule combo init:
        cboAnalogType.Items.Clear()
        cboAnalogType.Items.AddRange(New Object() {"VOLT", "CURR"})
        cboAnalogType.SelectedIndex = 0

        cboAnalogCh.Items.Clear()
        cboAnalogCh.Items.AddRange(New Object() {"1", "2"})
        cboAnalogCh.SelectedIndex = 0

        cboAnalogOp.Items.Clear()
        cboAnalogOp.Items.AddRange(New Object() {"ABOVE", "BELOW", "EQUAL", "IN_RANGE"})
        cboAnalogOp.SelectedIndex = 0

        AddHandler cboAnalogOp.SelectedIndexChanged,
            Sub()
                Dim op = cboAnalogOp.SelectedItem.ToString().ToUpperInvariant()
                txtAnalogV2.Enabled = (op = "IN_RANGE")
                txtAnalogTol.Visible = (op = "EQUAL")
                LblTol.Visible = (op = "EQUAL")
            End Sub


        cboSchedMode.SelectedIndex = 0

        cboTrigDI.Items.AddRange(New Object() {"1", "2", "3", "4", "5", "6", "7", "8"})
        cboTrigDI.SelectedIndex = 0

        cboEdge.Items.AddRange(New Object() {"RISING", "FALLING", "BOTH", "HIGH", "LOW"})
        cboEdge.SelectedIndex = 0

        cboRelayAction.Items.AddRange(New Object() {"ON", "OFF", "TOGGLE"})
        cboRelayAction.SelectedIndex = 2

        cboDac1Range.Items.AddRange(New Object() {"5V", "10V"})
        cboDac2Range.Items.AddRange(New Object() {"5V", "10V"})
        cboDac1Range.SelectedIndex = 0
        cboDac2Range.SelectedIndex = 0

        ' Default DOW: all days enabled
        chkMon.Checked = True
        chkTue.Checked = True
        chkWed.Checked = True
        chkThu.Checked = True
        chkFri.Checked = True
        chkSat.Checked = True
        chkSun.Checked = True

        ' Schedule buttons
        AddHandler btnSchedNew.Click, AddressOf btnSchedNew_Click
        AddHandler btnSchedAdd.Click, AddressOf btnSchedAdd_Click
        AddHandler btnSchedUpdate.Click, AddressOf btnSchedUpdate_Click
        AddHandler btnSchedDelete.Click, AddressOf btnSchedDelete_Click

        AddHandler btnSchedApply.Click, AddressOf btnSchedApply_Click      ' refresh list
        AddHandler btnSchedDisable.Click, AddressOf btnSchedDisable_Click  ' disable selected
        AddHandler btnSchedQuery.Click, AddressOf btnSchedQuery_Click      ' list from device

        AddHandler lstSchedules.SelectedIndexChanged, AddressOf lstSchedules_SelectedIndexChanged

        ' Settings buttons
        AddHandler btnSettingsRefresh.Click, AddressOf btnSettingsRefresh_Click
        AddHandler btnSettingsApplyToConnection.Click, AddressOf btnSettingsApplyToConnection_Click
        AddHandler btnSettingsSaveReboot.Click, AddressOf btnSettingsSaveReboot_Click
        AddHandler chkUserDhcp.CheckedChanged, AddressOf chkUserDhcp_CheckedChanged

        ' Timers
        reconnectTimer = New Timer()
        reconnectTimer.Interval = 1200
        AddHandler reconnectTimer.Tick, AddressOf ReconnectTimer_Tick

        pollTimer = New Timer()
        pollTimer.Interval = 250 ' faster snapshots for real-time DI
        AddHandler pollTimer.Tick, AddressOf PollTimer_Tick

        ' NEW: Serial UI timer (flushes to textbox without UI freezing)
        _serialUiTimer = New Timer()
        _serialUiTimer.Interval = 120
        AddHandler _serialUiTimer.Tick, AddressOf SerialUiTimer_Tick
        _serialUiTimer.Start()

        ApplyProfessionalTheme()
        UpdateConnectionUi(False)
        SettingsUiDefaults()
        ApplyDhcpUiLock()

        ' NEW: Serial UI setup (Settings tab)
        InitSerialUi()
        InitModbusUi()
        RefreshSerialPortList()
        UpdateSerialUi(False, "Serial: Closed")

        AddHandler Me.FormClosing, AddressOf FormMain_FormClosing
    End Sub

    Private Sub FormMain_FormClosing(sender As Object, e As FormClosingEventArgs)
        _closing = True
        Try : pollTimer.Stop() : Catch : End Try
        Try : reconnectTimer.Stop() : Catch : End Try
        Try : _serialUiTimer.Stop() : Catch : End Try

        DisconnectInternal(False)
        SerialCloseSafe()
    End Sub

    Private Sub ApplyProfessionalTheme()
        BackColor = Color.White

        Dim accent1 = Color.FromArgb(0, 90, 170)
        Dim bad = Color.FromArgb(160, 0, 0)

        lblStatus.ForeColor = bad
        lblNetInfo.ForeColor = accent1

        grpNet.ForeColor = accent1
        grpRelays.ForeColor = accent1
        grpDI.ForeColor = accent1
        grpAnalog.ForeColor = accent1
        grpDAC.ForeColor = accent1
        grpTemp.ForeColor = accent1
        grpRTC.ForeColor = accent1
        grpBeep.ForeColor = accent1
        grpMisc.ForeColor = accent1
        grpSchedule.ForeColor = accent1

        'txtLog.BackColor = Color.FromArgb(250, 250, 250)
        txtLog.Font = New Font("Consolas", 9.0F)
        'txtLog.ForeColor = Color.FromArgb(30, 30, 30)

        ' NEW: Serial group highlight theme
        If grpSerial IsNot Nothing Then
            grpSerial.ForeColor = Color.FromArgb(160, 85, 0)
            txtSerialLog.BackColor = Color.FromArgb(12, 16, 24)
            txtSerialLog.ForeColor = Color.FromArgb(220, 240, 210)
            txtSerialLog.Font = New Font("Consolas", 8.5F)
        End If
    End Sub

    Private Sub SettingsUiDefaults()
        _suppressDhcpUiEvent = True
        chkUserDhcp.Checked = True
        _suppressDhcpUiEvent = False

        txtUserMac.Text = "DE:AD:BE:EF:FE:ED"
        txtUserIp.Text = ""
        txtUserMask.Text = "255.255.255.0"
        txtUserGw.Text = ""
        txtUserDns.Text = "8.8.8.8"
        txtUserTcpPort.Text = "5000"

        txtUserApSsid.Text = "A8RM-SETUP"
        txtUserApPass.Text = "cortexlink"

        RenderDeviceCfgToExistingLabels(Nothing)
    End Sub

    Private Sub ApplyDhcpUiLock()
        Dim dhcp = chkUserDhcp.Checked
        txtUserIp.Enabled = Not dhcp
        txtUserMask.Enabled = Not dhcp
        txtUserGw.Enabled = Not dhcp
        txtUserDns.Enabled = True
    End Sub

    Private Sub chkUserDhcp_CheckedChanged(sender As Object, e As EventArgs)
        If _suppressDhcpUiEvent Then Return
        ApplyDhcpUiLock()
    End Sub

    ' =========================
    ' NEW: Serial UI init + handlers
    ' =========================
    Private Sub InitSerialUi()
        ' Combo defaults
        cboBaud.Items.Clear()
        cboBaud.Items.AddRange(New Object() {"115200", "921600", "57600", "38400", "19200", "9600"})
        cboBaud.SelectedItem = "115200"

        cboDataBits.Items.Clear()
        cboDataBits.Items.AddRange(New Object() {"8", "7"})
        cboDataBits.SelectedItem = "8"

        cboParity.Items.Clear()
        cboParity.Items.AddRange(New Object() {"None", "Odd", "Even", "Mark", "Space"})
        cboParity.SelectedItem = "None"

        cboStopBits.Items.Clear()
        cboStopBits.Items.AddRange(New Object() {"One", "Two", "OnePointFive"})
        cboStopBits.SelectedItem = "One"

        'chkSerialDtr.Checked = False
        'chkSerialRts.Checked = False

        AddHandler btnSerialRefresh.Click, AddressOf btnSerialRefresh_Click
        AddHandler btnSerialConnect.Click, AddressOf btnSerialOpen_Click
        AddHandler btnSerialDisconnect.Click, AddressOf btnSerialClose_Click
        AddHandler btnSerialNetDump.Click, AddressOf btnSerialNetDump_Click
        AddHandler btnSerialClear.Click, AddressOf btnSerialClear_Click

        ' Make terminal a bit taller if designer used a small height (safe runtime tweak)
        Try
            If txtSerialLog.Height < 70 Then
                txtSerialLog.Height = 75
            End If
        Catch
        End Try
    End Sub

    Private Sub btnSerialRefresh_Click(sender As Object, e As EventArgs)
        RefreshSerialPortList()
    End Sub

    Private Sub btnSerialOpen_Click(sender As Object, e As EventArgs)
        SerialOpenSafe()
    End Sub

    Private Sub btnSerialClose_Click(sender As Object, e As EventArgs)
        SerialCloseSafe()
    End Sub

    Private Sub btnSerialClear_Click(sender As Object, e As EventArgs)
        If txtSerialLog.InvokeRequired Then
            txtSerialLog.Invoke(Sub() btnSerialClear_Click(sender, e))
            Return
        End If
        txtSerialLog.Clear()
        serialRxBuffer.Clear()
        _serialBytesPending = 0
        _serialLastPieceAt = DateTime.MinValue
        UpdateSerialUi(IsSerialOpen(), If(IsSerialOpen(), "Serial: Open", "Serial: Closed"))
    End Sub

    Private Sub btnSerialNetDump_Click(sender As Object, e As EventArgs)
        ' Device supports: SERIALNET or NETDUMP (prints pretty table to Serial)
        If Not IsSerialOpen() Then
            SerialAppendLineLocal("[WARN] Serial not open. Select COM and click Open first.")
            UpdateSerialUi(False, "Serial: Closed")
            Return
        End If

        SerialAppendLineLocal("[TX] SERIALNET")
        SerialWriteLine("SERIALNET")
    End Sub

    Private Sub RefreshSerialPortList()
        Try
            Dim ports = SerialPort.GetPortNames().OrderBy(Function(p) p).ToArray()

            Dim prev As String = ""
            If cboComPort.SelectedItem IsNot Nothing Then prev = cboComPort.SelectedItem.ToString()

            cboComPort.Items.Clear()
            cboComPort.Items.AddRange(ports)

            If ports.Length > 0 Then
                If prev.Length > 0 AndAlso ports.Contains(prev) Then
                    cboComPort.SelectedItem = prev
                Else
                    cboComPort.SelectedIndex = 0
                End If
            End If

            UpdateSerialUi(IsSerialOpen(), If(IsSerialOpen(), $"Serial: Open ({serial.PortName})", "Serial: Closed"))
        Catch ex As Exception
            SerialAppendLineLocal("[ERR] Refresh COM list failed: " & ex.Message)
        End Try
    End Sub

    Private Function IsSerialOpen() As Boolean
        Try
            Return serial IsNot Nothing AndAlso serial.IsOpen
        Catch
            Return False
        End Try
    End Function

    Private Sub SerialOpenSafe()
        If _closing Then Return
        If IsSerialOpen() Then
            UpdateSerialUi(True, $"Serial: Open ({serial.PortName})")
            Return
        End If

        Dim portName As String = ""
        If cboComPort.SelectedItem IsNot Nothing Then portName = cboComPort.SelectedItem.ToString()

        If String.IsNullOrWhiteSpace(portName) Then
            SerialAppendLineLocal("[WARN] No COM port selected.")
            UpdateSerialUi(False, "Serial: Closed")
            Return
        End If

        Dim baud As Integer = ToInt(If(cboBaud.SelectedItem, "115200").ToString(), 115200)
        Dim dataBits As Integer = ToInt(If(cboDataBits.SelectedItem, "8").ToString(), 8)

        Dim parity As Parity = Parity.None
        Select Case If(cboParity.SelectedItem, "None").ToString().Trim().ToUpperInvariant()
            Case "ODD" : parity = Parity.Odd
            Case "EVEN" : parity = Parity.Even
            Case "MARK" : parity = Parity.Mark
            Case "SPACE" : parity = Parity.Space
            Case Else : parity = Parity.None
        End Select

        Dim stopBits As StopBits = StopBits.One
        Select Case If(cboStopBits.SelectedItem, "One").ToString().Trim().ToUpperInvariant()
            Case "TWO" : stopBits = StopBits.Two
            Case "ONEPOINTFIVE" : stopBits = StopBits.OnePointFive
            Case Else : stopBits = StopBits.One
        End Select

        Try
            _serialOpenToken += 1
            serialRxBuffer.Clear()
            _serialBytesPending = 0
            _serialLastPieceAt = DateTime.MinValue

            serial = New SerialPort(portName, baud, parity, dataBits, stopBits)
            serial.Encoding = Encoding.ASCII
            serial.NewLine = vbLf
            serial.ReadTimeout = 500
            serial.WriteTimeout = 500
            'serial.DtrEnable = chkSerialDtr.Checked
            'serial.RtsEnable = chkSerialRts.Checked

            AddHandler serial.DataReceived, AddressOf Serial_DataReceived

            serial.Open()

            UpdateSerialUi(True, $"Serial: Open ({portName} @ {baud})")
            SerialAppendLineLocal($"[OK] Opened {portName} @ {baud}, {dataBits}{parity.ToString().Substring(0, 1)},{stopBits}")

            ' Helpful: ask the device to print network table (user can click button too)
            ' Many boards print on boot; this makes it easy after opening without rebooting.
            SerialWriteLine("SERIALNET")
            SerialAppendLineLocal("[TX] SERIALNET")
        Catch ex As Exception
            SerialAppendLineLocal("[ERR] Unable to open serial: " & ex.Message)
            UpdateSerialUi(False, "Serial: Closed")
            SerialCloseSafe()
        End Try
    End Sub

    Private Sub SerialCloseSafe()
        Try
            If serial IsNot Nothing Then
                Try
                    RemoveHandler serial.DataReceived, AddressOf Serial_DataReceived
                Catch
                End Try

                Try
                    If serial.IsOpen Then serial.Close()
                Catch
                End Try

                Try
                    serial.Dispose()
                Catch
                End Try
            End If
        Catch
        End Try

        serial = Nothing
        UpdateSerialUi(False, "Serial: Closed")
    End Sub

    Private Sub SerialWriteLine(line As String)
        Try
            If Not IsSerialOpen() Then Return
            serial.Write(line & vbLf)
        Catch ex As Exception
            SerialAppendLineLocal("[ERR] Serial write failed: " & ex.Message)
            SerialCloseSafe()
        End Try
    End Sub

    Private Sub Serial_DataReceived(sender As Object, e As SerialDataReceivedEventArgs)
        ' Runs on a non-UI thread
        Try
            If serial Is Nothing OrElse Not serial.IsOpen Then Return

            Dim chunk As String = serial.ReadExisting()
            If String.IsNullOrEmpty(chunk) Then Return

            SyncLock serialRxBuffer
                serialRxBuffer.Append(chunk)
                _serialBytesPending += chunk.Length
                _serialLastPieceAt = DateTime.Now
            End SyncLock
        Catch
            ' ignore; UI will show close if needed
        End Try
    End Sub

    Private Sub SerialUiTimer_Tick(sender As Object, e As EventArgs)
        If _closing Then Return

        Dim toFlush As String = Nothing
        SyncLock serialRxBuffer
            If serialRxBuffer.Length > 0 Then
                toFlush = serialRxBuffer.ToString()
                serialRxBuffer.Clear()
                _serialBytesPending = 0
            End If
        End SyncLock

        If Not String.IsNullOrEmpty(toFlush) Then
            SerialAppendTextColored(toFlush)
        End If

        ' Keep status updated
        If IsSerialOpen() Then
            lblSerialStatus.Text = $"Serial: Open ({serial.PortName})"
            lblSerialStatus.ForeColor = Color.DarkGreen
        Else
            lblSerialStatus.Text = "Serial: Closed"
            lblSerialStatus.ForeColor = Color.FromArgb(120, 60, 0)
        End If
    End Sub

    Private Sub UpdateSerialUi(isOpen As Boolean, status As String)
        If lblSerialStatus Is Nothing Then Return

        If lblSerialStatus.InvokeRequired Then
            lblSerialStatus.Invoke(Sub() UpdateSerialUi(isOpen, status))
            Return
        End If

        lblSerialStatus.Text = status
        lblSerialStatus.ForeColor = If(isOpen, Color.DarkGreen, Color.FromArgb(120, 60, 0))

        btnSerialConnect.Enabled = Not isOpen
        btnSerialDisconnect.Enabled = isOpen
        btnSerialNetDump.Enabled = isOpen
    End Sub

    Private Sub SerialAppendLineLocal(line As String)
        SerialAppendTextColored(line & Environment.NewLine)
    End Sub

    Private Sub SerialAppendTextColored(text As String)
        If txtSerialLog Is Nothing Then Return

        If txtSerialLog.InvokeRequired Then
            txtSerialLog.BeginInvoke(Sub() SerialAppendTextColored(text))
            Return
        End If

        Dim processed = StripAnsi(text)

        ' Soft highlight: make key tokens easier to spot without RichTextBox
        ' (Since txtSerialLog is a TextBox, we keep it simple and add separators.)
        txtSerialLog.AppendText(processed)

        ' auto-scroll
        Try
            txtSerialLog.SelectionStart = txtSerialLog.TextLength
            txtSerialLog.ScrollToCaret()
        Catch
        End Try

        ' Optional: try to infer IP/PORT and suggest in UI (non-invasive)
        TryParseSerialForConnectHints(processed)
    End Sub

    Private Function StripAnsi(s As String) As String
        If String.IsNullOrEmpty(s) Then Return s
        ' Remove ANSI escape sequences like: ESC[1;36m
        Dim sb As New StringBuilder(s.Length)
        Dim i As Integer = 0
        While i < s.Length
            Dim ch = s(i)
            If ch = ChrW(27) Then
                ' skip until 'm' or end
                i += 1
                If i < s.Length AndAlso s(i) = "["c Then
                    i += 1
                    While i < s.Length AndAlso s(i) <> "m"c
                        i += 1
                    End While
                    If i < s.Length AndAlso s(i) = "m"c Then i += 1
                End If
            Else
                sb.Append(ch)
                i += 1
            End If
        End While
        Return sb.ToString()
    End Function

    Private Sub TryParseSerialForConnectHints(text As String)
        ' We do NOT change user values automatically here; we only provide gentle help:
        ' - When we see TCPPORT=xxxx, update txtPort if empty or invalid.
        ' - When we see IP_NOW or IP_CUR, we can update txtIp if it still has default.
        Try
            ' Look for "TCPPORT: 5000" or "TCPPORT=5000"
            Dim port As Integer = -1

            Dim idxEq = text.ToUpperInvariant().IndexOf("TCPPORT=")
            If idxEq >= 0 Then
                Dim p = ExtractTokenAfter(text, idxEq + "TCPPORT=".Length)
                port = ToInt(p, -1)
            End If

            Dim idxColon = text.ToUpperInvariant().IndexOf("TCPPORT")
            If port < 0 AndAlso idxColon >= 0 Then
                ' handle "TCPPORT: 5000"
                Dim after = text.Substring(idxColon)
                Dim c = after.IndexOf(":", StringComparison.Ordinal)
                If c >= 0 Then
                    Dim v = after.Substring(c + 1).Trim()
                    Dim take = v.Split(" "c, vbTab, vbCr, vbLf).FirstOrDefault()
                    port = ToInt(take, -1)
                End If
            End If

            If port > 0 AndAlso port <= 65535 Then
                If Not Integer.TryParse(txtPort.Text.Trim(), Nothing) OrElse txtPort.Text.Trim() = "8080" Then
                    txtPort.Text = port.ToString()
                End If
            End If

            ' Prefer runtime IP when shown: IP_NOW, IP_CUR, IP_NOW:
            Dim ipCandidate As String = ""

            ipCandidate = FindIpAfterKey(text, "IP_NOW=")
            If ipCandidate.Length = 0 Then ipCandidate = FindIpAfterKey(text, "IP_CUR=")
            If ipCandidate.Length = 0 Then ipCandidate = FindIpAfterKey(text, "IP_NOW")
            If ipCandidate.Length = 0 Then ipCandidate = FindIpAfterKey(text, "IP_CUR")
            If ipCandidate.Length = 0 Then ipCandidate = FindIpAfterKey(text, "IP=") ' stored pref

            If LooksLikeIp(ipCandidate) Then
                If txtIp.Text.Trim() = "169.254.85.149" OrElse txtIp.Text.Trim().Length = 0 Then
                    txtIp.Text = ipCandidate
                End If
            End If
        Catch
        End Try
    End Sub

    Private Function FindIpAfterKey(text As String, key As String) As String
        Dim up = text.ToUpperInvariant()
        Dim kUp = key.ToUpperInvariant()
        Dim idx = up.IndexOf(kUp, StringComparison.Ordinal)
        If idx < 0 Then Return ""

        Dim start As Integer = idx + key.Length

        ' If key didn't include '=' and the text uses ":" then jump
        If key.EndsWith("IP_NOW", StringComparison.OrdinalIgnoreCase) OrElse key.EndsWith("IP_CUR", StringComparison.OrdinalIgnoreCase) Then
            Dim colon = text.IndexOf(":"c, idx)
            If colon >= 0 AndAlso colon < idx + 18 Then
                start = colon + 1
            End If
        End If

        Dim tail = text.Substring(start).Trim()
        Dim token = tail.Split(" "c, vbTab, vbCr, vbLf).FirstOrDefault()
        If token Is Nothing Then Return ""
        token = token.Trim()
        ' strip trailing punctuation
        token = token.TrimEnd(","c, ";"c)
        Return token
    End Function

    Private Function ExtractTokenAfter(text As String, pos As Integer) As String
        If pos < 0 OrElse pos >= text.Length Then Return ""
        Dim tail = text.Substring(pos).Trim()
        Dim token = tail.Split(" "c, vbTab, vbCr, vbLf).FirstOrDefault()
        If token Is Nothing Then Return ""
        Return token.Trim()
    End Function

    Private Function LooksLikeIp(s As String) As Boolean
        If String.IsNullOrWhiteSpace(s) Then Return False
        Dim parts = s.Split("."c)
        If parts.Length <> 4 Then Return False
        For Each p In parts
            Dim v As Integer
            If Not Integer.TryParse(p, v) Then Return False
            If v < 0 OrElse v > 255 Then Return False
        Next
        Return True
    End Function

    ' =========================
    ' Connection management
    ' =========================
    Private Sub btnConnect_Click(sender As Object, e As EventArgs) Handles btnConnect.Click
        If IsConnected() OrElse _connecting Then
            CancelReconnectPlan()
            Disconnect()
        Else
            _reconnectPlan = ReconnectPlan.Idle
            ConnectToBoard()
        End If
    End Sub

    Private Function IsConnected() As Boolean
        Return client IsNot Nothing AndAlso client.Connected AndAlso stream IsNot Nothing
    End Function

    Private Sub ConnectToBoard()
        If _closing Then Return
        If _connecting Then Return
        If IsConnected() Then Return

        Dim ip = txtIp.Text.Trim()
        Dim port As Integer
        If Not Integer.TryParse(txtPort.Text.Trim(), port) Then port = 5000

        If ip.Length = 0 OrElse port < 1 OrElse port > 65535 Then
            LogLine("Connect blocked: invalid IP or port")
            UpdateConnectionUi(False)
            Return
        End If

        Try
            _connecting = True

            client = New TcpClient()
            client.ReceiveTimeout = 2000
            client.SendTimeout = 2000
            client.NoDelay = True

            Dim tokenAtStart = _readToken
            client.BeginConnect(ip, port,
                Sub(ar)
                    ConnectCallback(ar, tokenAtStart)
                End Sub,
                Nothing)

            LogLine("Connecting to " & ip & ":" & port & " ...")
        Catch ex As Exception
            _connecting = False
            LogLine("Connect error: " & ex.Message)
            UpdateConnectionUi(False)
            ScheduleReconnectNormal()
        End Try
    End Sub

    Private Sub ConnectCallback(ar As IAsyncResult, tokenAtStart As Integer)
        ' Runs on worker thread
        Try
            ' If a disconnect happened while connecting, ignore completion
            If tokenAtStart <> _readToken OrElse _closing Then
                SafeCloseClient()
                Return
            End If

            client.EndConnect(ar)

            If client IsNot Nothing AndAlso client.Connected Then
                stream = client.GetStream()
                Dim myToken = _readToken

                Me.Invoke(Sub()
                              _connecting = False
                              LogLine("Connected.")
                              UpdateConnectionUi(True)
                              pollTimer.Start()

                              ' Start read loop AFTER UI updated
                              BeginRead(myToken)

                              ' Initial commands
                              SendCommand("PING")
                              SendCommand("SNAPJSON")
                              SendCommand("NET?")
                              SendCommand("SCHED LIST")
                              SendCommand("NETCFG GET") ' MUST refresh existing settings after connect

                              ' Stop reconnect loop if it was running
                              If reconnectTimer IsNot Nothing Then reconnectTimer.Stop()
                              If _reconnectPlan = ReconnectPlan.DhcpDiscover Then
                                  ' We are now connected; request authoritative config to learn CLIENTIP
                                  SendCommand("NETCFG GET")
                              End If
                              _reconnectPlan = ReconnectPlan.Idle
                          End Sub)
            Else
                Me.Invoke(Sub()
                              _connecting = False
                              LogLine("Connect failed (no connection).")
                              UpdateConnectionUi(False)
                              HandleReconnectFailure()
                          End Sub)
            End If
        Catch ex As Exception
            Me.Invoke(Sub()
                          _connecting = False
                          LogLine("Connect callback error: " & ex.Message)
                          UpdateConnectionUi(False)
                          HandleReconnectFailure()
                      End Sub)
        End Try
    End Sub

    Private Sub HandleReconnectFailure()
        ' Called on UI thread
        If _closing Then Return

        Select Case _reconnectPlan
            Case ReconnectPlan.AfterReboot
                ' keep timer running until window ends
                If DateTime.Now > _rebootReconnectUntil Then
                    ScheduleReconnectNormal()
                Else
                    reconnectTimer.Start()
                End If

            Case ReconnectPlan.DhcpDiscover
                If DateTime.Now > _dhcpDiscoveryUntil Then
                    ' stop discovery; fall back to normal auto reconnect if enabled
                    ScheduleReconnectNormal()
                Else
                    reconnectTimer.Start()
                End If

            Case Else
                ScheduleReconnectNormal()
        End Select
    End Sub

    Private Sub Disconnect()
        DisconnectInternal(True)
    End Sub

    Private Sub DisconnectInternal(logIt As Boolean)
        If _closing Then logIt = False

        ' Invalidate any outstanding reads immediately
        _readToken += 1

        Try : pollTimer.Stop() : Catch : End Try

        Try
            If logIt Then LogLine("Disconnected.")
        Catch
        End Try

        SafeCloseClient()

        Try
            UpdateConnectionUi(False)
        Catch
        End Try
    End Sub

    Private Sub SafeCloseClient()
        ' IMPORTANT: must avoid NullReference in ReadCallback: do NOT assume stream is non-null there.
        Try
            If stream IsNot Nothing Then
                Try : stream.Close() : Catch : End Try
            End If
        Catch
        End Try

        Try
            If client IsNot Nothing Then
                Try : client.Close() : Catch : End Try
            End If
        Catch
        End Try

        stream = Nothing
        client = Nothing
        _connecting = False
    End Sub

    Private Sub UpdateConnectionUi(connected As Boolean)
        If InvokeRequired Then
            Invoke(Sub() UpdateConnectionUi(connected))
            Return
        End If

        btnConnect.Text = If(connected, "Disconnect", "Connect")
        lblStatus.Text = If(connected, "Connected", "Disconnected")
        lblStatus.ForeColor = If(connected, Color.DarkGreen, Color.DarkRed)

        grpRelays.Enabled = connected
        grpDAC.Enabled = connected
        grpBeep.Enabled = connected
        grpRTC.Enabled = connected
        grpMisc.Enabled = connected
        chkAutoPoll.Enabled = connected

        grpSchedule.Enabled = connected

        grpExistingSettings.Enabled = connected
        grpUserSettings.Enabled = connected
    End Sub

    ' =========================
    ' TCP send / receive
    ' =========================
    Private Sub SendCommand(cmd As String)
        If _closing Then Return
        Dim localClient = client
        Dim localStream = stream

        If localClient Is Nothing OrElse localStream Is Nothing OrElse Not localClient.Connected Then
            LogLine("Send failed (not connected): " & cmd)
            Return
        End If

        Try
            Dim data = Encoding.ASCII.GetBytes(cmd & vbLf)
            localStream.Write(data, 0, data.Length)
            LogLine("TX: " & cmd)
        Catch ex As Exception
            LogLine("Send error: " & ex.Message)
            DisconnectInternal(False)
            HandleReconnectFailure()
        End Try
    End Sub

    Private Class ReadState
        Public Property Token As Integer
        Public Property Buffer As Byte()
    End Class

    Private Sub BeginRead(token As Integer)
        If _closing Then Return
        Dim localClient = client
        Dim localStream = stream
        If localClient Is Nothing OrElse localStream Is Nothing OrElse Not localClient.Connected Then Return

        Dim st As New ReadState With {
            .Token = token,
            .Buffer = New Byte(1023) {}
        }

        Try
            localStream.BeginRead(st.Buffer, 0, st.Buffer.Length, AddressOf ReadCallback, st)
        Catch ex As Exception
            LogLine("BeginRead error: " & ex.Message)
            DisconnectInternal(False)
            HandleReconnectFailure()
        End Try
    End Sub

    Private Sub ReadCallback(ar As IAsyncResult)
        Dim st As ReadState = Nothing
        Try
            st = CType(ar.AsyncState, ReadState)
        Catch
            st = Nothing
        End Try

        ' If state/token mismatch, ignore (old connection callback).
        If st Is Nothing OrElse st.Token <> _readToken OrElse _closing Then
            Return
        End If

        Dim bytesRead As Integer = 0
        Try
            ' IMPORTANT: avoid NullReferenceException when stream was cleared by DisconnectInternal.
            Dim localStream = stream
            If localStream Is Nothing Then Return

            bytesRead = localStream.EndRead(ar)
        Catch ex As Exception
            ' Any exception here should not crash UI; just reconnect if desired
            Try
                Me.Invoke(Sub()
                              LogLine("Read error: " & ex.Message)
                              DisconnectInternal(False)
                              HandleReconnectFailure()
                          End Sub)
            Catch
            End Try
            Return
        End Try

        If st.Token <> _readToken OrElse _closing Then Return

        If bytesRead <= 0 Then
            Try
                Me.Invoke(Sub()
                              LogLine("Remote closed connection.")
                              DisconnectInternal(False)
                              HandleReconnectFailure()
                          End Sub)
            Catch
            End Try
            Return
        End If

        Dim text As String = ""
        Try
            text = Encoding.ASCII.GetString(st.Buffer, 0, bytesRead)
        Catch
            text = ""
        End Try

        Try
            Me.Invoke(Sub() ProcessIncomingText(text))
        Catch
        End Try

        ' Continue reading if still same connection
        If st.Token = _readToken AndAlso Not _closing Then
            BeginRead(st.Token)
        End If
    End Sub

    Private Sub ProcessIncomingText(text As String)
        recvBuffer.Append(text)
        While True
            Dim s = recvBuffer.ToString()
            Dim idx = s.IndexOf(vbLf, StringComparison.Ordinal)
            If idx < 0 Then Exit While
            Dim line = s.Substring(0, idx).Trim()
            recvBuffer.Remove(0, idx + 1)
            If line.Length > 0 Then HandleLine(line)
        End While
    End Sub

    Private Sub HandleLine(line As String)
        If String.IsNullOrWhiteSpace(line) Then Return
        LogLine("RX: " & line)

        If line.StartsWith("HELLO", StringComparison.OrdinalIgnoreCase) Then Return
        If line.Equals("PONG", StringComparison.OrdinalIgnoreCase) Then Return

        If line.StartsWith("{") AndAlso line.EndsWith("}") Then
            lastSnapshotJson = line
            lastSnapshotTime = DateTime.Now
            UpdateFromJson(line)
            Return
        End If

        If line.StartsWith("RTC ", StringComparison.OrdinalIgnoreCase) Then
            txtRtcCurrent.Text = line.Substring(4)
            Return
        End If

        If line.StartsWith("NET ", StringComparison.OrdinalIgnoreCase) Then
            lblNetInfo.Text = line.Substring(4)
            Return
        End If

        If line.StartsWith("NETCFG ", StringComparison.OrdinalIgnoreCase) Then
            HandleNetCfgLine(line)
            Return
        End If

        If line.StartsWith("SCHED ", StringComparison.OrdinalIgnoreCase) Then
            HandleSchedLine(line)
            Return
        End If
    End Sub

    ' =========================
    ' SETTINGS: NETCFG protocol parsing
    ' =========================
    Private Sub HandleNetCfgLine(line As String)
        Dim up = line.ToUpperInvariant()

        If up.StartsWith("NETCFG OK") Then
            LogLine("Settings saved on device.")
            Return
        End If

        If up.StartsWith("NETCFG REBOOTING") Then
            LogLine("Device is rebooting...")
            Return
        End If

        Dim args As String = line.Substring(7).Trim()

        Dim cfg As New DeviceNetConfig()
        cfg.EthMac = Nz(GetTok(args, "MAC"), "")
        cfg.EthDhcp = (ToInt(GetTok(args, "DHCP"), 1) <> 0)
        cfg.EthIp = Nz(GetTok(args, "IP"), "")
        cfg.EthMask = Nz(GetTok(args, "MASK"), "")
        cfg.EthGw = Nz(GetTok(args, "GW"), "")
        cfg.EthDns = Nz(GetTok(args, "DNS"), "")
        cfg.TcpPort = ToInt(GetTok(args, "TCPPORT"), 5000)
        cfg.ApSsid = Nz(GetTok(args, "APSSID"), "")
        cfg.ApPass = Nz(GetTok(args, "APPASS"), "")
        cfg.ClientIp = Nz(GetTok(args, "CLIENTIP"), "")
        cfg.LinkUp = (Nz(GetTok(args, "LINK"), "").ToUpperInvariant() = "UP")

        lastDeviceCfg = cfg

        ' Always update Existing Configuration on every NETCFG GET reply
        RenderDeviceCfgToExistingLabels(cfg)

        ' Only push into user editor when user isn't actively editing? (simple approach: always update)
        RenderDeviceCfgToUserEditor(cfg)

        ' DHCP: we must connect to runtime IP (CLIENTIP), not the stored static IP.
        If cfg.EthDhcp AndAlso cfg.ClientIp.Length > 0 Then
            ' Refresh dashboard connect target so the user can reconnect (and auto reconnect will use it)
            txtIp.Text = cfg.ClientIp
            txtPort.Text = cfg.TcpPort.ToString()
        End If
    End Sub

    Private Sub RenderDeviceCfgToExistingLabels(cfg As DeviceNetConfig)
        If cfg Is Nothing Then
            lblExistingMac.Text = "MAC: (n/a)"
            lblExistingDhcp.Text = "DHCP: (n/a)"
            lblExistingIp.Text = "IP: (n/a)"
            lblExistingMask.Text = "Subnet Mask: (n/a)"
            lblExistingGw.Text = "Gateway: (n/a)"
            lblExistingDns.Text = "DNS: (n/a)"
            lblExistingTcpPort.Text = "TCP Port: (n/a)"
            lblExistingApSsid.Text = "AP SSID: (n/a)"
            lblExistingApPass.Text = "AP PASS: (n/a)"
            Return
        End If

        lblExistingMac.Text = "MAC: " & cfg.EthMac
        lblExistingDhcp.Text = "DHCP: " & If(cfg.EthDhcp, "YES", "NO")

        ' IMPORTANT: show actual runtime IP when DHCP is enabled by using CLIENTIP
        Dim shownIp As String
        If cfg.EthDhcp Then
            shownIp = If(cfg.ClientIp.Length > 0, cfg.ClientIp, "(DHCP - unknown)")
        Else
            shownIp = If(cfg.EthIp.Length > 0, cfg.EthIp, "(static - empty)")
        End If

        lblExistingIp.Text = "IP: " & shownIp
        lblExistingMask.Text = "Subnet Mask: " & cfg.EthMask
        lblExistingGw.Text = "Gateway: " & cfg.EthGw
        lblExistingDns.Text = "DNS: " & cfg.EthDns
        lblExistingTcpPort.Text = "TCP Port: " & cfg.TcpPort.ToString()
        lblExistingApSsid.Text = "AP SSID: " & cfg.ApSsid
        lblExistingApPass.Text = "AP PASS: " & If(cfg.ApPass.Length > 0, New String("*"c, Math.Min(10, cfg.ApPass.Length)), "(empty)")

        lblExistingDhcp.ForeColor = If(cfg.EthDhcp, Color.DarkGreen, Color.DarkOrange)
        lblExistingIp.ForeColor = Color.FromArgb(0, 90, 170)
        lblExistingTcpPort.ForeColor = Color.FromArgb(0, 90, 170)
    End Sub

    Private Sub RenderDeviceCfgToUserEditor(cfg As DeviceNetConfig)
        If cfg Is Nothing Then Return
        _suppressDhcpUiEvent = True
        chkUserDhcp.Checked = cfg.EthDhcp
        _suppressDhcpUiEvent = False

        txtUserMac.Text = cfg.EthMac

        ' If DHCP, keep static IP field blank (device stores "", runtime reported via CLIENTIP)
        txtUserIp.Text = If(cfg.EthDhcp, "", cfg.EthIp)

        txtUserMask.Text = cfg.EthMask
        txtUserGw.Text = cfg.EthGw
        txtUserDns.Text = cfg.EthDns
        txtUserTcpPort.Text = cfg.TcpPort.ToString()

        If cfg.ApSsid.Length > 0 Then txtUserApSsid.Text = cfg.ApSsid
        If cfg.ApPass.Length > 0 Then txtUserApPass.Text = cfg.ApPass

        ApplyDhcpUiLock()
    End Sub

    Private Sub btnSettingsRefresh_Click(sender As Object, e As EventArgs)
        If Not IsConnected() Then
            LogLine("Settings refresh: not connected. Connect first.")
            Return
        End If
        SendCommand("NETCFG GET")
    End Sub

    Private Sub btnSettingsApplyToConnection_Click(sender As Object, e As EventArgs)
        Dim ip As String = txtUserIp.Text.Trim()
        Dim port = ToInt(txtUserTcpPort.Text.Trim(), 5000)

        ' If DHCP is enabled, prefer the last known runtime IP from NETCFG GET
        If chkUserDhcp.Checked Then
            Dim cip = Nz(lastDeviceCfg.ClientIp, "")
            If cip.Length > 0 Then
                ip = cip
            Else
                ' If we don't know yet, keep txtIp unchanged and let the DHCP discovery plan handle it
                LogLine("DHCP is enabled but runtime IP is unknown. Use Refresh after connecting to learn CLIENTIP.")
            End If
        End If

        If ip.Length > 0 Then txtIp.Text = ip
        txtPort.Text = port.ToString()

        LogLine("Applied Settings -> Connect fields updated.")
        tabMain.SelectedTab = tabPageDashboard
    End Sub

    Private Sub btnSettingsSaveReboot_Click(sender As Object, e As EventArgs)
        ' Validate minimal values
        Dim mac = txtUserMac.Text.Trim()
        Dim apSsid = txtUserApSsid.Text.Trim()
        Dim apPass = txtUserApPass.Text.Trim()
        Dim dhcp = chkUserDhcp.Checked
        Dim tcpPort = ToInt(txtUserTcpPort.Text.Trim(), 5000)

        If tcpPort < 1 OrElse tcpPort > 65535 Then
            MessageBox.Show("Invalid TCP port.", "Validation", MessageBoxButtons.OK, MessageBoxIcon.Warning)
            Return
        End If

        If mac.Length < 11 Then
            MessageBox.Show("Invalid MAC format. Example: DE:AD:BE:EF:FE:ED", "Validation", MessageBoxButtons.OK, MessageBoxIcon.Warning)
            Return
        End If

        Dim ip = txtUserIp.Text.Trim()
        Dim mask = txtUserMask.Text.Trim()
        Dim gw = txtUserGw.Text.Trim()
        Dim dns = txtUserDns.Text.Trim()

        If Not dhcp Then
            If ip.Length = 0 Then
                MessageBox.Show("Static IP required when DHCP is disabled.", "Validation", MessageBoxButtons.OK, MessageBoxIcon.Warning)
                Return
            End If
            If mask.Length = 0 Then
                MessageBox.Show("Subnet mask required when DHCP is disabled.", "Validation", MessageBoxButtons.OK, MessageBoxIcon.Warning)
                Return
            End If
        Else
            ' in DHCP mode, we should not send stale/static IP tokens if user left text in the box
            ip = ""
        End If

        Dim cmd As New StringBuilder()
        cmd.Append("NETCFG SET ")
        cmd.Append("DHCP=").Append(If(dhcp, "1", "0")).Append(" ")
        cmd.Append("MAC=").Append(mac).Append(" ")
        cmd.Append("IP=").Append(ip).Append(" ")
        cmd.Append("MASK=").Append(mask).Append(" ")
        cmd.Append("GW=").Append(gw).Append(" ")
        cmd.Append("DNS=").Append(dns).Append(" ")
        cmd.Append("TCPPORT=").Append(tcpPort.ToString()).Append(" ")
        cmd.Append("APSSID=").Append(apSsid).Append(" ")
        cmd.Append("APPASS=").Append(apPass).Append(" ")
        cmd.Append("REBOOT=1")

        If MessageBox.Show("Save settings to ESP32 flash and reboot now?", "Confirm",
                           MessageBoxButtons.YesNo, MessageBoxIcon.Question) <> DialogResult.Yes Then
            Return
        End If

        ' Update dashboard target:
        txtPort.Text = tcpPort.ToString()

        If Not dhcp AndAlso ip.Length > 0 Then
            ' Static: we know new IP
            txtIp.Text = ip
        Else
            ' DHCP: we do NOT know the new DHCP IP yet.
            ' Keep current txtIp, and start DHCP discovery plan after reboot.
            LogLine("DHCP enabled: runtime IP will be learned after reconnect (NETCFG GET CLIENTIP).")
        End If

        ' Must be connected to send the command
        If IsConnected() Then
            SendCommand(cmd.ToString())
        Else
            LogLine("Not connected; cannot send NETCFG SET. Connect first, then Save & Reboot.")
            Return
        End If

        ' Start reconnect plan BEFORE disconnecting
        If dhcp Then
            ScheduleReconnectDhcpDiscover()
        Else
            ScheduleReconnectAfterReboot()
        End If

        ' Intentional disconnect (prevents EndRead on disposed stream)
        DisconnectInternal(False)

        ' Always start reconnect timer for reboot actions
        reconnectTimer.Start()
    End Sub

    ' =========================
    ' Reconnect plan helpers
    ' =========================
    Private Sub CancelReconnectPlan()
        _reconnectPlan = ReconnectPlan.Idle
        _rebootReconnectUntil = DateTime.MinValue
        _dhcpDiscoveryUntil = DateTime.MinValue
        _dhcpProbeStep = 0
        Try : reconnectTimer.Stop() : Catch : End Try
    End Sub

    Private Sub ScheduleReconnectNormal()
        If _closing Then Return
        If chkAutoReconnect.Checked Then
            _reconnectPlan = ReconnectPlan.Normal
            If reconnectTimer IsNot Nothing Then reconnectTimer.Start()
        End If
    End Sub

    Private Sub ScheduleReconnectAfterReboot()
        If _closing Then Return
        _reconnectPlan = ReconnectPlan.AfterReboot
        _rebootReconnectUntil = DateTime.Now.AddSeconds(45) ' allow time for ESP32 reboot + link-up
        If reconnectTimer IsNot Nothing Then reconnectTimer.Start()
    End Sub

    ' DHCP discovery strategy:
    '  1) Try reconnecting to the current txtIp (may still be valid if DHCP lease unchanged)
    '  2) Try AutoIP 169.254.x.y derived from our PC's link-local subnet (simple sweep)
    '  3) When connected, request NETCFG GET to learn CLIENTIP -> update txtIp and keep stable.
    Private Sub ScheduleReconnectDhcpDiscover()
        If _closing Then Return
        _reconnectPlan = ReconnectPlan.DhcpDiscover
        _dhcpDiscoveryUntil = DateTime.Now.AddSeconds(60)
        _dhcpProbeStep = 0
        reconnectTimer.Start()
    End Sub

    ' =========================
    ' Schedule protocol parsing
    ' =========================
    Private Sub HandleSchedLine(line As String)
        lblSchedStatus.Text = line

        Dim up = line.ToUpperInvariant()

        If up = "SCHED LIST BEGIN" Then
            schedules.Clear()
            lstSchedules.Items.Clear()
            Return
        End If

        If up = "SCHED LIST END" Then
            RefreshScheduleListView()
            Return
        End If

        If up.StartsWith("SCHED ITEM ") Then
            Dim args As String = line.Substring(10).Trim()
            Dim item As ScheduleItem = ParseScheduleItem(args)
            If item IsNot Nothing Then
                schedules(item.Id) = item
            End If
            Return
        End If
    End Sub

    Private Function ParseScheduleItem(args As String) As ScheduleItem
        Try
            Dim it As New ScheduleItem()
            it.Id = ToInt(GetTok(args, "ID"), 0)
            it.Enabled = (ToInt(GetTok(args, "EN"), 0) <> 0)
            it.Mode = Nz(GetTok(args, "MODE"), "DI").ToUpperInvariant()
            it.DaysMask = ToInt(GetTok(args, "DAYS"), 127)
            it.StartHHMM = Nz(GetTok(args, "START"), "00:00")
            it.EndHHMM = Nz(GetTok(args, "END"), "00:00")
            it.Recurring = (ToInt(GetTok(args, "RECUR"), 1) <> 0)
            it.TriggerDI = ToInt(GetTok(args, "DI"), 1)
            it.Edge = Nz(GetTok(args, "EDGE"), "BOTH").ToUpperInvariant()
            it.RelayMask = ToInt(GetTok(args, "RELMASK"), 0)
            it.RelayAction = Nz(GetTok(args, "RELACT"), "TOGGLE").ToUpperInvariant()
            it.Dac1mV = ToInt(GetTok(args, "DAC1MV"), 0)
            it.Dac2mV = ToInt(GetTok(args, "DAC2MV"), 0)
            it.Dac1R = ToInt(GetTok(args, "DAC1R"), 5)
            it.Dac2R = ToInt(GetTok(args, "DAC2R"), 5)
            it.BuzzEnable = (ToInt(GetTok(args, "BUZEN"), 0) <> 0)
            it.BuzzFreq = ToInt(GetTok(args, "BUZFREQ"), 2000)
            it.BuzzOn = ToInt(GetTok(args, "BUZON"), 200)
            it.BuzzOff = ToInt(GetTok(args, "BUZOFF"), 200)
            it.BuzzRep = ToInt(GetTok(args, "BUZREP"), 5)
            it.AnalogEnable = (ToInt(GetTok(args, "AEN"), 0) <> 0)
            it.AnalogType = Nz(GetTok(args, "ATYPE"), "VOLT").ToUpperInvariant()
            it.AnalogCh = ToInt(GetTok(args, "ACH"), 1)
            it.AnalogOp = Nz(GetTok(args, "AOP"), "ABOVE").ToUpperInvariant()
            it.AnalogV1 = ToInt(GetTok(args, "AV1"), 0)
            it.AnalogV2 = ToInt(GetTok(args, "AV2"), 0)
            it.AnalogHys = ToInt(GetTok(args, "AHYS"), 50)
            it.AnalogDbMs = ToInt(GetTok(args, "ADBMS"), 200)
            it.AnalogTol = ToInt(GetTok(args, "ATOL"), 50)
            If it.Id < 0 Then Return Nothing
            Return it
        Catch ex As Exception
            LogLine("SCHED parse error: " & ex.Message)
            Return Nothing
        End Try
    End Function

    Private Sub RefreshScheduleListView()
        lstSchedules.BeginUpdate()
        Try
            lstSchedules.Items.Clear()
            For Each kv In schedules.OrderBy(Function(k) k.Key)
                Dim s = kv.Value
                Dim daysStr = DaysMaskToShort(s.DaysMask)
                Dim windowStr = s.StartHHMM & "-" & s.EndHHMM
                Dim trigStr = If(s.Mode = "DI" OrElse s.Mode = "DI_RTC", $"DI{s.TriggerDI}/{s.Edge}", "RTC")
                Dim analogStr As String = "-"
                If s.AnalogEnable Then
                    analogStr = $"{s.AnalogType}{s.AnalogCh} {s.AnalogOp} {s.AnalogV1}"
                    If s.AnalogOp = "IN_RANGE" Then analogStr &= $"..{s.AnalogV2}"
                    analogStr &= $" H={s.AnalogHys} D={s.AnalogDbMs}"
                    If s.AnalogOp = "EQUAL" Then analogStr &= $" T={s.AnalogTol}"
                End If

                Dim actionsStr = $"REL({s.RelayAction}:{s.RelayMask}) DAC({s.Dac1mV},{s.Dac2mV}) BUZ({If(s.BuzzEnable, "Y", "N")}) AN({analogStr})"

                Dim li As New ListViewItem(s.Id.ToString())
                li.SubItems.Add(If(s.Enabled, "1", "0"))
                li.SubItems.Add(s.Mode)
                li.SubItems.Add(daysStr)
                li.SubItems.Add(windowStr)
                li.SubItems.Add(trigStr)
                li.SubItems.Add(actionsStr)
                li.Tag = s
                lstSchedules.Items.Add(li)
            Next
        Finally
            lstSchedules.EndUpdate()
        End Try
    End Sub

    Private Sub lstSchedules_SelectedIndexChanged(sender As Object, e As EventArgs)
        If lstSchedules.SelectedItems.Count = 0 Then Return
        Dim it = TryCast(lstSchedules.SelectedItems(0).Tag, ScheduleItem)
        If it Is Nothing Then Return
        LoadScheduleToEditor(it)
    End Sub

    Private Sub LoadScheduleToEditor(it As ScheduleItem)
        txtSchedId.Text = it.Id.ToString()
        chkSchedEnabled.Checked = it.Enabled
        cboSchedMode.SelectedItem = it.Mode

        SetDaysChecks(it.DaysMask)

        cboTrigDI.SelectedItem = it.TriggerDI.ToString()
        cboEdge.SelectedItem = it.Edge

        txtWinStart.Text = it.StartHHMM
        txtWinEnd.Text = it.EndHHMM
        chkRecurring.Checked = it.Recurring

        SetRelayChecks(it.RelayMask)
        cboRelayAction.SelectedItem = it.RelayAction

        txtSchedDac1Mv.Text = it.Dac1mV.ToString()
        txtSchedDac2Mv.Text = it.Dac2mV.ToString()
        cboDac1Range.SelectedItem = If(it.Dac1R = 10, "10V", "5V")
        cboDac2Range.SelectedItem = If(it.Dac2R = 10, "10V", "5V")

        chkBuzzEnable.Checked = it.BuzzEnable
        txtBuzzFreq.Text = it.BuzzFreq.ToString()
        txtBuzzOnMs.Text = it.BuzzOn.ToString()
        txtBuzzOffMs.Text = it.BuzzOff.ToString()
        txtBuzzRepeats.Text = it.BuzzRep.ToString()

        'populate new analog editor fields
        chkAnalogEnable.Checked = it.AnalogEnable
        cboAnalogType.SelectedItem = If(it.AnalogType = "CURR", "CURR", "VOLT")
        cboAnalogCh.SelectedItem = it.AnalogCh.ToString()
        cboAnalogOp.SelectedItem = it.AnalogOp
        txtAnalogV1.Text = it.AnalogV1.ToString()
        txtAnalogV2.Text = it.AnalogV2.ToString()
        txtAnalogHys.Text = it.AnalogHys.ToString()
        txtAnalogDb.Text = it.AnalogDbMs.ToString()
        txtAnalogTol.Text = it.AnalogTol.ToString()

    End Sub

    ' =========================
    ' JSON snapshot parsing
    ' =========================
    Private Sub UpdateFromJson(json As String)
        Try
            Dim diBits As Integer = ToInt(ExtractJsonNumber(json, "di"), 0)
            Dim relBits As Integer = ToInt(ExtractJsonNumber(json, "rel"), 0)

            chkDI1.Checked = (diBits And (1 << 0)) <> 0
            chkDI2.Checked = (diBits And (1 << 1)) <> 0
            chkDI3.Checked = (diBits And (1 << 2)) <> 0
            chkDI4.Checked = (diBits And (1 << 3)) <> 0
            chkDI5.Checked = (diBits And (1 << 4)) <> 0
            chkDI6.Checked = (diBits And (1 << 5)) <> 0
            chkDI7.Checked = (diBits And (1 << 6)) <> 0
            chkDI8.Checked = (diBits And (1 << 7)) <> 0

            chkRel1.Checked = ((relBits And (1 << 0)) = 0)
            chkRel2.Checked = ((relBits And (1 << 1)) = 0)
            chkRel3.Checked = ((relBits And (1 << 2)) = 0)
            chkRel4.Checked = ((relBits And (1 << 3)) = 0)
            chkRel5.Checked = ((relBits And (1 << 4)) = 0)
            chkRel6.Checked = ((relBits And (1 << 5)) = 0)

            Dim v1 As Double = GetArrayValue(json, "ai_v", 0)
            Dim v2 As Double = GetArrayValue(json, "ai_v", 1)
            Dim i1 As Double = GetArrayValue(json, "ai_i", 0)
            Dim i2 As Double = GetArrayValue(json, "ai_i", 1)

            txtV1.Text = v1.ToString("0.000")
            txtV2.Text = v2.ToString("0.000")
            txtI1.Text = i1.ToString("0.000")
            txtI2.Text = i2.ToString("0.000")

            Dim aiVrange As Integer = ToInt(ExtractJsonNumber(json, "ai_vrange"), 5)

            Dim vraw0 As Integer = CInt(GetArrayValue(json, "ai_v_raw", 0))
            Dim vraw1 As Integer = CInt(GetArrayValue(json, "ai_v_raw", 1))
            Dim iraw0 As Integer = CInt(GetArrayValue(json, "ai_i_raw", 0))
            Dim iraw1 As Integer = CInt(GetArrayValue(json, "ai_i_raw", 1))

            ' Decide what to display based on current editor selection
            Dim atype = cboAnalogType.SelectedItem.ToString().ToUpperInvariant()
            Dim ach As Integer = ToInt(cboAnalogCh.SelectedItem.ToString(), 1)
            Dim idx As Integer = If(ach <= 1, 0, 1)

            If atype = "VOLT" Then
                Dim v As Double = GetArrayValue(json, "ai_v", idx)
                Dim raw As Integer = If(idx = 0, vraw0, vraw1)
                lblAnalogLive.Text = $"Live VOLT{ach}: raw={raw}  v={v:0.000}V  range={aiVrange}V"
            Else
                Dim mA As Double = GetArrayValue(json, "ai_i", idx)
                Dim raw As Integer = If(idx = 0, iraw0, iraw1)
                lblAnalogLive.Text = $"Live CURR{ach}: raw={raw}  i={mA:0.000}mA"
            End If


            Dim mv0 As Integer = CInt(GetArrayValue(json, "dac_mv", 0))
            Dim mv1 As Integer = CInt(GetArrayValue(json, "dac_mv", 1))
            txtDAC1.Text = mv0.ToString()
            txtDAC2.Text = mv1.ToString()

            ' DHT array has objects with t/h possibly null; our naive parser will return NaN
            Dim t1 As Double = GetNestedNumber(json, "dht", 0, "t")
            Dim h1 As Double = GetNestedNumber(json, "dht", 0, "h")
            Dim t2 As Double = GetNestedNumber(json, "dht", 1, "t")
            Dim h2 As Double = GetNestedNumber(json, "dht", 1, "h")

            txtT1.Text = If(Double.IsNaN(t1), "NaN", t1.ToString("0.0"))
            txtH1.Text = If(Double.IsNaN(h1), "NaN", h1.ToString("0.0"))
            txtT2.Text = If(Double.IsNaN(t2), "NaN", t2.ToString("0.0"))
            txtH2.Text = If(Double.IsNaN(h2), "NaN", h2.ToString("0.0"))

            Dim dsCnt As Integer = ToInt(ExtractJsonNumber(json, "ds18_cnt"), 0)
            Dim ds0 As Double = ToDbl(ExtractJsonNumber(json, "ds18_0"), Double.NaN)
            txtDsCount.Text = dsCnt.ToString()
            txtDs0.Text = If(Double.IsNaN(ds0), "NaN", ds0.ToString("0.0"))

            Dim rtcStr As String = ExtractJsonString(json, "rtc")
            txtRtcCurrent.Text = rtcStr

            lblLastSnap.Text = "Last snapshot: " & DateTime.Now.ToString("HH:mm:ss")
        Catch ex As Exception
            LogLine("JSON parse error: " & ex.Message)
        End Try
    End Sub

    ' =========================
    ' JSON helper functions
    ' =========================
    Private Function ExtractJsonNumber(json As String, key As String) As String
        Dim k = """" & key & """"
        Dim idx = json.IndexOf(k, StringComparison.Ordinal)
        If idx < 0 Then Return ""
        idx = json.IndexOf(":", idx, StringComparison.Ordinal)
        If idx < 0 Then Return ""
        idx += 1
        While idx < json.Length AndAlso (json(idx) = " "c OrElse json(idx) = vbTab)
            idx += 1
        End While

        ' Allow "null"
        If idx + 3 < json.Length AndAlso json.Substring(idx, 4).ToLowerInvariant() = "null" Then
            Return ""
        End If

        Dim sb As New StringBuilder()
        While idx < json.Length AndAlso "-+0123456789.eE".IndexOf(json(idx)) >= 0
            sb.Append(json(idx))
            idx += 1
        End While
        Return sb.ToString()
    End Function

    Private Function ExtractJsonString(json As String, key As String) As String
        Dim k = """" & key & """"
        Dim idx = json.IndexOf(k, StringComparison.Ordinal)
        If idx < 0 Then Return ""
        idx = json.IndexOf(":", idx, StringComparison.Ordinal)
        If idx < 0 Then Return ""
        idx = json.IndexOf(""""c, idx)
        If idx < 0 Then Return ""
        idx += 1
        Dim endIdx = json.IndexOf(""""c, idx)
        If endIdx < 0 Then Return ""
        Return json.Substring(idx, endIdx - idx)
    End Function

    Private Function GetArrayValue(json As String, arrayKey As String, index As Integer) As Double
        Dim k = """" & arrayKey & """"
        Dim idx = json.IndexOf(k, StringComparison.Ordinal)
        If idx < 0 Then Return Double.NaN
        idx = json.IndexOf("[", idx, StringComparison.Ordinal)
        If idx < 0 Then Return Double.NaN
        idx += 1

        Dim currentIdx As Integer = 0
        While currentIdx < index AndAlso idx < json.Length
            If json(idx) = ","c Then currentIdx += 1
            idx += 1
        End While

        ' allow spaces
        While idx < json.Length AndAlso (json(idx) = " "c OrElse json(idx) = vbTab)
            idx += 1
        End While

        ' allow null
        If idx + 3 < json.Length AndAlso json.Substring(idx, 4).ToLowerInvariant() = "null" Then
            Return Double.NaN
        End If

        Dim sb As New StringBuilder()
        While idx < json.Length AndAlso "-+0123456789.eE".IndexOf(json(idx)) >= 0
            sb.Append(json(idx))
            idx += 1
        End While
        Return ToDbl(sb.ToString(), Double.NaN)
    End Function

    Private Function GetNestedNumber(json As String, arrayKey As String, index As Integer, innerKey As String) As Double
        Dim k = """" & arrayKey & """"
        Dim idx = json.IndexOf(k, StringComparison.Ordinal)
        If idx < 0 Then Return Double.NaN
        idx = json.IndexOf("[", idx, StringComparison.Ordinal)
        If idx < 0 Then Return Double.NaN
        idx += 1

        Dim objStart As Integer = -1
        Dim currentIdx As Integer = 0
        While idx < json.Length
            If json(idx) = "{"c Then
                If currentIdx = index Then
                    objStart = idx
                    Exit While
                Else
                    Dim depth As Integer = 1
                    idx += 1
                    While idx < json.Length AndAlso depth > 0
                        If json(idx) = "{"c Then depth += 1
                        If json(idx) = "}"c Then depth -= 1
                        idx += 1
                    End While
                    currentIdx += 1
                End If
            Else
                idx += 1
            End If
        End While
        If objStart < 0 Then Return Double.NaN

        Dim objEnd = json.IndexOf("}", objStart, StringComparison.Ordinal)
        If objEnd < 0 Then Return Double.NaN
        Dim subJson = json.Substring(objStart, objEnd - objStart + 1)

        Dim n = ExtractJsonNumber(subJson, innerKey)
        If String.IsNullOrEmpty(n) Then Return Double.NaN
        Return ToDbl(n, Double.NaN)
    End Function

    ' =========================
    ' Timers
    ' =========================
    Private Sub PollTimer_Tick(sender As Object, e As EventArgs)
        If chkAutoPoll.Checked AndAlso IsConnected() Then
            SendCommand("SNAPJSON")
        End If
    End Sub

    Private Sub ReconnectTimer_Tick(sender As Object, e As EventArgs)
        If _closing Then
            reconnectTimer.Stop()
            Return
        End If

        ' Avoid spamming connect attempts
        If (DateTime.Now - _lastReconnectAttempt).TotalMilliseconds < RECONNECT_MIN_GAP_MS Then
            Return
        End If

        ' If already connected, stop
        If IsConnected() Then
            CancelReconnectPlan()
            Return
        End If

        ' If a connect attempt in-flight, wait
        If _connecting Then Return

        If _reconnectPlan = ReconnectPlan.Idle Then
            ' Respect UI auto reconnect if user checked it
            If chkAutoReconnect.Checked Then
                _reconnectPlan = ReconnectPlan.Normal
            Else
                reconnectTimer.Stop()
                Return
            End If
        End If

        ' Plan timeouts
        If _reconnectPlan = ReconnectPlan.AfterReboot AndAlso DateTime.Now > _rebootReconnectUntil Then
            _reconnectPlan = ReconnectPlan.Normal
        End If

        If _reconnectPlan = ReconnectPlan.DhcpDiscover AndAlso DateTime.Now > _dhcpDiscoveryUntil Then
            _reconnectPlan = ReconnectPlan.Normal
        End If

        If _reconnectPlan = ReconnectPlan.Normal AndAlso Not chkAutoReconnect.Checked Then
            reconnectTimer.Stop()
            Return
        End If

        ' DHCP discovery: rotate candidate IPs (best-effort without scanning whole LAN)
        If _reconnectPlan = ReconnectPlan.DhcpDiscover Then
            RotateDhcpDiscoveryTarget()
        End If

        _lastReconnectAttempt = DateTime.Now
        ConnectToBoard()
    End Sub

    Private Sub RotateDhcpDiscoveryTarget()
        ' Try current txtIp first, then a small set of common link-local last octets.
        ' This does not guarantee discovery on routed networks, but works well with W5500 AutoIP fallback
        ' and with typical DHCP-unchanged scenarios.
        Dim current = txtIp.Text.Trim()
        Dim port = ToInt(txtPort.Text.Trim(), 5000)

        Dim candidates As New List(Of String)()

        If current.Length > 0 Then candidates.Add(current)

        ' add last known CLIENTIP if any
        Dim lastClientIp = Nz(lastDeviceCfg.ClientIp, "")
        If lastClientIp.Length > 0 AndAlso Not candidates.Contains(lastClientIp) Then
            candidates.Add(lastClientIp)
        End If

        ' small AutoIP set (common)
        candidates.AddRange(New String() {
            "169.254.1.1",
            "169.254.1.2",
            "169.254.85.149",
            "169.254.85.150",
            "169.254.100.1",
            "169.254.200.1"
        })

        If candidates.Count = 0 Then Return

        Dim idx As Integer = _dhcpProbeStep Mod candidates.Count
        _dhcpProbeStep += 1

        Dim nextIp = candidates(idx)
        If nextIp <> txtIp.Text.Trim() Then
            txtIp.Text = nextIp
            LogLine($"DHCP discover: trying {nextIp}:{port}")
        End If
    End Sub

    ' =========================
    ' UI event handlers – control commands
    ' =========================
    Private Sub RelayClick(sender As Object, e As EventArgs) _
        Handles chkRel1.Click, chkRel2.Click, chkRel3.Click,
                chkRel4.Click, chkRel5.Click, chkRel6.Click

        Dim chk = CType(sender, CheckBox)
        Dim idx As Integer = Integer.Parse(chk.Tag.ToString())
        Dim val As String = If(chk.Checked, "1", "0")
        SendCommand($"REL {idx} {val}")
    End Sub

    Private Sub btnDacSet_Click(sender As Object, e As EventArgs) Handles btnDacSet.Click
        Dim mv1, mv2 As Integer
        If Integer.TryParse(txtDAC1.Text.Trim(), mv1) Then
            SendCommand($"DACV 1 {mv1}")
        End If
        If Integer.TryParse(txtDAC2.Text.Trim(), mv2) Then
            SendCommand($"DACV 2 {mv2}")
        End If
    End Sub

    Private Sub btnBeep_Click(sender As Object, e As EventArgs) Handles btnBeep.Click
        Dim f, ms As Integer
        If Not Integer.TryParse(txtBeepFreq.Text.Trim(), f) Then f = 1000
        If Not Integer.TryParse(txtBeepMs.Text.Trim(), ms) Then ms = 200
        SendCommand($"BEEP {f} {ms}")
    End Sub

    Private Sub btnBuzzOff_Click(sender As Object, e As EventArgs) Handles btnBuzzOff.Click
        SendCommand("BUZZOFF")
    End Sub

    Private Sub btnRtcGet_Click(sender As Object, e As EventArgs) Handles btnRtcGet.Click
        SendCommand("RTCGET")
    End Sub

    Private Sub btnRtcSet_Click(sender As Object, e As EventArgs) Handles btnRtcSet.Click
        Dim iso = txtRtcSet.Text.Trim()
        If String.IsNullOrEmpty(iso) Then
            iso = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss")
            txtRtcSet.Text = iso
        End If
        SendCommand("RTCSET " & iso)
    End Sub

    Private Sub btnUnitApply_Click(sender As Object, e As EventArgs) Handles btnUnitApply.Click
        Dim u = cboTempUnit.SelectedItem.ToString()
        SendCommand("UNIT " & u)
    End Sub

    Private Sub btnPing_Click(sender As Object, e As EventArgs) Handles btnPing.Click
        SendCommand("PING")
    End Sub

    Private Sub btnSnapOnce_Click(sender As Object, e As EventArgs) Handles btnSnapOnce.Click
        SendCommand("SNAPJSON")
    End Sub

    Private Sub chkAutoReconnect_CheckedChanged(sender As Object, e As EventArgs) Handles chkAutoReconnect.CheckedChanged
        If chkAutoReconnect.Checked AndAlso Not IsConnected() AndAlso Not _closing Then
            _reconnectPlan = ReconnectPlan.Normal
            reconnectTimer.Start()
        Else
            If _reconnectPlan = ReconnectPlan.Normal Then
                reconnectTimer.Stop()
                _reconnectPlan = ReconnectPlan.Idle
            End If
        End If
    End Sub

    ' =========================
    ' Multi-schedule UI handlers
    ' =========================
    Private Sub btnSchedNew_Click(sender As Object, e As EventArgs)
        ClearScheduleEditor()
    End Sub

    Private Sub btnSchedAdd_Click(sender As Object, e As EventArgs)
        Dim it = CaptureEditorToSchedule()
        it.Id = 0
        SendCommand(BuildSchedUpsertCmd(it))
        SendCommand("SCHED LIST")
    End Sub

    Private Sub btnSchedUpdate_Click(sender As Object, e As EventArgs)
        Dim it = CaptureEditorToSchedule()
        If it.Id <= 0 Then
            MessageBox.Show("Select an existing schedule or enter a valid ID to update.", "Update", MessageBoxButtons.OK, MessageBoxIcon.Information)
            Return
        End If
        SendCommand(BuildSchedUpsertCmd(it))
        SendCommand("SCHED LIST")
    End Sub

    Private Sub btnSchedDelete_Click(sender As Object, e As EventArgs)
        Dim id As Integer = ToInt(txtSchedId.Text.Trim(), -1)
        If id <= 0 Then
            If lstSchedules.SelectedItems.Count > 0 Then
                Dim it = TryCast(lstSchedules.SelectedItems(0).Tag, ScheduleItem)
                If it IsNot Nothing Then id = it.Id
            End If
        End If
        If id <= 0 Then
            MessageBox.Show("Select an existing schedule to delete.", "Delete", MessageBoxButtons.OK, MessageBoxIcon.Information)
            Return
        End If
        SendCommand($"SCHED DEL ID={id}")
        SendCommand("SCHED LIST")
    End Sub

    Private Sub btnSchedApply_Click(sender As Object, e As EventArgs)
        SendCommand("SCHED LIST")
    End Sub

    Private Sub btnSchedDisable_Click(sender As Object, e As EventArgs)
        Dim id As Integer = -1
        If lstSchedules.SelectedItems.Count > 0 Then
            Dim it = TryCast(lstSchedules.SelectedItems(0).Tag, ScheduleItem)
            If it IsNot Nothing Then id = it.Id
        End If
        If id <= 0 Then
            id = ToInt(txtSchedId.Text.Trim(), -1)
        End If
        If id <= 0 Then
            MessageBox.Show("Select an existing schedule to disable.", "Disable", MessageBoxButtons.OK, MessageBoxIcon.Information)
            Return
        End If
        SendCommand($"SCHED DISABLE ID={id}")
        SendCommand("SCHED LIST")
    End Sub

    Private Sub btnSchedQuery_Click(sender As Object, e As EventArgs)
        SendCommand("SCHED LIST")
    End Sub

    Private Sub ClearScheduleEditor()
        txtSchedId.Text = "0"
        chkSchedEnabled.Checked = True

        cboSchedMode.SelectedItem = "DI"
        chkMon.Checked = True : chkTue.Checked = True : chkWed.Checked = True : chkThu.Checked = True : chkFri.Checked = True : chkSat.Checked = True : chkSun.Checked = True

        cboTrigDI.SelectedItem = "1"
        cboEdge.SelectedItem = "RISING"
        txtWinStart.Text = "08:00"
        txtWinEnd.Text = "17:00"
        chkRecurring.Checked = True

        chkSRel1.Checked = False : chkSRel2.Checked = False : chkSRel3.Checked = False : chkSRel4.Checked = False : chkSRel5.Checked = False : chkSRel6.Checked = False
        cboRelayAction.SelectedItem = "TOGGLE"

        txtSchedDac1Mv.Text = "0"
        txtSchedDac2Mv.Text = "0"
        cboDac1Range.SelectedItem = "5V"
        cboDac2Range.SelectedItem = "5V"

        chkBuzzEnable.Checked = False
        txtBuzzFreq.Text = "2000"
        txtBuzzOnMs.Text = "200"
        txtBuzzOffMs.Text = "200"
        txtBuzzRepeats.Text = "5"

        chkAnalogEnable.Checked = False
        cboAnalogType.SelectedItem = "VOLT"
        cboAnalogCh.SelectedItem = "1"
        cboAnalogOp.SelectedItem = "ABOVE"
        txtAnalogV1.Text = "0"
        txtAnalogV2.Text = "0"
        txtAnalogHys.Text = "50"
        txtAnalogDb.Text = "200"
        txtAnalogTol.Text = "50"
        txtAnalogTol.Visible = True
        LblTol.Visible = True
        txtAnalogV2.Enabled = True
    End Sub

    Private Function CaptureEditorToSchedule() As ScheduleItem
        Dim it As New ScheduleItem()

        it.Id = ToInt(txtSchedId.Text.Trim(), 0)
        it.Enabled = chkSchedEnabled.Checked

        it.Mode = cboSchedMode.SelectedItem.ToString()
        it.DaysMask = BuildDaysMask()

        it.TriggerDI = Integer.Parse(cboTrigDI.SelectedItem.ToString())
        it.Edge = cboEdge.SelectedItem.ToString()

        it.StartHHMM = txtWinStart.Text.Trim()
        it.EndHHMM = txtWinEnd.Text.Trim()
        it.Recurring = chkRecurring.Checked

        it.RelayMask = BuildRelayMask()
        it.RelayAction = cboRelayAction.SelectedItem.ToString()

        it.Dac1mV = ToInt(txtSchedDac1Mv.Text.Trim(), 0)
        it.Dac2mV = ToInt(txtSchedDac2Mv.Text.Trim(), 0)
        it.Dac1R = If(cboDac1Range.SelectedItem.ToString().ToUpperInvariant().Contains("10"), 10, 5)
        it.Dac2R = If(cboDac2Range.SelectedItem.ToString().ToUpperInvariant().Contains("10"), 10, 5)

        it.BuzzEnable = chkBuzzEnable.Checked
        it.BuzzFreq = ToInt(txtBuzzFreq.Text.Trim(), 2000)
        it.BuzzOn = ToInt(txtBuzzOnMs.Text.Trim(), 200)
        it.BuzzOff = ToInt(txtBuzzOffMs.Text.Trim(), 200)
        it.BuzzRep = ToInt(txtBuzzRepeats.Text.Trim(), 5)

        'capture analog trigger settings
        it.AnalogEnable = chkAnalogEnable.Checked
        it.AnalogType = cboAnalogType.SelectedItem.ToString().ToUpperInvariant()
        it.AnalogCh = ToInt(cboAnalogCh.SelectedItem.ToString(), 1)
        it.AnalogOp = cboAnalogOp.SelectedItem.ToString().ToUpperInvariant()
        it.AnalogV1 = ToInt(txtAnalogV1.Text.Trim(), 0)
        it.AnalogV2 = ToInt(txtAnalogV2.Text.Trim(), 0)
        it.AnalogHys = ToInt(txtAnalogHys.Text.Trim(), 50)
        it.AnalogDbMs = ToInt(txtAnalogDb.Text.Trim(), 200)
        it.AnalogTol = ToInt(txtAnalogTol.Text.Trim(), 50)

        Return it
    End Function

    Private Function BuildSchedUpsertCmd(it As ScheduleItem) As String
        Dim cmd = $"SCHED UPSERT ID={it.Id} EN={If(it.Enabled, 1, 0)} MODE={it.Mode} DAYS={it.DaysMask} " &
                  $"DI={it.TriggerDI} EDGE={it.Edge} START={it.StartHHMM} END={it.EndHHMM} RECUR={If(it.Recurring, 1, 0)} " &
                  $"RELMASK={it.RelayMask} RELACT={it.RelayAction} " &
                  $"DAC1MV={it.Dac1mV} DAC2MV={it.Dac2mV} DAC1R={it.Dac1R} DAC2R={it.Dac2R} " &
                  $"BUZEN={If(it.BuzzEnable, 1, 0)} BUZFREQ={it.BuzzFreq} BUZON={it.BuzzOn} BUZOFF={it.BuzzOff} BUZREP={it.BuzzRep}"
        cmd &= $" AEN={If(it.AnalogEnable, 1, 0)} ATYPE={it.AnalogType} ACH={it.AnalogCh} AOP={it.AnalogOp} AV1={it.AnalogV1} AV2={it.AnalogV2} AHYS={it.AnalogHys} ADBMS={it.AnalogDbMs} ATOL={it.AnalogTol}"
        Return cmd
    End Function

    Private Function BuildRelayMask() As Integer
        Dim mask As Integer = 0
        If chkSRel1.Checked Then mask = mask Or (1 << 0)
        If chkSRel2.Checked Then mask = mask Or (1 << 1)
        If chkSRel3.Checked Then mask = mask Or (1 << 2)
        If chkSRel4.Checked Then mask = mask Or (1 << 3)
        If chkSRel5.Checked Then mask = mask Or (1 << 4)
        If chkSRel6.Checked Then mask = mask Or (1 << 5)
        Return mask
    End Function

    Private Sub SetRelayChecks(mask As Integer)
        chkSRel1.Checked = ((mask And (1 << 0)) <> 0)
        chkSRel2.Checked = ((mask And (1 << 1)) <> 0)
        chkSRel3.Checked = ((mask And (1 << 2)) <> 0)
        chkSRel4.Checked = ((mask And (1 << 3)) <> 0)
        chkSRel5.Checked = ((mask And (1 << 4)) <> 0)
        chkSRel6.Checked = ((mask And (1 << 5)) <> 0)
    End Sub

    Private Function BuildDaysMask() As Integer
        Dim m As Integer = 0
        If chkMon.Checked Then m = m Or (1 << 0)
        If chkTue.Checked Then m = m Or (1 << 1)
        If chkWed.Checked Then m = m Or (1 << 2)
        If chkThu.Checked Then m = m Or (1 << 3)
        If chkFri.Checked Then m = m Or (1 << 4)
        If chkSat.Checked Then m = m Or (1 << 5)
        If chkSun.Checked Then m = m Or (1 << 6)
        Return m
    End Function

    Private Sub SetDaysChecks(daysMask As Integer)
        chkMon.Checked = ((daysMask And (1 << 0)) <> 0)
        chkTue.Checked = ((daysMask And (1 << 1)) <> 0)
        chkWed.Checked = ((daysMask And (1 << 2)) <> 0)
        chkThu.Checked = ((daysMask And (1 << 3)) <> 0)
        chkFri.Checked = ((daysMask And (1 << 4)) <> 0)
        chkSat.Checked = ((daysMask And (1 << 5)) <> 0)
        chkSun.Checked = ((daysMask And (1 << 6)) <> 0)
    End Sub

    Private Function DaysMaskToShort(daysMask As Integer) As String
        Dim parts As New List(Of String)()
        If (daysMask And (1 << 0)) <> 0 Then parts.Add("M")
        If (daysMask And (1 << 1)) <> 0 Then parts.Add("T")
        If (daysMask And (1 << 2)) <> 0 Then parts.Add("W")
        If (daysMask And (1 << 3)) <> 0 Then parts.Add("Th")
        If (daysMask And (1 << 4)) <> 0 Then parts.Add("F")
        If (daysMask And (1 << 5)) <> 0 Then parts.Add("Sa")
        If (daysMask And (1 << 6)) <> 0 Then parts.Add("Su")
        If parts.Count = 0 Then Return "-"
        Return String.Join(",", parts)
    End Function

    ' =========================
    ' Small helpers
    ' =========================
    Private Function GetTok(args As String, key As String) As String
        Dim k As String = key & "="
        Dim p As Integer = args.IndexOf(k, StringComparison.OrdinalIgnoreCase)
        If p < 0 Then Return ""
        Dim s As Integer = p + k.Length
        Dim e As Integer = args.IndexOf(" "c, s)
        If e < 0 Then e = args.Length
        Return args.Substring(s, e - s).Trim()
    End Function

    Private Function Nz(s As String, defVal As String) As String
        If s Is Nothing OrElse s.Trim().Length = 0 Then Return defVal
        Return s
    End Function

    Private Function ToInt(s As String, defVal As Integer) As Integer
        Dim v As Integer
        If Integer.TryParse(s, v) Then Return v
        Return defVal
    End Function

    Private Function ToDbl(s As String, defVal As Double) As Double
        Dim d As Double
        If Double.TryParse(s, NumberStyles.Any, CultureInfo.InvariantCulture, d) Then Return d
        Return defVal
    End Function

    ' =========================
    ' Logging
    ' =========================
    Private Sub LogLine(msg As String)
        If txtLog.InvokeRequired Then
            txtLog.Invoke(Sub() LogLine(msg))
            Return
        End If
        Dim line = $"[{DateTime.Now:HH:mm:ss}] {msg}"
        txtLog.AppendText(line & Environment.NewLine)
    End Sub

    Private Sub btnSerialApplyIpPort_Click(sender As Object, e As EventArgs) Handles btnSerialApplyIpPort.Click
        Try
            Dim raw As String = If(txtSerialLog.Text, "").Replace(vbCrLf, vbLf) & vbLf

            ' Strip ANSI escape sequences (firmware prints colored output)
            ' Pattern: ESC [ ... letter
            raw = System.Text.RegularExpressions.Regex.Replace(raw, "\x1B\[[0-9;]*[A-Za-z]", "")

            Dim ipNow As String = ""
            Dim tcpPort As String = ""

            ' Find "IP_NOW:" (preferred runtime for TCP connect)
            Dim mIp = System.Text.RegularExpressions.Regex.Match(raw, "(?im)^\s*IP_NOW\s*:\s*([0-9]{1,3}(?:\.[0-9]{1,3}){3})\s*$")
            If mIp.Success Then ipNow = mIp.Groups(1).Value.Trim()

            ' Find "TCPPORT:" from stored preferences (this is the TCP server port)
            Dim mPort = System.Text.RegularExpressions.Regex.Match(raw, "(?im)^\s*TCPPORT\s*:\s*(\d{1,5})\s*$")
            If mPort.Success Then tcpPort = mPort.Groups(1).Value.Trim()

            ' Basic validation
            Dim portVal As Integer = 0
            If tcpPort.Length > 0 Then Integer.TryParse(tcpPort, portVal)

            If String.IsNullOrWhiteSpace(ipNow) OrElse portVal < 1 OrElse portVal > 65535 Then
                MessageBox.Show("Could not extract valid IP_NOW and/or TCPPORT from Serial output." & Environment.NewLine &
                                "Make sure you clicked NET DUMP and the device printed the settings.", "Serial Apply",
                                MessageBoxButtons.OK, MessageBoxIcon.Warning)
                Return
            End If

            ' Apply to Dashboard connect fields (same fields used by TCP connect)
            txtIp.Text = ipNow
            txtPort.Text = portVal.ToString()

            ' Switch user back to Dashboard
            tabMain.SelectedTab = tabPageDashboard

            ' Close serial port after applying
            Try
                SerialCloseSafely()  ' <-- assumes your serial open/close implementation uses this helper
            Catch
                ' Fallback: if you didn't implement SerialCloseSafely(), try calling your close button handler
                Try
                    btnSerialDisconnect.PerformClick()
                Catch
                End Try
            End Try

            ' Feedback
            LogLine($"Serial applied -> IP={ipNow}, TCP Port={portVal}. Serial closed.")
        Catch ex As Exception
            MessageBox.Show("btnSerialApplyIpPort error: " & ex.Message, "Serial Apply",
                            MessageBoxButtons.OK, MessageBoxIcon.Error)
        End Try
    End Sub

    ' Close the SerialPort safely (no exceptions / no deadlocks)
    Private Sub SerialCloseSafely()
        Try
            ' If you are using a field like: Private serial As IO.Ports.SerialPort
            ' adjust the variable name below accordingly.
            If serial IsNot Nothing Then
                Try
                    If serial.IsOpen Then
                        ' Stop async callbacks from touching UI after close
                        Try
                            RemoveHandler serial.DataReceived, AddressOf Serial_DataReceived
                        Catch
                            ' ignore if handler not attached
                        End Try

                        Try : serial.DiscardInBuffer() : Catch : End Try
                        Try : serial.DiscardOutBuffer() : Catch : End Try

                        serial.Close()
                    End If
                Finally
                    serial.Dispose()
                    serial = Nothing
                End Try
            End If
        Catch ex As Exception
            ' Never throw from "close" helper; just log
            Try
                LogLine("Serial close error: " & ex.Message)
            Catch
            End Try
        End Try

        ' Update UI if the serial controls exist
        Try
            If Me.IsHandleCreated AndAlso Not Me.IsDisposed Then
                If Me.InvokeRequired Then
                    Me.BeginInvoke(Sub() SerialCloseSafely_UpdateUi())
                Else
                    SerialCloseSafely_UpdateUi()
                End If
            End If
        Catch
        End Try
    End Sub

    Private Sub SerialCloseSafely_UpdateUi()
        ' If your UI names differ, adjust accordingly.
        If lblSerialStatus IsNot Nothing Then
            lblSerialStatus.Text = "Serial: Closed"
            lblSerialStatus.ForeColor = Color.FromArgb(120, 60, 0)
        End If

        If btnSerialConnect IsNot Nothing Then btnSerialConnect.Enabled = True
        If btnSerialDisconnect IsNot Nothing Then btnSerialDisconnect.Enabled = False
        If btnSerialNetDump IsNot Nothing Then btnSerialNetDump.Enabled = False
        If btnSerialApplyIpPort IsNot Nothing Then btnSerialApplyIpPort.Enabled = False
    End Sub

    Private Sub chkAnalogEnable_CheckedChanged(sender As Object, e As EventArgs) Handles chkAnalogEnable.CheckedChanged

    End Sub
End Class


' ... keep your existing FormMain code ...
