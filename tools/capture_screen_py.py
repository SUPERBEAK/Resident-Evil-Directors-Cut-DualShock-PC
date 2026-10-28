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
time.sleep(3.5)

try:
    user32 = ctypes.windll.user32
    gdi32 = ctypes.windll.gdi32

    found_hwnd = None
    found_title = ""
    def enum_cb(hwnd, lparam):
        global found_hwnd, found_title
        if user32.IsWindowVisible(hwnd):
            length = user32.GetWindowTextLengthW(hwnd)
            if length > 0:
                buff = ctypes.create_unicode_buffer(length + 1)
                user32.GetWindowTextW(hwnd, buff, length + 1)
                if "Resident Evil" in buff.value:
                    found_hwnd = hwnd
                    found_title = buff.value
                    return False
        return True

    WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, wintypes.HWND, wintypes.LPARAM)
    user32.EnumWindows(WNDENUMPROC(enum_cb), 0)

    if not found_hwnd:
        print("Game window not found!")
    else:
        print(f"Found game window HWND: {found_hwnd} ('{found_title}')")
        user32.ShowWindow(found_hwnd, 9) # SW_RESTORE
        user32.SetForegroundWindow(found_hwnd)
        time.sleep(0.5)

        rect = wintypes.RECT()
        user32.GetWindowRect(found_hwnd, ctypes.byref(rect))
        x = rect.left
        y = rect.top
        w = rect.right - rect.left
        h = rect.bottom - rect.top
        print(f"Window rect: ({x}, {y}, {w}x{h})")

        # Capture from Desktop DC (HWND 0)
        hdc_screen = user32.GetDC(0)
        hdc_dest = gdi32.CreateCompatibleDC(hdc_screen)
        hbm = gdi32.CreateCompatibleBitmap(hdc_screen, w, h)
        gdi32.SelectObject(hdc_dest, hbm)

        gdi32.BitBlt(hdc_dest, 0, 0, w, h, hdc_screen, x, y, 0x00CC0020) # SRCCOPY

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

        file_size = 14 + 40 + w * h * 4
        bmp_hdr = struct.pack('<2sIHHI', b'BM', file_size, 0, 0, 54)
        info_hdr = struct.pack('<IIIHHIIIIII', 40, w, h, 1, 32, 0, w * h * 4, 0, 0, 0, 0)

        with open('tools/screen_capture.bmp', 'wb') as f:
            f.write(bmp_hdr)
            f.write(info_hdr)
            f.write(buf)

        print("Saved tools/screen_capture.bmp successfully!")

        gdi32.DeleteObject(hbm)
        gdi32.DeleteDC(hdc_dest)
        user32.ReleaseDC(0, hdc_screen)

finally:
    proc.terminate()
    try:
        proc.wait(timeout=2.0)
    except subprocess.TimeoutExpired:
        proc.kill()
    print("Process finished.")
