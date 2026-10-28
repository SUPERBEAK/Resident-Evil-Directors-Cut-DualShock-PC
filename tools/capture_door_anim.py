import subprocess, time, ctypes
from ctypes import wintypes
import struct

p = subprocess.Popen([r'H:\Antigravity Projects\ultimate dualshock\port\build\Release\re1_dualshock_port.exe', '--test-door'],
                     cwd=r'H:\Antigravity Projects\ultimate dualshock\port\build\Release')
time.sleep(1.2)

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32

try:
    hwnd = user32.FindWindowW('ResidentEvilDualShockPortWndClass', None)
    if hwnd:
        rect = wintypes.RECT()
        user32.GetClientRect(hwnd, ctypes.byref(rect))
        w = rect.right - rect.left
        h = rect.bottom - rect.top

        hdc = user32.GetDC(hwnd)
        memdc = gdi32.CreateCompatibleDC(hdc)
        hbmp = gdi32.CreateCompatibleBitmap(hdc, w, h)
        gdi32.SelectObject(memdc, hbmp)

        user32.PrintWindow(hwnd, memdc, 2)

        class BITMAPINFOHEADER(ctypes.Structure):
            _fields_ = [
                ('biSize', wintypes.DWORD),
                ('biWidth', wintypes.LONG),
                ('biHeight', wintypes.LONG),
                ('biPlanes', wintypes.WORD),
                ('biBitCount', wintypes.WORD),
                ('biCompression', wintypes.DWORD),
                ('biSizeImage', wintypes.DWORD),
                ('biXPelsPerMeter', wintypes.LONG),
                ('biYPelsPerMeter', wintypes.LONG),
                ('biClrUsed', wintypes.DWORD),
                ('biClrImportant', wintypes.DWORD),
            ]

        bmi = BITMAPINFOHEADER()
        bmi.biSize = ctypes.sizeof(BITMAPINFOHEADER)
        bmi.biWidth = w
        bmi.biHeight = -h
        bmi.biPlanes = 1
        bmi.biBitCount = 32
        bmi.biCompression = 0

        buf = ctypes.create_string_buffer(w * h * 4)
        gdi32.GetDIBits(memdc, hbmp, 0, h, buf, ctypes.byref(bmi), 0)

        file_hdr = struct.pack('<2sIHHI', b'BM', 14 + 40 + w * h * 4, 0, 0, 14 + 40)
        with open(r'H:\Antigravity Projects\ultimate dualshock\tools\capture_door.bmp', 'wb') as f:
            f.write(file_hdr)
            f.write(bytearray(bmi))
            f.write(buf)
finally:
    p.terminate()
    try:
        p.wait(timeout=2.0)
    except:
        p.kill()
