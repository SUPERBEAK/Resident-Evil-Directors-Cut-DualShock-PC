Add-Type -AssemblyName System.Drawing
= [System.Drawing.Image]::FromFile('H:\Antigravity Projects\ultimate dualshock\port\build\Release\status_bg_test.bmp')
.Save('H:\Antigravity Projects\ultimate dualshock\port\build\Release\status_bg_test.png', [System.Drawing.Imaging.ImageFormat]::Png)
.Dispose()
Write-Host 'Done'
