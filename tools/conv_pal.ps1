Add-Type -AssemblyName System.Drawing
= [System.Drawing.Image]::FromFile('H:\Antigravity Projects\ultimate dualshock\port\build\Release\pal_compare.bmp')
.Save('H:\Antigravity Projects\ultimate dualshock\port\build\Release\pal_compare.png', [System.Drawing.Imaging.ImageFormat]::Png)
.Dispose()
Write-Host 'Done'
