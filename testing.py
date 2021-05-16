import requests

def tests():
	r1 = requests.post("http://localhost:9080/settings/latime/2")
	if r1.text != "latime was set to 2":
		return "Error 1"
	r2 = requests.post("http://localhost:9080/settings/lungime/2")
	if r2.text != "lungime was set to 2":
		return "Error 2"
	r3 = requests.post("http://localhost:9080/settings/matrice/1234")
	if r3.text != "matrice was set to 1234":
		return "Error 3"
	r4 = requests.get("http://localhost:9080/settings/matrice")
	if r4.text != "matrice is 1234":
		return "Error 4"
	r5 = requests.post("http://localhost:9080/init/100")
	if r5.text != "Battery was set to 100":
		return "Error 5"
	r6 = requests.get("http://localhost:9080/baterie")
	if r6.text != "Battery has level: 100%":
		return "Error 6"
	r7 = requests.post("http://localhost:9080/goToCharge")
	if r7.text != "Battery was set to: 100%":
		return "Error 7"


if __name__ == "__main__":
	print(tests())