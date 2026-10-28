import subprocess
import time
import os
import ctypes

exe_path = os.path.abspath('port/build/Release/re1_dualshock_port.exe')
cwd = os.path.dirname(exe_path)

# Delete existing screenshots
for f in os.listdir(cwd):
    if f.startswith('re1_dualshock_port') and f.endswith('.png'):
        try:
            os.remove(os.path.join(cwd, f))
        except:
            pass

print("Launching game with CAPCOM.STR...")
proc = subprocess.Popen([exe_path, "--play-movie", "0"], cwd=cwd)
time.sleep(3.5)

user32 = ctypes.windll.user32
VK_F5 = 0x74
KEYEVENTF_KEYUP = 0x0002

print("Triggering F5 ReShade screenshot...")
user32.keybd_event(VK_F5, 0, 0, 0)
time.sleep(0.1)
user32.keybd_event(VK_F5, 0, KEYEVENTF_KEYUP, 0)
time.sleep(1.0)

proc.terminate()

new_pngs = sorted([os.path.join(cwd, f) for f in os.listdir(cwd) if f.startswith('re1_dualshock_port') and f.endswith('.png')])
print(f"Generated {len(new_pngs)} screenshots:")
for p in new_pngs:
    print(p, os.path.getsize(p))
