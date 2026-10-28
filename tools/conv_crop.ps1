Add-Type -AssemblyName System.Drawing
$fullPath = Resolve-Path "port/build/Release/chris_new_crop.bmp"
$img = [System.Drawing.Bitmap]::FromFile($fullPath)
$pngPath = [System.IO.Path]::ChangeExtension($fullPath, ".png")
$img.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
$img.Dispose()
Write-Host "Saved: $pngPath"
