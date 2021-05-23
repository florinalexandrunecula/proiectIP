#include <algorithm>
#include <string> 

#include <pistache/net.h>
#include <pistache/http.h>
#include <pistache/peer.h>
#include <pistache/http_headers.h>
#include <pistache/cookie.h>
#include <pistache/router.h>
#include <pistache/endpoint.h>
#include <pistache/common.h>

#include <signal.h>

using namespace std;
using namespace Pistache;


void printCookies(const Http::Request& req) {
    auto cookies = req.cookies();
    std::cout << "Cookies: [" << std::endl;
    const std::string indent(4, ' ');
    for (const auto& c: cookies) {
        std::cout << indent << c.name << " = " << c.value << std::endl;
    }
    std::cout << "]" << std::endl;
}


namespace Generic {

    void handleReady(const Rest::Request&, Http::ResponseWriter response) {
        response.send(Http::Code::Ok, "1");
    }

}


class LawnMowerEndpoint {
public:
	explicit LawnMowerEndpoint(Address addr)
		: httpEndpoint(std::make_shared<Http::Endpoint>(addr))
	{}

	void init(size_t thr = 2) {
        auto opts = Http::Endpoint::options()
            .threads(static_cast<int>(thr));
        httpEndpoint->init(opts);
        // Server routes are loaded up
        setupRoutes();
    }

    void start() {
        httpEndpoint->setHandler(router.handler());
        httpEndpoint->serveThreaded();
    }

    void stop(){
        httpEndpoint->shutdown();
    }

private:
	void setupRoutes() {
        using namespace Rest;
        Routes::Get(router, "/auth", Routes::bind(&LawnMowerEndpoint::doAuth, this));
        Routes::Post(router, "/settings/:settingName/:value", Routes::bind(&LawnMowerEndpoint::setSettings, this));
        // initializeaza nivelul bateriei 100%
        Routes::Post(router, "/init/:value", Routes::bind(&LawnMowerEndpoint::initBattery, this));
        Routes::Get(router, "/baterie", Routes::bind(&LawnMowerEndpoint::getBattery, this));
        Routes::Get(router, "/settings/:settingName", Routes::bind(&LawnMowerEndpoint::getSettings, this));
        Routes::Post(router, "/goToCharge", Routes::bind(&LawnMowerEndpoint::goToCharge, this));
        // stop-resume-start
        Routes::Post(router, "/:stateName", Routes::bind(&LawnMowerEndpoint::setState, this));
        Routes::Get(router, "/state", Routes::bind(&LawnMowerEndpoint::getState, this));
        Routes::Get(router, "/buildRoad", Routes::bind(&LawnMowerEndpoint::getRoad, this));

    }

    void initBattery(const Rest::Request& request, Http::ResponseWriter response){
        string baterie;
        
        if (request.hasParam(":value")) {
            auto bat = request.param(":value");
            baterie = bat.as<string>();
        }

        int setResponse = lwn.setBaterie(baterie);
        
        if (setResponse == 1) {
            response.send(Http::Code::Ok, "Battery was set to " + baterie);
        }
        else {
            response.send(Http::Code::Not_Found, baterie + "' was not a valid value ");
        }


    }

    void getBattery(const Rest::Request& request, Http::ResponseWriter response){
        Guard guard(LawnMowerLock);

        string baterie = lwn.getBaterie();

        using namespace Http;
        response.headers()
                    .add<Header::Server>("pistache/0.1")
                    .add<Header::ContentType>(MIME(Text, Plain));

        response.send(Http::Code::Ok, "Battery has level: " + baterie + "%");
    
    }

    void doAuth(const Rest::Request& request, Http::ResponseWriter response) {
        printCookies(request);
        response.cookies()
            .add(Http::Cookie("lang", "en-US"));
        response.send(Http::Code::Ok);
    }

    void setSettings(const Rest::Request& request, Http::ResponseWriter response) {
    	auto settingName = request.param(":settingName").as<std::string>();

    	Guard guard(LawnMowerLock);

    	string val = "";
        if (request.hasParam(":value")) {
            auto value = request.param(":value");
            val = value.as<string>();
        }

    	int setResponse = lwn.set(settingName, val);

    	if (setResponse == 1) {
            response.send(Http::Code::Ok, settingName + " was set to " + val);
        }
        else {
            response.send(Http::Code::Not_Found, settingName + " was not found and or '" + val + "' was not a valid value ");
        }

    }

