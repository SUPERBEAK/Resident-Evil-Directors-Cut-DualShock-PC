import subprocess
import time
import os
import ctypes
from ctypes import wintypes
import struct

exe_path = os.path.abspath('port/build/Release/re1_dualshock_port.exe')
cwd = os.path.dirname(exe_path)

print("Launching game...")
proc = subprocess.Popen([exe_path], cwd=cwd)
time.sleep(3.0)

try:
    user32 = ctypes.windll.user32
    gdi32 = ctypes.windll.gdi32

    found_hwnd = None
    def enum_cb(hwnd, lparam):
        global found_hwnd
        if user32.IsWindowVisible(hwnd):
            length = user32.GetWindowTextLengthW(hwnd)
            buff = ctypes.create_unicode_buffer(length + 1)
            user32.GetWindowTextW(hwnd, buff, length + 1)
            if "Resident Evil" in buff.value:
                found_hwnd = hwnd
                return False
        return True

    WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, wintypes.HWND, wintypes.LPARAM)
    user32.EnumWindows(WNDENUMPROC(enum_cb), 0)

    if not found_hwnd:
        print("Game window not found!")
    else:
        print(f"Game window found: {found_hwnd}")
        rect = wintypes.RECT()
        user32.GetClientRect(found_hwnd, ctypes.byref(rect))
        w = rect.right - rect.left
        h = rect.bottom - rect.top
        print(f"Client size: {w}x{h}")

        hdc_src = user32.GetDC(found_hwnd)
        hdc_dest = gdi32.CreateCompatibleDC(hdc_src)
        hbm = gdi32.CreateCompatibleBitmap(hdc_src, w, h)
        gdi32.SelectObject(hdc_dest, hbm)

        # BitBlt from window client area
        gdi32.BitBlt(hdc_dest, 0, 0, w, h, hdc_src, 0, 0, 0x00CC0020) # SRCCOPY

        # Read pixels using GetDIBits
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
                ('biClrImportant', wintypes.DWORD)
            ]

        bmi = BITMAPINFOHEADER()
        bmi.biSize = ctypes.sizeof(BITMAPINFOHEADER)
        bmi.biWidth = w
        bmi.biHeight = h
        bmi.biPlanes = 1
        bmi.biBitCount = 32
        bmi.biCompression = 0 # BI_RGB

        buf = (ctypes.c_char * (w * h * 4))()
        gdi32.GetDIBits(hdc_dest, hbm, 0, h, buf, ctypes.byref(bmi), 0)

        # Write BMP
        file_size = 14 + 40 + w * h * 4
        bmp_hdr = struct.pack('<2sIHHI', b'BM', file_size, 0, 0, 54)
        info_hdr = struct.pack('<IIIHHIIIIII', 40, w, h, 1, 32, 0, w * h * 4, 0, 0, 0, 0)

        with open('tools/capture.bmp', 'wb') as f:
            f.write(bmp_hdr)
            f.write(info_hdr)
            f.write(buf)

        print("Saved tools/capture.bmp!")

        # Cleanup GDI
        gdi32.DeleteObject(hbm)
        gdi32.DeleteDC(hdc_dest)
        user32.ReleaseDC(found_hwnd, hdc_src)

finally:
    proc.terminate()
    try:
        proc.wait(timeout=2.0)
    except subprocess.TimeoutExpired:
        proc.kill()
    print("Game process exited.")
