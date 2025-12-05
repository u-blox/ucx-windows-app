using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading.Tasks;
using Avalonia.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using UcxAvaloniaApp.Services;
using UcxAvaloniaApp.Models;

namespace UcxAvaloniaApp.ViewModels;

public partial class MainWindowViewModel : ViewModelBase
{
    private UcxClient? _ucxClient;

    [ObservableProperty]
    private string? _selectedPort;

    [ObservableProperty]
    private string _atCommand = "";

    [ObservableProperty]
    private string _statusMessage = "Disconnected";

    [ObservableProperty]
    private bool _isConnected;

    [ObservableProperty]
    private bool _isScanning;

    [ObservableProperty]
    private string _wifiScanStatus = "";
    
    [ObservableProperty]
    private string _wifiScanOutput = "";
    
    [ObservableProperty]
    private string _wifiConnectStatus = "";
    
    [ObservableProperty]
    private string _selectedSsid = "";
    
    [ObservableProperty]
    private string _wifiPassword = "";
    
    [ObservableProperty]
    private bool _isConnecting;
    
    [ObservableProperty]
    private bool _isTableView = true;
    
    [ObservableProperty]
    private bool _isGridView = false;
    
    [ObservableProperty]
    private ObservableCollection<WifiNetwork> _wifiNetworks;

    public ObservableCollection<string> AvailablePorts { get; }
    public ObservableCollection<string> LogMessages { get; }

    public MainWindowViewModel()
    {
        AvailablePorts = new ObservableCollection<string>(SerialPortService.GetAvailablePorts());
        LogMessages = new ObservableCollection<string>();
        _wifiNetworks = new ObservableCollection<WifiNetwork>();
        
        SelectedPort = AvailablePorts.FirstOrDefault();
        
        AddLogMessage("Application started. Select a port to connect.");
    }
    
    partial void OnIsTableViewChanged(bool value)
    {
        if (value)
        {
            IsGridView = false;
        }
    }
    
    partial void OnIsGridViewChanged(bool value)
    {
        if (value)
        {
            IsTableView = false;
        }
    }

    [RelayCommand]
    private void RefreshPorts()
    {
        AvailablePorts.Clear();
        foreach (var port in SerialPortService.GetAvailablePorts())
        {
            AvailablePorts.Add(port);
        }
        AddLogMessage($"Found {AvailablePorts.Count} port(s)");
    }

    [RelayCommand]
    private async Task Connect()
    {
        if (string.IsNullOrEmpty(SelectedPort))
        {
            AddLogMessage("ERROR: No port selected");
            return;
        }

        try
        {
            AddLogMessage($"Connecting to {SelectedPort}...");
            AddLogMessage("Creating UCX client instance...");
            
            _ucxClient = new UcxClient(SelectedPort, 115200);
            _ucxClient.UrcReceived += OnUrcReceived;
            _ucxClient.LogReceived += OnLogReceived;
            
            AddLogMessage("Checking connection status...");
            IsConnected = _ucxClient.IsConnected;
            StatusMessage = IsConnected ? $"Connected to {SelectedPort}" : "Failed to connect";
            
            AddLogMessage(StatusMessage);

            if (IsConnected)
            {
                AddLogMessage("Sending test AT command...");
                // Send test command
                await SendCommand("AT");
            }
        }
        catch (Exception ex)
        {
            AddLogMessage($"ERROR: {ex.Message}");
            StatusMessage = "Connection failed";
            IsConnected = false;
        }
    }

    [RelayCommand]
    private void Disconnect()
    {
        if (_ucxClient != null)
        {
            _ucxClient.UrcReceived -= OnUrcReceived;
            _ucxClient.LogReceived -= OnLogReceived;
            _ucxClient.Dispose();
            _ucxClient = null;
        }

        IsConnected = false;
        StatusMessage = "Disconnected";
        AddLogMessage("Disconnected");
    }

    [RelayCommand]
    private async Task SendAtCommand()
    {
        if (_ucxClient == null || !IsConnected)
        {
            AddLogMessage("ERROR: Not connected");
            return;
        }

        if (string.IsNullOrWhiteSpace(AtCommand))
        {
            return;
        }

        await SendCommand(AtCommand);
        AtCommand = ""; // Clear input
    }

    private async System.Threading.Tasks.Task SendCommand(string command)
    {
        if (_ucxClient == null) return;

        try
        {
            AddLogMessage($"> {command}");
            string response = await _ucxClient.SendAtCommandAsync(command);
            
            if (!string.IsNullOrEmpty(response))
            {
                AddLogMessage($"< {response}");
            }
            else
            {
                AddLogMessage("< OK");
            }
        }
        catch (Exception ex)
        {
            AddLogMessage($"ERROR: {ex.Message}");
        }
    }

    private void OnUrcReceived(object? sender, UrcEventArgs e)
    {
        Dispatcher.UIThread.Post(() => AddLogMessage($"[URC] {e.UrcLine}"));
    }

