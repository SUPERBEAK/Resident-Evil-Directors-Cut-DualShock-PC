import subprocess, time, ctypes
from ctypes import wintypes
import struct
import os

p = subprocess.Popen([r'H:\Antigravity Projects\ultimate dualshock\port\build\Release\re1_dualshock_port.exe'], cwd=r'H:\Antigravity Projects\ultimate dualshock\port\build\Release')
time.sleep(3.5)

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32

try:
    hwnd = user32.FindWindowW('ResidentEvilDualShockPortWndClass', None)
    print('HWND:', hwnd)
    if hwnd:
        rect = wintypes.RECT()
        user32.GetClientRect(hwnd, ctypes.byref(rect))
        w = rect.right - rect.left
        h = rect.bottom - rect.top
        print(f'Client size: {w}x{h}')
        
        hdc = user32.GetDC(hwnd)
        memdc = gdi32.CreateCompatibleDC(hdc)
        hbmp = gdi32.CreateCompatibleBitmap(hdc, w, h)
        gdi32.SelectObject(memdc, hbmp)
        
        # 2 = PW_RENDERFULLCONTENT
        res = user32.PrintWindow(hwnd, memdc, 2)
        print("PrintWindow result:", res)
        
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
        bmi.biHeight = -h # top-down
        bmi.biPlanes = 1
        bmi.biBitCount = 32
        bmi.biCompression = 0
        
        buf = ctypes.create_string_buffer(w * h * 4)
        gdi32.GetDIBits(memdc, hbmp, 0, h, buf, ctypes.byref(bmi), 0)
        
        file_hdr = struct.pack('<2sIHHI', b'BM', 14 + 40 + w * h * 4, 0, 0, 14 + 40)
        with open(r'H:\Antigravity Projects\ultimate dualshock\port\build\Release\capture.bmp', 'wb') as f:
            f.write(file_hdr)
            f.write(bytearray(bmi))
            f.write(buf)
        print("Saved port/build/Release/capture.bmp successfully!")
        
        # Crop Chris: x=500..760, y=380..880
        x0, x1 = 500, 760
        y0, y1 = 380, 880
        crop_w = x1 - x0
        crop_h = y1 - y0
        raw_data = bytes(buf)
        crop_bytes = bytearray()
        row_stride = w * 4
        for y in range(y0, y1):
            start = y * row_stride + x0 * 4
            crop_bytes.extend(raw_data[start : start + crop_w * 4])
            
        crop_file_hdr = struct.pack('<2sIHHI', b'BM', 14 + 40 + len(crop_bytes), 0, 0, 54)
        crop_info_hdr = struct.pack('<iiiHHIIiiII', 40, crop_w, -crop_h, 1, 32, 0, len(crop_bytes), 0, 0, 0, 0)
        with open(r'H:\Antigravity Projects\ultimate dualshock\port\build\Release\chris_new_crop.bmp', 'wb') as f:
            f.write(crop_file_hdr)
            f.write(crop_info_hdr)
            f.write(crop_bytes)
        print("Saved port/build/Release/chris_new_crop.bmp successfully!")
finally:
    p.terminate()
    try:
        p.wait(timeout=2.0)
    except:
        p.kill()
    print("Terminated process.")
