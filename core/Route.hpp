#include<winsock2.h>
#include<windows.h>
#include<unordered_map>
#include<string>
#include<concepts>
#include<httpreq.hpp>
#include<httpres.hpp>
#include<functional>
#include<stdexcept>
template<typename T>
concept RouteHanlder = requires(T h, const HttpRequest& req ){
    { h(req) } -> std::same_as<HttpResponse>;
};
// Above RouteHandler will be used for better compile time support in future

class Router{
    std::unordered_map<std::string, std::unordered_map<HttpMethod, std::function<HttpResponse(const HttpRequest&)>>> routes;
    public:
        //template<RouteHanlder H>
        void addRoute(const std::string& path, HttpMethod m, auto handler){
            routes[path][m] = handler;
        }
        HttpResponse handleRoute(const HttpRequest& req){
            auto pathIt = routes.find(req.path);
            if(pathIt == routes.end()){
                HttpResponse response = HtmlResponse("<h1>Not Found Page</h1>",404);
                return response;
            }

            auto methodIt = pathIt->second.find(req.method);
            if(methodIt == pathIt->second.end()){
                HttpResponse response = HtmlResponse("<h1>Not Found Page</h1>",404);
                return response;
            }

            HttpResponse rep = methodIt->second(req);
            return rep;
        }
};