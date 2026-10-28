entry = """- **2026-10-25 [Antigravity]**: Resolved See-Through Chris, Color-Key Discard Holes, Depth Testing & Gun Attachment:
  1. Root Cause 1 (Missing Hardware Depth Buffer): In ps1_pipeline.cpp and dx12_renderer.cpp, DepthEnable was FALSE with no depth-stencil texture bound. Back-facing polygons and rear limbs were drawn on top of front-facing polygons due to painters algorithm failure, making Chris appear inside-out and translucent. Implemented authentic DXGI_FORMAT_D32_FLOAT depth buffer and DSV descriptor heap in dx12_renderer.cpp and configured D3D12_COMPARISON_FUNC_LESS_EQUAL depth testing in ps1_pipeline.cpp.
  2. Root Cause 2 (Color-Key Alpha Discard Holes): TimDecoder::C15ToRgba in psx_tim.cpp was returning alpha = 0 whenever 15-bit color was 0x0000 (pure black). In ps1_pipeline.hlsl, discard dropped every black pixel, punching transparent holes through Chris's hair, eyes, boots, vest, and belt where the background wall showed through. Updated C15ToRgba to assign full opacity (a = 0xFF) for 3D character textures.
  3. Root Cause 3 (Handgun Idle Damage Recoil Loop): In player_character.cpp, armed idle was set to frames 0..34. Inspection of W02.EMW proved frames 0..18 are Damage/Recoil reactions, while frames 19..34 are the authentic closed 15-frame breathing loop. Remapped armed idle to frames 19..34.
  4. Root Cause 4 (Holster & Gun Attachment): Object 15 (knife sheath/holster) lacked a bone parent and drew un-transformed at world origin (0, 0, 0) between his boots. Parented Object 15 to Joint 0 (Pelvis). In addition, the Beretta handgun mesh was parented to Joint 8 (which base offsets proved was Chris's left foot) instead of Joint 11 (Chris's right hand). Reparented handgun to Joint 11.
  5. Verified live rendering: Captured frame confirms Chris Redfield renders 100% solid, fully opaque, with authentic Gouraud normals, holstered sheath on his hip, Beretta in right hand, and zero transparency artifacts.
"""
with open('AGENTS.md', 'r', encoding='utf-8') as f:
    text = f.read()

text = text.rstrip() + '\n' + entry

with open('AGENTS.md', 'w', encoding='utf-8') as f:
    f.write(text)
print('AGENTS.md updated!')
