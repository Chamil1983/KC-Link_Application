<Global.Microsoft.VisualBasic.CompilerServices.DesignerGenerated()>
Partial Class FormMain
    Inherits System.Windows.Forms.Form

    'Form overrides dispose to clean up the component list.
    <System.Diagnostics.DebuggerNonUserCode()>
    Protected Overrides Sub Dispose(ByVal disposing As Boolean)
        Try
            If disposing AndAlso components IsNot Nothing Then
                components.Dispose()
            End If
        Finally
            MyBase.Dispose(disposing)
        End Try
    End Sub

    'Required by the Windows Form Designer
    Private components As System.ComponentModel.IContainer

    ' NOTE: Adjust layout visually in Designer after initial compile.
    Private Sub InitializeComponent()
        Me.txtIp = New System.Windows.Forms.TextBox()
        Me.txtPort = New System.Windows.Forms.TextBox()
        Me.btnConnect = New System.Windows.Forms.Button()
        Me.lblStatus = New System.Windows.Forms.Label()
        Me.grpNet = New System.Windows.Forms.GroupBox()
        Me.chkAutoReconnect = New System.Windows.Forms.CheckBox()
        Me.lblNetInfo = New System.Windows.Forms.Label()
        Me.btnPing = New System.Windows.Forms.Button()
        Me.grpRelays = New System.Windows.Forms.GroupBox()
        Me.chkRel6 = New System.Windows.Forms.CheckBox()
        Me.chkRel5 = New System.Windows.Forms.CheckBox()
        Me.chkRel4 = New System.Windows.Forms.CheckBox()
        Me.chkRel3 = New System.Windows.Forms.CheckBox()
        Me.chkRel2 = New System.Windows.Forms.CheckBox()
        Me.chkRel1 = New System.Windows.Forms.CheckBox()
        Me.grpDI = New System.Windows.Forms.GroupBox()
        Me.chkDI8 = New System.Windows.Forms.CheckBox()
        Me.chkDI7 = New System.Windows.Forms.CheckBox()
        Me.chkDI6 = New System.Windows.Forms.CheckBox()
        Me.chkDI5 = New System.Windows.Forms.CheckBox()
        Me.chkDI4 = New System.Windows.Forms.CheckBox()
        Me.chkDI3 = New System.Windows.Forms.CheckBox()
        Me.chkDI2 = New System.Windows.Forms.CheckBox()
        Me.chkDI1 = New System.Windows.Forms.CheckBox()
        Me.grpAnalog = New System.Windows.Forms.GroupBox()
        Me.txtI2 = New System.Windows.Forms.TextBox()
        Me.txtI1 = New System.Windows.Forms.TextBox()
        Me.txtV2 = New System.Windows.Forms.TextBox()
        Me.txtV1 = New System.Windows.Forms.TextBox()
        Me.lblLastSnap = New System.Windows.Forms.Label()
        Me.grpDAC = New System.Windows.Forms.GroupBox()
        Me.Label2 = New System.Windows.Forms.Label()
        Me.Label1 = New System.Windows.Forms.Label()
        Me.btnDacSet = New System.Windows.Forms.Button()
        Me.txtDAC2 = New System.Windows.Forms.TextBox()
        Me.txtDAC1 = New System.Windows.Forms.TextBox()
        Me.grpTemp = New System.Windows.Forms.GroupBox()
        Me.Label10 = New System.Windows.Forms.Label()
        Me.Label11 = New System.Windows.Forms.Label()
        Me.Label9 = New System.Windows.Forms.Label()
        Me.Label6 = New System.Windows.Forms.Label()
        Me.Label7 = New System.Windows.Forms.Label()
        Me.Label8 = New System.Windows.Forms.Label()
        Me.Label5 = New System.Windows.Forms.Label()
        Me.Label4 = New System.Windows.Forms.Label()
        Me.Label3 = New System.Windows.Forms.Label()
        Me.txtDs0 = New System.Windows.Forms.TextBox()
        Me.txtDsCount = New System.Windows.Forms.TextBox()
        Me.txtH2 = New System.Windows.Forms.TextBox()
        Me.txtT2 = New System.Windows.Forms.TextBox()
        Me.txtH1 = New System.Windows.Forms.TextBox()
        Me.txtT1 = New System.Windows.Forms.TextBox()
        Me.grpRTC = New System.Windows.Forms.GroupBox()
        Me.btnRtcSet = New System.Windows.Forms.Button()
        Me.btnRtcGet = New System.Windows.Forms.Button()
        Me.txtRtcSet = New System.Windows.Forms.TextBox()
        Me.txtRtcCurrent = New System.Windows.Forms.TextBox()
        Me.grpBeep = New System.Windows.Forms.GroupBox()
        Me.btnBuzzOff = New System.Windows.Forms.Button()
        Me.btnBeep = New System.Windows.Forms.Button()
        Me.txtBeepMs = New System.Windows.Forms.TextBox()
        Me.txtBeepFreq = New System.Windows.Forms.TextBox()
        Me.grpMisc = New System.Windows.Forms.GroupBox()
        Me.btnSnapOnce = New System.Windows.Forms.Button()
        Me.btnUnitApply = New System.Windows.Forms.Button()
        Me.cboTempUnit = New System.Windows.Forms.ComboBox()
        Me.chkAutoPoll = New System.Windows.Forms.CheckBox()
        Me.txtLog = New System.Windows.Forms.TextBox()
        Me.tabMain = New System.Windows.Forms.TabControl()
        Me.tabPageDashboard = New System.Windows.Forms.TabPage()
        Me.tabPageSchedule = New System.Windows.Forms.TabPage()
        Me.grpSchedule = New System.Windows.Forms.GroupBox()
        Me.grpSchedList = New System.Windows.Forms.GroupBox()
        Me.lstSchedules = New System.Windows.Forms.ListView()
        Me.colId = CType(New System.Windows.Forms.ColumnHeader(), System.Windows.Forms.ColumnHeader)
        Me.colEnabled = CType(New System.Windows.Forms.ColumnHeader(), System.Windows.Forms.ColumnHeader)
        Me.colMode = CType(New System.Windows.Forms.ColumnHeader(), System.Windows.Forms.ColumnHeader)
        Me.colDays = CType(New System.Windows.Forms.ColumnHeader(), System.Windows.Forms.ColumnHeader)
        Me.colWindow = CType(New System.Windows.Forms.ColumnHeader(), System.Windows.Forms.ColumnHeader)
        Me.colTrigger = CType(New System.Windows.Forms.ColumnHeader(), System.Windows.Forms.ColumnHeader)
        Me.colActions = CType(New System.Windows.Forms.ColumnHeader(), System.Windows.Forms.ColumnHeader)
        Me.lblSchedMode = New System.Windows.Forms.Label()
        Me.cboSchedMode = New System.Windows.Forms.ComboBox()
        Me.chkSchedEnabled = New System.Windows.Forms.CheckBox()
        Me.lblSchedId = New System.Windows.Forms.Label()
        Me.txtSchedId = New System.Windows.Forms.TextBox()
        Me.grpDays = New System.Windows.Forms.GroupBox()
        Me.chkMon = New System.Windows.Forms.CheckBox()
        Me.chkTue = New System.Windows.Forms.CheckBox()
        Me.chkWed = New System.Windows.Forms.CheckBox()
        Me.chkThu = New System.Windows.Forms.CheckBox()
        Me.chkFri = New System.Windows.Forms.CheckBox()
        Me.chkSat = New System.Windows.Forms.CheckBox()
        Me.chkSun = New System.Windows.Forms.CheckBox()
        Me.lblTrigDI = New System.Windows.Forms.Label()
        Me.cboTrigDI = New System.Windows.Forms.ComboBox()
        Me.lblEdge = New System.Windows.Forms.Label()
        Me.cboEdge = New System.Windows.Forms.ComboBox()
        Me.lblWinStart = New System.Windows.Forms.Label()
        Me.txtWinStart = New System.Windows.Forms.TextBox()
        Me.lblWinEnd = New System.Windows.Forms.Label()
        Me.txtWinEnd = New System.Windows.Forms.TextBox()
        Me.chkRecurring = New System.Windows.Forms.CheckBox()
        Me.grpSchedRelays = New System.Windows.Forms.GroupBox()
        Me.chkSRel1 = New System.Windows.Forms.CheckBox()
        Me.chkSRel2 = New System.Windows.Forms.CheckBox()
        Me.chkSRel3 = New System.Windows.Forms.CheckBox()
        Me.chkSRel4 = New System.Windows.Forms.CheckBox()
        Me.chkSRel5 = New System.Windows.Forms.CheckBox()
        Me.chkSRel6 = New System.Windows.Forms.CheckBox()
        Me.lblRelayAction = New System.Windows.Forms.Label()
        Me.cboRelayAction = New System.Windows.Forms.ComboBox()
        Me.grpSchedDac = New System.Windows.Forms.GroupBox()
        Me.lblDac1Mv = New System.Windows.Forms.Label()
        Me.txtSchedDac1Mv = New System.Windows.Forms.TextBox()
        Me.lblDac2Mv = New System.Windows.Forms.Label()
        Me.txtSchedDac2Mv = New System.Windows.Forms.TextBox()
        Me.lblDac1Range = New System.Windows.Forms.Label()
        Me.cboDac1Range = New System.Windows.Forms.ComboBox()
        Me.lblDac2Range = New System.Windows.Forms.Label()
        Me.cboDac2Range = New System.Windows.Forms.ComboBox()
        Me.grpSchedBuzzer = New System.Windows.Forms.GroupBox()
        Me.chkBuzzEnable = New System.Windows.Forms.CheckBox()
        Me.lblBuzzFreq = New System.Windows.Forms.Label()
        Me.txtBuzzFreq = New System.Windows.Forms.TextBox()
        Me.lblBuzzOnMs = New System.Windows.Forms.Label()
        Me.txtBuzzOnMs = New System.Windows.Forms.TextBox()
        Me.lblBuzzOffMs = New System.Windows.Forms.Label()
        Me.txtBuzzOffMs = New System.Windows.Forms.TextBox()
        Me.lblBuzzRepeats = New System.Windows.Forms.Label()
        Me.txtBuzzRepeats = New System.Windows.Forms.TextBox()
        Me.btnSchedNew = New System.Windows.Forms.Button()
        Me.btnSchedAdd = New System.Windows.Forms.Button()
        Me.btnSchedUpdate = New System.Windows.Forms.Button()
        Me.btnSchedDelete = New System.Windows.Forms.Button()
        Me.btnSchedQuery = New System.Windows.Forms.Button()
        Me.btnSchedDisable = New System.Windows.Forms.Button()
        Me.btnSchedApply = New System.Windows.Forms.Button()
        Me.lblSchedStatus = New System.Windows.Forms.Label()
        Me.tabPageSettings = New System.Windows.Forms.TabPage()
        Me.grpExistingSettings = New System.Windows.Forms.GroupBox()
        Me.btnSettingsRefresh = New System.Windows.Forms.Button()
        Me.lblExistingHint = New System.Windows.Forms.Label()
        Me.lblExistingTcpPort = New System.Windows.Forms.Label()
        Me.lblExistingDns = New System.Windows.Forms.Label()
        Me.lblExistingGw = New System.Windows.Forms.Label()
        Me.lblExistingMask = New System.Windows.Forms.Label()
        Me.lblExistingIp = New System.Windows.Forms.Label()
        Me.lblExistingMac = New System.Windows.Forms.Label()
        Me.lblExistingDhcp = New System.Windows.Forms.Label()
        Me.lblExistingApPass = New System.Windows.Forms.Label()
        Me.lblExistingApSsid = New System.Windows.Forms.Label()
        Me.grpUserSettings = New System.Windows.Forms.GroupBox()
        Me.btnSettingsSaveReboot = New System.Windows.Forms.Button()
        Me.btnSettingsApplyToConnection = New System.Windows.Forms.Button()
        Me.lblUserHint = New System.Windows.Forms.Label()
        Me.chkUserDhcp = New System.Windows.Forms.CheckBox()
        Me.txtUserMac = New System.Windows.Forms.TextBox()
        Me.txtUserIp = New System.Windows.Forms.TextBox()
        Me.txtUserMask = New System.Windows.Forms.TextBox()
        Me.txtUserGw = New System.Windows.Forms.TextBox()
        Me.txtUserDns = New System.Windows.Forms.TextBox()
        Me.txtUserTcpPort = New System.Windows.Forms.TextBox()
        Me.txtUserApSsid = New System.Windows.Forms.TextBox()
        Me.txtUserApPass = New System.Windows.Forms.TextBox()
        Me.lblUserMac = New System.Windows.Forms.Label()
        Me.lblUserIp = New System.Windows.Forms.Label()
        Me.lblUserMask = New System.Windows.Forms.Label()
        Me.lblUserGw = New System.Windows.Forms.Label()
        Me.lblUserDns = New System.Windows.Forms.Label()
        Me.lblUserTcpPort = New System.Windows.Forms.Label()
        Me.lblUserApSsid = New System.Windows.Forms.Label()
        Me.lblUserApPass = New System.Windows.Forms.Label()
        Me.grpSerial = New System.Windows.Forms.GroupBox()
        Me.btnSerialApplyIpPort = New System.Windows.Forms.Button()
        Me.txtSerialLog = New System.Windows.Forms.TextBox()
        Me.lblSerialStatus = New System.Windows.Forms.Label()
        Me.btnSerialNetDump = New System.Windows.Forms.Button()
        Me.btnSerialClear = New System.Windows.Forms.Button()
        Me.btnSerialDisconnect = New System.Windows.Forms.Button()
        Me.btnSerialConnect = New System.Windows.Forms.Button()
        Me.btnSerialRefresh = New System.Windows.Forms.Button()
        Me.cboStopBits = New System.Windows.Forms.ComboBox()
        Me.lblStopBits = New System.Windows.Forms.Label()
        Me.cboParity = New System.Windows.Forms.ComboBox()
        Me.lblParity = New System.Windows.Forms.Label()
        Me.cboDataBits = New System.Windows.Forms.ComboBox()
        Me.lblDataBits = New System.Windows.Forms.Label()
        Me.cboBaud = New System.Windows.Forms.ComboBox()
        Me.lblBaud = New System.Windows.Forms.Label()
        Me.cboComPort = New System.Windows.Forms.ComboBox()
        Me.lblComPort = New System.Windows.Forms.Label()
        Me.lblSerialHint = New System.Windows.Forms.Label()
        Me.grpNet.SuspendLayout()
        Me.grpRelays.SuspendLayout()
        Me.grpDI.SuspendLayout()
        Me.grpAnalog.SuspendLayout()
        Me.grpDAC.SuspendLayout()
        Me.grpTemp.SuspendLayout()
        Me.grpRTC.SuspendLayout()
        Me.grpBeep.SuspendLayout()
        Me.grpMisc.SuspendLayout()
        Me.tabMain.SuspendLayout()
        Me.tabPageDashboard.SuspendLayout()
        Me.tabPageSchedule.SuspendLayout()
        Me.grpSchedule.SuspendLayout()
        Me.grpSchedList.SuspendLayout()
        Me.grpDays.SuspendLayout()
        Me.grpSchedRelays.SuspendLayout()
        Me.grpSchedDac.SuspendLayout()
        Me.grpSchedBuzzer.SuspendLayout()
        Me.tabPageSettings.SuspendLayout()
        Me.grpExistingSettings.SuspendLayout()
        Me.grpUserSettings.SuspendLayout()
        Me.grpSerial.SuspendLayout()
        Me.SuspendLayout()
        '
        'txtIp
        '
        Me.txtIp.Location = New System.Drawing.Point(16, 22)
        Me.txtIp.Name = "txtIp"
        Me.txtIp.Size = New System.Drawing.Size(150, 20)
        Me.txtIp.TabIndex = 0
        '
        'txtPort
        '
        Me.txtPort.Location = New System.Drawing.Point(172, 22)
        Me.txtPort.Name = "txtPort"
        Me.txtPort.Size = New System.Drawing.Size(60, 20)
        Me.txtPort.TabIndex = 1
        '
        'btnConnect
        '
        Me.btnConnect.Location = New System.Drawing.Point(238, 20)
        Me.btnConnect.Name = "btnConnect"
        Me.btnConnect.Size = New System.Drawing.Size(75, 23)
        Me.btnConnect.TabIndex = 2
        Me.btnConnect.Text = "Connect"
        Me.btnConnect.UseVisualStyleBackColor = True
        '
        'lblStatus
        '
        Me.lblStatus.AutoSize = True
        Me.lblStatus.Location = New System.Drawing.Point(16, 52)
        Me.lblStatus.Name = "lblStatus"
        Me.lblStatus.Size = New System.Drawing.Size(73, 13)
        Me.lblStatus.TabIndex = 3
        Me.lblStatus.Text = "Disconnected"
        '
        'grpNet
        '
        Me.grpNet.Controls.Add(Me.chkAutoReconnect)
        Me.grpNet.Controls.Add(Me.lblNetInfo)
        Me.grpNet.Controls.Add(Me.btnPing)
        Me.grpNet.Controls.Add(Me.lblStatus)
        Me.grpNet.Controls.Add(Me.btnConnect)
        Me.grpNet.Controls.Add(Me.txtPort)
        Me.grpNet.Controls.Add(Me.txtIp)
        Me.grpNet.Location = New System.Drawing.Point(12, 12)
        Me.grpNet.Name = "grpNet"
        Me.grpNet.Size = New System.Drawing.Size(370, 110)
        Me.grpNet.TabIndex = 0
        Me.grpNet.TabStop = False
        Me.grpNet.Text = "Network"
        '
        'chkAutoReconnect
        '
        Me.chkAutoReconnect.AutoSize = True
        Me.chkAutoReconnect.Location = New System.Drawing.Point(238, 52)
        Me.chkAutoReconnect.Name = "chkAutoReconnect"
        Me.chkAutoReconnect.Size = New System.Drawing.Size(104, 17)
        Me.chkAutoReconnect.TabIndex = 6
        Me.chkAutoReconnect.Text = "Auto Reconnect"
        Me.chkAutoReconnect.UseVisualStyleBackColor = True
        '
        'lblNetInfo
        '
        Me.lblNetInfo.AutoSize = True
        Me.lblNetInfo.Location = New System.Drawing.Point(16, 71)
        Me.lblNetInfo.Name = "lblNetInfo"
        Me.lblNetInfo.Size = New System.Drawing.Size(49, 13)
        Me.lblNetInfo.TabIndex = 5
        Me.lblNetInfo.Text = "NET info"
        '
        'btnPing
        '
        Me.btnPing.Location = New System.Drawing.Point(319, 20)
        Me.btnPing.Name = "btnPing"
        Me.btnPing.Size = New System.Drawing.Size(45, 23)
        Me.btnPing.TabIndex = 4
        Me.btnPing.Text = "Ping"
        Me.btnPing.UseVisualStyleBackColor = True
        '
        'grpRelays
        '
        Me.grpRelays.Controls.Add(Me.chkRel6)
        Me.grpRelays.Controls.Add(Me.chkRel5)
        Me.grpRelays.Controls.Add(Me.chkRel4)
        Me.grpRelays.Controls.Add(Me.chkRel3)
        Me.grpRelays.Controls.Add(Me.chkRel2)
        Me.grpRelays.Controls.Add(Me.chkRel1)
        Me.grpRelays.Location = New System.Drawing.Point(12, 128)
        Me.grpRelays.Name = "grpRelays"
        Me.grpRelays.Size = New System.Drawing.Size(180, 100)
        Me.grpRelays.TabIndex = 1
        Me.grpRelays.TabStop = False
        Me.grpRelays.Text = "Relays"
        '
        'chkRel6
        '
        Me.chkRel6.AutoSize = True
        Me.chkRel6.Location = New System.Drawing.Point(80, 65)
        Me.chkRel6.Name = "chkRel6"
        Me.chkRel6.Size = New System.Drawing.Size(59, 17)
        Me.chkRel6.TabIndex = 5
        Me.chkRel6.Tag = "6"
        Me.chkRel6.Text = "Relay6"
        Me.chkRel6.UseVisualStyleBackColor = True
        '
        'chkRel5
        '
        Me.chkRel5.AutoSize = True
        Me.chkRel5.Location = New System.Drawing.Point(16, 65)
        Me.chkRel5.Name = "chkRel5"
        Me.chkRel5.Size = New System.Drawing.Size(59, 17)
        Me.chkRel5.TabIndex = 4
        Me.chkRel5.Tag = "5"
        Me.chkRel5.Text = "Relay5"
        Me.chkRel5.UseVisualStyleBackColor = True
        '
        'chkRel4
        '
        Me.chkRel4.AutoSize = True
        Me.chkRel4.Location = New System.Drawing.Point(80, 42)
        Me.chkRel4.Name = "chkRel4"
        Me.chkRel4.Size = New System.Drawing.Size(59, 17)
        Me.chkRel4.TabIndex = 3
        Me.chkRel4.Tag = "4"
        Me.chkRel4.Text = "Relay4"
        Me.chkRel4.UseVisualStyleBackColor = True
        '
        'chkRel3
        '
        Me.chkRel3.AutoSize = True
        Me.chkRel3.Location = New System.Drawing.Point(16, 42)
        Me.chkRel3.Name = "chkRel3"
        Me.chkRel3.Size = New System.Drawing.Size(59, 17)
        Me.chkRel3.TabIndex = 2
        Me.chkRel3.Tag = "3"
        Me.chkRel3.Text = "Relay3"
        Me.chkRel3.UseVisualStyleBackColor = True
        '
        'chkRel2
        '
        Me.chkRel2.AutoSize = True
        Me.chkRel2.Location = New System.Drawing.Point(80, 19)
        Me.chkRel2.Name = "chkRel2"
        Me.chkRel2.Size = New System.Drawing.Size(59, 17)
        Me.chkRel2.TabIndex = 1
        Me.chkRel2.Tag = "2"
        Me.chkRel2.Text = "Relay2"
        Me.chkRel2.UseVisualStyleBackColor = True
        '
        'chkRel1
        '
        Me.chkRel1.AutoSize = True
        Me.chkRel1.Location = New System.Drawing.Point(16, 19)
        Me.chkRel1.Name = "chkRel1"
        Me.chkRel1.Size = New System.Drawing.Size(59, 17)
        Me.chkRel1.TabIndex = 0
        Me.chkRel1.Tag = "1"
        Me.chkRel1.Text = "Relay1"
        Me.chkRel1.UseVisualStyleBackColor = True
        '
        'grpDI
        '
        Me.grpDI.Controls.Add(Me.chkDI8)
        Me.grpDI.Controls.Add(Me.chkDI7)
        Me.grpDI.Controls.Add(Me.chkDI6)
        Me.grpDI.Controls.Add(Me.chkDI5)
        Me.grpDI.Controls.Add(Me.chkDI4)
        Me.grpDI.Controls.Add(Me.chkDI3)
        Me.grpDI.Controls.Add(Me.chkDI2)
        Me.grpDI.Controls.Add(Me.chkDI1)
        Me.grpDI.Location = New System.Drawing.Point(198, 128)
        Me.grpDI.Name = "grpDI"
        Me.grpDI.Size = New System.Drawing.Size(184, 100)
        Me.grpDI.TabIndex = 2
        Me.grpDI.TabStop = False
        Me.grpDI.Text = "Digital Inputs"
        '
        'chkDI8
        '
        Me.chkDI8.AutoSize = True
        Me.chkDI8.Enabled = False
        Me.chkDI8.Location = New System.Drawing.Point(134, 42)
        Me.chkDI8.Name = "chkDI8"
        Me.chkDI8.Size = New System.Drawing.Size(46, 17)
        Me.chkDI8.TabIndex = 7
        Me.chkDI8.Text = "DI 8"
        '
        'chkDI7
        '
        Me.chkDI7.AutoSize = True
        Me.chkDI7.Enabled = False
        Me.chkDI7.Location = New System.Drawing.Point(134, 19)
        Me.chkDI7.Name = "chkDI7"
        Me.chkDI7.Size = New System.Drawing.Size(46, 17)
        Me.chkDI7.TabIndex = 6
        Me.chkDI7.Text = "DI 7"
        '
        'chkDI6
        '
        Me.chkDI6.AutoSize = True
        Me.chkDI6.Enabled = False
        Me.chkDI6.Location = New System.Drawing.Point(80, 65)
        Me.chkDI6.Name = "chkDI6"
        Me.chkDI6.Size = New System.Drawing.Size(46, 17)
        Me.chkDI6.TabIndex = 5
        Me.chkDI6.Text = "DI 6"
        '
        'chkDI5
        '
        Me.chkDI5.AutoSize = True
        Me.chkDI5.Enabled = False
        Me.chkDI5.Location = New System.Drawing.Point(16, 65)
        Me.chkDI5.Name = "chkDI5"
        Me.chkDI5.Size = New System.Drawing.Size(46, 17)
        Me.chkDI5.TabIndex = 4
        Me.chkDI5.Text = "DI 5"
        '
        'chkDI4
        '
        Me.chkDI4.AutoSize = True
        Me.chkDI4.Enabled = False
        Me.chkDI4.Location = New System.Drawing.Point(80, 42)
        Me.chkDI4.Name = "chkDI4"
        Me.chkDI4.Size = New System.Drawing.Size(46, 17)
        Me.chkDI4.TabIndex = 3
        Me.chkDI4.Text = "DI 4"
        '
        'chkDI3
        '
        Me.chkDI3.AutoSize = True
        Me.chkDI3.Enabled = False
        Me.chkDI3.Location = New System.Drawing.Point(16, 42)
        Me.chkDI3.Name = "chkDI3"
        Me.chkDI3.Size = New System.Drawing.Size(46, 17)
        Me.chkDI3.TabIndex = 2
        Me.chkDI3.Text = "DI 3"
        '
        'chkDI2
        '
        Me.chkDI2.AutoSize = True
        Me.chkDI2.Enabled = False
        Me.chkDI2.Location = New System.Drawing.Point(80, 19)
        Me.chkDI2.Name = "chkDI2"
        Me.chkDI2.Size = New System.Drawing.Size(46, 17)
        Me.chkDI2.TabIndex = 1
        Me.chkDI2.Text = "DI 2"
        '
        'chkDI1
        '
        Me.chkDI1.AutoSize = True
        Me.chkDI1.Enabled = False
        Me.chkDI1.Location = New System.Drawing.Point(16, 19)
        Me.chkDI1.Name = "chkDI1"
        Me.chkDI1.Size = New System.Drawing.Size(46, 17)
        Me.chkDI1.TabIndex = 0
        Me.chkDI1.Text = "DI 1"
        Me.chkDI1.UseVisualStyleBackColor = True
        '
        'grpAnalog
        '
        Me.grpAnalog.Controls.Add(Me.txtI2)
        Me.grpAnalog.Controls.Add(Me.txtI1)
        Me.grpAnalog.Controls.Add(Me.txtV2)
        Me.grpAnalog.Controls.Add(Me.txtV1)
        Me.grpAnalog.Controls.Add(Me.lblLastSnap)
        Me.grpAnalog.Location = New System.Drawing.Point(12, 234)
        Me.grpAnalog.Name = "grpAnalog"
        Me.grpAnalog.Size = New System.Drawing.Size(370, 90)
        Me.grpAnalog.TabIndex = 3
        Me.grpAnalog.TabStop = False
        Me.grpAnalog.Text = "Analog / Current"
        '
        'txtI2
        '
        Me.txtI2.Location = New System.Drawing.Point(92, 45)
        Me.txtI2.Name = "txtI2"
        Me.txtI2.ReadOnly = True
        Me.txtI2.Size = New System.Drawing.Size(70, 20)
        Me.txtI2.TabIndex = 3
        Me.txtI2.Text = "0.000"
        '
        'txtI1
        '
        Me.txtI1.Location = New System.Drawing.Point(16, 45)
        Me.txtI1.Name = "txtI1"
        Me.txtI1.ReadOnly = True
        Me.txtI1.Size = New System.Drawing.Size(70, 20)
        Me.txtI1.TabIndex = 2
        Me.txtI1.Text = "0.000"
        '
        'txtV2
        '
        Me.txtV2.Location = New System.Drawing.Point(92, 19)
        Me.txtV2.Name = "txtV2"
        Me.txtV2.ReadOnly = True
        Me.txtV2.Size = New System.Drawing.Size(70, 20)
        Me.txtV2.TabIndex = 1
        Me.txtV2.Text = "0.000"
        '
        'txtV1
        '
        Me.txtV1.Location = New System.Drawing.Point(16, 19)
        Me.txtV1.Name = "txtV1"
        Me.txtV1.ReadOnly = True
        Me.txtV1.Size = New System.Drawing.Size(70, 20)
        Me.txtV1.TabIndex = 0
        Me.txtV1.Text = "0.000"
        '
        'lblLastSnap
        '
        Me.lblLastSnap.AutoSize = True
        Me.lblLastSnap.Location = New System.Drawing.Point(200, 22)
        Me.lblLastSnap.Name = "lblLastSnap"
        Me.lblLastSnap.Size = New System.Drawing.Size(96, 13)
        Me.lblLastSnap.TabIndex = 4
        Me.lblLastSnap.Text = "Last snapshot: n/a"
        '
        'grpDAC
        '
        Me.grpDAC.Controls.Add(Me.Label2)
        Me.grpDAC.Controls.Add(Me.Label1)
        Me.grpDAC.Controls.Add(Me.btnDacSet)
        Me.grpDAC.Controls.Add(Me.txtDAC2)
        Me.grpDAC.Controls.Add(Me.txtDAC1)
        Me.grpDAC.Location = New System.Drawing.Point(388, 21)
        Me.grpDAC.Name = "grpDAC"
        Me.grpDAC.Size = New System.Drawing.Size(180, 115)
        Me.grpDAC.TabIndex = 4
        Me.grpDAC.TabStop = False
        Me.grpDAC.Text = "DAC Outputs (mV)"
        '
        'Label2
        '
        Me.Label2.AutoSize = True
        Me.Label2.Location = New System.Drawing.Point(13, 48)
        Me.Label2.Name = "Label2"
        Me.Label2.Size = New System.Drawing.Size(55, 13)
        Me.Label2.TabIndex = 6
        Me.Label2.Text = "Channel 2"
        '
        'Label1
        '
        Me.Label1.AutoSize = True
        Me.Label1.Location = New System.Drawing.Point(13, 22)
        Me.Label1.Name = "Label1"
        Me.Label1.Size = New System.Drawing.Size(55, 13)
        Me.Label1.TabIndex = 5
        Me.Label1.Text = "Channel 1"
        '
        'btnDacSet
        '
        Me.btnDacSet.Location = New System.Drawing.Point(47, 83)
        Me.btnDacSet.Name = "btnDacSet"
        Me.btnDacSet.Size = New System.Drawing.Size(75, 23)
        Me.btnDacSet.TabIndex = 2
        Me.btnDacSet.Text = "Apply"
        Me.btnDacSet.UseVisualStyleBackColor = True
        '
        'txtDAC2
        '
        Me.txtDAC2.Location = New System.Drawing.Point(99, 45)
        Me.txtDAC2.Name = "txtDAC2"
        Me.txtDAC2.Size = New System.Drawing.Size(60, 20)
        Me.txtDAC2.TabIndex = 1
        Me.txtDAC2.Text = "0"
        '
        'txtDAC1
        '
        Me.txtDAC1.Location = New System.Drawing.Point(99, 19)
        Me.txtDAC1.Name = "txtDAC1"
        Me.txtDAC1.Size = New System.Drawing.Size(60, 20)
        Me.txtDAC1.TabIndex = 0
        Me.txtDAC1.Text = "0"
        '
        'grpTemp
        '
        Me.grpTemp.Controls.Add(Me.Label10)
        Me.grpTemp.Controls.Add(Me.Label11)
        Me.grpTemp.Controls.Add(Me.Label9)
        Me.grpTemp.Controls.Add(Me.Label6)
        Me.grpTemp.Controls.Add(Me.Label7)
        Me.grpTemp.Controls.Add(Me.Label8)
        Me.grpTemp.Controls.Add(Me.Label5)
        Me.grpTemp.Controls.Add(Me.Label4)
        Me.grpTemp.Controls.Add(Me.Label3)
        Me.grpTemp.Controls.Add(Me.txtDs0)
        Me.grpTemp.Controls.Add(Me.txtDsCount)
        Me.grpTemp.Controls.Add(Me.txtH2)
        Me.grpTemp.Controls.Add(Me.txtT2)
        Me.grpTemp.Controls.Add(Me.txtH1)
        Me.grpTemp.Controls.Add(Me.txtT1)
        Me.grpTemp.Location = New System.Drawing.Point(388, 140)
        Me.grpTemp.Name = "grpTemp"
        Me.grpTemp.Size = New System.Drawing.Size(180, 184)
        Me.grpTemp.TabIndex = 5
        Me.grpTemp.TabStop = False
        Me.grpTemp.Text = "Temp / Humidity / DS18"
        '
        'Label10
        '
        Me.Label10.AutoSize = True
        Me.Label10.Location = New System.Drawing.Point(127, 162)
        Me.Label10.Name = "Label10"
        Me.Label10.Size = New System.Drawing.Size(34, 13)
        Me.Label10.TabIndex = 14
        Me.Label10.Text = "Temp"
        '
        'Label11
        '
        Me.Label11.AutoSize = True
        Me.Label11.Location = New System.Drawing.Point(39, 162)
        Me.Label11.Name = "Label11"
        Me.Label11.Size = New System.Drawing.Size(35, 13)
        Me.Label11.TabIndex = 13
        Me.Label11.Text = "Count"
        '
        'Label9
        '
        Me.Label9.AutoSize = True
        Me.Label9.Location = New System.Drawing.Point(26, 123)
        Me.Label9.Name = "Label9"
        Me.Label9.Size = New System.Drawing.Size(89, 13)
        Me.Label9.TabIndex = 12
        Me.Label9.Text = "DS18B20 Sensor"
        '
        'Label6
        '
        Me.Label6.AutoSize = True
        Me.Label6.Location = New System.Drawing.Point(93, 96)
        Me.Label6.Name = "Label6"
        Me.Label6.Size = New System.Drawing.Size(15, 13)
        Me.Label6.TabIndex = 11
        Me.Label6.Text = "H"
        '
        'Label7
        '
        Me.Label7.AutoSize = True
        Me.Label7.Location = New System.Drawing.Point(10, 96)
        Me.Label7.Name = "Label7"
        Me.Label7.Size = New System.Drawing.Size(17, 13)
        Me.Label7.TabIndex = 10
        Me.Label7.Text = "T "
        '
        'Label8
        '
        Me.Label8.AutoSize = True
        Me.Label8.Location = New System.Drawing.Point(26, 75)
        Me.Label8.Name = "Label8"
        Me.Label8.Size = New System.Drawing.Size(104, 13)
        Me.Label8.TabIndex = 9
        Me.Label8.Text = "DHT11/22 Sensor 1"
        '
        'Label5
        '
        Me.Label5.AutoSize = True
        Me.Label5.Location = New System.Drawing.Point(93, 44)
        Me.Label5.Name = "Label5"
        Me.Label5.Size = New System.Drawing.Size(15, 13)
        Me.Label5.TabIndex = 8
        Me.Label5.Text = "H"
        '
        'Label4
        '
        Me.Label4.AutoSize = True
        Me.Label4.Location = New System.Drawing.Point(10, 44)
        Me.Label4.Name = "Label4"
        Me.Label4.Size = New System.Drawing.Size(17, 13)
        Me.Label4.TabIndex = 7
        Me.Label4.Text = "T "
        '
        'Label3
        '
        Me.Label3.AutoSize = True
        Me.Label3.Location = New System.Drawing.Point(26, 21)
        Me.Label3.Name = "Label3"
        Me.Label3.Size = New System.Drawing.Size(104, 13)
        Me.Label3.TabIndex = 6
        Me.Label3.Text = "DHT11/22 Sensor 1"
        '
        'txtDs0
        '
        Me.txtDs0.Location = New System.Drawing.Point(113, 139)
        Me.txtDs0.Name = "txtDs0"
        Me.txtDs0.ReadOnly = True
        Me.txtDs0.Size = New System.Drawing.Size(60, 20)
        Me.txtDs0.TabIndex = 0
        '
        'txtDsCount
        '
        Me.txtDsCount.Location = New System.Drawing.Point(27, 139)
        Me.txtDsCount.Name = "txtDsCount"
        Me.txtDsCount.ReadOnly = True
        Me.txtDsCount.Size = New System.Drawing.Size(60, 20)
        Me.txtDsCount.TabIndex = 1
        '
        'txtH2
        '
        Me.txtH2.Location = New System.Drawing.Point(113, 93)
        Me.txtH2.Name = "txtH2"
        Me.txtH2.ReadOnly = True
        Me.txtH2.Size = New System.Drawing.Size(60, 20)
        Me.txtH2.TabIndex = 2
        '
        'txtT2
        '
        Me.txtT2.Location = New System.Drawing.Point(27, 93)
        Me.txtT2.Name = "txtT2"
        Me.txtT2.ReadOnly = True
        Me.txtT2.Size = New System.Drawing.Size(60, 20)
        Me.txtT2.TabIndex = 3
        '
        'txtH1
        '
        Me.txtH1.Location = New System.Drawing.Point(113, 41)
        Me.txtH1.Name = "txtH1"
        Me.txtH1.ReadOnly = True
        Me.txtH1.Size = New System.Drawing.Size(60, 20)
        Me.txtH1.TabIndex = 4
        '
        'txtT1
        '
        Me.txtT1.Location = New System.Drawing.Point(27, 41)
        Me.txtT1.Name = "txtT1"
        Me.txtT1.ReadOnly = True
        Me.txtT1.Size = New System.Drawing.Size(60, 20)
        Me.txtT1.TabIndex = 5
        '
        'grpRTC
        '
        Me.grpRTC.Controls.Add(Me.btnRtcSet)
        Me.grpRTC.Controls.Add(Me.btnRtcGet)
        Me.grpRTC.Controls.Add(Me.txtRtcSet)
        Me.grpRTC.Controls.Add(Me.txtRtcCurrent)
        Me.grpRTC.Location = New System.Drawing.Point(12, 330)
        Me.grpRTC.Name = "grpRTC"
        Me.grpRTC.Size = New System.Drawing.Size(370, 80)
        Me.grpRTC.TabIndex = 6
        Me.grpRTC.TabStop = False
        Me.grpRTC.Text = "RTC"
        '
        'btnRtcSet
        '
        Me.btnRtcSet.Location = New System.Drawing.Point(214, 43)
        Me.btnRtcSet.Name = "btnRtcSet"
        Me.btnRtcSet.Size = New System.Drawing.Size(60, 23)
        Me.btnRtcSet.TabIndex = 3
        Me.btnRtcSet.Text = "Set"
        Me.btnRtcSet.UseVisualStyleBackColor = True
        '
        'btnRtcGet
        '
        Me.btnRtcGet.Location = New System.Drawing.Point(214, 17)
        Me.btnRtcGet.Name = "btnRtcGet"
        Me.btnRtcGet.Size = New System.Drawing.Size(60, 23)
        Me.btnRtcGet.TabIndex = 2
        Me.btnRtcGet.Text = "Get"
        Me.btnRtcGet.UseVisualStyleBackColor = True
        '
        'txtRtcSet
        '
        Me.txtRtcSet.Location = New System.Drawing.Point(16, 45)
        Me.txtRtcSet.Name = "txtRtcSet"
        Me.txtRtcSet.Size = New System.Drawing.Size(180, 20)
        Me.txtRtcSet.TabIndex = 1
        '
        'txtRtcCurrent
        '
        Me.txtRtcCurrent.Location = New System.Drawing.Point(16, 19)
        Me.txtRtcCurrent.Name = "txtRtcCurrent"
        Me.txtRtcCurrent.ReadOnly = True
        Me.txtRtcCurrent.Size = New System.Drawing.Size(180, 20)
        Me.txtRtcCurrent.TabIndex = 0
        '
        'grpBeep
        '
        Me.grpBeep.Controls.Add(Me.btnBuzzOff)
        Me.grpBeep.Controls.Add(Me.btnBeep)
        Me.grpBeep.Controls.Add(Me.txtBeepMs)
        Me.grpBeep.Controls.Add(Me.txtBeepFreq)
        Me.grpBeep.Location = New System.Drawing.Point(388, 330)
        Me.grpBeep.Name = "grpBeep"
        Me.grpBeep.Size = New System.Drawing.Size(180, 80)
        Me.grpBeep.TabIndex = 7
        Me.grpBeep.TabStop = False
        Me.grpBeep.Text = "Buzzer"
        '
        'btnBuzzOff
        '
        Me.btnBuzzOff.Location = New System.Drawing.Point(82, 45)
        Me.btnBuzzOff.Name = "btnBuzzOff"
        Me.btnBuzzOff.Size = New System.Drawing.Size(60, 23)
        Me.btnBuzzOff.TabIndex = 3
        Me.btnBuzzOff.Text = "Off"
        Me.btnBuzzOff.UseVisualStyleBackColor = True
        '
        'btnBeep
        '
        Me.btnBeep.Location = New System.Drawing.Point(16, 45)
        Me.btnBeep.Name = "btnBeep"
        Me.btnBeep.Size = New System.Drawing.Size(60, 23)
        Me.btnBeep.TabIndex = 2
        Me.btnBeep.Text = "Beep"
        Me.btnBeep.UseVisualStyleBackColor = True
        '
        'txtBeepMs
        '
        Me.txtBeepMs.Location = New System.Drawing.Point(82, 19)
        Me.txtBeepMs.Name = "txtBeepMs"
        Me.txtBeepMs.Size = New System.Drawing.Size(60, 20)
        Me.txtBeepMs.TabIndex = 1
        Me.txtBeepMs.Text = "200"
        '
        'txtBeepFreq
        '
        Me.txtBeepFreq.Location = New System.Drawing.Point(16, 19)
        Me.txtBeepFreq.Name = "txtBeepFreq"
        Me.txtBeepFreq.Size = New System.Drawing.Size(60, 20)
        Me.txtBeepFreq.TabIndex = 0
        Me.txtBeepFreq.Text = "1000"
        '
        'grpMisc
        '
        Me.grpMisc.Controls.Add(Me.btnSnapOnce)
        Me.grpMisc.Controls.Add(Me.btnUnitApply)
        Me.grpMisc.Controls.Add(Me.cboTempUnit)
        Me.grpMisc.Controls.Add(Me.chkAutoPoll)
        Me.grpMisc.Location = New System.Drawing.Point(12, 416)
        Me.grpMisc.Name = "grpMisc"
        Me.grpMisc.Size = New System.Drawing.Size(556, 50)
        Me.grpMisc.TabIndex = 8
        Me.grpMisc.TabStop = False
        Me.grpMisc.Text = "Misc / Polling"
        '
        'btnSnapOnce
        '
        Me.btnSnapOnce.Location = New System.Drawing.Point(270, 18)
        Me.btnSnapOnce.Name = "btnSnapOnce"
        Me.btnSnapOnce.Size = New System.Drawing.Size(93, 23)
        Me.btnSnapOnce.TabIndex = 3
        Me.btnSnapOnce.Text = "Snapshot Once"
        Me.btnSnapOnce.UseVisualStyleBackColor = True
        '
        'btnUnitApply
        '
        Me.btnUnitApply.Location = New System.Drawing.Point(176, 18)
        Me.btnUnitApply.Name = "btnUnitApply"
        Me.btnUnitApply.Size = New System.Drawing.Size(75, 23)
        Me.btnUnitApply.TabIndex = 2
        Me.btnUnitApply.Text = "Unit"
        Me.btnUnitApply.UseVisualStyleBackColor = True
        '
        'cboTempUnit
        '
        Me.cboTempUnit.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList
        Me.cboTempUnit.Location = New System.Drawing.Point(110, 20)
        Me.cboTempUnit.Name = "cboTempUnit"
        Me.cboTempUnit.Size = New System.Drawing.Size(60, 21)
        Me.cboTempUnit.TabIndex = 1
        '
        'chkAutoPoll
        '
        Me.chkAutoPoll.AutoSize = True
        Me.chkAutoPoll.Location = New System.Drawing.Point(16, 22)
        Me.chkAutoPoll.Name = "chkAutoPoll"
        Me.chkAutoPoll.Size = New System.Drawing.Size(68, 17)
        Me.chkAutoPoll.TabIndex = 0
        Me.chkAutoPoll.Text = "Auto Poll"
        Me.chkAutoPoll.UseVisualStyleBackColor = True
        '
        'txtLog
        '
        Me.txtLog.BackColor = System.Drawing.Color.MidnightBlue
        Me.txtLog.ForeColor = System.Drawing.Color.LemonChiffon
        Me.txtLog.Location = New System.Drawing.Point(12, 472)
        Me.txtLog.Multiline = True
        Me.txtLog.Name = "txtLog"
        Me.txtLog.ScrollBars = System.Windows.Forms.ScrollBars.Vertical
        Me.txtLog.Size = New System.Drawing.Size(556, 150)
        Me.txtLog.TabIndex = 9
        '
        'tabMain
        '
        Me.tabMain.Controls.Add(Me.tabPageDashboard)
        Me.tabMain.Controls.Add(Me.tabPageSchedule)
        Me.tabMain.Controls.Add(Me.tabPageSettings)
        Me.tabMain.Location = New System.Drawing.Point(0, 0)
        Me.tabMain.Name = "tabMain"
        Me.tabMain.SelectedIndex = 0
        Me.tabMain.Size = New System.Drawing.Size(592, 670)
        Me.tabMain.TabIndex = 100
        '
        'tabPageDashboard
        '
        Me.tabPageDashboard.Controls.Add(Me.grpNet)
        Me.tabPageDashboard.Controls.Add(Me.grpRelays)
        Me.tabPageDashboard.Controls.Add(Me.grpDI)
        Me.tabPageDashboard.Controls.Add(Me.grpAnalog)
        Me.tabPageDashboard.Controls.Add(Me.grpDAC)
        Me.tabPageDashboard.Controls.Add(Me.grpTemp)
        Me.tabPageDashboard.Controls.Add(Me.grpRTC)
        Me.tabPageDashboard.Controls.Add(Me.grpBeep)
        Me.tabPageDashboard.Controls.Add(Me.grpMisc)
        Me.tabPageDashboard.Controls.Add(Me.txtLog)
        Me.tabPageDashboard.Location = New System.Drawing.Point(4, 22)
        Me.tabPageDashboard.Name = "tabPageDashboard"
        Me.tabPageDashboard.Size = New System.Drawing.Size(584, 644)
        Me.tabPageDashboard.TabIndex = 0
        Me.tabPageDashboard.Text = "Dashboard"
        Me.tabPageDashboard.UseVisualStyleBackColor = True
        '
        'tabPageSchedule
        '
        Me.tabPageSchedule.Controls.Add(Me.grpSchedule)
        Me.tabPageSchedule.Location = New System.Drawing.Point(4, 22)
        Me.tabPageSchedule.Name = "tabPageSchedule"
        Me.tabPageSchedule.Size = New System.Drawing.Size(584, 644)
        Me.tabPageSchedule.TabIndex = 1
        Me.tabPageSchedule.Text = "Schedule"
        Me.tabPageSchedule.UseVisualStyleBackColor = True
        '
        'grpSchedule
        '
        Me.grpSchedule.Controls.Add(Me.grpSchedList)
        Me.grpSchedule.Controls.Add(Me.lblSchedMode)
        Me.grpSchedule.Controls.Add(Me.cboSchedMode)
        Me.grpSchedule.Controls.Add(Me.chkSchedEnabled)
        Me.grpSchedule.Controls.Add(Me.lblSchedId)
        Me.grpSchedule.Controls.Add(Me.txtSchedId)
        Me.grpSchedule.Controls.Add(Me.grpDays)
        Me.grpSchedule.Controls.Add(Me.lblTrigDI)
        Me.grpSchedule.Controls.Add(Me.cboTrigDI)
        Me.grpSchedule.Controls.Add(Me.lblEdge)
        Me.grpSchedule.Controls.Add(Me.cboEdge)
        Me.grpSchedule.Controls.Add(Me.lblWinStart)
        Me.grpSchedule.Controls.Add(Me.txtWinStart)
        Me.grpSchedule.Controls.Add(Me.lblWinEnd)
        Me.grpSchedule.Controls.Add(Me.txtWinEnd)
        Me.grpSchedule.Controls.Add(Me.chkRecurring)
        Me.grpSchedule.Controls.Add(Me.grpSchedRelays)
        Me.grpSchedule.Controls.Add(Me.grpSchedDac)
        Me.grpSchedule.Controls.Add(Me.grpSchedBuzzer)
        Me.grpSchedule.Controls.Add(Me.btnSchedNew)
        Me.grpSchedule.Controls.Add(Me.btnSchedAdd)
        Me.grpSchedule.Controls.Add(Me.btnSchedUpdate)
        Me.grpSchedule.Controls.Add(Me.btnSchedDelete)
        Me.grpSchedule.Controls.Add(Me.btnSchedQuery)
        Me.grpSchedule.Controls.Add(Me.btnSchedDisable)
        Me.grpSchedule.Controls.Add(Me.btnSchedApply)
        Me.grpSchedule.Controls.Add(Me.lblSchedStatus)
        Me.grpSchedule.Location = New System.Drawing.Point(12, 12)
        Me.grpSchedule.Name = "grpSchedule"
        Me.grpSchedule.Size = New System.Drawing.Size(556, 610)
        Me.grpSchedule.TabIndex = 200
        Me.grpSchedule.TabStop = False
        Me.grpSchedule.Text = "Scheduler"
        '
        'grpSchedList
        '
        Me.grpSchedList.Controls.Add(Me.lstSchedules)
        Me.grpSchedList.Location = New System.Drawing.Point(16, 20)
        Me.grpSchedList.Name = "grpSchedList"
        Me.grpSchedList.Size = New System.Drawing.Size(520, 150)
        Me.grpSchedList.TabIndex = 0
        Me.grpSchedList.TabStop = False
        Me.grpSchedList.Text = "Schedules List (stored on ESP32)"
        '
        'lstSchedules
        '
        Me.lstSchedules.Columns.AddRange(New System.Windows.Forms.ColumnHeader() {Me.colId, Me.colEnabled, Me.colMode, Me.colDays, Me.colWindow, Me.colTrigger, Me.colActions})
        Me.lstSchedules.FullRowSelect = True
        Me.lstSchedules.GridLines = True
        Me.lstSchedules.HideSelection = False
        Me.lstSchedules.Location = New System.Drawing.Point(10, 20)
        Me.lstSchedules.MultiSelect = False
        Me.lstSchedules.Name = "lstSchedules"
        Me.lstSchedules.Size = New System.Drawing.Size(500, 120)
        Me.lstSchedules.TabIndex = 0
        Me.lstSchedules.UseCompatibleStateImageBehavior = False
        Me.lstSchedules.View = System.Windows.Forms.View.Details
        '
        'colId
        '
        Me.colId.Text = "ID"
        Me.colId.Width = 35
        '
        'colEnabled
        '
        Me.colEnabled.Text = "EN"
        Me.colEnabled.Width = 35
        '
        'colMode
        '
        Me.colMode.Text = "Mode"
        Me.colMode.Width = 80
        '
        'colDays
        '
        Me.colDays.Text = "Days"
        Me.colDays.Width = 80
        '
        'colWindow
        '
        Me.colWindow.Text = "Window"
        Me.colWindow.Width = 90
        '
        'colTrigger
        '
        Me.colTrigger.Text = "Trigger"
        Me.colTrigger.Width = 80
        '
        'colActions
        '
        Me.colActions.Text = "Actions"
        Me.colActions.Width = 180
        '
        'lblSchedMode
        '
        Me.lblSchedMode.AutoSize = True
        Me.lblSchedMode.Location = New System.Drawing.Point(16, 210)
        Me.lblSchedMode.Name = "lblSchedMode"
        Me.lblSchedMode.Size = New System.Drawing.Size(37, 13)
        Me.lblSchedMode.TabIndex = 10
        Me.lblSchedMode.Text = "Mode:"
        '
        'cboSchedMode
        '
        Me.cboSchedMode.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList
        Me.cboSchedMode.Location = New System.Drawing.Point(110, 206)
        Me.cboSchedMode.Name = "cboSchedMode"
        Me.cboSchedMode.Size = New System.Drawing.Size(220, 21)
        Me.cboSchedMode.TabIndex = 11
        '
        'chkSchedEnabled
        '
        Me.chkSchedEnabled.AutoSize = True
        Me.chkSchedEnabled.Checked = True
        Me.chkSchedEnabled.CheckState = System.Windows.Forms.CheckState.Checked
        Me.chkSchedEnabled.Location = New System.Drawing.Point(110, 178)
        Me.chkSchedEnabled.Name = "chkSchedEnabled"
        Me.chkSchedEnabled.Size = New System.Drawing.Size(65, 17)
        Me.chkSchedEnabled.TabIndex = 3
        Me.chkSchedEnabled.Text = "Enabled"
        Me.chkSchedEnabled.UseVisualStyleBackColor = True
        '
        'lblSchedId
        '
        Me.lblSchedId.AutoSize = True
        Me.lblSchedId.Location = New System.Drawing.Point(16, 180)
        Me.lblSchedId.Name = "lblSchedId"
        Me.lblSchedId.Size = New System.Drawing.Size(21, 13)
        Me.lblSchedId.TabIndex = 1
        Me.lblSchedId.Text = "ID:"
        '
        'txtSchedId
        '
        Me.txtSchedId.Location = New System.Drawing.Point(45, 176)
        Me.txtSchedId.Name = "txtSchedId"
        Me.txtSchedId.Size = New System.Drawing.Size(40, 20)
        Me.txtSchedId.TabIndex = 2
        Me.txtSchedId.Text = "0"
        '
        'grpDays
        '
        Me.grpDays.Controls.Add(Me.chkMon)
        Me.grpDays.Controls.Add(Me.chkTue)
        Me.grpDays.Controls.Add(Me.chkWed)
        Me.grpDays.Controls.Add(Me.chkThu)
        Me.grpDays.Controls.Add(Me.chkFri)
        Me.grpDays.Controls.Add(Me.chkSat)
        Me.grpDays.Controls.Add(Me.chkSun)
        Me.grpDays.Location = New System.Drawing.Point(16, 235)
        Me.grpDays.Name = "grpDays"
        Me.grpDays.Size = New System.Drawing.Size(520, 45)
        Me.grpDays.TabIndex = 12
        Me.grpDays.TabStop = False
        Me.grpDays.Text = "Days of Week"
        '
        'chkMon
        '
        Me.chkMon.Location = New System.Drawing.Point(10, 18)
        Me.chkMon.Name = "chkMon"
        Me.chkMon.Size = New System.Drawing.Size(50, 18)
        Me.chkMon.TabIndex = 0
        Me.chkMon.Text = "Mon"
        '
        'chkTue
        '
        Me.chkTue.Location = New System.Drawing.Point(65, 18)
        Me.chkTue.Name = "chkTue"
        Me.chkTue.Size = New System.Drawing.Size(50, 18)
        Me.chkTue.TabIndex = 1
        Me.chkTue.Text = "Tue"
        '
        'chkWed
        '
        Me.chkWed.Location = New System.Drawing.Point(120, 18)
        Me.chkWed.Name = "chkWed"
        Me.chkWed.Size = New System.Drawing.Size(55, 18)
        Me.chkWed.TabIndex = 2
        Me.chkWed.Text = "Wed"
        '
        'chkThu
        '
        Me.chkThu.Location = New System.Drawing.Point(180, 18)
        Me.chkThu.Name = "chkThu"
        Me.chkThu.Size = New System.Drawing.Size(55, 18)
        Me.chkThu.TabIndex = 3
        Me.chkThu.Text = "Thu"
        '
        'chkFri
        '
        Me.chkFri.Location = New System.Drawing.Point(240, 18)
        Me.chkFri.Name = "chkFri"
        Me.chkFri.Size = New System.Drawing.Size(45, 18)
        Me.chkFri.TabIndex = 4
        Me.chkFri.Text = "Fri"
        '
        'chkSat
        '
        Me.chkSat.Location = New System.Drawing.Point(290, 18)
        Me.chkSat.Name = "chkSat"
        Me.chkSat.Size = New System.Drawing.Size(50, 18)
        Me.chkSat.TabIndex = 5
        Me.chkSat.Text = "Sat"
        '
        'chkSun
        '
        Me.chkSun.Location = New System.Drawing.Point(345, 18)
        Me.chkSun.Name = "chkSun"
        Me.chkSun.Size = New System.Drawing.Size(50, 18)
        Me.chkSun.TabIndex = 6
        Me.chkSun.Text = "Sun"
        '
        'lblTrigDI
        '
        Me.lblTrigDI.AutoSize = True
        Me.lblTrigDI.Location = New System.Drawing.Point(16, 290)
        Me.lblTrigDI.Name = "lblTrigDI"
        Me.lblTrigDI.Size = New System.Drawing.Size(57, 13)
        Me.lblTrigDI.TabIndex = 13
        Me.lblTrigDI.Text = "Trigger DI:"
        '
        'cboTrigDI
        '
        Me.cboTrigDI.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList
        Me.cboTrigDI.Location = New System.Drawing.Point(110, 286)
        Me.cboTrigDI.Name = "cboTrigDI"
        Me.cboTrigDI.Size = New System.Drawing.Size(80, 21)
        Me.cboTrigDI.TabIndex = 14
        '
        'lblEdge
        '
        Me.lblEdge.AutoSize = True
        Me.lblEdge.Location = New System.Drawing.Point(210, 290)
        Me.lblEdge.Name = "lblEdge"
        Me.lblEdge.Size = New System.Drawing.Size(35, 13)
        Me.lblEdge.TabIndex = 15
        Me.lblEdge.Text = "Edge:"
        '
        'cboEdge
        '
        Me.cboEdge.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList
        Me.cboEdge.Location = New System.Drawing.Point(255, 286)
        Me.cboEdge.Name = "cboEdge"
        Me.cboEdge.Size = New System.Drawing.Size(75, 21)
        Me.cboEdge.TabIndex = 16
        '
        'lblWinStart
        '
        Me.lblWinStart.AutoSize = True
        Me.lblWinStart.Location = New System.Drawing.Point(16, 318)
        Me.lblWinStart.Name = "lblWinStart"
        Me.lblWinStart.Size = New System.Drawing.Size(78, 13)
        Me.lblWinStart.TabIndex = 17
        Me.lblWinStart.Text = "Start (HH:MM):"
        '
        'txtWinStart
        '
        Me.txtWinStart.Location = New System.Drawing.Point(110, 314)
        Me.txtWinStart.Name = "txtWinStart"
        Me.txtWinStart.Size = New System.Drawing.Size(80, 20)
        Me.txtWinStart.TabIndex = 18
        Me.txtWinStart.Text = "08:00"
        '
        'lblWinEnd
        '
        Me.lblWinEnd.AutoSize = True
        Me.lblWinEnd.Location = New System.Drawing.Point(210, 318)
        Me.lblWinEnd.Name = "lblWinEnd"
        Me.lblWinEnd.Size = New System.Drawing.Size(75, 13)
        Me.lblWinEnd.TabIndex = 19
        Me.lblWinEnd.Text = "End (HH:MM):"
        '
        'txtWinEnd
        '
        Me.txtWinEnd.Location = New System.Drawing.Point(310, 314)
        Me.txtWinEnd.Name = "txtWinEnd"
        Me.txtWinEnd.Size = New System.Drawing.Size(80, 20)
        Me.txtWinEnd.TabIndex = 20
        Me.txtWinEnd.Text = "17:00"
        '
        'chkRecurring
        '
        Me.chkRecurring.AutoSize = True
        Me.chkRecurring.Checked = True
        Me.chkRecurring.CheckState = System.Windows.Forms.CheckState.Checked
        Me.chkRecurring.Location = New System.Drawing.Point(410, 316)
        Me.chkRecurring.Name = "chkRecurring"
        Me.chkRecurring.Size = New System.Drawing.Size(72, 17)
        Me.chkRecurring.TabIndex = 21
        Me.chkRecurring.Text = "Recurring"
        Me.chkRecurring.UseVisualStyleBackColor = True
        '
        'grpSchedRelays
        '
        Me.grpSchedRelays.Controls.Add(Me.chkSRel1)
        Me.grpSchedRelays.Controls.Add(Me.chkSRel2)
        Me.grpSchedRelays.Controls.Add(Me.chkSRel3)
        Me.grpSchedRelays.Controls.Add(Me.chkSRel4)
        Me.grpSchedRelays.Controls.Add(Me.chkSRel5)
        Me.grpSchedRelays.Controls.Add(Me.chkSRel6)
        Me.grpSchedRelays.Controls.Add(Me.lblRelayAction)
        Me.grpSchedRelays.Controls.Add(Me.cboRelayAction)
        Me.grpSchedRelays.Location = New System.Drawing.Point(16, 345)
        Me.grpSchedRelays.Name = "grpSchedRelays"
        Me.grpSchedRelays.Size = New System.Drawing.Size(520, 70)
        Me.grpSchedRelays.TabIndex = 22
        Me.grpSchedRelays.TabStop = False
        Me.grpSchedRelays.Text = "Digital Outputs (Relays) Action"
        '
        'chkSRel1
        '
        Me.chkSRel1.Location = New System.Drawing.Point(16, 20)
        Me.chkSRel1.Name = "chkSRel1"
        Me.chkSRel1.Size = New System.Drawing.Size(45, 17)
        Me.chkSRel1.TabIndex = 0
        Me.chkSRel1.Text = "R1"
        '
        'chkSRel2
        '
        Me.chkSRel2.Location = New System.Drawing.Point(62, 20)
        Me.chkSRel2.Name = "chkSRel2"
        Me.chkSRel2.Size = New System.Drawing.Size(45, 17)
        Me.chkSRel2.TabIndex = 1
        Me.chkSRel2.Text = "R2"
        '
        'chkSRel3
        '
        Me.chkSRel3.Location = New System.Drawing.Point(108, 20)
        Me.chkSRel3.Name = "chkSRel3"
        Me.chkSRel3.Size = New System.Drawing.Size(45, 17)
        Me.chkSRel3.TabIndex = 2
        Me.chkSRel3.Text = "R3"
        '
        'chkSRel4
        '
        Me.chkSRel4.Location = New System.Drawing.Point(154, 20)
        Me.chkSRel4.Name = "chkSRel4"
        Me.chkSRel4.Size = New System.Drawing.Size(45, 17)
        Me.chkSRel4.TabIndex = 3
        Me.chkSRel4.Text = "R4"
        '
        'chkSRel5
        '
        Me.chkSRel5.Location = New System.Drawing.Point(200, 20)
        Me.chkSRel5.Name = "chkSRel5"
        Me.chkSRel5.Size = New System.Drawing.Size(45, 17)
        Me.chkSRel5.TabIndex = 4
        Me.chkSRel5.Text = "R5"
        '
        'chkSRel6
        '
        Me.chkSRel6.Location = New System.Drawing.Point(246, 20)
        Me.chkSRel6.Name = "chkSRel6"
        Me.chkSRel6.Size = New System.Drawing.Size(45, 17)
        Me.chkSRel6.TabIndex = 5
        Me.chkSRel6.Text = "R6"
        '
        'lblRelayAction
        '
        Me.lblRelayAction.AutoSize = True
        Me.lblRelayAction.Location = New System.Drawing.Point(310, 20)
        Me.lblRelayAction.Name = "lblRelayAction"
        Me.lblRelayAction.Size = New System.Drawing.Size(40, 13)
        Me.lblRelayAction.TabIndex = 6
        Me.lblRelayAction.Text = "Action:"
        '
        'cboRelayAction
        '
        Me.cboRelayAction.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList
        Me.cboRelayAction.Location = New System.Drawing.Point(360, 16)
        Me.cboRelayAction.Name = "cboRelayAction"
        Me.cboRelayAction.Size = New System.Drawing.Size(140, 21)
        Me.cboRelayAction.TabIndex = 7
        '
        'grpSchedDac
        '
        Me.grpSchedDac.Controls.Add(Me.lblDac1Mv)
        Me.grpSchedDac.Controls.Add(Me.txtSchedDac1Mv)
        Me.grpSchedDac.Controls.Add(Me.lblDac2Mv)
        Me.grpSchedDac.Controls.Add(Me.txtSchedDac2Mv)
        Me.grpSchedDac.Controls.Add(Me.lblDac1Range)
        Me.grpSchedDac.Controls.Add(Me.cboDac1Range)
        Me.grpSchedDac.Controls.Add(Me.lblDac2Range)
        Me.grpSchedDac.Controls.Add(Me.cboDac2Range)
        Me.grpSchedDac.Location = New System.Drawing.Point(16, 420)
        Me.grpSchedDac.Name = "grpSchedDac"
        Me.grpSchedDac.Size = New System.Drawing.Size(520, 70)
        Me.grpSchedDac.TabIndex = 23
        Me.grpSchedDac.TabStop = False
        Me.grpSchedDac.Text = "Analog Outputs (DAC)"
        '
        'lblDac1Mv
        '
        Me.lblDac1Mv.AutoSize = True
        Me.lblDac1Mv.Location = New System.Drawing.Point(16, 22)
        Me.lblDac1Mv.Name = "lblDac1Mv"
        Me.lblDac1Mv.Size = New System.Drawing.Size(56, 13)
        Me.lblDac1Mv.TabIndex = 0
        Me.lblDac1Mv.Text = "DAC1 mV:"
        '
        'txtSchedDac1Mv
        '
        Me.txtSchedDac1Mv.Location = New System.Drawing.Point(75, 18)
        Me.txtSchedDac1Mv.Name = "txtSchedDac1Mv"
        Me.txtSchedDac1Mv.Size = New System.Drawing.Size(60, 20)
        Me.txtSchedDac1Mv.TabIndex = 1
        Me.txtSchedDac1Mv.Text = "0"
        '
        'lblDac2Mv
        '
        Me.lblDac2Mv.AutoSize = True
        Me.lblDac2Mv.Location = New System.Drawing.Point(150, 22)
        Me.lblDac2Mv.Name = "lblDac2Mv"
        Me.lblDac2Mv.Size = New System.Drawing.Size(56, 13)
        Me.lblDac2Mv.TabIndex = 2
        Me.lblDac2Mv.Text = "DAC2 mV:"
        '
        'txtSchedDac2Mv
        '
        Me.txtSchedDac2Mv.Location = New System.Drawing.Point(210, 18)
        Me.txtSchedDac2Mv.Name = "txtSchedDac2Mv"
        Me.txtSchedDac2Mv.Size = New System.Drawing.Size(60, 20)
        Me.txtSchedDac2Mv.TabIndex = 3
        Me.txtSchedDac2Mv.Text = "0"
        '
        'lblDac1Range
        '
        Me.lblDac1Range.AutoSize = True
        Me.lblDac1Range.Location = New System.Drawing.Point(290, 22)
        Me.lblDac1Range.Name = "lblDac1Range"
        Me.lblDac1Range.Size = New System.Drawing.Size(49, 13)
        Me.lblDac1Range.TabIndex = 4
        Me.lblDac1Range.Text = "DAC1 R:"
        '
        'cboDac1Range
        '
        Me.cboDac1Range.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList
        Me.cboDac1Range.Location = New System.Drawing.Point(345, 18)
        Me.cboDac1Range.Name = "cboDac1Range"
        Me.cboDac1Range.Size = New System.Drawing.Size(55, 21)
        Me.cboDac1Range.TabIndex = 5
        '
        'lblDac2Range
        '
        Me.lblDac2Range.AutoSize = True
        Me.lblDac2Range.Location = New System.Drawing.Point(410, 22)
        Me.lblDac2Range.Name = "lblDac2Range"
        Me.lblDac2Range.Size = New System.Drawing.Size(49, 13)
        Me.lblDac2Range.TabIndex = 6
        Me.lblDac2Range.Text = "DAC2 R:"
        '
        'cboDac2Range
        '
        Me.cboDac2Range.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList
        Me.cboDac2Range.Location = New System.Drawing.Point(465, 18)
        Me.cboDac2Range.Name = "cboDac2Range"
        Me.cboDac2Range.Size = New System.Drawing.Size(45, 21)
        Me.cboDac2Range.TabIndex = 7
        '
        'grpSchedBuzzer
        '
        Me.grpSchedBuzzer.Controls.Add(Me.chkBuzzEnable)
        Me.grpSchedBuzzer.Controls.Add(Me.lblBuzzFreq)
        Me.grpSchedBuzzer.Controls.Add(Me.txtBuzzFreq)
        Me.grpSchedBuzzer.Controls.Add(Me.lblBuzzOnMs)
        Me.grpSchedBuzzer.Controls.Add(Me.txtBuzzOnMs)
        Me.grpSchedBuzzer.Controls.Add(Me.lblBuzzOffMs)
        Me.grpSchedBuzzer.Controls.Add(Me.txtBuzzOffMs)
        Me.grpSchedBuzzer.Controls.Add(Me.lblBuzzRepeats)
        Me.grpSchedBuzzer.Controls.Add(Me.txtBuzzRepeats)
        Me.grpSchedBuzzer.Location = New System.Drawing.Point(16, 495)
        Me.grpSchedBuzzer.Name = "grpSchedBuzzer"
        Me.grpSchedBuzzer.Size = New System.Drawing.Size(520, 55)
        Me.grpSchedBuzzer.TabIndex = 24
        Me.grpSchedBuzzer.TabStop = False
        Me.grpSchedBuzzer.Text = "Buzzer Pattern"
        '
        'chkBuzzEnable
        '
        Me.chkBuzzEnable.AutoSize = True
        Me.chkBuzzEnable.Location = New System.Drawing.Point(16, 24)
        Me.chkBuzzEnable.Name = "chkBuzzEnable"
        Me.chkBuzzEnable.Size = New System.Drawing.Size(59, 17)
        Me.chkBuzzEnable.TabIndex = 0
        Me.chkBuzzEnable.Text = "Enable"
        '
        'lblBuzzFreq
        '
        Me.lblBuzzFreq.AutoSize = True
        Me.lblBuzzFreq.Location = New System.Drawing.Point(90, 25)
        Me.lblBuzzFreq.Name = "lblBuzzFreq"
        Me.lblBuzzFreq.Size = New System.Drawing.Size(31, 13)
        Me.lblBuzzFreq.TabIndex = 1
        Me.lblBuzzFreq.Text = "Freq:"
        '
        'txtBuzzFreq
        '
        Me.txtBuzzFreq.Location = New System.Drawing.Point(125, 22)
        Me.txtBuzzFreq.Name = "txtBuzzFreq"
        Me.txtBuzzFreq.Size = New System.Drawing.Size(50, 20)
        Me.txtBuzzFreq.TabIndex = 2
        Me.txtBuzzFreq.Text = "2000"
        '
        'lblBuzzOnMs
        '
        Me.lblBuzzOnMs.AutoSize = True
        Me.lblBuzzOnMs.Location = New System.Drawing.Point(185, 25)
        Me.lblBuzzOnMs.Name = "lblBuzzOnMs"
        Me.lblBuzzOnMs.Size = New System.Drawing.Size(40, 13)
        Me.lblBuzzOnMs.TabIndex = 3
        Me.lblBuzzOnMs.Text = "On ms:"
        '
        'txtBuzzOnMs
        '
        Me.txtBuzzOnMs.Location = New System.Drawing.Point(230, 22)
        Me.txtBuzzOnMs.Name = "txtBuzzOnMs"
        Me.txtBuzzOnMs.Size = New System.Drawing.Size(50, 20)
        Me.txtBuzzOnMs.TabIndex = 4
        Me.txtBuzzOnMs.Text = "200"
        '
        'lblBuzzOffMs
        '
        Me.lblBuzzOffMs.AutoSize = True
        Me.lblBuzzOffMs.Location = New System.Drawing.Point(290, 25)
        Me.lblBuzzOffMs.Name = "lblBuzzOffMs"
        Me.lblBuzzOffMs.Size = New System.Drawing.Size(40, 13)
        Me.lblBuzzOffMs.TabIndex = 5
        Me.lblBuzzOffMs.Text = "Off ms:"
        '
        'txtBuzzOffMs
        '
        Me.txtBuzzOffMs.Location = New System.Drawing.Point(335, 22)
        Me.txtBuzzOffMs.Name = "txtBuzzOffMs"
        Me.txtBuzzOffMs.Size = New System.Drawing.Size(50, 20)
        Me.txtBuzzOffMs.TabIndex = 6
        Me.txtBuzzOffMs.Text = "200"
        '
        'lblBuzzRepeats
        '
        Me.lblBuzzRepeats.AutoSize = True
        Me.lblBuzzRepeats.Location = New System.Drawing.Point(395, 25)
        Me.lblBuzzRepeats.Name = "lblBuzzRepeats"
        Me.lblBuzzRepeats.Size = New System.Drawing.Size(50, 13)
        Me.lblBuzzRepeats.TabIndex = 7
        Me.lblBuzzRepeats.Text = "Repeats:"
        '
        'txtBuzzRepeats
        '
        Me.txtBuzzRepeats.Location = New System.Drawing.Point(450, 22)
        Me.txtBuzzRepeats.Name = "txtBuzzRepeats"
        Me.txtBuzzRepeats.Size = New System.Drawing.Size(40, 20)
        Me.txtBuzzRepeats.TabIndex = 8
        Me.txtBuzzRepeats.Text = "5"
        '
        'btnSchedNew
        '
        Me.btnSchedNew.Location = New System.Drawing.Point(190, 174)
        Me.btnSchedNew.Name = "btnSchedNew"
        Me.btnSchedNew.Size = New System.Drawing.Size(70, 24)
        Me.btnSchedNew.TabIndex = 4
        Me.btnSchedNew.Text = "New"
        Me.btnSchedNew.UseVisualStyleBackColor = True
        '
        'btnSchedAdd
        '
        Me.btnSchedAdd.Location = New System.Drawing.Point(266, 174)
        Me.btnSchedAdd.Name = "btnSchedAdd"
        Me.btnSchedAdd.Size = New System.Drawing.Size(70, 24)
        Me.btnSchedAdd.TabIndex = 5
        Me.btnSchedAdd.Text = "Add"
        Me.btnSchedAdd.UseVisualStyleBackColor = True
        '
        'btnSchedUpdate
        '
        Me.btnSchedUpdate.Location = New System.Drawing.Point(342, 174)
        Me.btnSchedUpdate.Name = "btnSchedUpdate"
        Me.btnSchedUpdate.Size = New System.Drawing.Size(70, 24)
        Me.btnSchedUpdate.TabIndex = 6
        Me.btnSchedUpdate.Text = "Update"
        Me.btnSchedUpdate.UseVisualStyleBackColor = True
        '
        'btnSchedDelete
        '
        Me.btnSchedDelete.Location = New System.Drawing.Point(418, 174)
        Me.btnSchedDelete.Name = "btnSchedDelete"
        Me.btnSchedDelete.Size = New System.Drawing.Size(70, 24)
        Me.btnSchedDelete.TabIndex = 7
        Me.btnSchedDelete.Text = "Delete"
        Me.btnSchedDelete.UseVisualStyleBackColor = True
        '
        'btnSchedQuery
        '
        Me.btnSchedQuery.Location = New System.Drawing.Point(280, 555)
        Me.btnSchedQuery.Name = "btnSchedQuery"
        Me.btnSchedQuery.Size = New System.Drawing.Size(120, 28)
        Me.btnSchedQuery.TabIndex = 32
        Me.btnSchedQuery.Text = "Query (LIST)"
        '
        'btnSchedDisable
        '
        Me.btnSchedDisable.Location = New System.Drawing.Point(148, 555)
        Me.btnSchedDisable.Name = "btnSchedDisable"
        Me.btnSchedDisable.Size = New System.Drawing.Size(120, 28)
        Me.btnSchedDisable.TabIndex = 31
        Me.btnSchedDisable.Text = "Disable Selected"
        '
        'btnSchedApply
        '
        Me.btnSchedApply.Location = New System.Drawing.Point(16, 555)
        Me.btnSchedApply.Name = "btnSchedApply"
        Me.btnSchedApply.Size = New System.Drawing.Size(120, 28)
        Me.btnSchedApply.TabIndex = 30
        Me.btnSchedApply.Text = "Refresh List"
        '
        'lblSchedStatus
        '
        Me.lblSchedStatus.AutoSize = True
        Me.lblSchedStatus.Location = New System.Drawing.Point(16, 590)
        Me.lblSchedStatus.Name = "lblSchedStatus"
        Me.lblSchedStatus.Size = New System.Drawing.Size(78, 13)
        Me.lblSchedStatus.TabIndex = 33
        Me.lblSchedStatus.Text = "Scheduler: n/a"
        '
        'tabPageSettings
        '
        Me.tabPageSettings.Controls.Add(Me.grpExistingSettings)
        Me.tabPageSettings.Controls.Add(Me.grpUserSettings)
        Me.tabPageSettings.Controls.Add(Me.grpSerial)
        Me.tabPageSettings.Location = New System.Drawing.Point(4, 22)
        Me.tabPageSettings.Name = "tabPageSettings"
        Me.tabPageSettings.Size = New System.Drawing.Size(584, 644)
        Me.tabPageSettings.TabIndex = 2
        Me.tabPageSettings.Text = "Settings"
        Me.tabPageSettings.UseVisualStyleBackColor = True
        '
        'grpExistingSettings
        '
        Me.grpExistingSettings.BackColor = System.Drawing.Color.FromArgb(CType(CType(245, Byte), Integer), CType(CType(248, Byte), Integer), CType(CType(255, Byte), Integer))
        Me.grpExistingSettings.Controls.Add(Me.btnSettingsRefresh)
        Me.grpExistingSettings.Controls.Add(Me.lblExistingHint)
        Me.grpExistingSettings.Controls.Add(Me.lblExistingTcpPort)
        Me.grpExistingSettings.Controls.Add(Me.lblExistingDns)
        Me.grpExistingSettings.Controls.Add(Me.lblExistingGw)
        Me.grpExistingSettings.Controls.Add(Me.lblExistingMask)
        Me.grpExistingSettings.Controls.Add(Me.lblExistingIp)
        Me.grpExistingSettings.Controls.Add(Me.lblExistingMac)
        Me.grpExistingSettings.Controls.Add(Me.lblExistingDhcp)
        Me.grpExistingSettings.Controls.Add(Me.lblExistingApPass)
        Me.grpExistingSettings.Controls.Add(Me.lblExistingApSsid)
        Me.grpExistingSettings.Location = New System.Drawing.Point(12, 12)
        Me.grpExistingSettings.Name = "grpExistingSettings"
        Me.grpExistingSettings.Size = New System.Drawing.Size(560, 210)
        Me.grpExistingSettings.TabIndex = 0
        Me.grpExistingSettings.TabStop = False
        Me.grpExistingSettings.Text = "Existing Configuration (Read Only)"
        '
        'btnSettingsRefresh
        '
        Me.btnSettingsRefresh.Location = New System.Drawing.Point(430, 20)
        Me.btnSettingsRefresh.Name = "btnSettingsRefresh"
        Me.btnSettingsRefresh.Size = New System.Drawing.Size(110, 24)
        Me.btnSettingsRefresh.TabIndex = 0
        Me.btnSettingsRefresh.Text = "Refresh"
        Me.btnSettingsRefresh.UseVisualStyleBackColor = True
        '
        'lblExistingHint
        '
        Me.lblExistingHint.AutoSize = True
        Me.lblExistingHint.ForeColor = System.Drawing.Color.FromArgb(CType(CType(30, Byte), Integer), CType(CType(60, Byte), Integer), CType(CType(130, Byte), Integer))
        Me.lblExistingHint.Location = New System.Drawing.Point(16, 24)
        Me.lblExistingHint.Name = "lblExistingHint"
        Me.lblExistingHint.Size = New System.Drawing.Size(288, 13)
        Me.lblExistingHint.TabIndex = 10
        Me.lblExistingHint.Text = "Shows settings currently stored in ESP32 Preferences flash."
        '
        'lblExistingTcpPort
        '
        Me.lblExistingTcpPort.AutoSize = True
        Me.lblExistingTcpPort.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.0!, System.Drawing.FontStyle.Bold)
        Me.lblExistingTcpPort.ForeColor = System.Drawing.Color.FromArgb(CType(CType(40, Byte), Integer), CType(CType(40, Byte), Integer), CType(CType(40, Byte), Integer))
        Me.lblExistingTcpPort.Location = New System.Drawing.Point(16, 175)
        Me.lblExistingTcpPort.Name = "lblExistingTcpPort"
        Me.lblExistingTcpPort.Size = New System.Drawing.Size(101, 15)
        Me.lblExistingTcpPort.TabIndex = 9
        Me.lblExistingTcpPort.Text = "TCP Port: (n/a)"
        '
        'lblExistingDns
        '
        Me.lblExistingDns.AutoSize = True
        Me.lblExistingDns.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.0!, System.Drawing.FontStyle.Bold)
        Me.lblExistingDns.Location = New System.Drawing.Point(16, 150)
        Me.lblExistingDns.Name = "lblExistingDns"
        Me.lblExistingDns.Size = New System.Drawing.Size(74, 15)
        Me.lblExistingDns.TabIndex = 8
        Me.lblExistingDns.Text = "DNS: (n/a)"
        '
        'lblExistingGw
        '
        Me.lblExistingGw.AutoSize = True
        Me.lblExistingGw.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.0!, System.Drawing.FontStyle.Bold)
        Me.lblExistingGw.Location = New System.Drawing.Point(16, 125)
        Me.lblExistingGw.Name = "lblExistingGw"
        Me.lblExistingGw.Size = New System.Drawing.Size(99, 15)
        Me.lblExistingGw.TabIndex = 7
        Me.lblExistingGw.Text = "Gateway: (n/a)"
        '
        'lblExistingMask
        '
        Me.lblExistingMask.AutoSize = True
        Me.lblExistingMask.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.0!, System.Drawing.FontStyle.Bold)
        Me.lblExistingMask.Location = New System.Drawing.Point(16, 100)
        Me.lblExistingMask.Name = "lblExistingMask"
        Me.lblExistingMask.Size = New System.Drawing.Size(128, 15)
        Me.lblExistingMask.TabIndex = 6
        Me.lblExistingMask.Text = "Subnet Mask: (n/a)"
        '
        'lblExistingIp
        '
        Me.lblExistingIp.AutoSize = True
        Me.lblExistingIp.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.0!, System.Drawing.FontStyle.Bold)
        Me.lblExistingIp.Location = New System.Drawing.Point(16, 75)
        Me.lblExistingIp.Name = "lblExistingIp"
        Me.lblExistingIp.Size = New System.Drawing.Size(58, 15)
        Me.lblExistingIp.TabIndex = 5
        Me.lblExistingIp.Text = "IP: (n/a)"
        '
        'lblExistingMac
        '
        Me.lblExistingMac.AutoSize = True
        Me.lblExistingMac.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.0!, System.Drawing.FontStyle.Bold)
        Me.lblExistingMac.Location = New System.Drawing.Point(16, 50)
        Me.lblExistingMac.Name = "lblExistingMac"
        Me.lblExistingMac.Size = New System.Drawing.Size(74, 15)
        Me.lblExistingMac.TabIndex = 4
        Me.lblExistingMac.Text = "MAC: (n/a)"
        '
        'lblExistingDhcp
        '
        Me.lblExistingDhcp.AutoSize = True
        Me.lblExistingDhcp.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.0!, System.Drawing.FontStyle.Bold)
        Me.lblExistingDhcp.Location = New System.Drawing.Point(260, 50)
        Me.lblExistingDhcp.Name = "lblExistingDhcp"
        Me.lblExistingDhcp.Size = New System.Drawing.Size(83, 15)
        Me.lblExistingDhcp.TabIndex = 3
        Me.lblExistingDhcp.Text = "DHCP: (n/a)"
        '
        'lblExistingApPass
        '
        Me.lblExistingApPass.AutoSize = True
        Me.lblExistingApPass.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.0!, System.Drawing.FontStyle.Bold)
        Me.lblExistingApPass.Location = New System.Drawing.Point(260, 100)
        Me.lblExistingApPass.Name = "lblExistingApPass"
        Me.lblExistingApPass.Size = New System.Drawing.Size(101, 15)
        Me.lblExistingApPass.TabIndex = 2
        Me.lblExistingApPass.Text = "AP PASS: (n/a)"
        '
        'lblExistingApSsid
        '
        Me.lblExistingApSsid.AutoSize = True
        Me.lblExistingApSsid.Font = New System.Drawing.Font("Microsoft Sans Serif", 9.0!, System.Drawing.FontStyle.Bold)
        Me.lblExistingApSsid.Location = New System.Drawing.Point(260, 75)
        Me.lblExistingApSsid.Name = "lblExistingApSsid"
        Me.lblExistingApSsid.Size = New System.Drawing.Size(98, 15)
        Me.lblExistingApSsid.TabIndex = 1
        Me.lblExistingApSsid.Text = "AP SSID: (n/a)"
        '
        'grpUserSettings
        '
        Me.grpUserSettings.BackColor = System.Drawing.Color.FromArgb(CType(CType(248, Byte), Integer), CType(CType(255, Byte), Integer), CType(CType(248, Byte), Integer))
        Me.grpUserSettings.Controls.Add(Me.btnSettingsSaveReboot)
        Me.grpUserSettings.Controls.Add(Me.btnSettingsApplyToConnection)
        Me.grpUserSettings.Controls.Add(Me.lblUserHint)
        Me.grpUserSettings.Controls.Add(Me.chkUserDhcp)
        Me.grpUserSettings.Controls.Add(Me.txtUserMac)
        Me.grpUserSettings.Controls.Add(Me.txtUserIp)
        Me.grpUserSettings.Controls.Add(Me.txtUserMask)
        Me.grpUserSettings.Controls.Add(Me.txtUserGw)
        Me.grpUserSettings.Controls.Add(Me.txtUserDns)
        Me.grpUserSettings.Controls.Add(Me.txtUserTcpPort)
        Me.grpUserSettings.Controls.Add(Me.txtUserApSsid)
        Me.grpUserSettings.Controls.Add(Me.txtUserApPass)
        Me.grpUserSettings.Controls.Add(Me.lblUserMac)
        Me.grpUserSettings.Controls.Add(Me.lblUserIp)
        Me.grpUserSettings.Controls.Add(Me.lblUserMask)
        Me.grpUserSettings.Controls.Add(Me.lblUserGw)
        Me.grpUserSettings.Controls.Add(Me.lblUserDns)
        Me.grpUserSettings.Controls.Add(Me.lblUserTcpPort)
        Me.grpUserSettings.Controls.Add(Me.lblUserApSsid)
        Me.grpUserSettings.Controls.Add(Me.lblUserApPass)
        Me.grpUserSettings.Location = New System.Drawing.Point(12, 230)
        Me.grpUserSettings.Name = "grpUserSettings"
        Me.grpUserSettings.Size = New System.Drawing.Size(560, 238)
        Me.grpUserSettings.TabIndex = 1
        Me.grpUserSettings.TabStop = False
        Me.grpUserSettings.Text = "User Settings (Edit + Save to ESP32)"
        '
        'btnSettingsSaveReboot
        '
        Me.btnSettingsSaveReboot.Location = New System.Drawing.Point(400, 179)
        Me.btnSettingsSaveReboot.Name = "btnSettingsSaveReboot"
        Me.btnSettingsSaveReboot.Size = New System.Drawing.Size(140, 28)
        Me.btnSettingsSaveReboot.TabIndex = 19
        Me.btnSettingsSaveReboot.Text = "Save && Reboot"
        Me.btnSettingsSaveReboot.UseVisualStyleBackColor = True
        '
        'btnSettingsApplyToConnection
        '
        Me.btnSettingsApplyToConnection.Location = New System.Drawing.Point(400, 141)
        Me.btnSettingsApplyToConnection.Name = "btnSettingsApplyToConnection"
        Me.btnSettingsApplyToConnection.Size = New System.Drawing.Size(140, 28)
        Me.btnSettingsApplyToConnection.TabIndex = 18
        Me.btnSettingsApplyToConnection.Text = "Use These for Connect"
        Me.btnSettingsApplyToConnection.UseVisualStyleBackColor = True
        '
        'lblUserHint
        '
        Me.lblUserHint.AutoSize = True
        Me.lblUserHint.ForeColor = System.Drawing.Color.FromArgb(CType(CType(20, Byte), Integer), CType(CType(110, Byte), Integer), CType(CType(20, Byte), Integer))
        Me.lblUserHint.Location = New System.Drawing.Point(16, 24)
        Me.lblUserHint.Name = "lblUserHint"
        Me.lblUserHint.Size = New System.Drawing.Size(416, 13)
        Me.lblUserHint.TabIndex = 17
        Me.lblUserHint.Text = "Edit values, then Save. The board will reboot and you can reconnect with new IP/p" &
    "ort."
        '
        'chkUserDhcp
        '
        Me.chkUserDhcp.AutoSize = True
        Me.chkUserDhcp.Location = New System.Drawing.Point(110, 52)
        Me.chkUserDhcp.Name = "chkUserDhcp"
        Me.chkUserDhcp.Size = New System.Drawing.Size(78, 17)
        Me.chkUserDhcp.TabIndex = 2
        Me.chkUserDhcp.Text = "Use DHCP"
        Me.chkUserDhcp.UseVisualStyleBackColor = True
        '
        'txtUserMac
        '
        Me.txtUserMac.Location = New System.Drawing.Point(110, 75)
        Me.txtUserMac.Name = "txtUserMac"
        Me.txtUserMac.Size = New System.Drawing.Size(160, 20)
        Me.txtUserMac.TabIndex = 3
        '
        'txtUserIp
        '
        Me.txtUserIp.Location = New System.Drawing.Point(110, 101)
        Me.txtUserIp.Name = "txtUserIp"
        Me.txtUserIp.Size = New System.Drawing.Size(160, 20)
        Me.txtUserIp.TabIndex = 4
        '
        'txtUserMask
        '
        Me.txtUserMask.Location = New System.Drawing.Point(110, 127)
        Me.txtUserMask.Name = "txtUserMask"
        Me.txtUserMask.Size = New System.Drawing.Size(160, 20)
        Me.txtUserMask.TabIndex = 5
        '
        'txtUserGw
        '
        Me.txtUserGw.Location = New System.Drawing.Point(110, 153)
        Me.txtUserGw.Name = "txtUserGw"
        Me.txtUserGw.Size = New System.Drawing.Size(160, 20)
        Me.txtUserGw.TabIndex = 6
        '
        'txtUserDns
        '
        Me.txtUserDns.Location = New System.Drawing.Point(110, 179)
        Me.txtUserDns.Name = "txtUserDns"
        Me.txtUserDns.Size = New System.Drawing.Size(160, 20)
        Me.txtUserDns.TabIndex = 7
        '
        'txtUserTcpPort
        '
        Me.txtUserTcpPort.Location = New System.Drawing.Point(110, 205)
        Me.txtUserTcpPort.Name = "txtUserTcpPort"
        Me.txtUserTcpPort.Size = New System.Drawing.Size(60, 20)
        Me.txtUserTcpPort.TabIndex = 8
        '
        'txtUserApSsid
        '
        Me.txtUserApSsid.Location = New System.Drawing.Point(380, 75)
        Me.txtUserApSsid.Name = "txtUserApSsid"
        Me.txtUserApSsid.Size = New System.Drawing.Size(160, 20)
        Me.txtUserApSsid.TabIndex = 10
        '
        'txtUserApPass
        '
        Me.txtUserApPass.Location = New System.Drawing.Point(380, 101)
        Me.txtUserApPass.Name = "txtUserApPass"
        Me.txtUserApPass.Size = New System.Drawing.Size(160, 20)
        Me.txtUserApPass.TabIndex = 11
        '
        'lblUserMac
        '
        Me.lblUserMac.AutoSize = True
        Me.lblUserMac.Location = New System.Drawing.Point(16, 78)
        Me.lblUserMac.Name = "lblUserMac"
        Me.lblUserMac.Size = New System.Drawing.Size(33, 13)
        Me.lblUserMac.TabIndex = 0
        Me.lblUserMac.Text = "MAC:"
        '
        'lblUserIp
        '
        Me.lblUserIp.AutoSize = True
        Me.lblUserIp.Location = New System.Drawing.Point(16, 104)
        Me.lblUserIp.Name = "lblUserIp"
        Me.lblUserIp.Size = New System.Drawing.Size(50, 13)
        Me.lblUserIp.TabIndex = 1
        Me.lblUserIp.Text = "Static IP:"
        '
        'lblUserMask
        '
        Me.lblUserMask.AutoSize = True
        Me.lblUserMask.Location = New System.Drawing.Point(16, 130)
        Me.lblUserMask.Name = "lblUserMask"
        Me.lblUserMask.Size = New System.Drawing.Size(73, 13)
        Me.lblUserMask.TabIndex = 9
        Me.lblUserMask.Text = "Subnet Mask:"
        '
        'lblUserGw
        '
        Me.lblUserGw.AutoSize = True
        Me.lblUserGw.Location = New System.Drawing.Point(16, 156)
        Me.lblUserGw.Name = "lblUserGw"
        Me.lblUserGw.Size = New System.Drawing.Size(52, 13)
        Me.lblUserGw.TabIndex = 12
        Me.lblUserGw.Text = "Gateway:"
        '
        'lblUserDns
        '
        Me.lblUserDns.AutoSize = True
        Me.lblUserDns.Location = New System.Drawing.Point(16, 182)
        Me.lblUserDns.Name = "lblUserDns"
        Me.lblUserDns.Size = New System.Drawing.Size(33, 13)
        Me.lblUserDns.TabIndex = 13
        Me.lblUserDns.Text = "DNS:"
        '
        'lblUserTcpPort
        '
        Me.lblUserTcpPort.AutoSize = True
        Me.lblUserTcpPort.Location = New System.Drawing.Point(16, 208)
        Me.lblUserTcpPort.Name = "lblUserTcpPort"
        Me.lblUserTcpPort.Size = New System.Drawing.Size(53, 13)
        Me.lblUserTcpPort.TabIndex = 14
        Me.lblUserTcpPort.Text = "TCP Port:"
        '
        'lblUserApSsid
        '
        Me.lblUserApSsid.AutoSize = True
        Me.lblUserApSsid.Location = New System.Drawing.Point(300, 78)
        Me.lblUserApSsid.Name = "lblUserApSsid"
        Me.lblUserApSsid.Size = New System.Drawing.Size(52, 13)
        Me.lblUserApSsid.TabIndex = 15
        Me.lblUserApSsid.Text = "AP SSID:"
        '
        'lblUserApPass
        '
        Me.lblUserApPass.AutoSize = True
        Me.lblUserApPass.Location = New System.Drawing.Point(300, 104)
        Me.lblUserApPass.Name = "lblUserApPass"
        Me.lblUserApPass.Size = New System.Drawing.Size(55, 13)
        Me.lblUserApPass.TabIndex = 16
        Me.lblUserApPass.Text = "AP PASS:"
        '
        'grpSerial
        '
        Me.grpSerial.BackColor = System.Drawing.Color.FromArgb(CType(CType(255, Byte), Integer), CType(CType(250, Byte), Integer), CType(CType(240, Byte), Integer))
        Me.grpSerial.Controls.Add(Me.btnSerialApplyIpPort)
        Me.grpSerial.Controls.Add(Me.txtSerialLog)
        Me.grpSerial.Controls.Add(Me.lblSerialStatus)
        Me.grpSerial.Controls.Add(Me.btnSerialNetDump)
        Me.grpSerial.Controls.Add(Me.btnSerialClear)
        Me.grpSerial.Controls.Add(Me.btnSerialDisconnect)
        Me.grpSerial.Controls.Add(Me.btnSerialConnect)
        Me.grpSerial.Controls.Add(Me.btnSerialRefresh)
        Me.grpSerial.Controls.Add(Me.cboStopBits)
        Me.grpSerial.Controls.Add(Me.lblStopBits)
        Me.grpSerial.Controls.Add(Me.cboParity)
        Me.grpSerial.Controls.Add(Me.lblParity)
        Me.grpSerial.Controls.Add(Me.cboDataBits)
        Me.grpSerial.Controls.Add(Me.lblDataBits)
        Me.grpSerial.Controls.Add(Me.cboBaud)
        Me.grpSerial.Controls.Add(Me.lblBaud)
        Me.grpSerial.Controls.Add(Me.cboComPort)
        Me.grpSerial.Controls.Add(Me.lblComPort)
        Me.grpSerial.Controls.Add(Me.lblSerialHint)
        Me.grpSerial.Location = New System.Drawing.Point(12, 473)
        Me.grpSerial.Name = "grpSerial"
        Me.grpSerial.Size = New System.Drawing.Size(560, 168)
        Me.grpSerial.TabIndex = 2
        Me.grpSerial.TabStop = False
        Me.grpSerial.Text = "Serial Port (Pre-Connect: Read Network Settings from ESP32)"
        '
        'btnSerialApplyIpPort
        '
        Me.btnSerialApplyIpPort.Location = New System.Drawing.Point(472, 117)
        Me.btnSerialApplyIpPort.Name = "btnSerialApplyIpPort"
        Me.btnSerialApplyIpPort.Size = New System.Drawing.Size(76, 45)
        Me.btnSerialApplyIpPort.TabIndex = 18
        Me.btnSerialApplyIpPort.Text = "Use IP/Port"
        Me.btnSerialApplyIpPort.UseVisualStyleBackColor = True
        '
        'txtSerialLog
        '
        Me.txtSerialLog.BackColor = System.Drawing.Color.FromArgb(CType(CType(20, Byte), Integer), CType(CType(20, Byte), Integer), CType(CType(20, Byte), Integer))
        Me.txtSerialLog.Font = New System.Drawing.Font("Consolas", 9.0!)
        Me.txtSerialLog.ForeColor = System.Drawing.Color.FromArgb(CType(CType(210, Byte), Integer), CType(CType(255, Byte), Integer), CType(CType(210, Byte), Integer))
        Me.txtSerialLog.Location = New System.Drawing.Point(16, 92)
        Me.txtSerialLog.Multiline = True
        Me.txtSerialLog.Name = "txtSerialLog"
        Me.txtSerialLog.ScrollBars = System.Windows.Forms.ScrollBars.Vertical
        Me.txtSerialLog.Size = New System.Drawing.Size(450, 70)
        Me.txtSerialLog.TabIndex = 17
        '
        'lblSerialStatus
        '
        Me.lblSerialStatus.AutoSize = True
        Me.lblSerialStatus.ForeColor = System.Drawing.Color.FromArgb(CType(CType(0, Byte), Integer), CType(CType(90, Byte), Integer), CType(CType(170, Byte), Integer))
        Me.lblSerialStatus.Location = New System.Drawing.Point(446, 12)
        Me.lblSerialStatus.Name = "lblSerialStatus"
        Me.lblSerialStatus.Size = New System.Drawing.Size(0, 13)
        Me.lblSerialStatus.TabIndex = 16
        '
        'btnSerialNetDump
        '
        Me.btnSerialNetDump.Location = New System.Drawing.Point(472, 63)
        Me.btnSerialNetDump.Name = "btnSerialNetDump"
        Me.btnSerialNetDump.Size = New System.Drawing.Size(76, 23)
        Me.btnSerialNetDump.TabIndex = 15
        Me.btnSerialNetDump.Text = "SERIALNET"
        Me.btnSerialNetDump.UseVisualStyleBackColor = True
        '
        'btnSerialClear
        '
        Me.btnSerialClear.Location = New System.Drawing.Point(398, 63)
        Me.btnSerialClear.Name = "btnSerialClear"
        Me.btnSerialClear.Size = New System.Drawing.Size(68, 23)
        Me.btnSerialClear.TabIndex = 14
        Me.btnSerialClear.Text = "Clear"
        Me.btnSerialClear.UseVisualStyleBackColor = True
        '
        'btnSerialDisconnect
        '
        Me.btnSerialDisconnect.Location = New System.Drawing.Point(324, 63)
        Me.btnSerialDisconnect.Name = "btnSerialDisconnect"
        Me.btnSerialDisconnect.Size = New System.Drawing.Size(68, 23)
        Me.btnSerialDisconnect.TabIndex = 13
        Me.btnSerialDisconnect.Text = "Close"
        Me.btnSerialDisconnect.UseVisualStyleBackColor = True
        '
        'btnSerialConnect
        '
        Me.btnSerialConnect.Location = New System.Drawing.Point(250, 63)
        Me.btnSerialConnect.Name = "btnSerialConnect"
        Me.btnSerialConnect.Size = New System.Drawing.Size(68, 23)
        Me.btnSerialConnect.TabIndex = 12
        Me.btnSerialConnect.Text = "Open"
        Me.btnSerialConnect.UseVisualStyleBackColor = True
        '
        'btnSerialRefresh
        '
        Me.btnSerialRefresh.Location = New System.Drawing.Point(179, 63)
        Me.btnSerialRefresh.Name = "btnSerialRefresh"
        Me.btnSerialRefresh.Size = New System.Drawing.Size(65, 23)
        Me.btnSerialRefresh.TabIndex = 11
        Me.btnSerialRefresh.Text = "Refresh"
        Me.btnSerialRefresh.UseVisualStyleBackColor = True
        '
        'cboStopBits
        '
        Me.cboStopBits.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList
        Me.cboStopBits.FormattingEnabled = True
        Me.cboStopBits.Location = New System.Drawing.Point(76, 64)
        Me.cboStopBits.Name = "cboStopBits"
        Me.cboStopBits.Size = New System.Drawing.Size(90, 21)
        Me.cboStopBits.TabIndex = 10
        '
        'lblStopBits
        '
        Me.lblStopBits.AutoSize = True
        Me.lblStopBits.Location = New System.Drawing.Point(16, 68)
        Me.lblStopBits.Name = "lblStopBits"
        Me.lblStopBits.Size = New System.Drawing.Size(51, 13)
        Me.lblStopBits.TabIndex = 9
        Me.lblStopBits.Text = "Stop bits:"
        '
        'cboParity
        '
        Me.cboParity.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList
        Me.cboParity.FormattingEnabled = True
        Me.cboParity.Location = New System.Drawing.Point(463, 38)
        Me.cboParity.Name = "cboParity"
        Me.cboParity.Size = New System.Drawing.Size(85, 21)
        Me.cboParity.TabIndex = 8
        '
        'lblParity
        '
        Me.lblParity.AutoSize = True
        Me.lblParity.Location = New System.Drawing.Point(425, 42)
        Me.lblParity.Name = "lblParity"
        Me.lblParity.Size = New System.Drawing.Size(36, 13)
        Me.lblParity.TabIndex = 7
        Me.lblParity.Text = "Parity:"
        '
        'cboDataBits
        '
        Me.cboDataBits.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList
        Me.cboDataBits.FormattingEnabled = True
        Me.cboDataBits.Location = New System.Drawing.Point(362, 38)
        Me.cboDataBits.Name = "cboDataBits"
        Me.cboDataBits.Size = New System.Drawing.Size(55, 21)
        Me.cboDataBits.TabIndex = 6
        '
        'lblDataBits
        '
        Me.lblDataBits.AutoSize = True
        Me.lblDataBits.Location = New System.Drawing.Point(305, 42)
        Me.lblDataBits.Name = "lblDataBits"
        Me.lblDataBits.Size = New System.Drawing.Size(52, 13)
        Me.lblDataBits.TabIndex = 5
        Me.lblDataBits.Text = "Data bits:"
        '
        'cboBaud
        '
        Me.cboBaud.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList
        Me.cboBaud.FormattingEnabled = True
        Me.cboBaud.Location = New System.Drawing.Point(215, 38)
        Me.cboBaud.Name = "cboBaud"
        Me.cboBaud.Size = New System.Drawing.Size(80, 21)
        Me.cboBaud.TabIndex = 4
        '
        'lblBaud
        '
        Me.lblBaud.AutoSize = True
        Me.lblBaud.Location = New System.Drawing.Point(176, 42)
        Me.lblBaud.Name = "lblBaud"
        Me.lblBaud.Size = New System.Drawing.Size(35, 13)
        Me.lblBaud.TabIndex = 3
        Me.lblBaud.Text = "Baud:"
        '
        'cboComPort
        '
        Me.cboComPort.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList
        Me.cboComPort.FormattingEnabled = True
        Me.cboComPort.Location = New System.Drawing.Point(76, 38)
        Me.cboComPort.Name = "cboComPort"
        Me.cboComPort.Size = New System.Drawing.Size(90, 21)
        Me.cboComPort.TabIndex = 2
        '
        'lblComPort
        '
        Me.lblComPort.AutoSize = True
        Me.lblComPort.Location = New System.Drawing.Point(16, 42)
        Me.lblComPort.Name = "lblComPort"
        Me.lblComPort.Size = New System.Drawing.Size(56, 13)
        Me.lblComPort.TabIndex = 1
        Me.lblComPort.Text = "COM Port:"
        '
        'lblSerialHint
        '
        Me.lblSerialHint.AutoSize = True
        Me.lblSerialHint.ForeColor = System.Drawing.Color.FromArgb(CType(CType(160, Byte), Integer), CType(CType(80, Byte), Integer), CType(CType(0, Byte), Integer))
        Me.lblSerialHint.Location = New System.Drawing.Point(16, 20)
        Me.lblSerialHint.Name = "lblSerialHint"
        Me.lblSerialHint.Size = New System.Drawing.Size(251, 13)
        Me.lblSerialHint.TabIndex = 0
        Me.lblSerialHint.Text = "Use SERIALNET on ESP32 to print network config."
        '
        'FormMain
        '
        Me.ClientSize = New System.Drawing.Size(592, 670)
        Me.Controls.Add(Me.tabMain)
        Me.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog
        Me.Name = "FormMain"
        Me.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen
        Me.Text = "KC-Link A8RM"
        Me.grpNet.ResumeLayout(False)
        Me.grpNet.PerformLayout()
        Me.grpRelays.ResumeLayout(False)
        Me.grpRelays.PerformLayout()
        Me.grpDI.ResumeLayout(False)
        Me.grpDI.PerformLayout()
        Me.grpAnalog.ResumeLayout(False)
        Me.grpAnalog.PerformLayout()
        Me.grpDAC.ResumeLayout(False)
        Me.grpDAC.PerformLayout()
        Me.grpTemp.ResumeLayout(False)
        Me.grpTemp.PerformLayout()
        Me.grpRTC.ResumeLayout(False)
        Me.grpRTC.PerformLayout()
        Me.grpBeep.ResumeLayout(False)
        Me.grpBeep.PerformLayout()
        Me.grpMisc.ResumeLayout(False)
        Me.grpMisc.PerformLayout()
        Me.tabMain.ResumeLayout(False)
        Me.tabPageDashboard.ResumeLayout(False)
        Me.tabPageDashboard.PerformLayout()
        Me.tabPageSchedule.ResumeLayout(False)
        Me.grpSchedule.ResumeLayout(False)
        Me.grpSchedule.PerformLayout()
        Me.grpSchedList.ResumeLayout(False)
        Me.grpDays.ResumeLayout(False)
        Me.grpSchedRelays.ResumeLayout(False)
        Me.grpSchedRelays.PerformLayout()
        Me.grpSchedDac.ResumeLayout(False)
        Me.grpSchedDac.PerformLayout()
        Me.grpSchedBuzzer.ResumeLayout(False)
        Me.grpSchedBuzzer.PerformLayout()
        Me.tabPageSettings.ResumeLayout(False)
        Me.grpExistingSettings.ResumeLayout(False)
        Me.grpExistingSettings.PerformLayout()
        Me.grpUserSettings.ResumeLayout(False)
        Me.grpUserSettings.PerformLayout()
        Me.grpSerial.ResumeLayout(False)
        Me.grpSerial.PerformLayout()
        Me.ResumeLayout(False)

    End Sub

    Friend WithEvents txtIp As TextBox
    Friend WithEvents txtPort As TextBox
    Friend WithEvents btnConnect As Button
    Friend WithEvents lblStatus As Label
    Friend WithEvents grpNet As GroupBox
    Friend WithEvents btnPing As Button
    Friend WithEvents lblNetInfo As Label
    Friend WithEvents chkAutoReconnect As CheckBox
    Friend WithEvents grpRelays As GroupBox
    Friend WithEvents chkRel6 As CheckBox
    Friend WithEvents chkRel5 As CheckBox
    Friend WithEvents chkRel4 As CheckBox
    Friend WithEvents chkRel3 As CheckBox
    Friend WithEvents chkRel2 As CheckBox
    Friend WithEvents chkRel1 As CheckBox
    Friend WithEvents grpDI As GroupBox
    Friend WithEvents chkDI8 As CheckBox
    Friend WithEvents chkDI7 As CheckBox
    Friend WithEvents chkDI6 As CheckBox
    Friend WithEvents chkDI5 As CheckBox
    Friend WithEvents chkDI4 As CheckBox
    Friend WithEvents chkDI3 As CheckBox
    Friend WithEvents chkDI2 As CheckBox
    Friend WithEvents chkDI1 As CheckBox
    Friend WithEvents grpAnalog As GroupBox
    Friend WithEvents txtI2 As TextBox
    Friend WithEvents txtI1 As TextBox
    Friend WithEvents txtV2 As TextBox
    Friend WithEvents txtV1 As TextBox
    Friend WithEvents lblLastSnap As Label
    Friend WithEvents grpDAC As GroupBox
    Friend WithEvents btnDacSet As Button
    Friend WithEvents txtDAC2 As TextBox
    Friend WithEvents txtDAC1 As TextBox
    Friend WithEvents grpTemp As GroupBox
    Friend WithEvents txtDs0 As TextBox
    Friend WithEvents txtDsCount As TextBox
    Friend WithEvents txtH2 As TextBox
    Friend WithEvents txtT2 As TextBox
    Friend WithEvents txtH1 As TextBox
    Friend WithEvents txtT1 As TextBox
    Friend WithEvents grpRTC As GroupBox
    Friend WithEvents btnRtcSet As Button
    Friend WithEvents btnRtcGet As Button
    Friend WithEvents txtRtcSet As TextBox
    Friend WithEvents txtRtcCurrent As TextBox
    Friend WithEvents grpBeep As GroupBox
    Friend WithEvents btnBuzzOff As Button
    Friend WithEvents btnBeep As Button
    Friend WithEvents txtBeepMs As TextBox
    Friend WithEvents txtBeepFreq As TextBox
    Friend WithEvents grpMisc As GroupBox
    Friend WithEvents btnSnapOnce As Button
    Friend WithEvents btnUnitApply As Button
    Friend WithEvents cboTempUnit As ComboBox
    Friend WithEvents chkAutoPoll As CheckBox
    Friend WithEvents txtLog As TextBox

    Friend WithEvents tabMain As TabControl
    Friend WithEvents tabPageDashboard As TabPage
    Friend WithEvents tabPageSchedule As TabPage
    Friend WithEvents grpSchedule As GroupBox

    Friend WithEvents lblSchedMode As Label
    Friend WithEvents cboSchedMode As ComboBox
    Friend WithEvents lblTrigDI As Label
    Friend WithEvents cboTrigDI As ComboBox
    Friend WithEvents lblEdge As Label
    Friend WithEvents cboEdge As ComboBox
    Friend WithEvents lblWinStart As Label
    Friend WithEvents txtWinStart As TextBox
    Friend WithEvents lblWinEnd As Label
    Friend WithEvents txtWinEnd As TextBox
    Friend WithEvents chkRecurring As CheckBox

    Friend WithEvents grpSchedRelays As GroupBox
    Friend WithEvents chkSRel1 As CheckBox
    Friend WithEvents chkSRel2 As CheckBox
    Friend WithEvents chkSRel3 As CheckBox
    Friend WithEvents chkSRel4 As CheckBox
    Friend WithEvents chkSRel5 As CheckBox
    Friend WithEvents chkSRel6 As CheckBox
    Friend WithEvents lblRelayAction As Label
    Friend WithEvents cboRelayAction As ComboBox

    Friend WithEvents grpSchedDac As GroupBox
    Friend WithEvents lblDac1Mv As Label
    Friend WithEvents lblDac2Mv As Label
    Friend WithEvents txtSchedDac1Mv As TextBox
    Friend WithEvents txtSchedDac2Mv As TextBox
    Friend WithEvents lblDac1Range As Label
    Friend WithEvents lblDac2Range As Label
    Friend WithEvents cboDac1Range As ComboBox
    Friend WithEvents cboDac2Range As ComboBox

    Friend WithEvents grpSchedBuzzer As GroupBox
    Friend WithEvents chkBuzzEnable As CheckBox
    Friend WithEvents lblBuzzFreq As Label
    Friend WithEvents txtBuzzFreq As TextBox
    Friend WithEvents lblBuzzOnMs As Label
    Friend WithEvents txtBuzzOnMs As TextBox
    Friend WithEvents lblBuzzOffMs As Label
    Friend WithEvents txtBuzzOffMs As TextBox
    Friend WithEvents lblBuzzRepeats As Label
    Friend WithEvents txtBuzzRepeats As TextBox

    Friend WithEvents btnSchedApply As Button
    Friend WithEvents btnSchedDisable As Button
    Friend WithEvents btnSchedQuery As Button
    Friend WithEvents lblSchedStatus As Label

    Friend WithEvents Label2 As Label
    Friend WithEvents Label1 As Label
    Friend WithEvents Label9 As Label
    Friend WithEvents Label6 As Label
    Friend WithEvents Label7 As Label
    Friend WithEvents Label8 As Label
    Friend WithEvents Label5 As Label
    Friend WithEvents Label4 As Label
    Friend WithEvents Label3 As Label
    Friend WithEvents Label10 As Label
    Friend WithEvents Label11 As Label

    Friend WithEvents grpSchedList As GroupBox
    Friend WithEvents lstSchedules As ListView
    Friend WithEvents colId As ColumnHeader
    Friend WithEvents colEnabled As ColumnHeader
    Friend WithEvents colMode As ColumnHeader
    Friend WithEvents colDays As ColumnHeader
    Friend WithEvents colWindow As ColumnHeader
    Friend WithEvents colTrigger As ColumnHeader
    Friend WithEvents colActions As ColumnHeader

    Friend WithEvents btnSchedNew As Button
    Friend WithEvents btnSchedAdd As Button
    Friend WithEvents btnSchedUpdate As Button
    Friend WithEvents btnSchedDelete As Button
    Friend WithEvents chkSchedEnabled As CheckBox
    Friend WithEvents lblSchedId As Label
    Friend WithEvents txtSchedId As TextBox

    Friend WithEvents grpDays As GroupBox
    Friend WithEvents chkMon As CheckBox
    Friend WithEvents chkTue As CheckBox
    Friend WithEvents chkWed As CheckBox
    Friend WithEvents chkThu As CheckBox
    Friend WithEvents chkFri As CheckBox
    Friend WithEvents chkSat As CheckBox
    Friend WithEvents chkSun As CheckBox

    ' ===== Settings Tab Controls =====
    Friend WithEvents tabPageSettings As TabPage
    Friend WithEvents grpExistingSettings As GroupBox
    Friend WithEvents grpUserSettings As GroupBox

    Friend WithEvents btnSettingsRefresh As Button
    Friend WithEvents lblExistingHint As Label
    Friend WithEvents lblExistingMac As Label
    Friend WithEvents lblExistingDhcp As Label
    Friend WithEvents lblExistingIp As Label
    Friend WithEvents lblExistingMask As Label
    Friend WithEvents lblExistingGw As Label
    Friend WithEvents lblExistingDns As Label
    Friend WithEvents lblExistingTcpPort As Label
    Friend WithEvents lblExistingApSsid As Label
    Friend WithEvents lblExistingApPass As Label

    Friend WithEvents lblUserHint As Label
    Friend WithEvents chkUserDhcp As CheckBox
    Friend WithEvents txtUserMac As TextBox
    Friend WithEvents txtUserIp As TextBox
    Friend WithEvents txtUserMask As TextBox
    Friend WithEvents txtUserGw As TextBox
    Friend WithEvents txtUserDns As TextBox
    Friend WithEvents txtUserTcpPort As TextBox
    Friend WithEvents txtUserApSsid As TextBox
    Friend WithEvents txtUserApPass As TextBox
    Friend WithEvents lblUserMac As Label
    Friend WithEvents lblUserIp As Label
    Friend WithEvents lblUserMask As Label
    Friend WithEvents lblUserGw As Label
    Friend WithEvents lblUserDns As Label
    Friend WithEvents lblUserTcpPort As Label
    Friend WithEvents lblUserApSsid As Label
    Friend WithEvents lblUserApPass As Label
    Friend WithEvents btnSettingsApplyToConnection As Button
    Friend WithEvents btnSettingsSaveReboot As Button

    ' ===== Serial Tab Controls (Settings) =====
    Friend WithEvents grpSerial As GroupBox
    Friend WithEvents lblSerialHint As Label
    Friend WithEvents lblComPort As Label
    Friend WithEvents cboComPort As ComboBox
    Friend WithEvents lblBaud As Label
    Friend WithEvents cboBaud As ComboBox
    Friend WithEvents lblDataBits As Label
    Friend WithEvents cboDataBits As ComboBox
    Friend WithEvents lblParity As Label
    Friend WithEvents cboParity As ComboBox
    Friend WithEvents lblStopBits As Label
    Friend WithEvents cboStopBits As ComboBox
    Friend WithEvents btnSerialRefresh As Button
    Friend WithEvents btnSerialConnect As Button
    Friend WithEvents btnSerialDisconnect As Button
    Friend WithEvents btnSerialClear As Button
    Friend WithEvents btnSerialNetDump As Button
    Friend WithEvents lblSerialStatus As Label
    Friend WithEvents txtSerialLog As TextBox
    Friend WithEvents btnSerialApplyIpPort As Button

End Class