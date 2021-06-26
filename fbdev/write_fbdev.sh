#!/usr/bin/env bash

fbdev=/dev/fb0 ; width=1280 ; height=720; bpp=2 ;
color="\x00\xF8" #red in rgb565
#color="\xE0\x07" #green in rgb565
#color="\x1F\x00" #blue in rgb565

function pixel() {
   xx=$1 ; yy=$2
   printf "$color" | dd bs=$bpp seek=$(($yy * $width + $xx)) > $fbdev 2>/dev/null
}
x=0 ; y=0
#画线：
for i in $(seq 1 $height); do
   pixel $((x++)) $((y++))
done
#画整个屏幕：
#for i in $(seq 1 $width); do
#    for j in $(seq 1 $height); do
#        pixel $i $j
#    done
#done

# 附，color计算：
#function rgb888ToRgb565(r, g, b) {
#  return ((r & 0b11111000) << 8) | ((g & 0b11111100) << 3) | (b >> 3);
#}
#function paddingLeft (str, length, padding = ' ') {
#  if (str.length >= length) return str;
#  return Array(length - str.length).fill(padding).join('') + str;
#}
#function intToLittleEndHexString(v) {
#  let res = '';
#  while (v > 0) {
#    res += paddingLeft((v & 0xFF).toString(16).toUpperCase(), 2, '0');
#    v = v >> 8;
#  }
#  return res;
#}
