#include<string>
#include<unordered_map>
#include<vector>
class HttpResponse{
    public:
        inline static const std::unordered_map<unsigned short, std::string> codeMap = {
                {200, "OK"}, {201, "Created"}, {400, "Bad Request"},
                {401, "Unauthorized"}, {403, "Forbidden"}, {404, "Not Found"},
                {500, "Internal Server Error"}, {502, "Bad Gateway"}, {503, "Service Unavailable"},
                {429,"Too Many Requests"}
            };
        unsigned short status;
        std::string reason;
        std::vector<char> body;    
        std::string contentType;
        std::unordered_map<std::string,std::string> queryParams;
        explicit HttpResponse(const std::vector<char>& b = {}, const unsigned short& s=200, const std::string& t="", const std::string& r=""): status(s), body(b), contentType(t){
            if (!r.empty()) {
                reason = r;
            } else {
                auto it = codeMap.find(s);
                reason = (it != codeMap.end()) ? it->second : "";
            }
        }

        std::string headers() const{
            return "HTTP/1.1 " + std::to_string(status)+" " + reason + "\r\n" +
               "Content-Type: " + contentType + "\r\n" +
               "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";
        };
};


class HtmlResponse: public HttpResponse{
    public:
        HtmlResponse(const std::string& html, const unsigned short& s=200)
        : HttpResponse(std::vector<char>(html.begin(), html.end()),s,"text/html") {}
};

class CSSResponse: public HttpResponse{
    public:
        CSSResponse(const std::string& css, const unsigned short& s=200)
        : HttpResponse(std::vector<char>(css.begin(), css.end()),s,"text/css") {}
};

class JSResponse: public HttpResponse{
    public:
        JSResponse(const std::string& js, const unsigned short& s=200)
        : HttpResponse(std::vector<char>(js.begin(), js.end()),s,"application/javascript") {}
};

class JsonResponse: public HttpResponse{
    public:
        JsonResponse(const std::string& json, const unsigned short& s=200)
        : HttpResponse(std::vector<char>(json.begin(), json.end()),s,"application/json") {}
};

class BinaryResponse : public HttpResponse {
public:
    BinaryResponse(const std::vector<char>& data, const std::string& type, const unsigned short& s=200)
        : HttpResponse(data, s, type) {}
};