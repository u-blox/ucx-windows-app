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

        #region UCX API Struct Definitions

        // IP Address structures (from u_cx_at_params.h)
        public enum USockAddressType
        {
            V4 = 0,
            V6 = 6
        }

        [StructLayout(LayoutKind.Explicit, Size = 20)]
        public struct USockIpAddress
        {
            [FieldOffset(0)]
            public USockAddressType type;
            
            [FieldOffset(4)]
            public uint ipv4;
            
            [FieldOffset(4)]
            public uint ipv6_0;
            [FieldOffset(8)]
            public uint ipv6_1;
            [FieldOffset(12)]
            public uint ipv6_2;
            [FieldOffset(16)]
            public uint ipv6_3;
        }

        // WiFi scan structures
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        public struct UcxMacAddress
        {
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 6)]
            public byte[] address;
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        public struct UcxWifiStationScan
        {
            public UcxMacAddress bssid;
            public IntPtr ssid;  // const char* in C
            public int channel;
            public int rssi;
            public int authentication_suites;
            public int unicast_ciphers;
            public int group_ciphers;
        }

        #endregion

        #region Core Helper Functions

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

        #region Core Helper Functions

        /// <summary>
        /// Call uCxEnd to terminate a Begin/GetNext sequence (e.g., after WiFi scan).
        /// </summary>
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "ucx_End")]
        public static extern void ucx_End(IntPtr handle);

        #endregion
    }

    // Alias the partial class to match existing code
    public static partial class UcxNative
    {
    }
}
