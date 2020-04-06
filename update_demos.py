#!/usr/bin/python

import os

def main():
    all_files = os.listdir(".")
    i = 1
    while i < 250:
         map_number = "%03d" % i
         old_demo = "content/%s.bd" % (map_number)
         new_demo = "content/%s_new.bd" % (map_number)
         cmd = "./game -map %d -play %s -record %s -winw 1500 -winh 1500 -speed 5" % (i, old_demo, new_demo)
         print(cmd)
         # os.system(cmd)
         cmd = "mv -f %s %s" % (new_demo, old_demo)
         print(cmd)
         # os.system(cmd)
         i = i + 1
    pass

main()
