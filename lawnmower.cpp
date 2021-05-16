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

        int setResponse = lwn.setBaterie("100");

        if (setResponse == 1) {
            response.send(Http::Code::Ok, "Battery was set to: 100%");
        }
        else {
            response.send(Http::Code::Not_Found," invalid ");
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

    private:
    	struct settings{
    		int latime;
    		int lungime;
    		int matrice[500][500];
    	}curte;

        struct masina{
            int nivel;
        }baterie;

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