import requests

def t1():
	r1 = requests.post("localhost:9080/settings/latime/2")
	print(r1.text)
	r2 = requests.post("localhost:9080/settings/lungime/2")
	print(r2.text)
	r3 = requests.post("localhost:9080/settings/matrice")
	print(r3.text)

if __name__ == "__main__":
	t1()