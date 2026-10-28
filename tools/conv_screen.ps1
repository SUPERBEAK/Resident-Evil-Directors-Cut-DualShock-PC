Add-Type -AssemblyName System.Drawing
$fullPath = Resolve-Path "tools/screen_capture.bmp"
$img = [System.Drawing.Bitmap]::FromFile($fullPath)
$pngPath = [System.IO.Path]::ChangeExtension($fullPath, ".png")
$img.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)

$cropRect = New-Object System.Drawing.Rectangle(450, 300, 400, 520)
$cropBmp = $img.Clone($cropRect, $img.PixelFormat)
$cropBmp.Save("tools/chris_verified_crop.png", [System.Drawing.Imaging.ImageFormat]::Png)
$cropBmp.Dispose()
$img.Dispose()
Write-Host "Done: tools/chris_verified_crop.png"
