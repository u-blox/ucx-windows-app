using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;

namespace UcxAvaloniaApp.Models;

[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
public struct WifiScanResult
{
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 6)]
    private byte[] _bssid;
    
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 33)]
    private string _ssid;
    
    private int _channel;
    private int _rssi;
    private int _authSuites;
    private int _unicastCiphers;
    private int _groupCiphers;
    
    public byte[] Bssid => _bssid;
    public string Ssid => _ssid ?? string.Empty;
    public int Channel => _channel;
    public int Rssi => _rssi;
    public int AuthSuites => _authSuites;
    public int UnicastCiphers => _unicastCiphers;
    public int GroupCiphers => _groupCiphers;
    
    public string BssidString => _bssid != null ? 
        string.Join(":", _bssid.Select(b => b.ToString("X2"))) : "N/A";
    
    public string SecurityString
    {
        get
        {
            if (_authSuites == 0) return "Open";
            
            var auth = new List<string>();
            if ((_authSuites & (1 << 3)) != 0) auth.Add("WPA");
            if ((_authSuites & (1 << 4)) != 0) auth.Add("WPA2");
            if ((_authSuites & (1 << 5)) != 0) auth.Add("WPA3");
            if ((_authSuites & (1 << 1)) != 0) auth.Add("PSK");
            
            return auth.Count > 0 ? string.Join("/", auth) : "Unknown";
        }
    }
}