    void getSettings(const Rest::Request& request, Http::ResponseWriter response){
        auto settingName = request.param(":settingName").as<std::string>();

        Guard guard(LawnMowerLock);

        string valueSetting = lwn.get(settingName);

        if (valueSetting != "") {
            using namespace Http;
            response.headers()
                        .add<Header::Server>("pistache/0.1")
                        .add<Header::ContentType>(MIME(Text, Plain));

            response.send(Http::Code::Ok, settingName + " is " + valueSetting);
        }
        else {
            response.send(Http::Code::Not_Found, settingName + " was not found");
        }
    }

    void goToCharge(const Rest::Request& request, Http::ResponseWriter response) {

        Guard guard(LawnMowerLock);

        //apeleaza initializarea lui Cami
        int setResponseCharge = lwn.ChargeLocation();
        if (setResponseCharge == 0) {
            response.send(Http::Code::Not_Found," No charge statation available in your yard");
        }

        int setResponse = lwn.setBaterie("100");

        if (setResponse == 1) {
            response.send(Http::Code::Ok, "Battery was set to: 100%");
        }
        else {
            response.send(Http::Code::Not_Found," invalid ");
        }
    }

    void setState(const Rest::Request& request, Http::ResponseWriter response){

        auto stateName = request.param(":stateName").as<std::string>();
        int setResponse = lwn.setState(stateName);

        string batteryLevel = lwn.getBaterie();
        // todo cat mai are de tuns

        if (setResponse == 1) {
            if(stateName == "stop"){
                response.send(Http::Code::Ok, "The Lawnmower was stopped and the battery has level " + batteryLevel);
            }
            else if(stateName == "resume"){
                response.send(Http::Code::Ok, "The Lawnmower was resumed");
            } else {
            	int road[10000];
    			lwn.determineRoad(road);
    			int roadLength = road[0];
    			int batteryLevelInt = lwn.getBaterieAsInt();
    			if (roadLength  > batteryLevelInt) {
    				response.send(Http::Code::Ok, "Battery level not enough for this operation!");
    			} else {
    				response.send(Http::Code::Ok, "The Lawnmower has started. It will finish in " + std::to_string(roadLength * 30) + " seconds");
    			}

            }
        }
        else {
            response.send(Http::Code::Not_Found,"not a valid state");
        }

    }

    void getState(const Rest::Request& request, Http::ResponseWriter response){
        Guard guard(LawnMowerLock);

        bool state = lwn.getState();

        using namespace Http;
        response.headers()
                    .add<Header::Server>("pistache/0.1")
                    .add<Header::ContentType>(MIME(Text, Plain));

        if(state){

        response.send(Http::Code::Ok, "The lawnmower is working");
        }
        else{
            response.send(Http::Code::Ok, "The lawnmower is not working");
        }

    }

    void getRoad(const Rest::Request& request, Http::ResponseWriter response) {
    	std::cout<<"Aici 1";
    	Guard guard(LawnMowerLock);

    	//int* road = lwn.determineRoad();
    	//int road = lwn.getChargeLocation();

    	int road[10000];
    	lwn.determineRoad(road);



    	std::cout<<"Aici 2";


    	using namespace Http;
        response.headers()
                    .add<Header::Server>("pistache/0.1")
                    .add<Header::ContentType>(MIME(Text, Plain));

        if(road[0] != 0){
        response.send(Http::Code::Ok, "Road length is " + std::to_string(road[0]));
        }
        else{
            response.send(Http::Code::Ok, "Error!");
        }

    }


    // conventie: latime = coloane; lungime = linii !!!!
    // Q.E.D. box


    class LawnMower {
    public:
    	explicit LawnMower() { }

