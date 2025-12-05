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
    
    private TaskCompletionSource<bool>? _networkUpEvent;
    
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
        // Check for network up URC (+UEWSNU - WiFi Station Network Up)
        if (e.UrcLine.Contains("+UEWSNU"))
        {
            System.Console.WriteLine("[ViewModel] Received network up URC: " + e.UrcLine);
            _networkUpEvent?.TrySetResult(true);
        }
        
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
        System.Console.WriteLine("[ViewModel] WifiConnect COMMAND STARTED");
        System.Diagnostics.Debug.WriteLine("[ViewModel] WifiConnect COMMAND STARTED");
        
        if (_ucxClient == null || !IsConnected)
        {
            WifiConnectStatus = "ERROR: Not connected";
            AddLogMessage("ERROR: Not connected");
            System.Console.WriteLine("[ViewModel] ERROR: Not connected to module");
            return;
        }

        if (string.IsNullOrWhiteSpace(SelectedSsid))
        {
            WifiConnectStatus = "ERROR: No SSID entered";
            AddLogMessage("ERROR: No SSID entered");
            System.Console.WriteLine("[ViewModel] ERROR: No SSID entered");
            return;
        }

        try
        {
            System.Console.WriteLine($"[ViewModel] Setting IsConnecting = true");
            IsConnecting = true;
            WifiConnectStatus = $"Connecting to {SelectedSsid}...";
            AddLogMessage($"Connecting to WiFi: {SelectedSsid}");
            
            // Create event to wait for network up URC
            _networkUpEvent = new TaskCompletionSource<bool>();
            
            System.Console.WriteLine($"[ViewModel] About to call ConnectWifiAsync for '{SelectedSsid}'");
            await _ucxClient.ConnectWifiAsync(SelectedSsid, string.IsNullOrWhiteSpace(WifiPassword) ? null : WifiPassword);
            
            System.Console.WriteLine($"[ViewModel] ConnectWifiAsync returned successfully (connection initiated)");
            WifiConnectStatus = $"Waiting for network up...";
            
            // Wait for network up URC (+UEWSNU) with timeout
            System.Console.WriteLine($"[ViewModel] Waiting for network up URC (+UEWSNU)...");
            using var cts = new System.Threading.CancellationTokenSource(TimeSpan.FromSeconds(20));
            var networkUpTask = _networkUpEvent.Task;
            var timeoutTask = Task.Delay(-1, cts.Token);
            
            var completedTask = await Task.WhenAny(networkUpTask, timeoutTask);
            
            if (completedTask == networkUpTask && await networkUpTask)
            {
                System.Console.WriteLine($"[ViewModel] Network up event received!");
            }
            else
            {
                System.Console.WriteLine($"[ViewModel] Timeout waiting for network up URC");
                WifiConnectStatus = "Connected (link up) but network timeout";
                AddLogMessage("WiFi link established but network layer timed out (no IP address)");
                return;
            }
            
            // Get connection info
            System.Console.WriteLine($"[ViewModel] Getting connection info...");
            var connInfo = await _ucxClient.GetWifiConnectionInfoAsync();
            
            WifiConnectStatus = $"Connected to {SelectedSsid}";
            
            // Build detailed connection info message
            var infoMsg = new System.Text.StringBuilder();
            infoMsg.AppendLine($"Successfully connected to {SelectedSsid}");
            infoMsg.AppendLine($"");
            infoMsg.AppendLine($"  IP Address:    {connInfo.ip_address}");
            infoMsg.AppendLine($"  Subnet Mask:   {connInfo.subnet_mask}");
            infoMsg.AppendLine($"  Gateway:       {connInfo.gateway}");
            infoMsg.AppendLine($"  Channel:       {connInfo.channel}");
            infoMsg.AppendLine($"  Signal (RSSI): {connInfo.rssi} dBm");
            
            string connectionDetails = infoMsg.ToString();
            AddLogMessage(connectionDetails);
            
            // Also display in the scan output area
            WifiScanOutput = connectionDetails;
            
            // Clear password for security
            WifiPassword = "";
            System.Console.WriteLine($"[ViewModel] WifiConnect COMPLETED SUCCESSFULLY");
        }
        catch (Exception ex)
        {
            System.Console.WriteLine($"[ViewModel] WifiConnect EXCEPTION: {ex.GetType().Name}: {ex.Message}");
            System.Console.WriteLine($"[ViewModel] Stack trace: {ex.StackTrace}");
            System.Diagnostics.Debug.WriteLine($"[ViewModel] WifiConnect EXCEPTION: {ex}");
            WifiConnectStatus = $"Connection failed: {ex.Message}";
            AddLogMessage($"ERROR: WiFi connection failed - {ex.Message}");
        }
        finally
        {
            System.Console.WriteLine($"[ViewModel] Setting IsConnecting = false");
            IsConnecting = false;
            System.Console.WriteLine($"[ViewModel] WifiConnect COMMAND ENDED");
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