    private void OnLogReceived(object? sender, LogEventArgs e)
    {
        // Log AT RX/TX messages - strip ANSI color codes if present
        string message = e.Message.Replace("\u001b[0;36m", "")
                                   .Replace("\u001b[0;35m", "")
                                   .Replace("\u001b[0m", "")
                                   .TrimEnd('\r', '\n');
        
        if (!string.IsNullOrWhiteSpace(message))
        {
            Dispatcher.UIThread.Post(() => AddLogMessage(message));
        }
    }

    [RelayCommand]
    private async Task WifiScan()
    {
        if (_ucxClient == null || !IsConnected)
        {
            WifiScanStatus = "ERROR: Not connected";
            AddLogMessage("ERROR: Not connected");
            return;
        }

        try
        {
            IsScanning = true;
            WifiScanStatus = "Scanning...";
            WifiScanOutput = "Scanning for WiFi networks...\n";
            WifiNetworks.Clear();
            AddLogMessage("Starting WiFi scan...");
            
            var networks = await _ucxClient.ScanWifiAsync();
            
            WifiScanStatus = $"Scan complete - found {networks.Count} network(s)";
            AddLogMessage($"Found {networks.Count} network(s)");
            
            if (networks.Count == 0)
            {
                WifiScanOutput = "No networks found.";
                WifiScanStatus = "No networks found";
                return;
            }
            
            // Build formatted text output
            var output = new System.Text.StringBuilder();
            output.AppendLine($"Found {networks.Count} network(s):\n");
            output.AppendLine("SSID                              BSSID              CH  RSSI  Security");
            output.AppendLine("================================  =================  ==  ====  ========");
            
            // Convert all results to WifiNetwork objects and format
            var networkList = new List<WifiNetwork>();
            foreach (var result in networks.OrderByDescending(n => n.rssi))
            {
                var network = WifiNetwork.FromScanResult(result);
                networkList.Add(network);
                
                // Format: SSID (32 chars), BSSID (17 chars), Channel (2), RSSI (4), Security
                output.AppendLine($"{network.Ssid,-32}  {network.BssidString,-17}  {network.Channel,2}  {network.Rssi,4}  {network.SecurityString}");
                
                AddLogMessage($"  {network.Ssid} - Channel {network.Channel}, RSSI {network.Rssi} dBm, {network.SecurityString}");
            }
            
            WifiScanOutput = output.ToString();
            
            // Also keep the collection for when we switch back to DataGrid
            WifiNetworks = new ObservableCollection<WifiNetwork>(networkList);
            
            AddLogMessage($"Scan complete - found {networks.Count} network(s)");
        }
        catch (Exception ex)
        {
            WifiScanStatus = $"Scan failed: {ex.Message}";
            AddLogMessage($"ERROR: WiFi scan failed - {ex.Message}");
        }
        finally
        {
            IsScanning = false;
        }
    }

    [RelayCommand]
    private async Task WifiConnect()
    {
        if (_ucxClient == null || !IsConnected)
        {
            WifiConnectStatus = "ERROR: Not connected";
            AddLogMessage("ERROR: Not connected");
            return;
        }

        if (string.IsNullOrWhiteSpace(SelectedSsid))
        {
            WifiConnectStatus = "ERROR: No SSID entered";
            AddLogMessage("ERROR: No SSID entered");
            return;
        }

        try
        {
            IsConnecting = true;
            WifiConnectStatus = $"Connecting to {SelectedSsid}...";
            AddLogMessage($"Connecting to WiFi: {SelectedSsid}");
            
            await _ucxClient.ConnectWifiAsync(SelectedSsid, string.IsNullOrWhiteSpace(WifiPassword) ? null : WifiPassword);
            
            WifiConnectStatus = $"Connected to {SelectedSsid}";
            AddLogMessage($"Successfully connected to {SelectedSsid}");
            
            // Clear password for security
            WifiPassword = "";
        }
        catch (Exception ex)
        {
            WifiConnectStatus = $"Connection failed: {ex.Message}";
            AddLogMessage($"ERROR: WiFi connection failed - {ex.Message}");
        }
        finally
        {
            IsConnecting = false;
        }
    }

    [RelayCommand]
    private async Task WifiDisconnect()
    {
        if (_ucxClient == null || !IsConnected)
        {
            WifiConnectStatus = "ERROR: Not connected to module";
            AddLogMessage("ERROR: Not connected to module");
            return;
        }

        try
        {
            WifiConnectStatus = "Disconnecting...";
            AddLogMessage("Disconnecting from WiFi");
            
            await _ucxClient.DisconnectWifiAsync();
            
            WifiConnectStatus = "Disconnected from WiFi";
            AddLogMessage("Successfully disconnected from WiFi");
        }
        catch (Exception ex)
        {
            WifiConnectStatus = $"Disconnect failed: {ex.Message}";
            AddLogMessage($"ERROR: WiFi disconnect failed - {ex.Message}");
        }
    }

    private void AddLogMessage(string message)
    {
        var timestamp = DateTime.Now.ToString("HH:mm:ss.fff");
        LogMessages.Add($"[{timestamp}] {message}");
        
        // Keep only last 1000 messages
        while (LogMessages.Count > 1000)
        {
            LogMessages.RemoveAt(0);
        }
    }
}
