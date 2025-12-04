using System;
using System.Collections.Generic;
using System.IO.Ports;
using System.Linq;

namespace UcxAvaloniaApp.Services;

public class SerialPortService
{
    /// <summary>
    /// Get list of available COM ports
    /// </summary>
    public static List<string> GetAvailablePorts()
    {
        return SerialPort.GetPortNames()
            .OrderBy(p => p)
            .ToList();
    }

    /// <summary>
    /// Check if a port name is valid
    /// </summary>
    public static bool IsValidPort(string? portName)
    {
        if (string.IsNullOrWhiteSpace(portName))
            return false;

        return GetAvailablePorts().Contains(portName);
    }
}
