import os
import re

regex_includes = re.compile('#include "[^"]*"')

src_header_file = open("sk_gpu_dev.h", "r")
src_header      = src_header_file.read()
src_header_file.close()

dest_header = src_header

for match in re.findall(regex_includes, dest_header):
    include_filename = match.split('"')[1]
    include_file     = open(include_filename, "r")
    include_text     = include_file.read()
    include_file.close()
    
    body_filename    = include_filename.replace(".h", ".cpp")
    body_file        = open(body_filename, "r")
    body_text        = body_file.read().replace('#include "sk_gpu_dev.h"', '')
    body_file.close()

    dest_header = dest_header.replace(match, include_text)
    dest_header += body_text

folder_path = os.path.dirname(os.path.realpath(__file__))
folder_path = os.path.abspath(os.path.join(folder_path, os.pardir))
final_file = open(os.path.join(folder_path, "sk_gpu.h"), "w")
final_file.write(dest_header)
final_file.close()
