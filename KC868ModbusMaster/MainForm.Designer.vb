Option Strict On
Option Explicit On

Imports System.ComponentModel
Imports System.Windows.Forms

Namespace KC868ModbusMaster
    <Global.Microsoft.VisualBasic.CompilerServices.DesignerGenerated()>
    Partial Class MainForm
        Inherits Form

        Private components As IContainer

        Friend WithEvents tabMain As TabControl
        Friend WithEvents tabDashboard As TabPage
        Friend WithEvents tabModbus As TabPage

        Friend WithEvents grpConnection As GroupBox
        Friend WithEvents cboPort As ComboBox
        Friend WithEvents cboBaud As ComboBox
        Friend WithEvents cboDataBits As ComboBox
        Friend WithEvents cboParity As ComboBox
        Friend WithEvents cboStopBits As ComboBox
        Friend WithEvents txtSlaveId As TextBox
        Friend WithEvents btnConnect As Button
        Friend WithEvents btnDisconnect As Button
        Friend WithEvents pnlConnLed As Panel
        Friend WithEvents lblConnState As Label

        Friend WithEvents chkAutoPoll As CheckBox
        Friend WithEvents numPollMs As NumericUpDown
        Friend WithEvents lblPoll As Label

        ' Dashboard controls
        Friend WithEvents grpInputs As GroupBox
        Friend WithEvents grpOutputs As GroupBox
        Friend WithEvents grpAnalog As GroupBox
        Friend WithEvents grpSensors As GroupBox
        Friend WithEvents grpRtc As GroupBox

        Friend pnlDi() As Panel
        Friend lblDi() As Label
        Friend pnlDirect() As Panel
        Friend lblDirect() As Label

        Friend chkDo() As CheckBox
        Friend WithEvents chkMasterEnable As CheckBox
        Friend WithEvents chkNightMode As CheckBox

        Friend lblAiRaw() As Label
        Friend lblAiMv() As Label

        Friend WithEvents lblDht1 As Label
        Friend WithEvents lblDht2 As Label
        Friend WithEvents lblDs18 As Label

        Friend WithEvents lblRtc As Label
        Friend WithEvents lblHeartbeat As Label
        Friend WithEvents lblChangeSeq As Label
        Friend WithEvents pnlEthLed As Panel
        Friend WithEvents pnlWifiLed As Panel
        Friend WithEvents pnlApLed As Panel
        Friend WithEvents pnlRestartLed As Panel
        Friend WithEvents pnlModbusLed As Panel

        ' MODBUS tab controls
        Friend WithEvents gridRegs As DataGridView
        Friend WithEvents btnRefreshRegs As Button
        Friend WithEvents btnWriteSelected As Button
        Friend WithEvents btnExportCsv As Button
        Friend WithEvents grpManual As GroupBox
        Friend WithEvents cboManualTable As ComboBox
        Friend WithEvents txtManualAddr As TextBox
        Friend WithEvents txtManualCount As TextBox
        Friend WithEvents txtManualWrite As TextBox
        Friend WithEvents btnManualRead As Button
        Friend WithEvents btnManualWrite As Button
        Friend WithEvents txtLog As TextBox

        <Global.System.Diagnostics.DebuggerNonUserCodeAttribute()>
        Protected Overrides Sub Dispose(disposing As Boolean)
            If disposing AndAlso components IsNot Nothing Then
                components.Dispose()
            End If
            MyBase.Dispose(disposing)
        End Sub

        <Global.System.Diagnostics.DebuggerStepThroughAttribute()>
        Private Sub InitializeComponent()
            Me.SuspendLayout()
            '
            'MainForm
            '
            Me.ClientSize = New System.Drawing.Size(765, 529)
            Me.Name = "MainForm"
            Me.ResumeLayout(False)

        End Sub
    End Class
End Namespace