    	int set(std::string name, std::string value) {
    		if(name == "latime") {
    			curte.latime = std::stoi(value);
    			return 1;
    		}
    		if(name == "lungime") {
    			curte.lungime = std::stoi(value);
    			return 1;
    		}
    		if(name == "matrice") {
    			//curte.matrice = value;
    			//return 1;
    			if(curte.latime != NULL && curte.lungime!=NULL){
    				int latime = curte.latime;
    				int lungime = curte.lungime;

    				if((latime * lungime) != value.length()) {
    					return 0;
    				}

    				int index = 0;

    				for(int i = 0; i < lungime; i++) {
    					for(int j = 0; j < latime; j++) {
    						curte.matrice[i][j] = (int)value[index] - 48;
    						index ++;
    					}
    				}

    				return 1;
    			}
    			return 0;
    		}
    		return 0;
    	}

    	string get(std::string name){
    		if(name == "latime"){
    			return std::to_string(curte.latime);
    		}
    		if(name == "lungime"){
    			return std::to_string(curte.lungime);
    		}
            
    		if(name == "matrice"){
    			string k = "";
    			for (int i = 0; i < curte.lungime;i++){
        			for (int j=0; j < curte.latime;j++){
            			std::string s = std::to_string(curte.matrice[i][j]); 
            			k = k + s;
        			}
    			}
    			return k;
    		}
    		return "";


    	}

        int setBaterie(std::string value){
            int nivel = std::stoi(value);

            if(nivel > 100 || nivel < 0){
                return 0;
            }
            else{
                baterie.nivel = nivel;
            }

            return 1;
        }

        
        string getBaterie(){
            return std::to_string(baterie.nivel);
        }

        int getBaterieAsInt(){
        	return baterie.nivel;
        }


    	int ChargeLocation(){
        	for (int i = 0; i < curte.lungime; i++) {
            	for (int j = 0; j < curte.latime; j++) {
                	if (curte.matrice[i][j] == 2) {
                    	return 1;
                	}
           		}
        	}
        	return 0;
    	}

    	int getChargeLocation() {
    		for (int i = 0; i < curte.lungime; i++) {
            	for (int j = 0; j < curte.latime; j++) {
                	if (curte.matrice[i][j] == 2) {
                    	return i*10000 + j;
                	}
           		}
        	}
    	}

    	int setState(std::string name){

            if(name == "stop"){
                stop.state = false;
            }
            else if(name == "resume") {
                stop.state = true;
            }
            else if(name == "start"){
            	stop.state = true;
            }
            else{
                return 0;
            }
            return 1;
        }


        bool getState(){

            return stop.state;
        }

