Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms

$code = @"
using System;
using System.Runtime.InteropServices;

public class Win32 {
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")]
    public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }
}
"@
Add-Type -TypeDefinition $code

$exePath = Resolve-Path "port/build/Release/re1_dualshock_port.exe"
$workingDir = Split-Path -Parent $exePath

$proc = Start-Process -FilePath $exePath -WorkingDirectory $workingDir -PassThru
Write-Host "Started game PID $($proc.Id)"
Start-Sleep -Seconds 3

try {
    $hwnd = [Win32]::FindWindow($null, $null)
    # Search for Resident Evil in title
    $targetHwnd = [IntPtr]::Zero
    $procs = Get-Process -Name "re1_dualshock_port" -ErrorAction SilentlyContinue
    if ($procs) {
        $targetHwnd = $procs[0].MainWindowHandle
    }

    if ($targetHwnd -ne [IntPtr]::Zero) {
        Write-Host "Found Game Window: $targetHwnd"
        [Win32]::ShowWindow($targetHwnd, 9) # SW_RESTORE
        [Win32]::SetForegroundWindow($targetHwnd)
        Start-Sleep -Milliseconds 500

        $rect = New-Object Win32+RECT
        [Win32]::GetWindowRect($targetHwnd, [ref]$rect)

        $w = $rect.Right - $rect.Left
        $h = $rect.Bottom - $rect.Top
        Write-Host "Window Rect: $($rect.Left), $($rect.Top), $w x $h"

        $bmp = New-Object System.Drawing.Bitmap($w, $h)
        $gfx = [System.Drawing.Graphics]::FromImage($bmp)
        $point = New-Object System.Drawing.Point($rect.Left, $rect.Top)
        $gfx.CopyFromScreen($point, [System.Drawing.Point]::Empty, (New-Object System.Drawing.Size($w, $h)))
        $gfx.Dispose()

        $bmp.Save("port/build/Release/chris_depth_screen.png", [System.Drawing.Imaging.ImageFormat]::Png)
        Write-Host "Saved port/build/Release/chris_depth_screen.png"

        # Crop Chris in center area
        $cropRect = New-Object System.Drawing.Rectangle(420, 250, 440, 520)
        $cropBmp = $bmp.Clone($cropRect, $bmp.PixelFormat)
        $cropBmp.Save("port/build/Release/chris_depth_cropped.png", [System.Drawing.Imaging.ImageFormat]::Png)
        $cropBmp.Dispose()
        $bmp.Dispose()
        Write-Host "Saved port/build/Release/chris_depth_cropped.png"
    } else {
        Write-Host "Game window not found!"
    }
} finally {
    if (!$proc.HasExited) {
        $proc.Kill()
        Write-Host "Game terminated."
    }
}
