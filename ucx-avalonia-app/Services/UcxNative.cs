using System;
using System.Runtime.InteropServices;

namespace UcxAvaloniaApp.Services;

/// <summary>
/// P/Invoke declarations for ucxclient_wrapper native library (cross-platform)
/// </summary>
internal static class UcxNative
{
    // Platform-specific library name
    private const string DllName = "ucxclient_wrapper";
    
    // Note: .NET will automatically append .dll (Windows), .so (Linux), or .dylib (macOS)

    // Error codes
    public const int UCX_OK = 0;
    public const int UCX_ERROR_INVALID_PARAM = -1;
    public const int UCX_ERROR_NO_MEMORY = -2;
    public const int UCX_ERROR_TIMEOUT = -3;
    public const int UCX_ERROR_NOT_CONNECTED = -4;
    public const int UCX_ERROR_AT_FAIL = -5;
    public const int UCX_ERROR_UART_OPEN_FAIL = -6;

    // Callback delegates
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void UrcCallback([MarshalAs(UnmanagedType.LPStr)] string urcLine, IntPtr userData);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void LogCallback(int level, [MarshalAs(UnmanagedType.LPStr)] string message, IntPtr userData);

    // Native functions
    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr ucx_create(
        [MarshalAs(UnmanagedType.LPStr)] string portName,
        int baudRate);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void ucx_destroy(IntPtr handle);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    public static extern bool ucx_is_connected(IntPtr handle);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int ucx_send_at_command(
        IntPtr handle,
        [MarshalAs(UnmanagedType.LPStr)] string command,
        IntPtr response,
        int responseLen,
        int timeoutMs);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void ucx_set_urc_callback(
        IntPtr handle,
        UrcCallback callback,
        IntPtr userData);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void ucx_set_log_callback(
        IntPtr handle,
        LogCallback callback,
        IntPtr userData);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.LPStr)]
    public static extern string? ucx_get_last_error(IntPtr handle);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int ucx_wifi_scan(
        IntPtr handle,
        [Out, MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 2)] Models.WifiScanResult[] results,
        int maxResults,
        int timeoutMs);
}
