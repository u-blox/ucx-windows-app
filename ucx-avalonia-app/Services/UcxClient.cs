using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using UcxAvaloniaApp.Models;

namespace UcxAvaloniaApp.Services;

public class UrcEventArgs : EventArgs
{
    public string UrcLine { get; }
    public DateTime Timestamp { get; }

    public UrcEventArgs(string urcLine)
    {
        UrcLine = urcLine;
        Timestamp = DateTime.Now;
    }
}

public class LogEventArgs : EventArgs
{
    public int Level { get; }
    public string Message { get; }
    public DateTime Timestamp { get; }

    public LogEventArgs(int level, string message)
    {
        Level = level;
        Message = message;
        Timestamp = DateTime.Now;
    }
}

/// <summary>
/// Managed wrapper around the native ucxclient_wrapper.dll
/// </summary>
public class UcxClient : IDisposable
{
    private IntPtr _handle;
    private UcxNative.UrcCallback? _urcCallback;
    private UcxNative.LogCallback? _logCallback;
    private bool _disposed;

    public event EventHandler<UrcEventArgs>? UrcReceived;
    public event EventHandler<LogEventArgs>? LogReceived;

    public bool IsConnected => _handle != IntPtr.Zero;

    public UcxClient(string portName, int baudRate = 115200)
    {
        _handle = UcxNative.ucx_create(portName, baudRate);
        if (_handle == IntPtr.Zero)
        {
            string? error = UcxNative.ucx_get_last_error(IntPtr.Zero);
            throw new InvalidOperationException($"Failed to create UCX client on port {portName}: {error ?? "Unknown error"}");
        }

        // Set up URC callback
        _urcCallback = OnUrcReceived;
        UcxNative.ucx_set_urc_callback(_handle, _urcCallback, IntPtr.Zero);

        // Set up log callback
        _logCallback = OnLogReceived;
        UcxNative.ucx_set_log_callback(_handle, _logCallback, IntPtr.Zero);
    }

    private void OnUrcReceived(string urcLine, IntPtr userData)
    {
        try
        {
            System.Console.WriteLine($"[UcxClient-NativeCallback] URC received: {urcLine}");
            // URC callback is called from native thread - must not throw exceptions
            UrcReceived?.Invoke(this, new UrcEventArgs(urcLine));
            System.Console.WriteLine($"[UcxClient-NativeCallback] URC event invoked successfully");
        }
        catch (Exception ex)
        {
            // Log error but don't let it propagate to native code
            System.Console.WriteLine($"[UcxClient-NativeCallback] URC callback EXCEPTION: {ex.GetType().Name}: {ex.Message}");
            System.Console.WriteLine($"[UcxClient-NativeCallback] Stack trace: {ex.StackTrace}");
            System.Diagnostics.Debug.WriteLine($"[UcxClient] URC callback exception: {ex}");
        }
    }

    private void OnLogReceived(int level, string message, IntPtr userData)
    {
        try
        {
            // Log callback is called from native thread - must not throw exceptions
            LogReceived?.Invoke(this, new LogEventArgs(level, message));
        }
        catch (Exception ex)
        {
            // Log error but don't let it propagate to native code
            System.Console.WriteLine($"[UcxClient-NativeCallback] Log callback EXCEPTION: {ex.GetType().Name}: {ex.Message}");
            System.Diagnostics.Debug.WriteLine($"[UcxClient] Log callback exception: {ex}");
        }
    }

    public async Task<string> SendAtCommandAsync(string command, int timeoutMs = 5000)
    {
        // TODO: Implement using generated ucx_at_* functions
        await Task.CompletedTask;
        throw new NotImplementedException("SendAtCommandAsync not yet implemented with generated wrapper");
    }

    public string? GetLastError()
    {
        if (_handle == IntPtr.Zero)
            return null;

        return UcxNative.ucx_get_last_error(_handle);
    }

    public async Task<List<WifiScanResult>> ScanWifiAsync(int timeoutMs = 10000)
    {
        return await Task.Run(() =>
        {
            if (_handle == IntPtr.Zero)
            {
                throw new ObjectDisposedException(nameof(UcxClient));
            }

            System.Diagnostics.Debug.WriteLine($"[UcxClient] Starting WiFi scan using generated API...");
            
            // Use generated API: StationScan1Begin(0) for passive scan
            UcxNativeGenerated.ucx_wifi_StationScan1Begin(_handle, 0);
            
            var results = new List<WifiScanResult>();
            
            // Note: StationScan1GetNext requires proper struct handling
            // For now, return empty list - full implementation needs UCX struct definitions
            System.Diagnostics.Debug.WriteLine($"[UcxClient] Scan initiated (result parsing not yet implemented)");
            
            return results;
        });
    }

