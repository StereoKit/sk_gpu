import os
import re

src_path = os.path.dirname(os.path.abspath(__file__))

regex_includes = re.compile('#include "[^"]*"')

src_header_file = open(os.path.join(src_path, "sk_gpu_dev.h"), "r")
src_header      = src_header_file.read()
src_header_file.close()

dest_header = src_header
dest_header += "\n#ifdef SKG_IMPL\n"

for match in re.findall(regex_includes, dest_header):
    include_filename = match.split('"')[1]

    # some APIs aren't ready yet
    if include_filename == "sk_gpu_dx12.h" or include_filename == "sk_gpu_vk.h":
        dest_header = dest_header.replace(match, "")
        continue
    
    include_file     = open(os.path.join(src_path, include_filename), "r")
    include_text     = include_file.read().replace('#pragma once', '').replace('#include "sk_gpu_dev.h"', '')
    include_file.close()
    
    body_filename    = include_filename.replace(".h", ".cpp")
    body_file        = open(os.path.join(src_path, body_filename), "r")
    body_text        = body_file.read().replace('#include "sk_gpu_dev.h"', '')
    body_file.close()

    dest_header = dest_header.replace(match, include_text)
    dest_header += body_text
    
dest_header += "\n#endif // SKG_IMPL\n"
dest_header += """/*
Copyright (c) 2020-2024 Nick Klingensmith
Copyright (c) 2024 Qualcomm Technologies, Inc.
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/"""

folder_path = os.path.dirname(os.path.realpath(__file__))
folder_path = os.path.abspath(os.path.join(folder_path, os.pardir))
final_file = open(os.path.join(folder_path, "sk_gpu.h"), "w")
final_file.write(dest_header)
final_file.close()
