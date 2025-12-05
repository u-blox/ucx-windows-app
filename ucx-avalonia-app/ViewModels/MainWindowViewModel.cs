using System;
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
            WifiNetworks.Clear();
            AddLogMessage("Starting WiFi scan...");
            System.Diagnostics.Debug.WriteLine("[ViewModel] Calling ScanWifiAsync...");
            
            var networks = await _ucxClient.ScanWifiAsync();
            
            System.Diagnostics.Debug.WriteLine($"[ViewModel] Scan returned {networks.Count} networks");
            WifiScanStatus = $"Scan complete - found {networks.Count} network(s)";
            AddLogMessage($"Found {networks.Count} network(s)");
            
            int addedCount = 0;
            foreach (var result in networks.OrderByDescending(n => n.rssi))
            {
                System.Diagnostics.Debug.WriteLine($"[ViewModel] Processing result: ssid='{result.ssid}', rssi={result.rssi}");
                System.Console.WriteLine($"[ViewModel] Processing result: ssid='{result.ssid}', rssi={result.rssi}");
                
                var network = WifiNetwork.FromScanResult(result);
                System.Diagnostics.Debug.WriteLine($"[ViewModel] Converted to WifiNetwork: Ssid='{network.Ssid}', Rssi={network.Rssi}");
                System.Console.WriteLine($"[ViewModel] Converted to WifiNetwork: Ssid='{network.Ssid}', Rssi={network.Rssi}");
                
                AddLogMessage($"  Adding: {network.Ssid} - Channel {network.Channel}, RSSI {network.Rssi} dBm, {network.SecurityString}");
                
                // Add directly on current thread since we're already in async context
                WifiNetworks.Add(network);
                addedCount++;
                System.Diagnostics.Debug.WriteLine($"[ViewModel] Added to collection. Count now: {WifiNetworks.Count}");
                System.Console.WriteLine($"[ViewModel] Added to collection. Count now: {WifiNetworks.Count}");
            }
            
            System.Diagnostics.Debug.WriteLine($"[ViewModel] Finished adding {addedCount} networks. Collection count: {WifiNetworks.Count}");
            AddLogMessage($"Total networks in collection: {WifiNetworks.Count}");
            
            if (networks.Count == 0)
            {
                WifiScanStatus = "No networks found";
            }
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
