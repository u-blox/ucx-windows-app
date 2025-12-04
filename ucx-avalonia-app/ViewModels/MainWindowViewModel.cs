using System;
using System.Collections.ObjectModel;
using System.Linq;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using UcxAvaloniaApp.Services;

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

    public ObservableCollection<string> AvailablePorts { get; }
    public ObservableCollection<string> LogMessages { get; }

    public MainWindowViewModel()
    {
        AvailablePorts = new ObservableCollection<string>(SerialPortService.GetAvailablePorts());
        LogMessages = new ObservableCollection<string>();
        
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
    private async void Connect()
    {
        if (string.IsNullOrEmpty(SelectedPort))
        {
            AddLogMessage("ERROR: No port selected");
            return;
        }

        try
        {
            AddLogMessage($"Connecting to {SelectedPort}...");
            _ucxClient = new UcxClient(SelectedPort, 115200);
            _ucxClient.UrcReceived += OnUrcReceived;
            
            IsConnected = _ucxClient.IsConnected;
            StatusMessage = IsConnected ? $"Connected to {SelectedPort}" : "Failed to connect";
            
            AddLogMessage(StatusMessage);

            if (IsConnected)
            {
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
            _ucxClient.Dispose();
            _ucxClient = null;
        }

        IsConnected = false;
        StatusMessage = "Disconnected";
        AddLogMessage("Disconnected");
    }

    [RelayCommand]
    private async void SendAtCommand()
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
        AddLogMessage($"[URC] {e.UrcLine}");
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
