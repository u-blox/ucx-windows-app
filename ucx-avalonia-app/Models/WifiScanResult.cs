using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;

namespace UcxAvaloniaApp.Models;

[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
public struct WifiScanResult
{
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 6)]
    public byte[] bssid;
    
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 33)]
    public string ssid;
    
    public int channel;
    public int rssi;
    public int auth_suites;
    public int unicast_ciphers;
    public int group_ciphers;
    
    // Read-only properties for UI binding
    public readonly string Ssid => ssid ?? string.Empty;
    public readonly int Channel => channel;
    public readonly int Rssi => rssi;
    
    public readonly string BssidString => bssid != null ? 
        string.Join(":", bssid.Select(b => b.ToString("X2"))) : "N/A";
    
    public readonly string SecurityString
    {
        get
        {
            if (auth_suites == 0) return "Open";
            
            var authList = new List<string>();
            if ((auth_suites & (1 << 3)) != 0) authList.Add("WPA");
            if ((auth_suites & (1 << 4)) != 0) authList.Add("WPA2");
            if ((auth_suites & (1 << 5)) != 0) authList.Add("WPA3");
            if ((auth_suites & (1 << 1)) != 0) authList.Add("PSK");
            
            return authList.Count > 0 ? string.Join("/", authList) : "Unknown";
        }
    }
}
