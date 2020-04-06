#!/usr/bin/python

import os

def main():
    all_files = os.listdir("content")
    i = 1
    while i < 250:
         map_number = "%03d" % i
         new_map = "%s.bm" % map_number
         old_map = ""
         for f in all_files:
              if map_number in f and new_map != f and ".bm" in f:
                   old_map = f
         if old_map:
              full_path_new_map = "content/%s" % new_map
              full_path_old_map = "content/%s" % old_map
              if os.path.exists(full_path_new_map) and os.path.exists(full_path_old_map):
                   cmd = "mv -f %s %s" % (full_path_new_map, full_path_old_map)
                   print(cmd)
                   # os.system(cmd)
         i = i + 1
    pass

main()
