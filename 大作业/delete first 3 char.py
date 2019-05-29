import time


def helloworld():
	print("helloworld")
def deleteFirst2():
  with open("ftpcli.txt","r",encoding="utf-8") as f:
      lines = f.readlines() 
      print(lines)
      
  with open("ftpcli.txt","w",encoding="utf-8") as f_w:
      for line in lines:
          line=line[4:]
          print(line)
          f_w.write(line)

deleteFirst2()
helloworld()