    public async Task ConnectWifiAsync(string ssid, string? password, int timeoutMs = 30000)
    {
        await Task.Run(() =>
        {
            System.Diagnostics.Debug.WriteLine($"[UcxClient] ConnectWifiAsync START - SSID: {ssid}");
            System.Console.WriteLine($"[UcxClient] ConnectWifiAsync START - SSID: {ssid}");
            
            if (_handle == IntPtr.Zero)
            {
                System.Diagnostics.Debug.WriteLine("[UcxClient] ERROR: Handle is Zero");
                throw new ObjectDisposedException(nameof(UcxClient));
            }

            System.Diagnostics.Debug.WriteLine($"[UcxClient] Using generated API for WiFi connection...");
            System.Console.WriteLine($"[UcxClient] Using generated API for WiFi connection...");
            
            int wlan_handle = 0;  // Station interface
            int result;
            
            // Step 1: Set connection parameters (SSID) - MUST BE FIRST
            System.Console.WriteLine($"[UcxClient] Step 1: Setting connection parameters (SSID)...");
            result = UcxNativeGenerated.ucx_wifi_StationSetConnectionParams(_handle, wlan_handle, ssid);
            System.Console.WriteLine($"[UcxClient] StationSetConnectionParams returned: {result}");
            
            if (result != 0)
            {
                string? error = UcxNative.ucx_get_last_error(_handle);
                System.Console.WriteLine($"[UcxClient] Error details: {error}");
                throw new InvalidOperationException($"Failed to set connection parameters: {result} - {error}");
            }
            
            // Step 2: Set security
            if (!string.IsNullOrEmpty(password))
            {
                System.Console.WriteLine($"[UcxClient] Step 2: Setting WPA security with password length: {password.Length}...");
                // wpa_threshold: 0=WPA/WPA2, 1=WPA2 only, 2=WPA2/WPA3
                result = UcxNativeGenerated.ucx_wifi_StationSetSecurityWpa(_handle, wlan_handle, password, 0);
                System.Console.WriteLine($"[UcxClient] StationSetSecurityWpa returned: {result}");
                
                if (result != 0)
                {
                    string? error = UcxNative.ucx_get_last_error(_handle);
                    System.Console.WriteLine($"[UcxClient] Error details: {error}");
                    throw new InvalidOperationException($"Failed to set WiFi security: {result} - {error}");
                }
            }
            else
            {
                System.Console.WriteLine($"[UcxClient] Step 2: Setting open security...");
                result = UcxNativeGenerated.ucx_wifi_StationSetSecurityOpen(_handle, wlan_handle);
                System.Console.WriteLine($"[UcxClient] StationSetSecurityOpen returned: {result}");
                
                if (result != 0)
                {
                    string? error = UcxNative.ucx_get_last_error(_handle);
                    throw new InvalidOperationException($"Failed to set open security: {result} - {error}");
                }
            }
            
            // Step 3: Connect
            System.Console.WriteLine($"[UcxClient] Step 3: Initiating connection...");
            result = UcxNativeGenerated.ucx_wifi_StationConnect(_handle, wlan_handle);
            System.Console.WriteLine($"[UcxClient] StationConnect returned: {result}");
            
            if (result != 0)
            {
                string? error = UcxNative.ucx_get_last_error(_handle);
                throw new InvalidOperationException($"Failed to connect: {result} - {error}");
            }
            
            System.Console.WriteLine($"[UcxClient] WiFi connection initiated successfully!");
        });
    }

    public async Task DisconnectWifiAsync()
    {
        await Task.Run(() =>
        {
            if (_handle == IntPtr.Zero)
            {
                throw new ObjectDisposedException(nameof(UcxClient));
            }

            int result = UcxNativeGenerated.ucx_wifi_StationDisconnect(_handle);

            if (result != 0)
            {
                throw new InvalidOperationException($"WiFi disconnect failed: {result}");
            }
        });
    }

    public async Task<UcxNative.WifiConnectionInfo> GetWifiConnectionInfoAsync()
    {
        return await Task.Run(() =>
        {
            if (_handle == IntPtr.Zero)
            {
                throw new ObjectDisposedException(nameof(UcxClient));
            }

            UcxNative.WifiConnectionInfo info;
            int result = UcxNative.ucx_wifi_get_connection_info(_handle, out info);

            if (result < 0)
            {
                string? error = UcxNative.ucx_get_last_error(_handle);
                throw new InvalidOperationException($"Failed to get connection info: {error ?? "Unknown error"}");
            }

            return info;
        });
    }

    public void Dispose()
    {
        if (!_disposed)
        {
            if (_handle != IntPtr.Zero)
            {
                UcxNative.ucx_destroy(_handle);
                _handle = IntPtr.Zero;
            }
            _disposed = true;
        }
        GC.SuppressFinalize(this);
    }

    ~UcxClient()
    {
        Dispose();
    }
}
