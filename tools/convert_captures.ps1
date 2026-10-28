Add-Type -AssemblyName System.Drawing
$img = [System.Drawing.Image]::FromFile("tools/capture_staircase_verified.bmp")
$img.Save("tools/capture_staircase_verified.png", [System.Drawing.Imaging.ImageFormat]::Png)
$img.Dispose()
Write-Host "Converted tools/capture_staircase_verified.png"
