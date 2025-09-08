#include<server.hpp>
#include<filehandle.hpp>
#include <csignal>
#include <cstdlib>
#include <unistd.h>

struct Context{
    SOCKET sock;
};

Context* stx = nullptr;
void handle_exit(int signum) {
    if(stx){
        closesocket(stx->sock);
        WSACleanup();
        _exit(1);
    }
}


int main(){
    Router router = Router();
    Server server = Server(3000, router);
    Context c = {server.serverSocket};
    FileHandler ft = FileHandler();
    stx = &c;

    
    router.addRoute("/",HttpMethod::GET, [&](const HttpRequest& req){
        std::string data = ft.readTextFile("site/index.html");
        return HtmlResponse(data);
    });
    router.addRoute("/index.html",HttpMethod::GET, [&](const HttpRequest& req){
        std::string data = ft.readTextFile("site/index.html");
        return HtmlResponse(data);
    });
    router.addRoute("/about.html",HttpMethod::GET, [&](const HttpRequest& req){
        std::string data = ft.readTextFile("site/about.html");
        return HtmlResponse(data);
    });

    router.addRoute("/neon.css",HttpMethod::GET, [&](const HttpRequest& req){
        std::string data = ft.readTextFile("site/neon.css");
        return CSSResponse(data);
    });

    router.addRoute("/app.js",HttpMethod::GET, [&](const HttpRequest& req){
        std::string data = ft.readTextFile("site/app.js");
        return JSResponse(data);
    });

    router.addRoute("/assets/hero.jpg",HttpMethod::GET, [&](const HttpRequest& req){
        std::vector<char> data = ft.readBinaryFile("site/assets/hero.jpg");
        return BinaryResponse(data, "image/x-icon",200);
    });

    router.addRoute("/favicon.ico",HttpMethod::GET, [&](const HttpRequest& req){
        std::vector<char> data = ft.readBinaryFile("favicon.ico");
        return BinaryResponse(data, "image/jpeg",200);
    });
    router.addRoute("/api/getData", HttpMethod::GET, [](const HttpRequest& req){
        std::string json = R"({"message":"Response from C++ server"})";
        return JsonResponse(json);
    });

    router.addRoute("/putData", HttpMethod::POST, [](const HttpRequest& req) {
        std::string nojson = R"({"message":"Need Json body."})";
        std::string badjson = R"({"message":"Json body is bad."})";

        if (req.contentType == "application/json") {
            auto bodyOpt = req.parseJsonBody();

            if (!bodyOpt) return JsonResponse(badjson, 400);

            try {
                json body = *bodyOpt;
                std::string name = body.value("name", "");
                std::string resjson = R"({"message":"Name received )" + name + "\"}";
                return JsonResponse(resjson);
            }
            catch (...) {
                return JsonResponse(badjson, 400);
            }
        }

        return JsonResponse(nojson, 400);
    });

    server.bindAndListen();
    server.start(-1);
    std::signal(SIGINT, handle_exit);
    std::signal(SIGTERM, handle_exit);
    return 0;
}

