import os
import re

hasBlockComment=False

def parse(string):
    pattern1=re.compile(r'//.*')
    result = re.sub(pattern1, "", string)

    pattern2=re.compile(r'(/\*)((\*[^/])*|([^\*])*)(\*/)')
    result=re.sub(pattern2,"",result)

    return result


def main():
    fileName="./ftpcli-combine.c"
    file=open(fileName,"r")
    result=open("result.txt","w")

    content=""
    for eachline in file:
        content+=eachline
        print("Handling....")
    l=(str)(parse(content))
    result.write(l)
main()