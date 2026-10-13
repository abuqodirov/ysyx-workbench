from PIL import Image

img = Image.open("picture.png").convert("RGB").resize((640, 480))
depth = 640 * 480

with open("picture.mif", "w") as f:
    f.write(f"DEPTH = {depth};\n")
    f.write("WIDTH = 24;\n")
    f.write("ADDRESS_RADIX = DEC;\n")
    f.write("DATA_RADIX = HEX;\n\n")
    f.write("CONTENT BEGIN\n")
    
    addr = 0
    for y in range(480):
        for x in range(640):
            r, g, b = img.getpixel((x, y))
            f.write(f"  {addr:<6} : {r:02X}{g:02X}{b:02X};\n")
            addr += 1
            
    f.write("END;\n")

print("Created 640x480 picture.mif successfully!")
