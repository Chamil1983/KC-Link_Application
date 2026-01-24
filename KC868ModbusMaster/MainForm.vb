Option Strict On
Option Explicit On

Imports System
Imports System.Drawing
Imports System.ComponentModel
Imports System.Globalization
Imports System.IO
Imports System.IO.Ports
Imports System.Linq
Imports System.Text
Imports System.Threading
Imports System.Threading.Tasks
Imports System.Windows.Forms

' Resolve Timer ambiguity between System.Threading.Timer and System.Windows.Forms.Timer
Imports FormsTimer = System.Windows.Forms.Timer

Namespace KC868ModbusMaster
    Partial Public Class MainForm

        Private ReadOnly _client As New ModbusClient()
        Private _pollTimer As FormsTimer
        Private _lastHeartbeat As UShort = 0US
        Private _lastHeartbeatTick As DateTime = DateTime.MinValue
        Private _isPolling As Boolean = False
        Private _suppressWrites As Boolean = False
        Private chkAutoRefreshGrid As CheckBox
        Private lblPcClock As Label
        Private _pcClockTimer As FormsTimer
        Private _gridPollCount As Integer = 0

        ' ---- BACnet/IP ----
        Private ReadOnly _bacnet As New BacnetClientManager()
        Private tabBacnet As TabPage
        Private grpBacConn As GroupBox
        Private txtBacDeviceId As TextBox
        Private txtBacTargetIp As TextBox
        Private numBacLocalUdp As NumericUpDown
        Private btnBacStart As Button
        Private btnBacDiscover As Button
        Private btnBacDisconnect As Button
        Private pnlBacLed As Panel
        Private lblBacState As Label
        Private chkBacAutoRefresh As CheckBox
        Private numBacRefreshMs As NumericUpDown
        Private gridBac As DataGridView
        Private _bacTimer As FormsTimer
        ' BACnet polling is performed on a background Task to avoid UI freezes.
        Private _bacPollBusy As Integer = 0
        Private _bacPollCts As CancellationTokenSource = Nothing
        Private _bacRows As BindingList(Of BacnetRow)
        Private chkBacDo() As CheckBox
        Private pnlBacDi() As Panel

        ' BACnet object instance base:
        ' Some embedded stacks expose BI/BO/AI instances starting at 0 (0..15) instead of 1 (1..16).
        ' We auto-detect this at runtime so the UI can work with either model.
        Private _bacInstanceBase As Integer = 1
        Private _bacBaseDetected As Boolean = False

        ' ---- Threading / IO sync ----
        Private ReadOnly _ioSync As New Object()

        ' ---- Register write progress (Write Selected) ----
        Private prgRegOp As ProgressBar
        Private lblRegOpStatus As Label

        ' ---- Safe Maintenance UI (Option B) ----
        Private grpSafe As GroupBox
        Private btnCmdReboot As Button
        Private btnCmdSaveConfig As Button
        Private btnCmdReloadConfig As Button
        Private btnCmdFactoryDefaults As Button
        Private btnCmdClearLogs As Button
        Private btnCmdBackupSave As Button
        Private btnCmdBackupRestore As Button
        Private txtCmdArg0 As TextBox
        Private txtCmdArg1 As TextBox
        Private lblCmdStatus As Label
        Private prgCmd As ProgressBar


        Private _rows As BindingList(Of RegisterRow)

        Private ReadOnly Property SlaveId As Byte
            Get
                Dim v As Integer
                If Not Integer.TryParse(txtSlaveId.Text.Trim(), v) Then v = 1
                If v < 1 Then v = 1
                If v > 247 Then v = 247
                Return CByte(v)
            End Get
        End Property

        Public Sub New()
            InitializeComponent()
            EnsureUiBuilt()
            InitUiDefaults()
        End Sub

        Private Sub EnsureUiBuilt()
            ' The shipped Designer.vb is intentionally minimal; build UI at runtime so controls exist.
            If cboBaud IsNot Nothing Then Return
            BuildUi()
        End Sub

        Private Sub BuildUi()
            Me.Text = "KC868 A16 Modbus RTU Master"
            Me.MinimumSize = New Drawing.Size(980, 640)

            tabMain = New TabControl() With {.Dock = DockStyle.Fill}
            tabDashboard = New TabPage("Dashboard")
            tabModbus = New TabPage("MODBUS")
            tabBacnet = New TabPage("BACnet")
            tabMain.TabPages.Add(tabDashboard)
            tabMain.TabPages.Add(tabModbus)
            tabMain.TabPages.Add(tabBacnet)
            Me.Controls.Clear()
            Me.Controls.Add(tabMain)

            ' ---------- Dashboard layout ----------
            Dim dashRoot As New TableLayoutPanel() With {.Dock = DockStyle.Fill, .ColumnCount = 1, .RowCount = 2}
            dashRoot.RowStyles.Add(New RowStyle(SizeType.AutoSize))
            dashRoot.RowStyles.Add(New RowStyle(SizeType.Percent, 100.0F))
            tabDashboard.Controls.Add(dashRoot)

            grpConnection = New GroupBox() With {.Text = "Connection", .Dock = DockStyle.Top, .AutoSize = True}
            dashRoot.Controls.Add(grpConnection, 0, 0)
            Dim connLayout As New TableLayoutPanel() With {.Dock = DockStyle.Fill, .ColumnCount = 10, .RowCount = 2, .AutoSize = True}
            For i As Integer = 0 To 9
                connLayout.ColumnStyles.Add(New ColumnStyle(SizeType.AutoSize))
            Next
            connLayout.RowStyles.Add(New RowStyle(SizeType.AutoSize))
            connLayout.RowStyles.Add(New RowStyle(SizeType.AutoSize))
            grpConnection.Controls.Add(connLayout)

            Dim lblPort As New Label() With {.Text = "COM:", .AutoSize = True, .Anchor = AnchorStyles.Left}
            cboPort = New ComboBox() With {.DropDownStyle = ComboBoxStyle.DropDownList, .Width = 110}
            Dim lblBaud As New Label() With {.Text = "Baud:", .AutoSize = True, .Anchor = AnchorStyles.Left}
            cboBaud = New ComboBox() With {.DropDownStyle = ComboBoxStyle.DropDownList, .Width = 90}
            Dim lblDb As New Label() With {.Text = "Data:", .AutoSize = True, .Anchor = AnchorStyles.Left}
            cboDataBits = New ComboBox() With {.DropDownStyle = ComboBoxStyle.DropDownList, .Width = 60}
            Dim lblPar As New Label() With {.Text = "Parity:", .AutoSize = True, .Anchor = AnchorStyles.Left}
            cboParity = New ComboBox() With {.DropDownStyle = ComboBoxStyle.DropDownList, .Width = 80}
            Dim lblSb As New Label() With {.Text = "Stop:", .AutoSize = True, .Anchor = AnchorStyles.Left}
            cboStopBits = New ComboBox() With {.DropDownStyle = ComboBoxStyle.DropDownList, .Width = 60}
            Dim lblSid As New Label() With {.Text = "Slave:", .AutoSize = True, .Anchor = AnchorStyles.Left}
            txtSlaveId = New TextBox() With {.Text = "1", .Width = 40}

            btnConnect = New Button() With {.Text = "Connect", .Width = 90}
            btnDisconnect = New Button() With {.Text = "Disconnect", .Width = 90, .Enabled = False}
            pnlConnLed = New Panel() With {.Width = 16, .Height = 16, .BackColor = Drawing.Color.DarkRed, .Margin = New Padding(6, 6, 6, 6)}
            lblConnState = New Label() With {.Text = "DISCONNECTED", .AutoSize = True, .Anchor = AnchorStyles.Left}

            chkAutoPoll = New CheckBox() With {.Text = "Auto Poll", .AutoSize = True}
            lblPoll = New Label() With {.Text = "Poll (ms):", .AutoSize = True, .Anchor = AnchorStyles.Left}
            numPollMs = New NumericUpDown() With {.Minimum = 100, .Maximum = 10000, .Increment = 100, .Value = 500, .Width = 80}

            connLayout.Controls.Add(lblPort, 0, 0) : connLayout.Controls.Add(cboPort, 1, 0)
            connLayout.Controls.Add(lblBaud, 2, 0) : connLayout.Controls.Add(cboBaud, 3, 0)
            connLayout.Controls.Add(lblDb, 4, 0) : connLayout.Controls.Add(cboDataBits, 5, 0)
            connLayout.Controls.Add(lblPar, 6, 0) : connLayout.Controls.Add(cboParity, 7, 0)
            connLayout.Controls.Add(lblSb, 8, 0) : connLayout.Controls.Add(cboStopBits, 9, 0)

            connLayout.Controls.Add(lblSid, 0, 1) : connLayout.Controls.Add(txtSlaveId, 1, 1)
            connLayout.Controls.Add(btnConnect, 2, 1) : connLayout.Controls.Add(btnDisconnect, 3, 1)
            connLayout.Controls.Add(pnlConnLed, 4, 1) : connLayout.Controls.Add(lblConnState, 5, 1)
            connLayout.Controls.Add(chkAutoPoll, 6, 1) : connLayout.Controls.Add(lblPoll, 7, 1) : connLayout.Controls.Add(numPollMs, 8, 1)

            Dim dashGrid As New TableLayoutPanel() With {.Dock = DockStyle.Fill, .ColumnCount = 2, .RowCount = 3}
            dashGrid.ColumnStyles.Add(New ColumnStyle(SizeType.Percent, 50.0F))
            dashGrid.ColumnStyles.Add(New ColumnStyle(SizeType.Percent, 50.0F))
            dashGrid.RowStyles.Add(New RowStyle(SizeType.Percent, 40.0F))
            dashGrid.RowStyles.Add(New RowStyle(SizeType.Percent, 30.0F))
            dashGrid.RowStyles.Add(New RowStyle(SizeType.Percent, 30.0F))
            dashRoot.Controls.Add(dashGrid, 0, 1)

            grpInputs = New GroupBox() With {.Text = "Digital Inputs", .Dock = DockStyle.Fill}
            grpOutputs = New GroupBox() With {.Text = "Digital Outputs", .Dock = DockStyle.Fill}
            grpAnalog = New GroupBox() With {.Text = "Analog Inputs", .Dock = DockStyle.Fill}
            grpSensors = New GroupBox() With {.Text = "Sensors", .Dock = DockStyle.Fill}
            grpRtc = New GroupBox() With {.Text = "RTC / Status", .Dock = DockStyle.Fill}

            dashGrid.Controls.Add(grpInputs, 0, 0)
            dashGrid.Controls.Add(grpOutputs, 1, 0)
            dashGrid.Controls.Add(grpAnalog, 0, 1)
            dashGrid.Controls.Add(grpSensors, 1, 1)
            dashGrid.Controls.Add(grpRtc, 0, 2)
            dashGrid.SetColumnSpan(grpRtc, 2)
            ' Inputs (16 main + 3 direct) - clean aligned table layout
            ReDim pnlDi(15) : ReDim lblDi(15)
            ReDim pnlDirect(2) : ReDim lblDirect(2)

            Dim inGrid As New TableLayoutPanel() With {
                .Dock = DockStyle.Fill,
                .AutoScroll = True,
                .ColumnCount = 4,
                .RowCount = 6,
                .Padding = New Padding(8)
            }
            For c As Integer = 0 To 3
                inGrid.ColumnStyles.Add(New ColumnStyle(SizeType.Percent, 25.0F))
            Next
            For r As Integer = 0 To 5
                inGrid.RowStyles.Add(New RowStyle(SizeType.AutoSize))
            Next
            grpInputs.Controls.Add(inGrid)

            ' Main inputs (4 columns x 4 rows)
            For i As Integer = 0 To 15
                Dim led As Panel = Nothing
                Dim lab As Label = Nothing
                Dim item As Control = CreateLedItem($"IN{i + 1}", led, lab)
                pnlDi(i) = led : lblDi(i) = lab
                inGrid.Controls.Add(item, i Mod 4, i \ 4)
            Next

            ' Direct inputs (header + row)
            Dim hdr As New Label() With {
                .Text = "Direct Inputs",
                .AutoSize = True,
                .Font = New Font(Me.Font, FontStyle.Bold),
                .Margin = New Padding(4, 10, 4, 4)
            }
            inGrid.Controls.Add(hdr, 0, 4)
            inGrid.SetColumnSpan(hdr, 4)

            For i As Integer = 0 To 2
                Dim led As Panel = Nothing
                Dim lab As Label = Nothing
                Dim item As Control = CreateLedItem($"D{i + 1}", led, lab)
                pnlDirect(i) = led : lblDirect(i) = lab
                inGrid.Controls.Add(item, i, 5)
            Next
            ' Outputs - clean aligned table layout
            ReDim chkDo(15)

            Dim outRoot As New TableLayoutPanel() With {.Dock = DockStyle.Fill, .ColumnCount = 1, .RowCount = 2, .Padding = New Padding(8)}
            outRoot.RowStyles.Add(New RowStyle(SizeType.AutoSize))
            outRoot.RowStyles.Add(New RowStyle(SizeType.Percent, 100.0F))
            grpOutputs.Controls.Add(outRoot)

            Dim optRow As New TableLayoutPanel() With {.Dock = DockStyle.Top, .AutoSize = True, .ColumnCount = 2}
            optRow.ColumnStyles.Add(New ColumnStyle(SizeType.Percent, 50.0F))
            optRow.ColumnStyles.Add(New ColumnStyle(SizeType.Percent, 50.0F))
            chkMasterEnable = New CheckBox() With {.Text = "Master Enable", .AutoSize = True, .Margin = New Padding(3, 3, 3, 6)}
            chkNightMode = New CheckBox() With {.Text = "Night Mode", .AutoSize = True, .Margin = New Padding(3, 3, 3, 6)}
            optRow.Controls.Add(chkMasterEnable, 0, 0)
            optRow.Controls.Add(chkNightMode, 1, 0)
            outRoot.Controls.Add(optRow, 0, 0)

            Dim outGrid As New TableLayoutPanel() With {.Dock = DockStyle.Fill, .AutoScroll = True, .ColumnCount = 4, .RowCount = 4}
            For c As Integer = 0 To 3
                outGrid.ColumnStyles.Add(New ColumnStyle(SizeType.Percent, 25.0F))
            Next
            For r As Integer = 0 To 3
                outGrid.RowStyles.Add(New RowStyle(SizeType.AutoSize))
            Next
            outRoot.Controls.Add(outGrid, 0, 1)

            For i As Integer = 0 To 15
                Dim c As New CheckBox() With {.Text = $"CH{i + 1}", .AutoSize = True, .Margin = New Padding(6, 6, 6, 6)}
                chkDo(i) = c
                outGrid.Controls.Add(c, i Mod 4, i \ 4)
            Next

            ' Analog
            ReDim lblAiRaw(3) : ReDim lblAiMv(3)
            Dim anTbl As New TableLayoutPanel() With {.Dock = DockStyle.Fill, .ColumnCount = 3, .RowCount = 5, .Padding = New Padding(10)}
            anTbl.ColumnStyles.Add(New ColumnStyle(SizeType.AutoSize))
            anTbl.ColumnStyles.Add(New ColumnStyle(SizeType.Percent, 50.0F))
            anTbl.ColumnStyles.Add(New ColumnStyle(SizeType.Percent, 50.0F))
            anTbl.RowStyles.Add(New RowStyle(SizeType.AutoSize))
            For i As Integer = 1 To 4 : anTbl.RowStyles.Add(New RowStyle(SizeType.AutoSize)) : Next

            Dim fBold As New Font(Me.Font, FontStyle.Bold)
            anTbl.Controls.Add(New Label() With {.Text = "CH", .AutoSize = True, .Font = fBold, .Margin = New Padding(4, 6, 14, 6)}, 0, 0)
            anTbl.Controls.Add(New Label() With {.Text = "RAW", .AutoSize = True, .Font = fBold, .Margin = New Padding(4, 6, 14, 6)}, 1, 0)
            anTbl.Controls.Add(New Label() With {.Text = "mV", .AutoSize = True, .Font = fBold, .Margin = New Padding(4, 6, 14, 6)}, 2, 0)

            For i As Integer = 0 To 3
                anTbl.Controls.Add(New Label() With {.Text = (i + 1).ToString(), .AutoSize = True, .Margin = New Padding(4, 8, 14, 8)}, 0, i + 1)
                Dim lr As New Label() With {.Text = "-", .AutoSize = True, .Margin = New Padding(4, 8, 14, 8)} : lblAiRaw(i) = lr
                Dim lm As New Label() With {.Text = "-", .AutoSize = True, .Margin = New Padding(4, 8, 14, 8)} : lblAiMv(i) = lm
                anTbl.Controls.Add(lr, 1, i + 1)
                anTbl.Controls.Add(lm, 2, i + 1)
            Next
            grpAnalog.Controls.Add(anTbl)
            ' Sensors (spaced table layout)
            Dim sensTbl As New TableLayoutPanel() With {.Dock = DockStyle.Fill, .ColumnCount = 2, .RowCount = 3, .Padding = New Padding(10)}
            sensTbl.ColumnStyles.Add(New ColumnStyle(SizeType.AutoSize))
            sensTbl.ColumnStyles.Add(New ColumnStyle(SizeType.Percent, 100.0F))
            For i As Integer = 0 To 2 : sensTbl.RowStyles.Add(New RowStyle(SizeType.AutoSize)) : Next

            sensTbl.Controls.Add(New Label() With {.Text = "DHT1", .AutoSize = True, .Font = New Font(Me.Font, FontStyle.Bold), .Margin = New Padding(4, 8, 14, 8)}, 0, 0)
            lblDht1 = New Label() With {.Text = "--", .AutoSize = True, .Margin = New Padding(4, 8, 14, 8)}
            sensTbl.Controls.Add(lblDht1, 1, 0)

            sensTbl.Controls.Add(New Label() With {.Text = "DHT2", .AutoSize = True, .Font = New Font(Me.Font, FontStyle.Bold), .Margin = New Padding(4, 8, 14, 8)}, 0, 1)
            lblDht2 = New Label() With {.Text = "--", .AutoSize = True, .Margin = New Padding(4, 8, 14, 8)}
            sensTbl.Controls.Add(lblDht2, 1, 1)

            sensTbl.Controls.Add(New Label() With {.Text = "DS18B20", .AutoSize = True, .Font = New Font(Me.Font, FontStyle.Bold), .Margin = New Padding(4, 8, 14, 8)}, 0, 2)
            lblDs18 = New Label() With {.Text = "--", .AutoSize = True, .Margin = New Padding(4, 8, 14, 8)}
            sensTbl.Controls.Add(lblDs18, 1, 2)

            grpSensors.Controls.Add(sensTbl)

            ' RTC + status
            Dim rtcTbl As New TableLayoutPanel() With {.Dock = DockStyle.Fill, .ColumnCount = 6, .RowCount = 5}
            For i As Integer = 0 To 5 : rtcTbl.ColumnStyles.Add(New ColumnStyle(SizeType.Percent, 16.6F)) : Next
            For i As Integer = 0 To 4 : rtcTbl.RowStyles.Add(New RowStyle(SizeType.AutoSize)) : Next

            lblPcClock = New Label() With {.Text = "PC: --", .AutoSize = True}
            lblRtc = New Label() With {.Text = "RTC: --", .AutoSize = True}
            lblHeartbeat = New Label() With {.Text = "Heartbeat: --", .AutoSize = True}
            lblChangeSeq = New Label() With {.Text = "ChangeSeq: --", .AutoSize = True}

            pnlEthLed = New Panel() With {.Width = 14, .Height = 14, .BackColor = Drawing.Color.DarkRed}
            pnlWifiLed = New Panel() With {.Width = 14, .Height = 14, .BackColor = Drawing.Color.DarkRed}
            pnlApLed = New Panel() With {.Width = 14, .Height = 14, .BackColor = Drawing.Color.DarkRed}
            pnlRestartLed = New Panel() With {.Width = 14, .Height = 14, .BackColor = Drawing.Color.DarkRed}
            pnlModbusLed = New Panel() With {.Width = 14, .Height = 14, .BackColor = Drawing.Color.DarkRed}

            rtcTbl.Controls.Add(lblPcClock, 0, 0) : rtcTbl.SetColumnSpan(lblPcClock, 6)
            rtcTbl.Controls.Add(lblRtc, 0, 1) : rtcTbl.SetColumnSpan(lblRtc, 6)

            rtcTbl.Controls.Add(lblHeartbeat, 0, 2) : rtcTbl.SetColumnSpan(lblHeartbeat, 3)
            rtcTbl.Controls.Add(lblChangeSeq, 3, 2) : rtcTbl.SetColumnSpan(lblChangeSeq, 3)

            rtcTbl.Controls.Add(New Label() With {.Text = "ETH:", .AutoSize = True}, 0, 3) : rtcTbl.Controls.Add(pnlEthLed, 1, 3)
            rtcTbl.Controls.Add(New Label() With {.Text = "WiFi:", .AutoSize = True}, 2, 3) : rtcTbl.Controls.Add(pnlWifiLed, 3, 3)
            rtcTbl.Controls.Add(New Label() With {.Text = "AP:", .AutoSize = True}, 4, 3) : rtcTbl.Controls.Add(pnlApLed, 5, 3)

            rtcTbl.Controls.Add(New Label() With {.Text = "RST:", .AutoSize = True}, 0, 4) : rtcTbl.Controls.Add(pnlRestartLed, 1, 4)
            rtcTbl.Controls.Add(New Label() With {.Text = "MODBUS:", .AutoSize = True}, 2, 4) : rtcTbl.Controls.Add(pnlModbusLed, 3, 4)

            grpRtc.Controls.Add(rtcTbl)

            ' ---------- BACnet tab ----------
            BuildBacnetTab()

            ' PC clock timer (updates even when Modbus is disconnected)
            _pcClockTimer = New FormsTimer() With {.Interval = 1000}
            AddHandler _pcClockTimer.Tick, AddressOf PcClockTimer_Tick
            _pcClockTimer.Start()
            PcClockTimer_Tick(Nothing, EventArgs.Empty)


            ' ---------- MODBUS tab layout ----------
            Dim modSplit As New SplitContainer() With {.Dock = DockStyle.Fill, .Orientation = Orientation.Horizontal, .SplitterWidth = 6}
            InitSplitContainerSafe(modSplit, 320, 240, 180)

            tabModbus.Controls.Add(modSplit)

            Dim modTop As New TableLayoutPanel() With {.Dock = DockStyle.Fill, .ColumnCount = 1, .RowCount = 3, .Padding = New Padding(6)}
            modTop.ColumnStyles.Add(New ColumnStyle(SizeType.Percent, 100.0F))
            modTop.RowStyles.Add(New RowStyle(SizeType.AutoSize))   ' title
            modTop.RowStyles.Add(New RowStyle(SizeType.AutoSize))   ' toolbar
            modTop.RowStyles.Add(New RowStyle(SizeType.Percent, 100.0F)) ' grid
            modSplit.Panel1.Controls.Add(modTop)

            Dim lblGridTitle As New Label() With {.Text = "MODBUS Register Table", .AutoSize = True, .Font = New Font(Me.Font, FontStyle.Bold), .Margin = New Padding(3, 0, 3, 6)}
            modTop.Controls.Add(lblGridTitle, 0, 0)

            Dim btnBar As New FlowLayoutPanel() With {.Dock = DockStyle.Fill, .AutoSize = True, .WrapContents = True, .Margin = New Padding(0, 0, 0, 6)}
            btnRefreshRegs = New Button() With {.Text = "Refresh", .Width = 90}
            btnWriteSelected = New Button() With {.Text = "Write Selected", .Width = 110}
            btnExportCsv = New Button() With {.Text = "Export CSV", .Width = 100}
            btnBar.Controls.Add(btnRefreshRegs) : btnBar.Controls.Add(btnWriteSelected) : btnBar.Controls.Add(btnExportCsv)

            ' Write Selected progress
            lblRegOpStatus = New Label() With {.Text = "", .AutoSize = True, .Margin = New Padding(12, 10, 0, 0)}
            prgRegOp = New ProgressBar() With {.Width = 160, .Height = 18, .Margin = New Padding(6, 10, 0, 0), .Minimum = 0, .Maximum = 100, .Value = 0}
            btnBar.Controls.Add(lblRegOpStatus)
            btnBar.Controls.Add(prgRegOp)

            chkAutoRefreshGrid = New CheckBox() With {.Text = "Auto refresh grid", .AutoSize = True, .Checked = True, .Margin = New Padding(20, 8, 0, 0)}
            btnBar.Controls.Add(chkAutoRefreshGrid)
            modTop.Controls.Add(btnBar, 0, 1)

            Dim gridHost As New Panel() With {.Dock = DockStyle.Fill, .BorderStyle = BorderStyle.FixedSingle, .Padding = New Padding(2), .Margin = New Padding(0)}
            modTop.Controls.Add(gridHost, 0, 2)

            gridRegs = New DataGridView() With {.Dock = DockStyle.Fill, .AllowUserToAddRows = False, .AllowUserToDeleteRows = False}
            gridRegs.AutoGenerateColumns = False
            gridRegs.RowHeadersVisible = False
            gridRegs.ColumnHeadersVisible = True
            gridRegs.AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill
            gridRegs.ColumnHeadersHeight = 32
            gridRegs.ColumnHeadersHeightSizeMode = DataGridViewColumnHeadersHeightSizeMode.DisableResizing
            gridRegs.AutoSizeRowsMode = DataGridViewAutoSizeRowsMode.AllCells
            gridRegs.AllowUserToResizeRows = False
            gridRegs.AllowUserToResizeColumns = True
            gridRegs.SelectionMode = DataGridViewSelectionMode.FullRowSelect
            gridRegs.MultiSelect = True
            gridRegs.BorderStyle = BorderStyle.None
            gridRegs.BackgroundColor = SystemColors.Window

            gridHost.Controls.Add(gridRegs)

            gridRegs.Columns.Clear()
            gridRegs.Columns.Add(New DataGridViewTextBoxColumn() With {.Name = "Table", .HeaderText = "Table", .DataPropertyName = "Table", .ReadOnly = True, .Width = 90})
            gridRegs.Columns.Add(New DataGridViewTextBoxColumn() With {.Name = "Ref", .HeaderText = "Ref", .DataPropertyName = "Ref", .ReadOnly = True, .Width = 70})
            gridRegs.Columns.Add(New DataGridViewTextBoxColumn() With {.Name = "Offset", .HeaderText = "Offset", .DataPropertyName = "Offset", .ReadOnly = True, .Width = 60})
            gridRegs.Columns.Add(New DataGridViewTextBoxColumn() With {.Name = "Access", .HeaderText = "R/W", .DataPropertyName = "Access", .ReadOnly = True, .Width = 50})
            gridRegs.Columns.Add(New DataGridViewTextBoxColumn() With {.Name = "Type", .HeaderText = "Type", .DataPropertyName = "DataType", .ReadOnly = True, .Width = 70})
            gridRegs.Columns.Add(New DataGridViewTextBoxColumn() With {.Name = "Count", .HeaderText = "Count", .DataPropertyName = "Count", .ReadOnly = True, .Width = 55})
            gridRegs.Columns.Add(New DataGridViewTextBoxColumn() With {.Name = "Units", .HeaderText = "Units", .DataPropertyName = "Units", .ReadOnly = True, .Width = 60})
            gridRegs.Columns.Add(New DataGridViewTextBoxColumn() With {.Name = "Description", .HeaderText = "Description", .DataPropertyName = "Description", .ReadOnly = True, .AutoSizeMode = DataGridViewAutoSizeColumnMode.Fill})
            gridRegs.Columns.Add(New DataGridViewTextBoxColumn() With {.Name = "Value", .HeaderText = "Value", .DataPropertyName = "Value", .ReadOnly = False, .Width = 140})

            AddHandler gridRegs.CellBeginEdit, AddressOf GridRegs_CellBeginEdit


            Dim botRoot As New SplitContainer() With {.Dock = DockStyle.Fill, .Orientation = Orientation.Vertical, .SplitterWidth = 6}
                        InitSplitContainerSafe(botRoot, 430, 260, 220)
