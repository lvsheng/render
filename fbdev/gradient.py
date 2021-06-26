xsize = 1280
ysize = 720

#with open('gradient.data.out', 'wb') as f:
with open('/dev/fb0', 'wb') as f:
  for y in range(0,ysize):
    for x in range(0,xsize):
      #r = 255
      #g = 0
      #b = 0
      # gradient:
      r = int(min(x / (xsize/256),255))
      g = int(min(y / (ysize/256),255))
      b = 0

      # for rgb888:
      #f.write((b).to_bytes(1, byteorder='little'))
      #f.write((g).to_bytes(1, byteorder='little'))
      #f.write((r).to_bytes(1, byteorder='little'))
      #f.write((0).to_bytes(1, byteorder='little'))

      # for rgb565:
      rgb = ((r & 0b11111000) << 8) | ((g & 0b11111100) << 3) | (b >> 3)
      f.write((rgb).to_bytes(2, byteorder='little'))
