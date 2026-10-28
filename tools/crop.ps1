Add-Type -AssemblyName System.Drawing
$fullPath = Resolve-Path "tools/capture.bmp"
$img = [System.Drawing.Bitmap]::FromFile($fullPath)
$pngPath = [System.IO.Path]::ChangeExtension($fullPath, ".png")
$img.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)

$cropRect = New-Object System.Drawing.Rectangle(400, 250, 480, 550)
$cropBmp = $img.Clone($cropRect, $img.PixelFormat)
$cropBmp.Save("tools/chris_depth_crop.png", [System.Drawing.Imaging.ImageFormat]::Png)

$cropBmp.Dispose()
$img.Dispose()
Write-Host "Success: tools/chris_depth_crop.png"
