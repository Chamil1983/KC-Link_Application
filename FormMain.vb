Imports System.Net.Sockets
Imports System.Text
Imports System.Globalization
Imports System.Windows.Forms
Imports System.Drawing

' NEW: Serial support
Imports System.IO.Ports

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
        Public Property Mode As String ' DI / RTC / COMBINED
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
        txtPort.Text = "8080"

        cboTempUnit.Items.AddRange(New Object() {"C", "F"})
        cboTempUnit.SelectedIndex = 0

        ' Schedules UI
        cboSchedMode.Items.AddRange(New Object() {"DI", "RTC", "COMBINED"})
        cboSchedMode.SelectedIndex = 0

        cboTrigDI.Items.AddRange(New Object() {"1", "2", "3", "4", "5", "6", "7", "8"})
        cboTrigDI.SelectedIndex = 0

        cboEdge.Items.AddRange(New Object() {"RISING", "FALLING", "BOTH"})
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
                Dim trigStr = If(s.Mode = "DI" OrElse s.Mode = "COMBINED", $"DI{s.TriggerDI}/{s.Edge}", "RTC")
                Dim actionsStr = $"REL({s.RelayAction}:{s.RelayMask}) DAC({s.Dac1mV},{s.Dac2mV}) BUZ({If(s.BuzzEnable, "Y", "N")})"
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

        Return it
    End Function

    Private Function BuildSchedUpsertCmd(it As ScheduleItem) As String
        Dim cmd = $"SCHED UPSERT ID={it.Id} EN={If(it.Enabled, 1, 0)} MODE={it.Mode} DAYS={it.DaysMask} " &
                  $"DI={it.TriggerDI} EDGE={it.Edge} START={it.StartHHMM} END={it.EndHHMM} RECUR={If(it.Recurring, 1, 0)} " &
                  $"RELMASK={it.RelayMask} RELACT={it.RelayAction} " &
                  $"DAC1MV={it.Dac1mV} DAC2MV={it.Dac2mV} DAC1R={it.Dac1R} DAC2R={it.Dac2R} " &
                  $"BUZEN={If(it.BuzzEnable, 1, 0)} BUZFREQ={it.BuzzFreq} BUZON={it.BuzzOn} BUZOFF={it.BuzzOff} BUZREP={it.BuzzRep}"
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


End Class