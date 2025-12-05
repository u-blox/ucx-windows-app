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

    public bool IsConnected => _handle != IntPtr.Zero && UcxNative.ucx_is_connected(_handle);

    public UcxClient(string portName, int baudRate = 115200)
    {
        _handle = UcxNative.ucx_create(portName, baudRate);
        if (_handle == IntPtr.Zero)
        {
            throw new InvalidOperationException($"Failed to create UCX client on port {portName}");
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
        UrcReceived?.Invoke(this, new UrcEventArgs(urcLine));
    }

    private void OnLogReceived(int level, string message, IntPtr userData)
    {
        LogReceived?.Invoke(this, new LogEventArgs(level, message));
    }

    public async Task<string> SendAtCommandAsync(string command, int timeoutMs = 5000)
    {
        return await Task.Run(() =>
        {
            if (_handle == IntPtr.Zero)
            {
                throw new ObjectDisposedException(nameof(UcxClient));
            }

            // Allocate response buffer
            int bufferSize = 1024;
            IntPtr responsePtr = Marshal.AllocHGlobal(bufferSize);
            try
            {
                // Send command
                int result = UcxNative.ucx_send_at_command(_handle, command, responsePtr, bufferSize, timeoutMs);

                if (result != UcxNative.UCX_OK)
                {
                    string? error = UcxNative.ucx_get_last_error(_handle);
                    throw new InvalidOperationException($"AT command failed: {error ?? "Unknown error"}");
                }

                // Marshal response
                string response = Marshal.PtrToStringAnsi(responsePtr) ?? string.Empty;
                return response;
            }
            finally
            {
                Marshal.FreeHGlobal(responsePtr);
            }
        });
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

            const int maxResults = 50;
            var results = new WifiScanResult[maxResults];

            System.Diagnostics.Debug.WriteLine($"[UcxClient] Calling native ucx_wifi_scan...");
            int count = UcxNative.ucx_wifi_scan(_handle, results, maxResults, timeoutMs);
            System.Diagnostics.Debug.WriteLine($"[UcxClient] Native scan returned: {count}");

            if (count < 0)
            {
                string? error = UcxNative.ucx_get_last_error(_handle);
                System.Diagnostics.Debug.WriteLine($"[UcxClient] Scan failed: {error}");
                throw new InvalidOperationException($"WiFi scan failed: {error ?? "Unknown error"}");
            }

            // Debug: print first few results
            for (int i = 0; i < Math.Min(3, count); i++)
            {
                System.Diagnostics.Debug.WriteLine($"[UcxClient] Result[{i}]: ssid='{results[i].ssid}', channel={results[i].channel}, rssi={results[i].rssi}");
            }

            return new List<WifiScanResult>(results.Take(count));
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