modSplit.Panel2.Controls.Add(botRoot)

            ' Left side: Safe Maintenance + Manual Read/Write (stacked)
            ' Use a TableLayoutPanel instead of a nested SplitContainer to avoid overlap/clipping issues
            Dim leftStack As New TableLayoutPanel() With {.Dock = DockStyle.Fill, .ColumnCount = 1, .RowCount = 2, .Padding = New Padding(0)}
            leftStack.ColumnStyles.Add(New ColumnStyle(SizeType.Percent, 100.0F))
            leftStack.RowStyles.Add(New RowStyle(SizeType.Percent, 58.0F))
            leftStack.RowStyles.Add(New RowStyle(SizeType.Percent, 42.0F))
            botRoot.Panel1.Controls.Add(leftStack)

            ' ---- Safe Maintenance Group ----
            grpSafe = New GroupBox() With {.Text = "Safe Maintenance (HR40610..HR40617)", .Dock = DockStyle.Fill, .Padding = New Padding(8)}
            grpSafe.MinimumSize = New Size(0, 160)
            leftStack.Controls.Add(grpSafe, 0, 0)

            btnCmdReboot = New Button() With {.Text = "Reboot"}
            btnCmdSaveConfig = New Button() With {.Text = "Save Config"}
            btnCmdReloadConfig = New Button() With {.Text = "Reload Config"}
            btnCmdFactoryDefaults = New Button() With {.Text = "Factory Defaults"}
            btnCmdClearLogs = New Button() With {.Text = "Clear Logs"}
            btnCmdBackupSave = New Button() With {.Text = "Backup Save"}
            btnCmdBackupRestore = New Button() With {.Text = "Backup Restore"}

            txtCmdArg0 = New TextBox() With {.Text = "0", .Width = 70}
            txtCmdArg1 = New TextBox() With {.Text = "0", .Width = 70}

            lblCmdStatus = New Label() With {.Text = "Status: (idle)", .AutoSize = True}
            prgCmd = New ProgressBar() With {.Style = ProgressBarStyle.Continuous, .Value = 0, .Minimum = 0, .Maximum = 100, .Dock = DockStyle.Fill}

            ' Root layout inside Safe group
            Dim safeRoot As New TableLayoutPanel() With {.Dock = DockStyle.Fill, .ColumnCount = 1, .RowCount = 3, .Padding = New Padding(0)}
            safeRoot.ColumnStyles.Add(New ColumnStyle(SizeType.Percent, 100.0F))
            safeRoot.RowStyles.Add(New RowStyle(SizeType.AutoSize))  ' buttons grid
            safeRoot.RowStyles.Add(New RowStyle(SizeType.AutoSize))  ' args
            safeRoot.RowStyles.Add(New RowStyle(SizeType.Percent, 100.0F)) ' status+progress
            grpSafe.Controls.Add(safeRoot)

            ' Button grid (equal size buttons, 4 columns x 2 rows)
            Dim btnGrid As New TableLayoutPanel() With {.Dock = DockStyle.Top, .ColumnCount = 4, .RowCount = 2, .AutoSize = True, .Margin = New Padding(0)}
            For i As Integer = 0 To 3
                btnGrid.ColumnStyles.Add(New ColumnStyle(SizeType.Percent, 25.0F))
            Next
            btnGrid.RowStyles.Add(New RowStyle(SizeType.AutoSize))
            btnGrid.RowStyles.Add(New RowStyle(SizeType.AutoSize))

            Dim allBtns() As Button = {btnCmdReboot, btnCmdSaveConfig, btnCmdReloadConfig, btnCmdFactoryDefaults, btnCmdClearLogs, btnCmdBackupSave, btnCmdBackupRestore}
            For Each b As Button In allBtns
                b.Dock = DockStyle.Fill
                b.Height = 28
                b.Margin = New Padding(3)
            Next

            btnGrid.Controls.Add(btnCmdReboot, 0, 0)
            btnGrid.Controls.Add(btnCmdSaveConfig, 1, 0)
            btnGrid.Controls.Add(btnCmdReloadConfig, 2, 0)
            btnGrid.Controls.Add(btnCmdFactoryDefaults, 3, 0)

            btnGrid.Controls.Add(btnCmdClearLogs, 0, 1)
            btnGrid.Controls.Add(btnCmdBackupSave, 1, 1)
            btnGrid.Controls.Add(btnCmdBackupRestore, 2, 1)
            ' last cell (3,1) kept empty for clean alignment

            safeRoot.Controls.Add(btnGrid, 0, 0)

            ' Arguments row
            Dim argsGrid As New TableLayoutPanel() With {.Dock = DockStyle.Top, .ColumnCount = 6, .AutoSize = True, .Margin = New Padding(0, 6, 0, 0)}
            argsGrid.ColumnStyles.Add(New ColumnStyle(SizeType.AutoSize))
            argsGrid.ColumnStyles.Add(New ColumnStyle(SizeType.Absolute, 80.0F))
            argsGrid.ColumnStyles.Add(New ColumnStyle(SizeType.AutoSize))
            argsGrid.ColumnStyles.Add(New ColumnStyle(SizeType.Absolute, 80.0F))
            argsGrid.ColumnStyles.Add(New ColumnStyle(SizeType.AutoSize))
            argsGrid.ColumnStyles.Add(New ColumnStyle(SizeType.Percent, 100.0F))

            argsGrid.Controls.Add(New Label() With {.Text = "ARG0:", .AutoSize = True, .Margin = New Padding(0, 6, 6, 0)}, 0, 0)
            argsGrid.Controls.Add(txtCmdArg0, 1, 0)
            argsGrid.Controls.Add(New Label() With {.Text = "ARG1:", .AutoSize = True, .Margin = New Padding(12, 6, 6, 0)}, 2, 0)
            argsGrid.Controls.Add(txtCmdArg1, 3, 0)
            argsGrid.Controls.Add(New Label() With {.Text = "(optional)", .AutoSize = True, .Margin = New Padding(12, 6, 0, 0)}, 4, 0)

            safeRoot.Controls.Add(argsGrid, 0, 1)

            ' Status + Progress (kept visible, no overlap)
            Dim statGrid As New TableLayoutPanel() With {.Dock = DockStyle.Fill, .ColumnCount = 2, .RowCount = 1, .Margin = New Padding(0, 6, 0, 0)}
            statGrid.ColumnStyles.Add(New ColumnStyle(SizeType.AutoSize))
            statGrid.ColumnStyles.Add(New ColumnStyle(SizeType.Percent, 100.0F))
            statGrid.Controls.Add(lblCmdStatus, 0, 0)
            statGrid.Controls.Add(prgCmd, 1, 0)
            safeRoot.Controls.Add(statGrid, 0, 2)

            ' ---- Manual Read/Write Group ----
            grpManual = New GroupBox() With {.Text = "Manual Read/Write", .Dock = DockStyle.Fill, .Padding = New Padding(8)}
            grpManual.MinimumSize = New Size(0, 120)
            leftStack.Controls.Add(grpManual, 0, 1)

            cboManualTable = New ComboBox() With {.DropDownStyle = ComboBoxStyle.DropDownList, .Dock = DockStyle.Fill}
            txtManualAddr = New TextBox() With {.Text = "0", .Dock = DockStyle.Fill}
            txtManualCount = New TextBox() With {.Text = "1", .Dock = DockStyle.Fill}
            txtManualWrite = New TextBox() With {.Text = "0", .Dock = DockStyle.Fill}
            btnManualRead = New Button() With {.Text = "Read", .Width = 90, .Height = 28}
            btnManualWrite = New Button() With {.Text = "Write", .Width = 90, .Height = 28}

            Dim manRoot As New TableLayoutPanel() With {.Dock = DockStyle.Fill, .ColumnCount = 4, .RowCount = 3, .Padding = New Padding(0)}
            manRoot.ColumnStyles.Add(New ColumnStyle(SizeType.AutoSize))
            manRoot.ColumnStyles.Add(New ColumnStyle(SizeType.Percent, 50.0F))
            manRoot.ColumnStyles.Add(New ColumnStyle(SizeType.AutoSize))
            manRoot.ColumnStyles.Add(New ColumnStyle(SizeType.Percent, 50.0F))
            manRoot.RowStyles.Add(New RowStyle(SizeType.AutoSize))
            manRoot.RowStyles.Add(New RowStyle(SizeType.AutoSize))
            manRoot.RowStyles.Add(New RowStyle(SizeType.AutoSize))
            grpManual.Controls.Add(manRoot)

            manRoot.Controls.Add(New Label() With {.Text = "Table", .AutoSize = True, .Margin = New Padding(0, 6, 6, 0)}, 0, 0)
            manRoot.Controls.Add(cboManualTable, 1, 0)
            manRoot.Controls.Add(New Label() With {.Text = "Address", .AutoSize = True, .Margin = New Padding(12, 6, 6, 0)}, 2, 0)
            manRoot.Controls.Add(txtManualAddr, 3, 0)

            manRoot.Controls.Add(New Label() With {.Text = "Count", .AutoSize = True, .Margin = New Padding(0, 6, 6, 0)}, 0, 1)
            manRoot.Controls.Add(txtManualCount, 1, 1)
            manRoot.Controls.Add(New Label() With {.Text = "Write value", .AutoSize = True, .Margin = New Padding(12, 6, 6, 0)}, 2, 1)
            manRoot.Controls.Add(txtManualWrite, 3, 1)

            Dim manBtns As New FlowLayoutPanel() With {.Dock = DockStyle.Fill, .AutoSize = True, .WrapContents = False, .FlowDirection = FlowDirection.LeftToRight, .Margin = New Padding(0, 8, 0, 0), .Padding = New Padding(0)}
            btnManualRead.Margin = New Padding(0, 0, 10, 0)
            btnManualWrite.Margin = New Padding(0, 0, 0, 0)
            manBtns.Controls.Add(btnManualRead)
            manBtns.Controls.Add(btnManualWrite)
            manRoot.Controls.Add(manBtns, 0, 2)
            manRoot.SetColumnSpan(manBtns, 4)

            ' Right side: log
            txtLog = New TextBox() With {.Multiline = True, .ScrollBars = ScrollBars.Vertical, .Dock = DockStyle.Fill, .ReadOnly = True, .WordWrap = False}
            botRoot.Panel2.Controls.Add(txtLog)
        End Sub

        Private Sub GridRegs_CellBeginEdit(sender As Object, e As DataGridViewCellCancelEventArgs)
            If gridRegs Is Nothing Then Return
            If e.RowIndex < 0 Then Return
            If gridRegs.Columns(e.ColumnIndex).Name <> "Value" Then
                e.Cancel = True
                Return
            End If
            Dim rr As RegisterRow = TryCast(gridRegs.Rows(e.RowIndex).DataBoundItem, RegisterRow)
            If rr Is Nothing OrElse rr.Def Is Nothing OrElse Not rr.Def.IsWritable Then
                e.Cancel = True
            End If
        End Sub


        Private Sub InitUiDefaults()
            ' Connection defaults
            cboBaud.Items.AddRange(New Object() {"38400", "9600", "19200", "57600", "115200"})
            cboBaud.SelectedItem = "38400"
            cboDataBits.Items.AddRange(New Object() {"8", "7"})
            cboDataBits.SelectedItem = "8"
            cboParity.Items.AddRange(New Object() {"None", "Odd", "Even"})
            cboParity.SelectedItem = "None"
            cboStopBits.Items.AddRange(New Object() {"1", "2"})
            cboStopBits.SelectedItem = "1"

            cboManualTable.Items.AddRange(New Object() {"Coils", "DiscreteInputs", "InputRegisters", "HoldingRegisters"})
            If (cboManualTable.Items.Count > 2) Then cboManualTable.SelectedIndex = 2
            'ElseIf cboManualTable.Items.Count > 0 Then cboManualTable.SelectedIndex = 0

            ' Grid
            _rows = New BindingList(Of RegisterRow)(RegisterMap.Map.Select(Function(d) New RegisterRow(d)).ToList())
            gridRegs.DataSource = _rows

            ' Poll timer
            _pollTimer = New FormsTimer()
            _pollTimer.Interval = CInt(numPollMs.Value)
            AddHandler _pollTimer.Tick, AddressOf PollTimer_Tick

            AddHandler numPollMs.ValueChanged, Sub() _pollTimer.Interval = CInt(numPollMs.Value)

            ' Wire output checkbox events
            For i As Integer = 0 To chkDo.Length - 1
                Dim idx As Integer = i
                AddHandler chkDo(i).CheckedChanged, Sub(sender, e) DoOutputChanged(idx)
            Next
            AddHandler chkMasterEnable.CheckedChanged, Sub(sender, e) DoMasterEnableChanged()
            AddHandler chkNightMode.CheckedChanged, Sub(sender, e) DoNightModeChanged()

            AddHandler btnConnect.Click, AddressOf BtnConnect_Click
            AddHandler btnDisconnect.Click, AddressOf BtnDisconnect_Click
            AddHandler chkAutoPoll.CheckedChanged, AddressOf ChkAutoPoll_CheckedChanged

            AddHandler btnRefreshRegs.Click, AddressOf BtnRefreshRegs_Click
            AddHandler btnWriteSelected.Click, AddressOf BtnWriteSelected_Click
            AddHandler btnExportCsv.Click, AddressOf BtnExportCsv_Click

            AddHandler btnManualRead.Click, AddressOf BtnManualRead_Click
            AddHandler btnManualWrite.Click, AddressOf BtnManualWrite_Click

            ' Safe Maintenance (Option B)
            If btnCmdReboot IsNot Nothing Then AddHandler btnCmdReboot.Click, Async Sub(sender, e) Await ExecuteSafeCommandAsync(1US, "REBOOT")
            If btnCmdSaveConfig IsNot Nothing Then AddHandler btnCmdSaveConfig.Click, Async Sub(sender, e) Await ExecuteSafeCommandAsync(2US, "SAVE_CONFIG")
            If btnCmdReloadConfig IsNot Nothing Then AddHandler btnCmdReloadConfig.Click, Async Sub(sender, e) Await ExecuteSafeCommandAsync(3US, "RELOAD_CONFIG")
            If btnCmdFactoryDefaults IsNot Nothing Then AddHandler btnCmdFactoryDefaults.Click, Async Sub(sender, e) Await ExecuteSafeCommandAsync(4US, "FACTORY_DEFAULTS")
            If btnCmdClearLogs IsNot Nothing Then AddHandler btnCmdClearLogs.Click, Async Sub(sender, e) Await ExecuteSafeCommandAsync(5US, "CLEAR_LOGS")
            If btnCmdBackupSave IsNot Nothing Then AddHandler btnCmdBackupSave.Click, Async Sub(sender, e) Await ExecuteSafeCommandAsync(6US, "CONFIG_BACKUP_SAVE")
            If btnCmdBackupRestore IsNot Nothing Then AddHandler btnCmdBackupRestore.Click, Async Sub(sender, e) Await ExecuteSafeCommandAsync(7US, "CONFIG_BACKUP_RESTORE")

            AddHandler Me.Load, AddressOf MainForm_Load
            AddHandler Me.FormClosing, AddressOf MainForm_Closing
        End Sub

        Private Sub MainForm_Load(sender As Object, e As EventArgs)
            RefreshComPorts()
        End Sub

        Private Sub MainForm_Closing(sender As Object, e As FormClosingEventArgs)
            Try
                _pollTimer.Stop()
            Catch
            End Try
            _client.Disconnect()
        End Sub

        Private Sub RefreshComPorts()
            Dim ports = SerialPort.GetPortNames().OrderBy(Function(p) p).ToArray()
            cboPort.Items.Clear()
            cboPort.Items.AddRange(ports)
            If ports.Length > 0 Then cboPort.SelectedIndex = 0
        End Sub

        Private Sub SetConnLed(connected As Boolean)
            pnlConnLed.BackColor = If(connected, Drawing.Color.LimeGreen, Drawing.Color.DarkRed)
            lblConnState.Text = If(connected, "CONNECTED", "DISCONNECTED")
        End Sub

        Private Sub SetLed(p As Panel, onState As Boolean)
            p.BackColor = If(onState, Drawing.Color.LimeGreen, Drawing.Color.DarkRed)
        End Sub

        Private Function CreateLedItem(caption As String, ByRef ledPanel As Panel, ByRef textLabel As Label) As Control
            Dim host As New Panel() With {.Dock = DockStyle.Fill, .Height = 28, .Margin = New Padding(4)}
            Dim fl As New FlowLayoutPanel() With {.Dock = DockStyle.Fill, .FlowDirection = FlowDirection.LeftToRight, .WrapContents = False, .AutoSize = True}
            ledPanel = New Panel() With {.Width = 14, .Height = 14, .BackColor = Drawing.Color.DarkRed, .Margin = New Padding(0, 6, 8, 0)}
            textLabel = New Label() With {.Text = caption, .AutoSize = True, .Margin = New Padding(0, 5, 0, 0)}
            fl.Controls.Add(ledPanel)
            fl.Controls.Add(textLabel)
            host.Controls.Add(fl)
            Return host
        End Function



        Private Sub PcClockTimer_Tick(sender As Object, e As EventArgs)
            If lblPcClock Is Nothing Then Return
            Dim now As DateTime = DateTime.Now
            lblPcClock.Text = "PC: " & now.ToString("yyyy-MM-dd  HH:mm:ss", CultureInfo.InvariantCulture)
        End Sub

        Private Sub AppendLog(msg As String)
            If txtLog Is Nothing OrElse txtLog.IsDisposed Then Return

            If txtLog.InvokeRequired Then
                txtLog.BeginInvoke(New Action(Of String)(AddressOf AppendLog), msg)
                Return
            End If

            txtLog.AppendText($"[{DateTime.Now:HH:mm:ss}] {msg}{Environment.NewLine}")
        End Sub


        Private Function GetParity() As Parity
            Select Case cboParity.SelectedItem?.ToString()
                Case "Odd" : Return Parity.Odd
                Case "Even" : Return Parity.Even
                Case Else : Return Parity.None
            End Select
        End Function

        Private Function GetStopBits() As StopBits
            Return If(cboStopBits.SelectedItem?.ToString() = "2", StopBits.Two, StopBits.One)
        End Function

        Private Async Sub BtnConnect_Click(sender As Object, e As EventArgs)
            Try
                If cboPort.SelectedItem Is Nothing Then
                    MessageBox.Show("Select a COM port.")
                    Return
                End If

                Dim baud As Integer = Integer.Parse(cboBaud.SelectedItem.ToString(), CultureInfo.InvariantCulture)
                Dim db As Integer = Integer.Parse(cboDataBits.SelectedItem.ToString(), CultureInfo.InvariantCulture)

                _client.Connect(cboPort.SelectedItem.ToString(), baud, db, GetParity(), GetStopBits())

                btnConnect.Enabled = False
                btnDisconnect.Enabled = True
                SetConnLed(True)
                AppendLog("Connected (port opened).")

                ' Quick handshake: read a small Input Register block (map version + heartbeat)
                ' This avoids an immediate Holding Register read timeout on systems where Modbus is not yet active.
                Try
                    Dim sid As Byte = SlaveId
                    Dim ir = Await Task.Run(Function() _client.ReadInputRegisters(sid, 0US, 3US))
                    AppendLog($"Handshake OK. MapVer=0x{ir(0):X4} Heartbeat={ir(2)}")
                Catch exHandshake As Exception
                    AppendLog("Handshake failed: " & exHandshake.Message)
                End Try

                ' Initial refresh of settings/identity (handled/awaited so exceptions are observed)
                Await RefreshRegistersAsync()

            Catch ex As Exception
                SetConnLed(False)
                AppendLog("Connect failed: " & ex.Message)
                MessageBox.Show(ex.Message, "Connect failed")
                Try
                    _client.Disconnect()
                Catch
                End Try
                btnConnect.Enabled = True
                btnDisconnect.Enabled = False
            End Try
        End Sub

        Private Sub BtnDisconnect_Click(sender As Object, e As EventArgs)
            _pollTimer.Stop()
            chkAutoPoll.Checked = False
            _client.Disconnect()
            btnConnect.Enabled = True
            btnDisconnect.Enabled = False
            SetConnLed(False)
            AppendLog("Disconnected.")
        End Sub

        Private Sub ChkAutoPoll_CheckedChanged(sender As Object, e As EventArgs)
            If chkAutoPoll.Checked Then
                If Not _client.IsConnected Then
                    chkAutoPoll.Checked = False
                    MessageBox.Show("Connect first.")
                    Return
                End If
                _pollTimer.Interval = CInt(numPollMs.Value)
                _pollTimer.Start()
                AppendLog($"Auto poll ON ({_pollTimer.Interval} ms).")
            Else
                _pollTimer.Stop()
                AppendLog("Auto poll OFF.")
            End If
        End Sub

        Private Async Sub PollTimer_Tick(sender As Object, e As EventArgs)
            If _isPolling Then Return
            _isPolling = True
            Dim reconnect As Boolean = False

            Try
                Await PollOnceAsync()
            Catch ex As Exception
                AppendLog("Poll error: " & ex.Message)
                ' auto reconnect
                Try
                    _client.Disconnect()
                Catch
                End Try
                SetConnLed(False)
                reconnect = chkAutoPoll.Checked
            Finally
                _isPolling = False
            End Try

            If reconnect Then
                Await Task.Delay(250)
                Try
                    BtnConnect_Click(Nothing, EventArgs.Empty)
                    If _client.IsConnected Then SetConnLed(True)
                Catch
                End Try
            End If
        End Sub

        Private Async Function PollOnceAsync() As Task
            Dim wantGrid As Boolean = (chkAutoRefreshGrid IsNot Nothing AndAlso chkAutoRefreshGrid.Checked)
            Dim readHold As Boolean = False
            If wantGrid Then
                _gridPollCount += 1
                readHold = (_gridPollCount Mod 5) = 0
            End If
            Await Task.Run(Sub()
                               Dim sid As Byte = SlaveId

                               ' Fast blocks
                               Dim coils = _client.ReadCoils(sid, 0US, 20US)
                               Dim di = _client.ReadInputs(sid, 0US, 24US)
                               Dim ir = _client.ReadInputRegisters(sid, 0US, 43US)

                               ' Optional: Holding registers for live grid refresh (read less often)
                               Dim hr0 As UShort() = Array.Empty(Of UShort)()
                               Dim hrMac As UShort() = Array.Empty(Of UShort)()
                               Dim hrComm As UShort() = Array.Empty(Of UShort)()
                               Dim hrHw As UShort() = Array.Empty(Of UShort)()
                               Dim hrCmd As UShort() = Array.Empty(Of UShort)()
                               If wantGrid AndAlso readHold Then
                                   Try : hr0 = _client.ReadHoldingRegisters(sid, 0US, 90US) : Catch : End Try
                                   Try : hrMac = _client.ReadHoldingRegisters(sid, 100US, 24US) : Catch : End Try
                                   Try : hrComm = _client.ReadHoldingRegisters(sid, 200US, 14US) : Catch : End Try
                                   Try : hrHw = _client.ReadHoldingRegisters(sid, 300US, 26US) : Catch : End Try
                                   Try : hrCmd = _client.ReadHoldingRegisters(sid, 609US, 8US) : Catch : End Try
                               End If

                               ' Update UI
                               Me.BeginInvoke(Sub()
                                                  ' Outputs
                                                  _suppressWrites = True
                                                  For i As Integer = 0 To 15
                                                      chkDo(i).Checked = coils(i)
                                                  Next
                                                  chkMasterEnable.Checked = coils(18)
                                                  chkNightMode.Checked = coils(19)
                                                  _suppressWrites = False

                                                  ' Inputs LEDs (active LOW shown green -> TRUE is active/low)
                                                  For i As Integer = 0 To 15
                                                      SetLed(pnlDi(i), di(i))
                                                  Next
                                                  For i As Integer = 0 To 2
                                                      SetLed(pnlDirect(i), di(16 + i))
                                                  Next

                                                  ' Analog
                                                  For ch As Integer = 0 To 3
                                                      Dim raw As UShort = ir(12 + ch)
                                                      Dim mv As UShort = ir(16 + ch)
                                                      lblAiRaw(ch).Text = raw.ToString(CultureInfo.InvariantCulture)
                                                      lblAiMv(ch).Text = mv.ToString(CultureInfo.InvariantCulture)
                                                  Next

                                                  ' Sensors
                                                  Dim dht1t As Short = CType(ir(20), Short)
                                                  Dim dht1h As UShort = ir(21)
                                                  Dim dht2t As Short = CType(ir(22), Short)
                                                  Dim dht2h As UShort = ir(23)
                                                  Dim ds18t As Short = CType(ir(24), Short)

                                                  lblDht1.Text = $"{dht1t / 10.0:F1}°C  {dht1h / 10.0:F1}%RH"
                                                  lblDht2.Text = $"{dht2t / 10.0:F1}°C  {dht2h / 10.0:F1}%RH"
                                                  lblDs18.Text = $"{ds18t / 10.0:F1}°C"

                                                  ' RTC + status
                                                  Dim now As DateTime = DateTime.Now
                                                  Dim pcText As String = now.ToString("yyyy-MM-dd  HH:mm:ss", CultureInfo.InvariantCulture)
                                                  If lblPcClock IsNot Nothing Then lblPcClock.Text = "PC: " & pcText

                                                  Dim deviceRtcValid As Boolean = False
                                                  Dim y As UShort = 0US
                                                  Dim mo As UShort = 0US
                                                  Dim d As UShort = 0US
                                                  Dim hh As UShort = 0US
                                                  Dim mm As UShort = 0US
                                                  Dim ss As UShort = 0US

                                                  If ir IsNot Nothing AndAlso ir.Length >= 34 Then
                                                      y = ir(28) : mo = ir(29) : d = ir(30) : hh = ir(31) : mm = ir(32) : ss = ir(33)
                                                      deviceRtcValid = (y > 0US AndAlso mo >= 1US AndAlso mo <= 12US AndAlso d >= 1US AndAlso d <= 31US)
                                                  End If

                                                  If deviceRtcValid Then
                                                      lblRtc.Text = "RTC: " & $"{y:0000}-{mo:00}-{d:00}  {hh:00}:{mm:00}:{ss:00}"
                                                  Else
                                                      lblRtc.Text = "RTC: " & pcText & " (PC)"
                                                  End If

                                                  Dim hb As UShort = ir(2)
                                                  Dim seq As UShort = ir(5)
                                                  lblHeartbeat.Text = $"Heartbeat: {hb}"
                                                  lblChangeSeq.Text = $"ChangeSeq: {seq}"

                                                  Dim sysFlags As UShort = ir(42)
                                                  SetLed(pnlEthLed, (sysFlags And (1US << 0)) <> 0)
                                                  SetLed(pnlWifiLed, (sysFlags And (1US << 1)) <> 0)
                                                  SetLed(pnlApLed, (sysFlags And (1US << 2)) <> 0)
                                                  SetLed(pnlRestartLed, (sysFlags And (1US << 4)) <> 0)
                                                  SetLed(pnlModbusLed, (sysFlags And (1US << 5)) <> 0)
                                                  ' Optional: live refresh of the register grid during polling
                                                  If wantGrid AndAlso _rows IsNot Nothing Then
                                                      For Each r In _rows
                                                          Select Case r.Def.Table
                                                              Case ModbusTable.Coils
                                                                  Dim addr As Integer = r.Def.Offset
                                                                  If addr >= 0 AndAlso addr < coils.Length Then r.Value = If(coils(addr), "1", "0")
                                                              Case ModbusTable.DiscreteInputs
                                                                  Dim addr As Integer = r.Def.Offset
                                                                  If addr >= 0 AndAlso addr < di.Length Then r.Value = If(di(addr), "1", "0")
                                                              Case ModbusTable.InputRegisters
                                                                  If r.Def IsNot Nothing AndAlso IsRtcInputOffset(r.Def.Offset) AndAlso (ir Is Nothing OrElse ir.Length < 34 OrElse ir(28) = 0US) Then
                                                                      r.Value = PcRtcValue(r.Def, DateTime.Now)
                                                                  Else
                                                                      r.Value = DecodeValue(r.Def, ir, 0)
                                                                  End If
                                                              Case ModbusTable.HoldingRegisters
                                                                  If readHold AndAlso hr0.Length > 0 Then r.Value = DecodeHoldingValue(r.Def, hr0, hrMac, hrComm, hrHw, hrCmd)
                                                          End Select
                                                      Next
                                                      gridRegs.Refresh()
                                                  End If

                                                  ' Heartbeat watchdog
                                                  If _lastHeartbeatTick = DateTime.MinValue Then
                                                      _lastHeartbeat = hb
                                                      _lastHeartbeatTick = DateTime.UtcNow
                                                  Else
                                                      If hb <> _lastHeartbeat Then
                                                          _lastHeartbeat = hb
                                                          _lastHeartbeatTick = DateTime.UtcNow
                                                      Else
                                                          ' if heartbeat didn't change for 5 seconds -> show warning
                                                          If (DateTime.UtcNow - _lastHeartbeatTick).TotalSeconds > 5 Then
                                                              pnlConnLed.BackColor = Drawing.Color.Gold
                                                          Else
                                                              pnlConnLed.BackColor = Drawing.Color.LimeGreen
                                                          End If
                                                      End If
                                                  End If
                                              End Sub)
                           End Sub)
        End Function

        ' ----- Output control handlers -----
        Private Sub DoOutputChanged(index As Integer)
            If _suppressWrites Then Return
            If Not _client.IsConnected Then Return
            Try
                Dim value As Boolean = chkDo(index).Checked
                _client.WriteSingleCoil(SlaveId, CUShort(index), value)
            Catch ex As Exception
                AppendLog($"Write DO{1 + index} failed: {ex.Message}")
            End Try
        End Sub

        Private Sub DoMasterEnableChanged()
            If _suppressWrites Then Return
            If Not _client.IsConnected Then Return
            Try
                _client.WriteSingleCoil(SlaveId, 18US, chkMasterEnable.Checked)
            Catch ex As Exception
                AppendLog("Write MasterEnable failed: " & ex.Message)
            End Try
        End Sub

        Private Sub DoNightModeChanged()
            If _suppressWrites Then Return
            If Not _client.IsConnected Then Return
            Try
                _client.WriteSingleCoil(SlaveId, 19US, chkNightMode.Checked)
            Catch ex As Exception
                AppendLog("Write NightMode failed: " & ex.Message)
            End Try
        End Sub


        Private Function IsRtcInputOffset(off As Integer) As Boolean
            Return off = 26 OrElse off = 28 OrElse off = 29 OrElse off = 30 OrElse off = 31 OrElse off = 32 OrElse off = 33
        End Function

        Private Function PcRtcValue(defn As RegisterDef, now As DateTime) As String
            Select Case defn.Offset
                Case 26
                    Dim unix As Long = CLng(New DateTimeOffset(now).ToUnixTimeSeconds())
                    Return unix.ToString(CultureInfo.InvariantCulture)
                Case 28
                    Return now.Year.ToString(CultureInfo.InvariantCulture)
                Case 29
                    Return now.Month.ToString(CultureInfo.InvariantCulture)
                Case 30
                    Return now.Day.ToString(CultureInfo.InvariantCulture)
                Case 31
                    Return now.Hour.ToString(CultureInfo.InvariantCulture)
                Case 32
                    Return now.Minute.ToString(CultureInfo.InvariantCulture)
                Case 33
                    Return now.Second.ToString(CultureInfo.InvariantCulture)
                Case Else
                    Return ""
            End Select
        End Function

        ' ----- Register Grid -----
        Private Async Sub BtnRefreshRegs_Click(sender As Object, e As EventArgs)
            Await RefreshRegistersAsync()
        End Sub

        Private Async Function RefreshRegistersAsync() As Task
            If Not _client.IsConnected Then Return

            Await Task.Run(Sub()
                               Dim sid As Byte = SlaveId
                               Dim logUi As Action(Of String) = Sub(msg As String)
                                                                    Me.BeginInvoke(Sub() AppendLog(msg))
                                                                End Sub

                               ' Read key register blocks (avoid reading all 617 every time).
                               ' Any single block can fail (timeout/CRC) without crashing the UI.
                               Dim hr0 As UShort() = Array.Empty(Of UShort)()
                               Dim hrMac As UShort() = Array.Empty(Of UShort)()
                               Dim hrComm As UShort() = Array.Empty(Of UShort)()
                               Dim hrHw As UShort() = Array.Empty(Of UShort)()
                               Dim hrCmd As UShort() = Array.Empty(Of UShort)()
                               Dim coils As Boolean() = Array.Empty(Of Boolean)()
                               Dim di As Boolean() = Array.Empty(Of Boolean)()
                               Dim ir As UShort() = Array.Empty(Of UShort)()

                               Try : hr0 = _client.ReadHoldingRegisters(sid, 0US, 90US) : Catch ex As Exception : logUi("HR 40001..40090 read failed: " & ex.Message) : End Try
                               Try : hrMac = _client.ReadHoldingRegisters(sid, 100US, 24US) : Catch ex As Exception : logUi("HR 40101..40124 read failed: " & ex.Message) : End Try
                               Try : hrComm = _client.ReadHoldingRegisters(sid, 200US, 14US) : Catch ex As Exception : logUi("HR 40201..40214 read failed: " & ex.Message) : End Try
                               Try : hrHw = _client.ReadHoldingRegisters(sid, 300US, 26US) : Catch ex As Exception : logUi("HR 40301..40326 read failed: " & ex.Message) : End Try
                               Try : hrCmd = _client.ReadHoldingRegisters(sid, 609US, 8US) : Catch ex As Exception : logUi("HR 40610..40617 read failed: " & ex.Message) : End Try

                               Try : coils = _client.ReadCoils(sid, 0US, 20US) : Catch ex As Exception : logUi("Coils 00001..00020 read failed: " & ex.Message) : End Try
                               Try : di = _client.ReadInputs(sid, 0US, 24US) : Catch ex As Exception : logUi("DI 10001..10024 read failed: " & ex.Message) : End Try
                               Try : ir = _client.ReadInputRegisters(sid, 0US, 43US) : Catch ex As Exception : logUi("IR 30001..30043 read failed: " & ex.Message) : End Try

                               Me.BeginInvoke(Sub()
                                                  For Each r In _rows
                                                      r.Value = ""
                                                      Select Case r.Def.Table
                                                          Case ModbusTable.Coils
                                                              Dim addr As Integer = r.Def.Offset
                                                              If addr >= 0 AndAlso addr < coils.Length Then r.Value = If(coils(addr), "1", "0")
                                                          Case ModbusTable.DiscreteInputs
                                                              Dim addr As Integer = r.Def.Offset
                                                              If addr >= 0 AndAlso addr < di.Length Then r.Value = If(di(addr), "1", "0")
                                                          Case ModbusTable.InputRegisters
                                                              If r.Def IsNot Nothing AndAlso IsRtcInputOffset(r.Def.Offset) AndAlso (ir Is Nothing OrElse ir.Length < 34 OrElse ir(28) = 0US) Then
                                                                  r.Value = PcRtcValue(r.Def, DateTime.Now)
                                                              Else
                                                                  r.Value = DecodeValue(r.Def, ir, 0)
                                                              End If
                                                          Case ModbusTable.HoldingRegisters
                                                              r.Value = DecodeHoldingValue(r.Def, hr0, hrMac, hrComm, hrHw, hrCmd)
                                                      End Select
                                                  Next
                                                  gridRegs.Refresh()
                                                  AppendLog("Registers refreshed.")
                                              End Sub)
                           End Sub)
        End Function

        Private Function DecodeValue(defn As RegisterDef, words As UShort(), baseOffset As Integer) As String
            Dim off As Integer = defn.Offset - baseOffset
            If off < 0 OrElse off + defn.Count > words.Length Then Return ""
            Select Case defn.DataType
                Case RegDataType.U16, RegDataType.BitmaskU16
                    Return words(off).ToString(CultureInfo.InvariantCulture)
                Case RegDataType.S16
                    Return CType(words(off), Short).ToString(CultureInfo.InvariantCulture)
                Case RegDataType.U32
                    Return RegisterMap.DecodeU32(words, off).ToString(CultureInfo.InvariantCulture)
                Case Else
                    Return ""
            End Select
        End Function

        Private Function DecodeHoldingValue(defn As RegisterDef, hr0 As UShort(), hrMac As UShort(), hrComm As UShort(), hrHw As UShort(), hrCmd As UShort()) As String
            Dim off As Integer = defn.Offset
            Dim source As UShort() = Nothing
            Dim baseOff As Integer = 0

            If off >= 0 AndAlso off < 90 Then
                source = hr0 : baseOff = 0
            ElseIf off >= 100 AndAlso off < 124 Then
                source = hrMac : baseOff = 100
            ElseIf off >= 200 AndAlso off < 214 Then
                source = hrComm : baseOff = 200
            ElseIf off >= 300 AndAlso off < 326 Then
                source = hrHw : baseOff = 300
            ElseIf off >= 609 AndAlso off < 617 Then
                source = hrCmd : baseOff = 609
            Else
                Return ""
            End If

            Dim rel As Integer = off - baseOff
            If rel < 0 OrElse rel + defn.Count > source.Length Then Return ""

            Select Case defn.DataType
                Case RegDataType.U16, RegDataType.BitmaskU16, RegDataType.BytesU16
                    If defn.Count = 1 Then
                        Return source(rel).ToString(CultureInfo.InvariantCulture)
                    Else
                        ' bytes blocks: show as MAC style if length=6
                        If defn.DataType = RegDataType.BytesU16 AndAlso defn.Count = 6 Then
                            Dim b(5) As Byte
                            For i As Integer = 0 To 5
                                b(i) = CByte(source(rel + i) And &HFFUS)
                            Next
                            Return $"{b(0):X2}:{b(1):X2}:{b(2):X2}:{b(3):X2}:{b(4):X2}:{b(5):X2}"
                        End If
                        Return String.Join(",", source.Skip(rel).Take(defn.Count).Select(Function(x) x.ToString(CultureInfo.InvariantCulture)))
                    End If
                Case RegDataType.StringAscii
                    Return RegisterMap.DecodeAscii(source, rel, defn.Count)
                Case RegDataType.Float32
                    Return RegisterMap.DecodeFloat32(source, rel).ToString(CultureInfo.InvariantCulture)
                Case Else
                    Return ""
            End Select
        End Function

        Private Async Sub BtnWriteSelected_Click(sender As Object, e As EventArgs)
            If Not _client.IsConnected Then Return
            If gridRegs Is Nothing Then Return

            ' Commit any in-progress edit
            Try
                gridRegs.EndEdit()
            Catch
            End Try

            ' Collect targets (selected rows first; fall back to current row)
            Dim targets As New List(Of RegisterRow)()
            If gridRegs.SelectedRows IsNot Nothing AndAlso gridRegs.SelectedRows.Count > 0 Then
                Dim rows = gridRegs.SelectedRows.Cast(Of DataGridViewRow)().OrderBy(Function(r) r.Index).ToList()
                For Each r In rows
                    Dim rr = TryCast(r.DataBoundItem, RegisterRow)
                    If rr IsNot Nothing Then targets.Add(rr)
                Next
            ElseIf gridRegs.CurrentRow IsNot Nothing Then
                Dim rr = TryCast(gridRegs.CurrentRow.DataBoundItem, RegisterRow)
                If rr IsNot Nothing Then targets.Add(rr)
            End If

            targets = targets.Where(Function(x) x IsNot Nothing AndAlso x.Def IsNot Nothing AndAlso x.Def.IsWritable).ToList()
            If targets.Count = 0 Then
                MessageBox.Show("No writable registers selected.")
                Return
            End If

            Dim wasPolling As Boolean = (_pollTimer IsNot Nothing AndAlso _pollTimer.Enabled)
            If _pollTimer IsNot Nothing Then _pollTimer.Enabled = False

            btnWriteSelected.Enabled = False
            If prgRegOp IsNot Nothing Then
                prgRegOp.Value = 0
                prgRegOp.Maximum = targets.Count
            End If
            If lblRegOpStatus IsNot Nothing Then lblRegOpStatus.Text = $"Writing 0/{targets.Count}..."

            Try
                Await Task.Run(Sub()
                                   Dim sid As Byte = SlaveId
                                   SyncLock _ioSync
                                       For i As Integer = 0 To targets.Count - 1
                                           ' IMPORTANT: copy loop vars before BeginInvoke() to avoid lambda capture warnings
                                           Dim iLocal As Integer = i
                                           Dim rrLocal = targets(i)

                                           WriteRegister(rrLocal.Def, rrLocal.Value)

                                           Me.BeginInvoke(Sub()
                                                              AppendLog($"Wrote {rrLocal.Def.Table} {rrLocal.Def.Ref} = {rrLocal.Value}")
                                                              If prgRegOp IsNot Nothing Then prgRegOp.Value = Math.Min(prgRegOp.Maximum, iLocal + 1)
                                                              If lblRegOpStatus IsNot Nothing Then lblRegOpStatus.Text = $"Writing {iLocal + 1}/{targets.Count}..."
                                                          End Sub)
                                       Next
                                   End SyncLock
                               End Sub)

                If lblRegOpStatus IsNot Nothing Then lblRegOpStatus.Text = $"Done ({targets.Count})."
            Catch ex As Exception
                AppendLog("Write Selected failed: " & ex.Message)
                MessageBox.Show(ex.Message, "Write Selected failed")
                If lblRegOpStatus IsNot Nothing Then lblRegOpStatus.Text = "Failed."
            Finally
                btnWriteSelected.Enabled = True
                If wasPolling AndAlso _pollTimer IsNot Nothing Then _pollTimer.Enabled = True
            End Try
        End Sub

        Private Sub WriteRegister(defn As RegisterDef, valueText As String)
            Dim sid As Byte = SlaveId
            Select Case defn.Table
                Case ModbusTable.Coils
                    Dim v As Boolean = ParseBool(valueText)
                    _client.WriteSingleCoil(sid, CUShort(defn.Offset), v)

                Case ModbusTable.HoldingRegisters
                    Select Case defn.DataType
                        Case RegDataType.U16, RegDataType.BitmaskU16
                            Dim u As UShort = ParseU16(valueText)
                            _client.WriteSingleRegister(sid, CUShort(defn.Offset), u)

                        Case RegDataType.StringAscii
                            Dim words = RegisterMap.EncodeAscii(valueText, defn.Count)
                            _client.WriteMultipleRegisters(sid, CUShort(defn.Offset), words)

                        Case RegDataType.Float32
                            Dim f As Single
                            If Not Single.TryParse(valueText, NumberStyles.Float, CultureInfo.InvariantCulture, f) Then
                                Throw New FormatException("Invalid float value.")
                            End If
                            Dim words = RegisterMap.EncodeFloat32(f)
                            _client.WriteMultipleRegisters(sid, CUShort(defn.Offset), words)

                        Case RegDataType.BytesU16
                            ' Expect MAC format AA:BB:CC:DD:EE:FF
                            Dim parts = valueText.Trim().Split(":"c)
                            If parts.Length <> defn.Count Then Throw New FormatException("Expected 6 bytes in AA:BB:.. format.")
                            Dim words(defn.Count - 1) As UShort
                            For i As Integer = 0 To defn.Count - 1
                                Dim b As Integer = Integer.Parse(parts(i), NumberStyles.HexNumber, CultureInfo.InvariantCulture)
                                If b < 0 OrElse b > 255 Then Throw New FormatException("MAC byte out of range.")
                                words(i) = CUShort(b)
                            Next
                            _client.WriteMultipleRegisters(sid, CUShort(defn.Offset), words)

                        Case Else
                            Throw New NotSupportedException("Unsupported write type.")
                    End Select

                Case Else
                    Throw New NotSupportedException("Only coils and holding registers are writable.")
            End Select
        End Sub

        Private Function ParseBool(s As String) As Boolean
            Dim t = s.Trim().ToLowerInvariant()
            If t = "1" OrElse t = "true" OrElse t = "on" OrElse t = "yes" Then Return True
            If t = "0" OrElse t = "false" OrElse t = "off" OrElse t = "no" Then Return False
            Throw New FormatException("Expected boolean (1/0/true/false).")
        End Function

        Private Function ParseU16(s As String) As UShort
            Dim t = s.Trim()
            If t.StartsWith("0x", StringComparison.OrdinalIgnoreCase) Then
                Return CUShort(Integer.Parse(t.Substring(2), NumberStyles.HexNumber, CultureInfo.InvariantCulture))
            End If
            Return CUShort(UInteger.Parse(t, CultureInfo.InvariantCulture))
        End Function


        Private Function ParseModbusAddress(userText As String, tableName As String) As UShort
            Dim a As UShort = ParseU16(userText)

            Select Case tableName
                Case "HoldingRegisters"
                    ' Allow entering 40610-style references or raw offsets
                    If a >= 40001US Then Return CUShort(a - 40001US)
                    Return a

                Case "InputRegisters"
                    If a >= 30001US Then Return CUShort(a - 30001US)
                    Return a

                Case "DiscreteInputs"
                    If a >= 10001US Then Return CUShort(a - 10001US)
                    ' also accept 1-based raw like 1..N
                    If a > 0US Then Return CUShort(a - 1US)
                    Return a

                Case "Coils"
                    ' accept 1-based like 1..N
                    If a > 0US AndAlso a < 10000US Then Return CUShort(a - 1US)
                    Return a

                Case Else
                    Return a
            End Select
        End Function

        Private Sub BtnExportCsv_Click(sender As Object, e As EventArgs)
            Using sfd As New SaveFileDialog() With {.Filter = "CSV (*.csv)|*.csv", .FileName = "kc868_modbus_register_map.csv"}
                If sfd.ShowDialog() <> DialogResult.OK Then Return
                Using sw As New StreamWriter(sfd.FileName, False, Encoding.UTF8)
                    sw.WriteLine("Table,Ref,Offset,Access,Type,Count,Units,Description")
                    For Each d In RegisterMap.Map
                        sw.WriteLine($"{d.Table},{d.Ref},{d.Offset},{d.Access},{d.DataType},{d.Count},{EscapeCsv(d.Units)},{EscapeCsv(d.Description)}")
                    Next
                End Using
            End Using
            AppendLog("Exported CSV.")
        End Sub

        Private Function EscapeCsv(s As String) As String
            If s Is Nothing Then Return ""
            If s.Contains(",") OrElse s.Contains("""") OrElse s.Contains(Environment.NewLine) Then
                ' CSV quoting: wrap in quotes and double any embedded quotes
                Return """" & s.Replace("""", """""") & """"
            End If
            Return s
        End Function

        ' ----- Manual read/write -----
        Private Async Sub BtnManualRead_Click(sender As Object, e As EventArgs)
            If Not _client.IsConnected Then Return
            Dim tableName As String = cboManualTable.SelectedItem.ToString()
            Dim addr As UShort = ParseModbusAddress(txtManualAddr.Text, tableName)
            Dim cnt As UShort = ParseU16(txtManualCount.Text)

            Await Task.Run(Sub()
                               Dim sid As Byte = SlaveId
                               Try
                                   Select Case tableName
                                       Case "Coils"
                                           Dim v = _client.ReadCoils(sid, addr, cnt)
                                           AppendLog(String.Join(",", v.Select(Function(b) If(b, "1", "0"))))
                                       Case "DiscreteInputs"
                                           Dim v = _client.ReadInputs(sid, addr, cnt)
                                           AppendLog(String.Join(",", v.Select(Function(b) If(b, "1", "0"))))
                                       Case "InputRegisters"
                                           Dim v = _client.ReadInputRegisters(sid, addr, cnt)
                                           AppendLog(String.Join(",", v.Select(Function(w) w.ToString())))
                                       Case "HoldingRegisters"
                                           Dim v = _client.ReadHoldingRegisters(sid, addr, cnt)
                                           AppendLog(String.Join(",", v.Select(Function(w) w.ToString())))
                                   End Select
                               Catch ex As Exception
                                   AppendLog("Manual read failed: " & ex.Message)
                               End Try
                           End Sub)
        End Sub

        Private Async Sub BtnManualWrite_Click(sender As Object, e As EventArgs)
            If Not _client.IsConnected Then Return
            Dim tableName As String = cboManualTable.SelectedItem.ToString()
            Dim addr As UShort = ParseModbusAddress(txtManualAddr.Text, tableName)
            Dim txt As String = txtManualWrite.Text.Trim()

            Await Task.Run(Sub()
                               Dim sid As Byte = SlaveId
                               Try
                                   Select Case tableName
                                       Case "Coils"
                                           Dim v As Boolean = ParseBool(txt)
                                           _client.WriteSingleCoil(sid, addr, v)
                                       Case "HoldingRegisters"
                                           ' Comma-separated list
                                           Dim parts = txt.Split(","c).Select(Function(p) ParseU16(p)).ToArray()
                                           If parts.Length = 1 Then
                                               _client.WriteSingleRegister(sid, addr, parts(0))
                                           Else
                                               _client.WriteMultipleRegisters(sid, addr, parts)
                                           End If
                                       Case Else
                                           Throw New NotSupportedException("Manual write supported for Coils and HoldingRegisters only.")
                                   End Select
                                   AppendLog("Manual write OK.")
                               Catch ex As Exception
                                   AppendLog("Manual write failed: " & ex.Message)
                               End Try
                           End Sub)
        End Sub


        ' ----- Safe Maintenance (Section 4.5) -----
        ' HR40610..HR40617 are offsets 609..616 (0-based for NModbus).
        ' Sequence: ARM (0xA55A) -> CODE -> ARG0/ARG1 -> CONFIRM (0x5AA5), then poll STATUS/RESULT/ERROR.
        Private Async Function ExecuteSafeCommandAsync(cmd As UShort, title As String) As Task
            If Not _client.IsConnected Then
                AppendLog("Not connected.")
                Return
            End If

            Dim arg0 As UShort = 0US
            Dim arg1 As UShort = 0US
            Try
                If txtCmdArg0 IsNot Nothing Then arg0 = ParseU16(txtCmdArg0.Text)
                If txtCmdArg1 IsNot Nothing Then arg1 = ParseU16(txtCmdArg1.Text)
            Catch ex As Exception
                MessageBox.Show("Invalid ARG0 / ARG1 value.")
                Return
            End Try

            SetSafeUiEnabled(False)
            If lblCmdStatus IsNot Nothing Then lblCmdStatus.Text = $"Status: Sending {title}..."
            If prgCmd IsNot Nothing Then prgCmd.Value = 10

            Dim wasPolling As Boolean = (_pollTimer IsNot Nothing AndAlso _pollTimer.Enabled)
            If _pollTimer IsNot Nothing Then _pollTimer.Enabled = False

            Try
                Await Task.Run(Sub()
                                   Dim sid As Byte = SlaveId
                                   SyncLock _ioSync
                                       ' ARM + CODE + ARGS + CONFIRM
                                       _client.WriteSingleRegister(sid, 609US, &HA55AUS)  ' HR40610
                                       _client.WriteSingleRegister(sid, 610US, cmd)      ' HR40611
                                       _client.WriteSingleRegister(sid, 611US, arg0)     ' HR40612
                                       _client.WriteSingleRegister(sid, 612US, arg1)     ' HR40613
                                       _client.WriteSingleRegister(sid, 613US, &H5AA5US) ' HR40614

                                       Dim status As UShort = 0US
                                       Dim result As UShort = 0US
                                       Dim lastErr As UShort = 0US

                                       ' Poll status up to ~6s (device may reboot for some commands)
                                       For i As Integer = 0 To 30
                                           Try
                                               status = _client.ReadHoldingRegisters(sid, 614US, 1US)(0) ' HR40615
                                               If status = 3US OrElse status = 4US Then Exit For
                                           Catch
                                               Exit For
                                           End Try
                                           Threading.Thread.Sleep(200)
                                       Next

                                       Try : result = _client.ReadHoldingRegisters(sid, 615US, 1US)(0) : Catch : End Try ' HR40616
                                       Try : lastErr = _client.ReadHoldingRegisters(sid, 616US, 1US)(0) : Catch : End Try ' HR40617

                                       Me.BeginInvoke(Sub()
                                                          AppendLog($"SafeCmd {title}: status={status}, result={result}, err={lastErr}")
                                                          If lblCmdStatus IsNot Nothing Then lblCmdStatus.Text = $"Status: {title} done (status={status}, result={result}, err={lastErr})"
                                                          If prgCmd IsNot Nothing Then prgCmd.Value = 100
                                                      End Sub)
                                   End SyncLock
                               End Sub)
            Catch ex As Exception
                AppendLog("SafeCmd failed: " & ex.Message)
                If lblCmdStatus IsNot Nothing Then lblCmdStatus.Text = "Status: Failed - " & ex.Message
            Finally
                If wasPolling AndAlso _pollTimer IsNot Nothing Then _pollTimer.Enabled = True
                SetSafeUiEnabled(True)
            End Try
        End Function

        Private Sub SetSafeUiEnabled(enabled As Boolean)
            If grpSafe Is Nothing OrElse grpSafe.IsDisposed Then Return

            If btnCmdReboot IsNot Nothing Then btnCmdReboot.Enabled = enabled
            If btnCmdSaveConfig IsNot Nothing Then btnCmdSaveConfig.Enabled = enabled
            If btnCmdReloadConfig IsNot Nothing Then btnCmdReloadConfig.Enabled = enabled
            If btnCmdFactoryDefaults IsNot Nothing Then btnCmdFactoryDefaults.Enabled = enabled
            If btnCmdClearLogs IsNot Nothing Then btnCmdClearLogs.Enabled = enabled
            If btnCmdBackupSave IsNot Nothing Then btnCmdBackupSave.Enabled = enabled
            If btnCmdBackupRestore IsNot Nothing Then btnCmdBackupRestore.Enabled = enabled
        End Sub


        ' Row view-model
        Public Class RegisterRow
            Public ReadOnly Property Def As RegisterDef
            Public ReadOnly Property Table As String
                Get
                    Return Def.Table.ToString()
                End Get
            End Property
            Public ReadOnly Property Ref As Integer
                Get
                    Return Def.Ref
                End Get
            End Property
            Public ReadOnly Property Offset As Integer
                Get
                    Return Def.Offset
                End Get
            End Property
            Public ReadOnly Property Access As String
                Get
                    Return Def.Access
                End Get
            End Property
            Public ReadOnly Property DataType As String
                Get
                    Return Def.DataType.ToString()
                End Get
            End Property
            Public ReadOnly Property Count As Integer
                Get
                    Return Def.Count
                End Get
            End Property
            Public ReadOnly Property Units As String
                Get
                    Return Def.Units
                End Get
            End Property
            Public ReadOnly Property Description As String
                Get
                    Return Def.Description
                End Get
            End Property

            Public Property Value As String

            Public Sub New(d As RegisterDef)
                Def = d
                Value = ""
            End Sub
        End Class
    
        ' SplitContainer safety: avoid "SplitterDistance must be between Panel1MinSize and Width/Height - Panel2MinSize"
' This happens when min-sizes are applied while the SplitContainer is still being created (Width/Height can be 0).
Private Sub InitSplitContainerSafe(sc As SplitContainer, desiredSplitterDistance As Integer, panel1Min As Integer, panel2Min As Integer)
    If sc Is Nothing Then Return

    ' Store preferences in Tag: {desiredDistance, p1Min, p2Min}
    sc.Tag = New Integer() {desiredSplitterDistance, panel1Min, panel2Min}

    ' IMPORTANT: do NOT set Panel1MinSize/Panel2MinSize yet (can throw when Width/Height is 0).
    sc.Panel1MinSize = 0
    sc.Panel2MinSize = 0

    ' Apply after handle exists and whenever the control is resized.
    AddHandler sc.HandleCreated, Sub(sender As Object, e As EventArgs) ApplySplitPrefs(sc)
    AddHandler sc.SizeChanged, Sub(sender As Object, e As EventArgs) ApplySplitPrefs(sc)
End Sub

Private Sub ApplySplitPrefs(sc As SplitContainer)
    If sc Is Nothing Then Return

    Dim prefs As Integer() = TryCast(sc.Tag, Integer())
    Dim desiredDistance As Integer = If(prefs IsNot Nothing AndAlso prefs.Length > 0, prefs(0), sc.SplitterDistance)
    Dim p1Min As Integer = If(prefs IsNot Nothing AndAlso prefs.Length > 1, prefs(1), 0)
    Dim p2Min As Integer = If(prefs IsNot Nothing AndAlso prefs.Length > 2, prefs(2), 0)

    Dim total As Integer = If(sc.Orientation = Orientation.Vertical, sc.Width, sc.Height)
    If total <= 0 Then Return

    Dim available As Integer = Math.Max(0, total - sc.SplitterWidth)

    Dim a1 As Integer = Math.Max(0, p1Min)
    Dim a2 As Integer = Math.Max(0, p2Min)

    ' If min sizes don't fit in current size, shrink them so the control never throws.
    If a1 + a2 > available Then
        Dim keep1 As Integer = Math.Min(a1, Math.Max(0, available - a2))
        Dim keep2 As Integer = Math.Min(a2, Math.Max(0, available - keep1))
        a1 = keep1
        a2 = keep2

        If a1 + a2 > available Then
            a1 = available \ 2
            a2 = available - a1
        End If
    End If

    Try
        ' Update mins only if different (reduces re-entrancy)
        If sc.Panel1MinSize <> a1 Then sc.Panel1MinSize = a1
        If sc.Panel2MinSize <> a2 Then sc.Panel2MinSize = a2

        Dim minD As Integer = a1
        Dim maxD As Integer = total - a2 - sc.SplitterWidth

        If maxD < minD Then
            ' Still impossible -> relax mins fully.
            If sc.Panel1MinSize <> 0 Then sc.Panel1MinSize = 0
            If sc.Panel2MinSize <> 0 Then sc.Panel2MinSize = 0
            minD = 0
            maxD = Math.Max(0, total - sc.SplitterWidth)
        End If

        Dim d As Integer = desiredDistance
        If d < minD Then d = minD
        If d > maxD Then d = maxD
        If d < 0 Then d = 0

        If sc.SplitterDistance <> d Then sc.SplitterDistance = d
    Catch
        ' Last resort: remove mins and clamp distance.
        Try
            If sc.Panel1MinSize <> 0 Then sc.Panel1MinSize = 0
            If sc.Panel2MinSize <> 0 Then sc.Panel2MinSize = 0
            Dim maxD As Integer = Math.Max(0, total - sc.SplitterWidth)
            Dim d As Integer = Math.Max(0, Math.Min(desiredDistance, maxD))
            If sc.SplitterDistance <> d Then sc.SplitterDistance = d
        Catch
        End Try
    End Try
End Sub


        ''' <summary>
        ''' Safely sets an initial SplitterDistance only after the SplitContainer has a valid size.
        ''' This prevents: "SplitterDistance must be between Panel1MinSize and (Width/Height) - Panel2MinSize".
        ''' </summary>
        Private Sub EnsureSplitterDistance(sc As SplitContainer, desiredDistance As Integer)
            If sc Is Nothing OrElse sc.IsDisposed Then Return
            Try
                Dim total As Integer = If(sc.Orientation = Orientation.Horizontal, sc.Height, sc.Width)
                If total <= 0 Then Return

                Dim minD As Integer = sc.Panel1MinSize
                Dim maxD As Integer = Math.Max(minD, total - sc.Panel2MinSize - sc.SplitterWidth)
                Dim d As Integer = Math.Max(minD, Math.Min(desiredDistance, maxD))

                If sc.SplitterDistance <> d Then
                    sc.SplitterDistance = d
                End If
            Catch
                ' ignore (layout race conditions)
            End Try
        End Sub


        ' =====================================================================
        ' BACnet TAB (System.IO.BACnet)
        ' =====================================================================

        Private Class BacnetRow
            Public Property Section As String
            Public Property Point As String
            Public Property ObjectRef As String
            Public Property Value As String
            Public Property Writable As String
        End Class

        Private Sub BuildBacnetTab()
            ' Layout: Connection group on top, then split (grid + I/O controls)
            Dim root As New TableLayoutPanel() With {.Dock = DockStyle.Fill, .ColumnCount = 1, .RowCount = 2, .Padding = New Padding(6)}
            root.RowStyles.Add(New RowStyle(SizeType.AutoSize))
            root.RowStyles.Add(New RowStyle(SizeType.Percent, 100.0F))
            tabBacnet.Controls.Add(root)

            grpBacConn = New GroupBox() With {.Text = "BACnet/IP Connection", .Dock = DockStyle.Top, .AutoSize = True}
            root.Controls.Add(grpBacConn, 0, 0)

            Dim conn As New TableLayoutPanel() With {.Dock = DockStyle.Fill, .ColumnCount = 10, .RowCount = 2, .AutoSize = True}
            For i As Integer = 0 To 9
                conn.ColumnStyles.Add(New ColumnStyle(SizeType.AutoSize))
            Next
            conn.RowStyles.Add(New RowStyle(SizeType.AutoSize))
            conn.RowStyles.Add(New RowStyle(SizeType.AutoSize))
            grpBacConn.Controls.Add(conn)

            Dim lblDevId As New Label() With {.Text = "Device ID:", .AutoSize = True, .Anchor = AnchorStyles.Left}
            txtBacDeviceId = New TextBox() With {.Text = "12345", .Width = 90}

            Dim lblLocalUdp As New Label() With {.Text = "Local UDP:", .AutoSize = True, .Anchor = AnchorStyles.Left}
            numBacLocalUdp = New NumericUpDown() With {.Minimum = 1, .Maximum = 65535, .Value = 47808D, .Width = 90}

            btnBacStart = New Button() With {.Text = "Start", .Width = 90}
            btnBacDiscover = New Button() With {.Text = "Discover", .Width = 90}
            btnBacDisconnect = New Button() With {.Text = "Disconnect", .Width = 90, .Enabled = False}

            pnlBacLed = New Panel() With {.Width = 16, .Height = 16, .BackColor = Drawing.Color.DarkRed, .Margin = New Padding(6, 6, 6, 6)}
            lblBacState = New Label() With {.Text = "STOPPED", .AutoSize = True, .Anchor = AnchorStyles.Left}

            chkBacAutoRefresh = New CheckBox() With {.Text = "Auto refresh", .AutoSize = True, .Checked = True}
            Dim lblRef As New Label() With {.Text = "Refresh (ms):", .AutoSize = True, .Anchor = AnchorStyles.Left}
            numBacRefreshMs = New NumericUpDown() With {.Minimum = 200, .Maximum = 10000, .Increment = 100, .Value = 1000D, .Width = 90}

            ' Optional manual target IP (helps when broadcast discovery is blocked)
            Dim lblTargetIp As New Label() With {.Text = "Target IP (optional):", .AutoSize = True, .Anchor = AnchorStyles.Left}
            txtBacTargetIp = New TextBox() With {.Text = "", .Width = 140}

            conn.Controls.Add(lblDevId, 0, 0) : conn.Controls.Add(txtBacDeviceId, 1, 0)
            conn.Controls.Add(lblLocalUdp, 2, 0) : conn.Controls.Add(numBacLocalUdp, 3, 0)
            conn.Controls.Add(btnBacStart, 4, 0)
            conn.Controls.Add(btnBacDiscover, 5, 0)
            conn.Controls.Add(btnBacDisconnect, 6, 0)
            conn.Controls.Add(pnlBacLed, 7, 0)
            conn.Controls.Add(lblBacState, 8, 0)

            conn.Controls.Add(chkBacAutoRefresh, 0, 1)
            conn.Controls.Add(lblRef, 1, 1)
            conn.Controls.Add(numBacRefreshMs, 2, 1)

            conn.Controls.Add(lblTargetIp, 3, 1)
            conn.Controls.Add(txtBacTargetIp, 4, 1)

            ' Main area
            Dim split As New SplitContainer() With {.Dock = DockStyle.Fill, .Orientation = Orientation.Vertical, .SplitterWidth = 6}
            InitSplitContainerSafe(split, 420, 240, 240)
            root.Controls.Add(split, 0, 1)

            ' Left: data table
            Dim left As New TableLayoutPanel() With {.Dock = DockStyle.Fill, .ColumnCount = 1, .RowCount = 2}
            left.RowStyles.Add(New RowStyle(SizeType.AutoSize))
            left.RowStyles.Add(New RowStyle(SizeType.Percent, 100.0F))
            split.Panel1.Controls.Add(left)

            Dim lblTitle As New Label() With {.Text = "BACnet Points / Device Information", .AutoSize = True, .Font = New Font(Me.Font, FontStyle.Bold), .Margin = New Padding(3, 0, 3, 6)}
            left.Controls.Add(lblTitle, 0, 0)

            Dim gridHost As New Panel() With {.Dock = DockStyle.Fill, .BorderStyle = BorderStyle.FixedSingle, .Padding = New Padding(2)}
            left.Controls.Add(gridHost, 0, 1)

            gridBac = New DataGridView() With {.Dock = DockStyle.Fill, .AllowUserToAddRows = False, .AllowUserToDeleteRows = False}
            gridBac.RowHeadersVisible = False
            gridBac.AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill
            gridBac.AllowUserToResizeRows = False
            gridBac.SelectionMode = DataGridViewSelectionMode.FullRowSelect
            gridBac.MultiSelect = False
            gridBac.ReadOnly = True
            gridHost.Controls.Add(gridBac)

            _bacRows = New BindingList(Of BacnetRow)()
            gridBac.DataSource = _bacRows

            ' Right: I/O quick controls (Outputs writeable)
            Dim right As New TableLayoutPanel() With {.Dock = DockStyle.Fill, .ColumnCount = 1, .RowCount = 2, .Padding = New Padding(6)}
            right.RowStyles.Add(New RowStyle(SizeType.Percent, 55.0F))
            right.RowStyles.Add(New RowStyle(SizeType.Percent, 45.0F))
            split.Panel2.Controls.Add(right)

            Dim grpOut As New GroupBox() With {.Text = "BACnet Outputs (BO 1..16)", .Dock = DockStyle.Fill}
            Dim grpIn As New GroupBox() With {.Text = "BACnet Inputs (BI 1..16)", .Dock = DockStyle.Fill}
            right.Controls.Add(grpOut, 0, 0)
            right.Controls.Add(grpIn, 0, 1)

            ReDim chkBacDo(15)
            Dim outGrid As New TableLayoutPanel() With {.Dock = DockStyle.Fill, .ColumnCount = 4, .RowCount = 4, .Padding = New Padding(8), .AutoScroll = True}
            For c As Integer = 0 To 3 : outGrid.ColumnStyles.Add(New ColumnStyle(SizeType.Percent, 25.0F)) : Next
            For r As Integer = 0 To 3 : outGrid.RowStyles.Add(New RowStyle(SizeType.AutoSize)) : Next
            grpOut.Controls.Add(outGrid)

            For i As Integer = 0 To 15
                Dim idx As Integer = i
                Dim cb As New CheckBox() With {.Text = $"CH{idx + 1}", .AutoSize = True, .Margin = New Padding(6)}
                chkBacDo(idx) = cb
                AddHandler cb.CheckedChanged, AddressOf BacnetOutput_CheckedChanged
                outGrid.Controls.Add(cb, idx Mod 4, idx \ 4)
            Next

            ReDim pnlBacDi(15)
            Dim inGrid As New TableLayoutPanel() With {.Dock = DockStyle.Fill, .ColumnCount = 4, .RowCount = 4, .Padding = New Padding(8), .AutoScroll = True}
            For c As Integer = 0 To 3 : inGrid.ColumnStyles.Add(New ColumnStyle(SizeType.Percent, 25.0F)) : Next
            For r As Integer = 0 To 3 : inGrid.RowStyles.Add(New RowStyle(SizeType.AutoSize)) : Next
            grpIn.Controls.Add(inGrid)

            For i As Integer = 0 To 15
                Dim pLed As Panel = Nothing
                Dim lab As Label = Nothing
                Dim item As Control = CreateLedItem($"IN{i + 1}", pLed, lab)
                pnlBacDi(i) = pLed
                inGrid.Controls.Add(item, i Mod 4, i \ 4)
            Next

            ' Events
            AddHandler btnBacStart.Click, AddressOf BtnBacStart_Click
            AddHandler btnBacDiscover.Click, AddressOf BtnBacDiscover_Click
            AddHandler btnBacDisconnect.Click, AddressOf BtnBacDisconnect_Click
            AddHandler numBacRefreshMs.ValueChanged, AddressOf BacnetRefresh_ValueChanged
            AddHandler chkBacAutoRefresh.CheckedChanged, AddressOf BacnetAutoRefresh_CheckedChanged

            AddHandler _bacnet.ConnectedChanged, AddressOf Bacnet_ConnectedChanged

            ' Build table rows (static map) so values can fill during polling
            BuildBacnetRows()

            _bacTimer = New FormsTimer() With {.Interval = CInt(numBacRefreshMs.Value)}
            AddHandler _bacTimer.Tick, AddressOf BacnetTimer_Tick
            _bacTimer.Start()
        End Sub

        Private Sub BuildBacnetRows()
            _bacRows.Clear()

            ' Section 1 - Board Information
            _bacRows.Add(New BacnetRow With {.Section = "Board", .Point = "Device Name", .ObjectRef = "Device:" & txtBacDeviceId.Text, .Value = "-", .Writable = "R"})
            _bacRows.Add(New BacnetRow With {.Section = "Board", .Point = "Manufacturer", .ObjectRef = "Device:" & txtBacDeviceId.Text, .Value = "Microcode Engineering", .Writable = "R"})
            _bacRows.Add(New BacnetRow With {.Section = "Board", .Point = "Firmware Version", .ObjectRef = "Device:" & txtBacDeviceId.Text, .Value = "-", .Writable = "R"})
            _bacRows.Add(New BacnetRow With {.Section = "Board", .Point = "Hardware Version", .ObjectRef = "Device:" & txtBacDeviceId.Text, .Value = "KC868-A16", .Writable = "R"})

            ' Section 2 - Network (BACnet uses IP/UDP; we expose device MAC/IP via strings if provided)
            _bacRows.Add(New BacnetRow With {.Section = "Network", .Point = "UDP Port", .ObjectRef = "Local", .Value = numBacLocalUdp.Value.ToString(), .Writable = "R"})

            ' Section 4 - Hardware points
            For i As Integer = 1 To 16
                _bacRows.Add(New BacnetRow With {.Section = "Digital Inputs", .Point = $"IN{i}", .ObjectRef = $"BI:{i} Present_Value", .Value = "-", .Writable = "R"})
            Next
            For i As Integer = 1 To 16
                _bacRows.Add(New BacnetRow With {.Section = "Digital Outputs", .Point = $"OUT{i}", .ObjectRef = $"BO:{i} Present_Value", .Value = "-", .Writable = "R/W"})
            Next
            For i As Integer = 1 To 4
                _bacRows.Add(New BacnetRow With {.Section = "Analog Inputs", .Point = $"AI{i}", .ObjectRef = $"AI:{i} Present_Value", .Value = "-", .Writable = "R"})
            Next
            _bacRows.Add(New BacnetRow With {.Section = "Sensors", .Point = "DHT1 Temp", .ObjectRef = "AV:1 Present_Value", .Value = "-", .Writable = "R"})
            _bacRows.Add(New BacnetRow With {.Section = "Sensors", .Point = "DHT1 Hum", .ObjectRef = "AV:2 Present_Value", .Value = "-", .Writable = "R"})
            _bacRows.Add(New BacnetRow With {.Section = "Sensors", .Point = "DHT2 Temp", .ObjectRef = "AV:3 Present_Value", .Value = "-", .Writable = "R"})
            _bacRows.Add(New BacnetRow With {.Section = "Sensors", .Point = "DHT2 Hum", .ObjectRef = "AV:4 Present_Value", .Value = "-", .Writable = "R"})
            _bacRows.Add(New BacnetRow With {.Section = "Sensors", .Point = "DS18B20 Temp", .ObjectRef = "AV:5 Present_Value", .Value = "-", .Writable = "R"})
        End Sub

        Private Sub BtnBacStart_Click(sender As Object, e As EventArgs)
            Try
                _bacnet.Start(CInt(numBacLocalUdp.Value))
                lblBacState.Text = "STARTED"
                pnlBacLed.BackColor = Drawing.Color.Goldenrod
            Catch ex As Exception
                lblBacState.Text = "START FAILED"
                pnlBacLed.BackColor = Drawing.Color.DarkRed
                MessageBox.Show("BACnet start failed: " & ex.Message, "BACnet", MessageBoxButtons.OK, MessageBoxIcon.Error)
            End Try
        End Sub

        Private Async Sub BtnBacDiscover_Click(sender As Object, e As EventArgs)
            ' IMPORTANT: Discovery + bind can take several seconds. Running this on the UI thread will freeze the form.
            ' We therefore do all BACnet discovery work on a background task.

            btnBacDiscover.Enabled = False
            btnBacStart.Enabled = False
            lblBacState.Text = "DISCOVERING..."
            pnlBacLed.BackColor = Drawing.Color.Goldenrod

            ' Ensure BACnet stack is started (users may press Discover without pressing Start)
            SyncLock _ioSync
                _bacInstanceBase = 1
                _bacBaseDetected = False
            End SyncLock
            Try
                _bacnet.Start(CInt(numBacLocalUdp.Value))
            Catch
                ' ignore start errors here; they will be reported when binding attempts fail
            End Try

            If String.IsNullOrWhiteSpace(txtBacDeviceId.Text) Then txtBacDeviceId.Text = "12345"
            Dim devId As UInteger
            If Not UInteger.TryParse(txtBacDeviceId.Text.Trim(), devId) Then devId = 0UI

            Dim ok As Boolean = False
            Dim boundIp As String = Nothing

            Try
                ok = Await Task.Run(Function()
                                        ' If user provided a target IP, try unicast bind first (works when broadcast discovery is blocked)
                                        Dim ipStr As String = If(txtBacTargetIp IsNot Nothing, txtBacTargetIp.Text, "")
                                        Dim localOk As Boolean = False

                                        If Not String.IsNullOrWhiteSpace(ipStr) AndAlso devId <> 0UI Then
                                            localOk = _bacnet.BindToIp(ipStr.Trim(), devId, 47808US, 2500)
                                            If localOk Then boundIp = ipStr.Trim()
                                        End If

                                        If Not localOk Then
                                            ' Many embedded BACnet devices send periodic I-Am broadcasts.
                                            ' Wait long enough to catch the next I-Am broadcast.
                                            localOk = _bacnet.DiscoverAndBind(devId, 15000)
                                        End If

                                        If Not localOk Then
                                            ' Fallback: ARP-cache probing is very effective on 169.254.x.x link-local networks.
                                            localOk = _bacnet.TryBindUsingArpCache(devId, 47808US, 2000)
                                        End If

                                        If localOk Then
                                            ' Best-effort: capture bound IP to show in UI
                                            Try
                                                Dim d = _bacnet.GetDiscoveredDevices()
                                                If d IsNot Nothing AndAlso d.ContainsKey(devId) Then
                                                    Dim adr = d(devId)
                                                    If adr IsNot Nothing AndAlso adr.adr IsNot Nothing AndAlso adr.adr.Length >= 4 Then
                                                        boundIp = $"{adr.adr(0)}.{adr.adr(1)}.{adr.adr(2)}.{adr.adr(3)}"
                                                    End If
                                                End If
                                            Catch
                                            End Try
                                        End If

                                        Return localOk
                                    End Function)
            Catch
                ok = False
            End Try

            If ok Then
                lblBacState.Text = "CONNECTED"
                pnlBacLed.BackColor = Drawing.Color.DarkGreen
                btnBacDisconnect.Enabled = True
                If txtBacTargetIp IsNot Nothing AndAlso Not String.IsNullOrWhiteSpace(boundIp) Then
                    txtBacTargetIp.Text = boundIp
                End If
            Else
                lblBacState.Text = "NOT FOUND"
                pnlBacLed.BackColor = Drawing.Color.DarkRed
                btnBacDisconnect.Enabled = False
            End If

            btnBacDiscover.Enabled = True
            btnBacStart.Enabled = True
        End Sub

        Private Sub BtnBacDisconnect_Click(sender As Object, e As EventArgs)
            ' Stop any in-flight background polling immediately.
            Try
                If _bacPollCts IsNot Nothing Then
                    _bacPollCts.Cancel()
                    _bacPollCts.Dispose()
                    _bacPollCts = Nothing
                End If
            Catch
            End Try
            _bacnet.Unbind()
            SyncLock _ioSync
                _bacInstanceBase = 1
                _bacBaseDetected = False
            End SyncLock
            btnBacDisconnect.Enabled = False
        End Sub

        Private Sub Bacnet_ConnectedChanged(isConnected As Boolean)
            If Me.IsDisposed Then Return
            If Me.InvokeRequired Then
                Me.BeginInvoke(New Action(Of Boolean)(AddressOf Bacnet_ConnectedChanged), isConnected)
                Return
            End If

            btnBacDisconnect.Enabled = isConnected
            lblBacState.Text = If(isConnected, "CONNECTED", "DISCONNECTED")
            pnlBacLed.BackColor = If(isConnected, Drawing.Color.DarkGreen, Drawing.Color.DarkRed)
        End Sub

        Private Sub BacnetAutoRefresh_CheckedChanged(sender As Object, e As EventArgs)
            If _bacTimer Is Nothing Then Return
            _bacTimer.Enabled = chkBacAutoRefresh.Checked

            ' If auto-refresh is disabled, cancel any ongoing poll so the UI remains responsive.
            If Not chkBacAutoRefresh.Checked Then
                Try
                    If _bacPollCts IsNot Nothing Then _bacPollCts.Cancel()
                Catch
                End Try
            End If
        End Sub

        Private Sub BacnetRefresh_ValueChanged(sender As Object, e As EventArgs)
            If _bacTimer Is Nothing Then Return
            _bacTimer.Interval = CInt(numBacRefreshMs.Value)
        End Sub

        Private Async Sub BacnetTimer_Tick(sender As Object, e As EventArgs)
            If Not _bacnet.IsConnected Then Return
            If Not chkBacAutoRefresh.Checked Then Return

            ' Prevent re-entrancy: if a poll is still running, skip this tick.
            If Threading.Interlocked.Exchange(_bacPollBusy, 1) = 1 Then Return

            ' Cancel previous poll (if any) and create a fresh token for this poll.
            Try
                If _bacPollCts IsNot Nothing Then
                    _bacPollCts.Cancel()
                    _bacPollCts.Dispose()
                End If
            Catch
            End Try
            _bacPollCts = New CancellationTokenSource()
            Dim ct As CancellationToken = _bacPollCts.Token

            Try
                Dim result = Await Task.Run(Function()
                                                Dim bi(15) As Boolean
                                                Dim bo(15) As Boolean
                                                Dim ai(3) As Double
                                                Dim av(4) As Double

                                                ' Auto-detect instance base (0-based vs 1-based) once per connection.
                                                Dim baseInst As Integer
                                                SyncLock _ioSync
                                                    baseInst = _bacInstanceBase
                                                End SyncLock

                                                If Not _bacBaseDetected Then
                                                    Dim tmp As Boolean = False
                                                    Dim ok1 As Boolean = False
                                                    Dim ok0 As Boolean = False

                                                    Try
                                                        ok1 = _bacnet.ReadBinaryOutput(1UI, tmp) OrElse _bacnet.ReadBinaryInput(1UI, tmp)
                                                    Catch
                                                        ok1 = False
                                                    End Try
                                                    If Not ok1 Then
                                                        Try
                                                            ok0 = _bacnet.ReadBinaryOutput(0UI, tmp) OrElse _bacnet.ReadBinaryInput(0UI, tmp)
                                                        Catch
                                                            ok0 = False
                                                        End Try
                                                    End If

                                                    If ok1 Then
                                                        baseInst = 1
                                                    ElseIf ok0 Then
                                                        baseInst = 0
                                                    Else
                                                        baseInst = 1
                                                    End If

                                                    SyncLock _ioSync
                                                        _bacInstanceBase = baseInst
                                                        _bacBaseDetected = True
                                                    End SyncLock
                                                End If

                                                ' Poll: BI 1..16, BO 1..16, AI 1..4, AV 1..5
                                                ' NOTE: Each BACnet ReadProperty can block until APDU timeout.
                                                ' Running on a background task keeps the UI responsive.
                                                For i As Integer = 0 To 15
                                                    If ct.IsCancellationRequested Then Exit For
                                                    Dim v As Boolean = False
                                                    If _bacnet.ReadBinaryInput(CUInt(i + baseInst), v) Then bi(i) = v
                                                Next

                                                For i As Integer = 0 To 15
                                                    If ct.IsCancellationRequested Then Exit For
                                                    Dim v As Boolean = False
                                                    If _bacnet.ReadBinaryOutput(CUInt(i + baseInst), v) Then bo(i) = v
                                                Next

                                                For i As Integer = 0 To 3
                                                    If ct.IsCancellationRequested Then Exit For
                                                    Dim v As Double = 0
                                                    If _bacnet.ReadAnalogInput(CUInt(i + baseInst), v) Then ai(i) = v
                                                Next

                                                For i As Integer = 0 To 4
                                                    If ct.IsCancellationRequested Then Exit For
                                                    Dim v As Double = 0
                                                    If _bacnet.ReadAnalogValue(CUInt(i + baseInst), v) Then av(i) = v
                                                Next

                                                Return Tuple.Create(bi, bo, ai, av)
                                            End Function, ct)

                If Not ct.IsCancellationRequested Then
                    UpdateBacnetUi(result.Item1, result.Item2, result.Item3, result.Item4)
                End If
            Catch
                ' ignore poll errors (auto-retry on next tick)
            Finally
                Threading.Interlocked.Exchange(_bacPollBusy, 0)
            End Try
        End Sub

        Private Sub UpdateBacnetUi(bi() As Boolean, bo() As Boolean, ai() As Double, av() As Double)
            If Me.IsDisposed Then Return
            If Me.InvokeRequired Then
                Me.BeginInvoke(New Action(Of Boolean(), Boolean(), Double(), Double())(AddressOf UpdateBacnetUi), bi, bo, ai, av)
                Return
            End If

            ' LEDs for inputs
            For i As Integer = 0 To 15
                If pnlBacDi IsNot Nothing AndAlso pnlBacDi(i) IsNot Nothing Then
                    pnlBacDi(i).BackColor = If(bi(i), Drawing.Color.Red, Drawing.Color.LimeGreen)
                End If
            Next

            ' Output checkboxes (prevent echo write)
            If chkBacDo IsNot Nothing Then
                For i As Integer = 0 To 15
                    Dim idx As Integer = i
                    RemoveHandler chkBacDo(idx).CheckedChanged, AddressOf BacnetOutput_CheckedChanged
                    chkBacDo(idx).Checked = bo(idx)
                    AddHandler chkBacDo(idx).CheckedChanged, AddressOf BacnetOutput_CheckedChanged
                Next
            End If

            ' Update grid row values
            For Each r In _bacRows
                If r.ObjectRef Is Nothing Then Continue For
                If r.ObjectRef.StartsWith("BI:", StringComparison.OrdinalIgnoreCase) Then
                    Dim inst As Integer = ParseInstance(r.ObjectRef)
                    If inst >= 1 AndAlso inst <= 16 Then r.Value = If(bi(inst - 1), "1", "0")
                ElseIf r.ObjectRef.StartsWith("BO:", StringComparison.OrdinalIgnoreCase) Then
                    Dim inst As Integer = ParseInstance(r.ObjectRef)
                    If inst >= 1 AndAlso inst <= 16 Then r.Value = If(bo(inst - 1), "ON", "OFF")
                ElseIf r.ObjectRef.StartsWith("AI:", StringComparison.OrdinalIgnoreCase) Then
                    Dim inst As Integer = ParseInstance(r.ObjectRef)
                    If inst >= 1 AndAlso inst <= 4 Then r.Value = ai(inst - 1).ToString("0.###") & " V"
                ElseIf r.ObjectRef.StartsWith("AV:", StringComparison.OrdinalIgnoreCase) Then
                    Dim inst As Integer = ParseInstance(r.ObjectRef)
                    If inst >= 1 AndAlso inst <= 5 Then r.Value = av(inst - 1).ToString("0.###")
                End If
            Next

            gridBac.Refresh()
        End Sub

        Private Function ParseInstance(objRef As String) As Integer
            Try
                Dim p As Integer = objRef.IndexOf(":"c)
                If p < 0 Then Return 0
                Dim rest As String = objRef.Substring(p + 1)
                Dim sp As Integer = rest.IndexOf(" "c)
                If sp > 0 Then rest = rest.Substring(0, sp)
                Dim n As Integer
                If Integer.TryParse(rest, n) Then Return n
            Catch
            End Try
            Return 0
        End Function

        Private Sub BacnetOutput_CheckedChanged(sender As Object, e As EventArgs)
            If Not _bacnet.IsConnected Then Return
            If sender Is Nothing Then Return

            Dim idx As Integer = Array.IndexOf(chkBacDo, CType(sender, CheckBox))
            If idx < 0 OrElse idx > 15 Then Return

            ' Prevent recursive trigger while we are performing a network write.
            If chkBacDo(idx).Tag IsNot Nothing AndAlso Convert.ToString(chkBacDo(idx).Tag) = "W" Then Return

            Dim desired As Boolean = chkBacDo(idx).Checked
            chkBacDo(idx).Tag = "W"
            chkBacDo(idx).Enabled = False

            ' BACnet write can block until APDU timeout; do it on background task to keep UI responsive.
            Task.Run(Sub()
                         Dim ok As Boolean = False
                         Try
                             Dim baseInst As Integer
                             SyncLock _ioSync
                                 baseInst = _bacInstanceBase
                             End SyncLock
                             ok = _bacnet.WriteBinaryOutput(CUInt(idx + baseInst), desired)
                         Catch
                             ok = False
                         End Try

                         ' marshal back to UI thread
                         If Me.IsDisposed Then Return
                         Try
                             Me.BeginInvoke(New Action(Sub()
                                                          Try
                                                              If Not ok Then
                                                                  ' revert visual if write failed
                                                                  RemoveHandler chkBacDo(idx).CheckedChanged, AddressOf BacnetOutput_CheckedChanged
                                                                  chkBacDo(idx).Checked = Not desired
                                                                  AddHandler chkBacDo(idx).CheckedChanged, AddressOf BacnetOutput_CheckedChanged
                                                              End If
                                                          Finally
                                                              chkBacDo(idx).Enabled = True
                                                              chkBacDo(idx).Tag = Nothing
                                                          End Try
                                                      End Sub))
                         Catch
                         End Try
                     End Sub)
        End Sub

End Class
End Namespace