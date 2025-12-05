// Manual UCX API declarations (high-level functions not in auto-generated wrapper)

using System;
using System.Runtime.InteropServices;

namespace UcxAvaloniaApp.Services
{
    public static partial class UcxNative
    {
        private const string DllName = "ucxclient_wrapper";

        #region Enums and Constants

        public const int UCX_OK = 0;
        public const int UCX_ERROR_INVALID_PARAM = -1;
        public const int UCX_ERROR_NO_MEMORY = -2;
        public const int UCX_ERROR_TIMEOUT = -3;
        public const int UCX_ERROR_NOT_CONNECTED = -4;
        public const int UCX_ERROR_AT_FAIL = -5;

        #endregion

        #region Structures

        /// <summary>
        /// WiFi connection information structure.
        /// Must match the C structure ucx_wifi_connection_info_t in ucxclient_wrapper.h
        /// </summary>
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        public struct WifiConnectionInfo
        {
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 40)]
            public string IpAddress;

            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 40)]
            public string SubnetMask;

            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 40)]
            public string Gateway;

            public int Channel;
            public int Rssi;
        }

        #endregion

        #region Core Functions (from ucxclient_wrapper_core.c)

        /// <summary>
        /// Create a UCX client instance and open the serial port.
        /// </summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr ucx_create(
            [MarshalAs(UnmanagedType.LPStr)] string portName,
            int baudRate);

        /// <summary>
        /// Destroy UCX client instance and close the serial port.
        /// </summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ucx_destroy(IntPtr handle);

        /// <summary>
        /// Get the last error message.
        /// </summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.LPStr)]
        public static extern string ucx_get_last_error(IntPtr handle);

        #endregion

        #region Callback Functions

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void UrcCallback(
            [MarshalAs(UnmanagedType.LPStr)] string urcLine,
            IntPtr userData);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void LogCallback(
            int level,
            [MarshalAs(UnmanagedType.LPStr)] string message,
            IntPtr userData);

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

        #endregion

        #region High-Level WiFi Functions (from ucxclient_wrapper_core.c)

        /// <summary>
        /// Connect to a WiFi network.
        /// </summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int ucx_wifi_connect(
            IntPtr handle,
            [MarshalAs(UnmanagedType.LPStr)] string ssid,
            [MarshalAs(UnmanagedType.LPStr)] string password,
            int timeoutMs);

        /// <summary>
        /// Disconnect from WiFi network.
        /// </summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int ucx_wifi_disconnect(IntPtr handle);

        /// <summary>
        /// Get WiFi connection information (IP address, gateway, etc).
        /// Uses ucx_wifi_StationGetNetworkStatus from the generated API.
        /// </summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int ucx_wifi_get_connection_info(
            IntPtr handle,
            out WifiConnectionInfo info);

        /// <summary>
        /// Scan for WiFi networks.
        /// </summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int ucx_wifi_scan(
            IntPtr handle,
            IntPtr results,
            int maxResults,
            int timeoutMs);

        #endregion
    }

    // Alias the partial class to match existing code
    public static partial class UcxNative
    {
    }
}
