import glob, os, sys

read_files = glob.glob(r'rtt_core_*.txt')

name = "rtt_" + str(sys.argv[1]) + ".txt"
with open(name, "wb") as outfile:
    for f in read_files:
        with open(f, "rb") as infile:
            outfile.write(infile.read())
    for f in read_files:
        os.remove(f)