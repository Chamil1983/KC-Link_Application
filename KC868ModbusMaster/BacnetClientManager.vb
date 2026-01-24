Option Strict On
Option Explicit On

Imports System
Imports System.Collections.Generic
Imports System.Threading
Imports System.Net
Imports System.Net.NetworkInformation
Imports System.Diagnostics

Imports System.Linq
' NuGet: BACnet (ela-compil) provides the namespace System.IO.BACnet
Imports System.IO.BACnet

Namespace KC868ModbusMaster

    ''' <summary>
    ''' Minimal BACnet/IP client wrapper using System.IO.BACnet (ela-compil) library.
    ''' Works with the KC868-A16 ESP32 BACnet/IP server added in the Arduino patch.
    ''' </summary>
    Public Class BacnetClientManager

        Public Event DeviceDiscovered(deviceId As UInteger, address As BacnetAddress)
        Public Event ConnectedChanged(isConnected As Boolean)

        Private ReadOnly _sync As New Object()
        Private _client As BacnetClient = Nothing
        Private _transport As BacnetTransportBase = Nothing
        Private _deviceAddress As BacnetAddress = Nothing
        Private _deviceId As UInteger = 0UI
        Private _isStarted As Boolean = False

        ' Discovered devices (from I-Am)
        Private ReadOnly _discovered As New Dictionary(Of UInteger, BacnetAddress)()

        Public ReadOnly Property IsConnected As Boolean
            Get
                SyncLock _sync
                    Return _isStarted AndAlso _deviceAddress IsNot Nothing
                End SyncLock
            End Get
        End Property

        Public ReadOnly Property BoundDeviceId As UInteger
            Get
                SyncLock _sync
                    Return _deviceId
                End SyncLock
            End Get
        End Property

        Public Sub Start(localUdpPort As Integer)
            SyncLock _sync
                If _client IsNot Nothing Then Return

                Try
                    ' BACnet/IP UDP transport. The port here is the local bind port.
                    ' The remote device usually listens on 47808, but BACnet/IP uses UDP and works via BVLL.
                    '
                    ' IMPORTANT (Windows broadcast reliability):
                    ' Some Windows stacks fail to deliver BACnet broadcast/I-Am traffic unless the socket is
                    ' bound in "exclusive" mode on the BACnet port (47808). This improves discovery for link-local
                    ' networks (169.254.x.x) and multi-NIC PCs.
                    _transport = New BacnetIpUdpProtocolTransport(localUdpPort, True)
                    _client = New BacnetClient(_transport)
                    AddHandler _client.OnIam, AddressOf HandleIam

                    ' Best-effort: configure broadcast address to active NIC subnet
                    TryConfigureBroadcastAddress()

                    _client.Start()
                    _isStarted = True

                    ' Try to improve broadcast discovery reliability on Windows by setting
                    ' subnet broadcast (x.x.x.255) instead of global 255.255.255.255.
                    TryConfigureBroadcastAddress()
                Catch
                    ' Ensure clean rollback if start fails (missing dependencies / port already used / etc.)
                    Try
                        If _client IsNot Nothing Then _client.Dispose()
                    Catch
                    End Try
                    Try
                        If _transport IsNot Nothing Then _transport.Dispose()
                    Catch
                    End Try
                    _client = Nothing
                    _transport = Nothing
                    _isStarted = False
                    Throw
                End Try
            End SyncLock
        End Sub

        ''' <summary>
        ''' Manual bind to a known device IPv4 address (unicast), then verifies by reading the Device ObjectName.
        ''' This is useful when broadcast discovery is blocked by routers/firewalls.
        ''' </summary>
        Public Function BindToIp(ipString As String, deviceId As UInteger, Optional udpPort As UShort = 47808US, Optional timeoutMs As Integer = 2500) As Boolean
            If String.IsNullOrWhiteSpace(ipString) Then Return False
            Dim ip As IPAddress = Nothing
            If Not IPAddress.TryParse(ipString.Trim(), ip) Then Return False
            If ip.AddressFamily <> Sockets.AddressFamily.InterNetwork Then Return False
            If deviceId = 0UI Then Return False

            Dim addr As BacnetAddress = Nothing
            Try
                ' BacnetAddress for IP uses 4 bytes IP + 2 bytes port
                Dim raw(5) As Byte
                Dim b = ip.GetAddressBytes()
                raw(0) = b(0) : raw(1) = b(1) : raw(2) = b(2) : raw(3) = b(3)
                raw(4) = CByte((udpPort >> 8) And &HFF)
                raw(5) = CByte(udpPort And &HFF)
                addr = New BacnetAddress(BacnetAddressTypes.IP, 0US, raw)
            Catch
                Return False
            End Try

            SyncLock _sync
                _deviceId = deviceId
                _deviceAddress = addr

                ' Keep in discovered map for UI helpers
                If _discovered.ContainsKey(deviceId) Then
                    _discovered(deviceId) = addr
                Else
                    _discovered.Add(deviceId, addr)
                End If
            End SyncLock

            ' Verify reachability by reading Device Name
            Dim name As String = Nothing
            Dim ok As Boolean = False
            Dim sw As New Stopwatch()
            sw.Start()
            Do
                ok = ReadDeviceString(BacnetPropertyIds.PROP_OBJECT_NAME, name)
                If ok Then Exit Do
                Thread.Sleep(120)
            Loop While sw.ElapsedMilliseconds < timeoutMs

            RaiseEvent ConnectedChanged(ok)
            Return ok
        End Function

        Public Sub [Stop]()
            SyncLock _sync
                If _client Is Nothing Then Return

                Try
                    RemoveHandler _client.OnIam, AddressOf HandleIam
                Catch
                End Try

                Try
                    _client.Dispose()
                Catch
                End Try

                Try
                    If _transport IsNot Nothing Then _transport.Dispose()
                Catch
                End Try

                _client = Nothing
                _transport = Nothing
                _deviceAddress = Nothing
                _deviceId = 0UI
                _isStarted = False

                _discovered.Clear()
            End SyncLock

            RaiseEvent ConnectedChanged(False)
        End Sub

        ''' <summary>
        ''' Broadcast WhoIs and wait for IAm. If preferredDeviceId = 0, binds to first reply.
        ''' </summary>
        Public Function DiscoverAndBind(preferredDeviceId As UInteger, timeoutMs As Integer) As Boolean
            ' Clear previous discovery results
            SyncLock _sync
                _discovered.Clear()
            End SyncLock

            ' Send WhoIs on all active interfaces (best effort)
            ' NOTE: some devices only send periodic I-Am (e.g. every 30s).
            ' Therefore we WAIT for up to timeoutMs and bind as soon as an I-Am arrives.
            SendWhoIsMultiBroadcast()

            Dim sw As New Stopwatch()
            sw.Start()
            Do
                SyncLock _sync
                    If preferredDeviceId <> 0UI AndAlso _discovered.ContainsKey(preferredDeviceId) Then Exit Do
                    If preferredDeviceId = 0UI AndAlso _discovered.Count > 0 Then Exit Do
                End SyncLock
                Thread.Sleep(80)
            Loop While sw.ElapsedMilliseconds < Math.Max(500, timeoutMs)

            Dim found As Boolean = False
            Dim bindId As UInteger = 0UI
            Dim bindAddr As BacnetAddress = Nothing

            SyncLock _sync
                ' Prefer requested deviceId if present; otherwise bind first discovered device.
                If preferredDeviceId <> 0UI AndAlso _discovered.ContainsKey(preferredDeviceId) Then
                    bindId = preferredDeviceId
                    bindAddr = _discovered(preferredDeviceId)
                ElseIf _discovered.Count > 0 Then
                    bindId = _discovered.Keys.First()
                    bindAddr = _discovered(bindId)
                End If

                If bindAddr IsNot Nothing Then
                    _deviceId = bindId
                    _deviceAddress = bindAddr
                    found = True
                End If
            End SyncLock

            ' Verify we can actually communicate with the bound device.
            ' Without this, the UI can show CONNECTED even though Read/Write will never work
            ' (for example, wrong instance number or blocked unicast responses).
            If found Then
                Dim okVerify As Boolean = VerifyBoundDevice(1800)
                If Not okVerify Then
                    Unbind()
                    found = False
                End If
            End If

            RaiseEvent ConnectedChanged(found)
            Return found
        End Function

        ''' <summary>
        ''' Best-effort verify that the currently bound device responds to a ReadProperty.
        ''' </summary>
        Public Function VerifyBoundDevice(Optional timeoutMs As Integer = 1500) As Boolean
            Dim sw As New Stopwatch()
            sw.Start()
            Do
                Dim name As String = Nothing
                If ReadDeviceString(BacnetPropertyIds.PROP_OBJECT_NAME, name) Then
                    Return True
                End If
                Thread.Sleep(100)
            Loop While sw.ElapsedMilliseconds < Math.Max(300, timeoutMs)
            Return False
        End Function

        ''' <summary>
        ''' Fallback discovery: try to bind by iterating IPv4 addresses from the local ARP cache.
        ''' This is very effective on link-local (169.254.x.x) networks when UDP broadcasts are blocked.
        ''' </summary>
        Public Function TryBindUsingArpCache(deviceId As UInteger, Optional udpPort As UShort = 47808US, Optional timeoutMs As Integer = 2500) As Boolean
            If deviceId = 0UI Then Return False

            Dim candidates As List(Of IPAddress) = GetArpIpv4Candidates()
            If candidates Is Nothing OrElse candidates.Count = 0 Then Return False

            ' Prefer link-local candidates first (common when ESP32 falls back to 169.254.x.x)
            candidates = candidates.OrderByDescending(Function(ip) ip.ToString().StartsWith("169.254.")).ToList()

            For Each ip In candidates
                Try
                    If BindToIp(ip.ToString(), deviceId, udpPort, timeoutMs) Then
                        Return True
                    End If
                Catch
                    ' ignore and continue
                End Try
            Next

            Return False
        End Function

        Private Sub HandleIam(sender As BacnetClient, adr As BacnetAddress, deviceId As UInteger, maxApdu As UInteger, segmentation As BacnetSegmentations, vendorId As UShort)
            SyncLock _sync
                ' Store latest address for each device instance
                If _discovered.ContainsKey(deviceId) Then
                    _discovered(deviceId) = adr
                Else
                    _discovered.Add(deviceId, adr)
                End If
            End SyncLock
            RaiseEvent DeviceDiscovered(deviceId, adr)
        End Sub

        Private Function GetArpIpv4Candidates() As List(Of IPAddress)
            Dim res As New List(Of IPAddress)()

            Try
                Dim psi As New ProcessStartInfo("arp", "-a") With {
                    .CreateNoWindow = True,
                    .UseShellExecute = False,
                    .RedirectStandardOutput = True,
                    .RedirectStandardError = True
                }

                Using p As Process = Process.Start(psi)
                    If p Is Nothing Then Return res
                    Dim output As String = p.StandardOutput.ReadToEnd()
                    Try
                        p.WaitForExit(1500)
                    Catch
                    End Try

                    Dim lines = output.Split({ControlChars.Cr, ControlChars.Lf}, StringSplitOptions.RemoveEmptyEntries)
                    For Each line In lines
                        Dim parts = line.Trim().Split(New Char() {" "c, ControlChars.Tab}, StringSplitOptions.RemoveEmptyEntries)
                        If parts.Length = 0 Then Continue For

                        Dim ip As IPAddress = Nothing
                        If IPAddress.TryParse(parts(0), ip) AndAlso ip.AddressFamily = Sockets.AddressFamily.InterNetwork Then
                            If Not res.Contains(ip) Then res.Add(ip)
                        End If
                    Next
                End Using
            Catch
                ' ignore
            End Try

            ' Remove obvious non-hosts
            res.RemoveAll(Function(ip) ip.ToString() = "0.0.0.0" OrElse ip.ToString() = "255.255.255.255")
            Return res
        End Function

        ''' <summary>
        ''' Returns a snapshot list of discovered BACnet devices (deviceId->address).
        ''' </summary>
        Public Function GetDiscoveredDevices() As Dictionary(Of UInteger, BacnetAddress)
            SyncLock _sync
                Return New Dictionary(Of UInteger, BacnetAddress)(_discovered)
            End SyncLock
        End Function

        ' ---------------------- Discovery Helpers ----------------------

        Private Sub TryConfigureBroadcastAddress()
            ' The System.IO.BACnet transport has a BroadcastAddress property in newer builds.
            ' We set it to the first active NIC subnet broadcast address when possible.
            Try
                Dim t As Object = _transport
                If t Is Nothing Then Return

                Dim prop = t.GetType().GetProperty("BroadcastAddress")
                If prop Is Nothing OrElse Not prop.CanWrite Then Return

                Dim bc = GetFirstSubnetBroadcast()
                If bc IsNot Nothing Then
                    prop.SetValue(t, bc, Nothing)
                End If
            Catch
                ' ignore
            End Try
        End Sub

        Private Sub SendWhoIsMultiBroadcast()
            Dim cli As BacnetClient = Nothing
            Dim t As Object = Nothing
            SyncLock _sync
                cli = _client
                t = _transport
            End SyncLock
            If cli Is Nothing OrElse t Is Nothing Then Return

            Dim prop = t.GetType().GetProperty("BroadcastAddress")
            Dim broadcasts As List(Of IPAddress) = GetSubnetBroadcasts()

            If broadcasts.Count = 0 Then
                ' fallback - normal broadcast
                Try
                    cli.WhoIs()
                Catch
                End Try
                Return
            End If

            ' Send WhoIs for each active NIC broadcast address
            For Each bc In broadcasts
                Try
                    If prop IsNot Nothing AndAlso prop.CanWrite Then
                        prop.SetValue(t, bc, Nothing)
                    End If
                    cli.WhoIs()
                Catch
                    ' ignore and continue
                End Try
                Thread.Sleep(60)
            Next
        End Sub

        Private Function GetFirstSubnetBroadcast() As IPAddress
            Dim bcs = GetSubnetBroadcasts()
            If bcs.Count > 0 Then Return bcs(0)
            Return Nothing
        End Function

        Private Function GetSubnetBroadcasts() As List(Of IPAddress)
            Dim res As New List(Of IPAddress)()

            Try
                For Each ni In NetworkInterface.GetAllNetworkInterfaces()
                    If ni.OperationalStatus <> OperationalStatus.Up Then Continue For
                    If ni.NetworkInterfaceType = NetworkInterfaceType.Loopback Then Continue For

                    Dim ipProps = ni.GetIPProperties()
                    For Each ua In ipProps.UnicastAddresses
                        If ua.Address Is Nothing OrElse ua.IPv4Mask Is Nothing Then Continue For
                        If ua.Address.AddressFamily <> Sockets.AddressFamily.InterNetwork Then Continue For

                        Dim bc = ComputeBroadcast(ua.Address, ua.IPv4Mask)
                        If bc IsNot Nothing AndAlso Not res.Contains(bc) Then
                            res.Add(bc)
                        End If
                    Next
                Next
            Catch
                ' ignore
            End Try

            Return res
        End Function

        Private Function ComputeBroadcast(ip As IPAddress, mask As IPAddress) As IPAddress
            Try
                Dim ipBytes = ip.GetAddressBytes()
                Dim maskBytes = mask.GetAddressBytes()
                If ipBytes.Length <> 4 OrElse maskBytes.Length <> 4 Then Return Nothing

                Dim bc(3) As Byte
                For i As Integer = 0 To 3
                    bc(i) = CByte(ipBytes(i) Or (maskBytes(i) Xor &HFF))
                Next
                Return New IPAddress(bc)
            Catch
                Return Nothing
            End Try
        End Function

        Public Sub Unbind()
            SyncLock _sync
                _deviceAddress = Nothing
                _deviceId = 0UI
            End SyncLock
            RaiseEvent ConnectedChanged(False)
        End Sub

        ' ---------------------- Read/Write helpers ----------------------

        Public Function ReadBinaryInput(instance As UInteger, ByRef valueOut As Boolean) As Boolean
            Return ReadBoolean(BacnetObjectTypes.OBJECT_BINARY_INPUT, instance, BacnetPropertyIds.PROP_PRESENT_VALUE, valueOut)
        End Function

        Public Function ReadBinaryOutput(instance As UInteger, ByRef valueOut As Boolean) As Boolean
            Return ReadBoolean(BacnetObjectTypes.OBJECT_BINARY_OUTPUT, instance, BacnetPropertyIds.PROP_PRESENT_VALUE, valueOut)
        End Function

        Public Function WriteBinaryOutput(instance As UInteger, value As Boolean, Optional priority As Byte = 16) As Boolean
            Dim v As New BacnetValue(BacnetApplicationTags.BACNET_APPLICATION_TAG_ENUMERATED, If(value, 1UI, 0UI))
            Return WriteProperty(BacnetObjectTypes.OBJECT_BINARY_OUTPUT, instance, BacnetPropertyIds.PROP_PRESENT_VALUE, New BacnetValue() {v}, priority)
        End Function

        Public Function ReadAnalogInput(instance As UInteger, ByRef valueOut As Double) As Boolean
            Return ReadReal(BacnetObjectTypes.OBJECT_ANALOG_INPUT, instance, BacnetPropertyIds.PROP_PRESENT_VALUE, valueOut)
        End Function

        Public Function ReadAnalogValue(instance As UInteger, ByRef valueOut As Double) As Boolean
            Return ReadReal(BacnetObjectTypes.OBJECT_ANALOG_VALUE, instance, BacnetPropertyIds.PROP_PRESENT_VALUE, valueOut)
        End Function

        Public Function ReadDeviceString(propId As BacnetPropertyIds, ByRef valueOut As String) As Boolean
            Dim vals As IList(Of BacnetValue) = Nothing
            Dim ok As Boolean = ReadProperty(BacnetObjectTypes.OBJECT_DEVICE, BoundDeviceId, propId, vals)
            If Not ok OrElse vals Is Nothing OrElse vals.Count = 0 Then Return False

	        Dim first = vals(0)
	        ' BacnetValue is a Structure (Value Type) in System.IO.BACnet, so it can never be Nothing.
	        If first.Value Is Nothing Then Return False

            valueOut = Convert.ToString(first.Value)
            Return True
        End Function

        Private Function ReadBoolean(objType As BacnetObjectTypes, instance As UInteger, propId As BacnetPropertyIds, ByRef valueOut As Boolean) As Boolean
            Dim vals As IList(Of BacnetValue) = Nothing
            If Not ReadProperty(objType, instance, propId, vals) Then Return False
            If vals Is Nothing OrElse vals.Count = 0 Then Return False

	        Dim v = vals(0)
	        ' BacnetValue is a Structure (Value Type) in System.IO.BACnet, so it can never be Nothing.
	        If v.Value Is Nothing Then Return False

            ' For Binary Input/Output Present_Value is ENUMERATED (0/1)
            Dim n As UInteger
            If TypeOf v.Value Is UInteger Then
                n = CUInt(v.Value)
            ElseIf TypeOf v.Value Is Integer Then
                n = CUInt(CInt(v.Value))
            ElseIf UInteger.TryParse(Convert.ToString(v.Value), n) Then
            Else
                Return False
            End If

            valueOut = (n <> 0UI)
            Return True
        End Function

        Private Function ReadReal(objType As BacnetObjectTypes, instance As UInteger, propId As BacnetPropertyIds, ByRef valueOut As Double) As Boolean
            Dim vals As IList(Of BacnetValue) = Nothing
            If Not ReadProperty(objType, instance, propId, vals) Then Return False
            If vals Is Nothing OrElse vals.Count = 0 Then Return False

	        Dim v = vals(0)
	        ' BacnetValue is a Structure (Value Type) in System.IO.BACnet, so it can never be Nothing.
	        If v.Value Is Nothing Then Return False

            If TypeOf v.Value Is Single Then
                valueOut = CDbl(CSng(v.Value))
                Return True
            End If
            If TypeOf v.Value Is Double Then
                valueOut = CDbl(v.Value)
                Return True
            End If

            Dim d As Double
            If Double.TryParse(Convert.ToString(v.Value), Globalization.NumberStyles.Float, Globalization.CultureInfo.InvariantCulture, d) Then
                valueOut = d
                Return True
            End If

            Return False
        End Function

        Private Function ReadProperty(objType As BacnetObjectTypes, instance As UInteger, propId As BacnetPropertyIds, ByRef valuesOut As IList(Of BacnetValue)) As Boolean
            Dim cli As BacnetClient = Nothing
            Dim addr As BacnetAddress = Nothing
            SyncLock _sync
                cli = _client
                addr = _deviceAddress
            End SyncLock

            If cli Is Nothing OrElse addr Is Nothing Then Return False

            Dim objId As New BacnetObjectId(objType, instance)
            Dim values As IList(Of BacnetValue) = Nothing

            Try
                Dim ok As Boolean = cli.ReadPropertyRequest(addr, objId, propId, values)
                If ok Then valuesOut = values
                Return ok
            Catch
                Return False
            End Try
        End Function

        Private Function WriteProperty(objType As BacnetObjectTypes, instance As UInteger, propId As BacnetPropertyIds, values As IEnumerable(Of BacnetValue), _priority As Byte) As Boolean
            Dim cli As BacnetClient = Nothing
            Dim addr As BacnetAddress = Nothing
            SyncLock _sync
                cli = _client
                addr = _deviceAddress
            End SyncLock

            If cli Is Nothing OrElse addr Is Nothing Then Return False

            Dim objId As New BacnetObjectId(objType, instance)

	        Try
	            ' System.IO.BACnet WritePropertyRequest signature (net48 build) does not take priority/confirmed flags.
	            ' We keep the "priority" argument in our API for future-proofing, but do not pass it to the library here.
	            Dim ok As Boolean = cli.WritePropertyRequest(addr, objId, propId, values.ToList())
	            Return ok
	        Catch
	            Return False
	        End Try
        End Function

    End Class

End Namespace
