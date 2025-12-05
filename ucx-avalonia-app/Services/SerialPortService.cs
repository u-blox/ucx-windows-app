using System;
using System.Collections.Generic;
using System.IO.Ports;
using System.Linq;
using System.Runtime.InteropServices;

namespace UcxAvaloniaApp.Services;

public class SerialPortService
{
    /// <summary>
    /// Get list of available serial ports (cross-platform)
    /// </summary>
    public static List<string> GetAvailablePorts()
    {
        var ports = SerialPort.GetPortNames().ToList();
        
        // On Linux/macOS, also check for common USB serial devices
        if (!RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
        {
            // Add common Linux/macOS USB serial port patterns
            var devDir = "/dev";
            if (System.IO.Directory.Exists(devDir))
            {
                var usbPorts = System.IO.Directory.GetFiles(devDir)
                    .Where(p => p.StartsWith("/dev/ttyUSB") || 
                                p.StartsWith("/dev/ttyACM") ||
                                p.StartsWith("/dev/cu.usbserial") ||
                                p.StartsWith("/dev/cu.usbmodem"))
                    .ToList();
                
                ports.AddRange(usbPorts);
            }
        }
        
        return ports.Distinct().OrderBy(p => p).ToList();
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
