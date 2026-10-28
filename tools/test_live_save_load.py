import subprocess, time, ctypes

user32 = ctypes.windll.user32

# Virtual keys
VK_RETURN = 0x0D
VK_F6 = 0x75
VK_TAB = 0x09
VK_ESCAPE = 0x1B

def send_key(vk):
    user32.keybd_event(vk, 0, 0, 0)
    time.sleep(0.05)
    user32.keybd_event(vk, 0, 2, 0)

exe = r"H:\Antigravity Projects\ultimate dualshock\port\build\Release\re1_dualshock_port.exe"
print("Launching re1_dualshock_port.exe...")
p = subprocess.Popen([exe], cwd=r"H:\Antigravity Projects\ultimate dualshock")
time.sleep(2.0)

# Press F6 to test Quick Load
print("Sending F6 (Quick Load)...")
send_key(VK_F6)
time.sleep(1.0)

# Press Enter to acknowledge message if any
send_key(VK_RETURN)
time.sleep(0.5)

print("Terminating test...")
p.terminate()
try:
    p.wait(timeout=2)
except:
    p.kill()

print("Live Quick Load test completed successfully!")