        void determineRoad(int* road){
        	std::cout<<"Aici 3";

        	int lin = curte.lungime;
        	int col = curte.latime;
        	int linStart = getChargeLocation() / 10000;
        	int colStart = getChargeLocation() % 10000;
	        road[0] = 0;
	        int i = linStart;
	        int j = colStart;
	        std::cout<<"Aici 4";


	        //daca nu e blocat in jos, merge in maxim in jos si se duce in coltul din dreapta
	        if (curte.matrice[i+1][j] != 1){
	            for (; i<=lin; i++){
	                road[0]++;
	                road[ road[0]] = i * 10000 + j;
	            }

	            for (; j<=col; j++){
	                road[0]++;
	                road[ road[0]] = i * 10000 + j;
	            }
	        }
	        else{ //altfel, intai merge dreapta maxim si dupa jos ca sa ajunga in acelasi colt
	            for (; j<=col; j++){
	                road[0]++;
	                road[ road[0]] = i * 10000 + j;
	            }
	            for (; i<=lin; i++){
	                road[0]++;
	                road[ road[0]] = i * 10000 + j;
	            }
	        }
	        
	        //1 pt spre dreapta, 0 pentru stanga
	        int lastdir = 1;
	        int lastj = col;
	        std::cout<<"Aici 5";
	        
	        //porneste in zig zag 
	        for (;i > 0 ; i--){
	            if (lastdir == 1){
	                for (j = lastj; j > 0; j--){
	                    road[0]++;
	                    road[ road[0]] = i * 10000 + j;
	                    if (curte.matrice[i][j-1] != 1){
	                        lastdir = 0;
	                        break;
	                    }
	                }
	                lastdir = 0;
	            }
	            else{
	                for (j = lastj; j <= col; j++){
	                    road[0]++;
	                    road[ road[0]] = i * 10000 + j;
	                    if (curte.matrice[i][j+1] != 1){
	                        lastdir = 1;
	                        break;
	                    }
	                }
	                lastdir = 1;
	            }
	            
	        }

	        //acum porneste iar in zig zag acoperind partea ramasa

	        for (;i <= lin ; i++){
	            if (lastdir == 1){
	                for (j = lastj; j > 0; j--){
	                    road[0]++;
	                    road[ road[0]] = i * 10000 + j;
	                    if (curte.matrice[i][j-1] != 1){
	                        lastdir = 0;
	                        break;
	                    }
	                }
	                lastdir = 0;
	            }
	            else{
	                for (j = lastj; j <= col; j++){
	                    road[0]++;
	                    road[ road[0]] = i * 10000 + j;
	                    if (curte.matrice[i][j+1] != 1){
	                        lastdir = 1;
	                        break;
	                    }
	                }
	                lastdir = 1;
	            }
	            
	        }

	        if (curte.matrice[linStart+1][colStart] == 0){
	            if (j==col){
	                for (; j>colStart; j--){
	                    road[0]++;
	                    road[ road[0]] = i * 10000 + j;
	                }
	            }
	            else{
	                for (; j>colStart; j++){
	                    road[0]++;
	                    road[ road[0]] = i * 10000 + j;
	                }
	            }
	            for (; i>=linStart; i--){
	                road[0]++;
	                road[ road[0]] = i * 10000 + j;
	            }
	        }
	        else{
	            for (; i>=linStart; i--){
	                road[0]++;
	                road[ road[0]] = i * 10000 + j;
	            }
	            if (j==col){
	                for (; j>colStart; j--){
	                    road[0]++;
	                    road[ road[0]] = i * 10000 + j;
	                }
	            }
	            else{
	                for (; j>colStart; j++){
	                    road[0]++;
	                    road[ road[0]] = i * 10000 + j;
	                }
	            }
	            for (; i>=linStart; i--){
	                road[0]++;
	                road[ road[0]] = i * 10000 + j;
	            }
	        }
	        std::cout<<"Aici 6";


	        // pe pozitia 0 este numarul de mutari
	        // pe restul pozitiilor se afla coordonatele ce pot fi aflate: lin = road[i]/10000; col = road[i]%10000
    	}	


    private:
    	struct settings{
    		int latime;
    		int lungime;
    		int matrice[500][500];
    	}curte;

        struct masina{
            int nivel;
        }baterie;

        struct boolState{
            bool state;
        }stop;

    };

    using Lock = std::mutex;
    using Guard = std::lock_guard<Lock>;
    Lock LawnMowerLock;

    LawnMower lwn;

    std::shared_ptr<Http::Endpoint> httpEndpoint;
    Rest::Router router;

};

int main(int argc, char *argv[]) {

    // This code is needed for gracefull shutdown of the server when no longer needed.
    sigset_t signals;
    if (sigemptyset(&signals) != 0
            || sigaddset(&signals, SIGTERM) != 0
            || sigaddset(&signals, SIGINT) != 0
            || sigaddset(&signals, SIGHUP) != 0
            || pthread_sigmask(SIG_BLOCK, &signals, nullptr) != 0) {
        perror("install signal handler failed");
        return 1;
    }

    // Set a port on which your server to communicate
    Port port(9080);

    // Number of threads used by the server
    int thr = 2;

    if (argc >= 2) {
        port = static_cast<uint16_t>(std::stol(argv[1]));

        if (argc == 3)
            thr = std::stoi(argv[2]);
    }

    Address addr(Ipv4::any(), port);

    cout << "Cores = " << hardware_concurrency() << endl;
    cout << "Using " << thr << " threads" << endl;

    // Instance of the class that defines what the server can do.
    LawnMowerEndpoint stats(addr);

    // Initialize and start the server
    stats.init(thr);
    stats.start();


    // Code that waits for the shutdown sinal for the server
    int signal = 0;
    int status = sigwait(&signals, &signal);
    if (status == 0)
    {
        std::cout << "received signal " << signal << std::endl;
    }
    else
    {
        std::cerr << "sigwait returns " << status << std::endl;
    }

    stats.stop();
}