using System;
using System.Collections.Generic;
using System.Linq;

namespace UcxAvaloniaApp.Models;

/// <summary>
/// Class representation of WiFi network for UI binding
/// </summary>
public class WifiNetwork
{
    public string Ssid { get; set; } = string.Empty;
    public string BssidString { get; set; } = string.Empty;
    public int Channel { get; set; }
    public int Rssi { get; set; }
    public string SecurityString { get; set; } = string.Empty;

    public static WifiNetwork FromScanResult(WifiScanResult result)
    {
        var authList = new List<string>();
        if (result.auth_suites == 0)
        {
            authList.Add("Open");
        }
        else
        {
            if ((result.auth_suites & (1 << 3)) != 0) authList.Add("WPA");
            if ((result.auth_suites & (1 << 4)) != 0) authList.Add("WPA2");
            if ((result.auth_suites & (1 << 5)) != 0) authList.Add("WPA3");
            if ((result.auth_suites & (1 << 1)) != 0) authList.Add("PSK");
        }

        var bssidString = result.bssid != null && result.bssid.Length == 6
            ? string.Join(":", result.bssid.Select(b => b.ToString("X2")))
            : "N/A";

        return new WifiNetwork
        {
            Ssid = result.ssid ?? string.Empty,
            BssidString = bssidString,
            Channel = result.channel,
            Rssi = result.rssi,
            SecurityString = authList.Count > 0 ? string.Join("/", authList) : "Unknown"
        };
    }
}
