#include<server.hpp>
#include<filehandle.hpp>
#include <csignal>
#include <cstdlib>
#include <atomic>

// Global shutdown flag — set by signal handler, checked by cleanup ──
static std::atomic<bool> g_shutdown{false};

// Global pointers for signal handler access (signal handlers can't use locals)
static Server*  g_server = nullptr;
static FileHandler* g_ft = nullptr;

// Signal handler
void handle_exit(int signum) {
    bool expected = false;
    if (!g_shutdown.compare_exchange_strong(expected, true)) {
        return;
    }

    // Close the listening socket — this breaks the accept() loop
    if (g_server) {
        g_server->shutdown();
    }

    // Clear file caches (releases preloaded memory)
    if (g_ft) {
        g_ft->clear();
    }

    WSACleanup();
    std::exit(0);
}


int main(){
    Router router = Router();
    Server server(3000, router);
    FileHandler ft;

    g_server = &server;
    g_ft     = &ft;

    // Register signal handlers BEFORE starting the server
    // server.start() is blocking — if we register after, signals won't work.
    std::signal(SIGINT, handle_exit);
    std::signal(SIGTERM, handle_exit);

    // Preload static files into memory at startup
    ft.preloadFiles({
        "site/index.html",
        "site/about.html",
        "site/neon.css",
        "site/app.js",
        "site/assets/hero.jpg",
        "favicon.ico"
    }, {
        false,  // text
        false,  // text
        false,  // text
        false,  // text
        true,   // binary
        true    // binary
    });

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

    constexpr bool ENABLE_RATE_LIMITING = false;
    server.setRateLimitingEnabled(ENABLE_RATE_LIMITING);

    server.bindAndListen();
    server.start(-1);
    return 0;
}

