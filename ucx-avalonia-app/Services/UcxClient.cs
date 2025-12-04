using System;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

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

/// <summary>
/// Managed wrapper around the native ucxclient_wrapper.dll
/// </summary>
public class UcxClient : IDisposable
{
    private IntPtr _handle;
    private UcxNative.UrcCallback? _urcCallback;
    private bool _disposed;

    public event EventHandler<UrcEventArgs>? UrcReceived;
    public event EventHandler<string>? LogMessage;

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
    }

    private void OnUrcReceived(string urcLine, IntPtr userData)
    {
        UrcReceived?.Invoke(this, new UrcEventArgs(urcLine));
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
