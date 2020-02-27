import os
import re
import sys

fileList = os.listdir("./")

for file in fileList:
    splitFile = os.path.splitext(file)
    fileName,fileType = splitFile
    result = re.match(r'record(.*)', fileName)
    if result:
        fp = open(file, "r")
        lines = fp.readlines()
        lastLine = lines[-1]
        recordFile = open("perform_"+ str(sys.argv[1]) +".txt", "a+")
        recordFile.write(lastLine)
        fp.close()
        recordFile.close()
