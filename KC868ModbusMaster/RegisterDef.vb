Option Strict On
Option Explicit On

Namespace KC868ModbusMaster
    Public Enum ModbusTable
        Coils
        DiscreteInputs
        InputRegisters
        HoldingRegisters
    End Enum

    Public Enum RegDataType
        Bool
        U16
        S16
        U32
        Float32
        StringAscii
        BitmaskU16
        BytesU16 ' one byte per register (0..255)
    End Enum

    Public Class RegisterDef
        Public Property Table As ModbusTable
        Public Property Ref As Integer          ' e.g. 30001, 40011, 00019 (as int)
        Public Property Offset As Integer       ' 0-based offset for master calls
        Public Property Count As Integer        ' number of 16-bit registers / bits
        Public Property Access As String        ' "R", "W", "R/W"
        Public Property DataType As RegDataType
        Public Property Units As String
        Public Property Description As String

        Public Sub New(table As ModbusTable, refNum As Integer, offset As Integer, count As Integer, access As String, dt As RegDataType, units As String, desc As String)
            Me.Table = table
            Me.Ref = refNum
            Me.Offset = offset
            Me.Count = count
            Me.Access = access
            Me.DataType = dt
            Me.Units = units
            Me.Description = desc
        End Sub

        Public ReadOnly Property IsWritable As Boolean
            Get
                Return Access.Contains("W")
            End Get
        End Property
    End Class
End Namespace
