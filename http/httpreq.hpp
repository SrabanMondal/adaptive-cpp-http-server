#include<string>
#include<unordered_map>
#include<vector>
#include "json.hpp"
using json = nlohmann::json;
enum class HttpMethod { GET, POST, PUT, DEL };
class HttpRequest{
    public:
        HttpMethod method;
        std::string path;
        std::vector<char> body;
        std::string query;
        std::size_t contentLength;
        std::string contentType;
        std::unordered_map<std::string,std::string> queryParams;
        HttpRequest(const std::string& req, const std::vector<char>& b={}):body(b){
            parseHeaders(req);
        }

        std::optional<json> parseJsonBody() const{
            std::string b(body.begin(),body.end());
            try {
                json j = json::parse(b);
                return j;
            } catch (json::parse_error &e) {
                return std::nullopt;
            }
        }

        std::unordered_map<std::string, std::string> parseFormBody() const {
            std::string b(body.begin(), body.end());
            std::unordered_map<std::string, std::string> form;
            size_t start = 0;
            while (start < b.size()) {
                size_t eq = b.find('=', start);
                if (eq == std::string::npos) break;
                size_t amp = b.find('&', eq);
                std::string key = urlDecode(b.substr(start, eq - start));
                std::string value = urlDecode(b.substr(eq + 1, amp - eq - 1));
                form[key] = value;
                if (amp == std::string::npos) break;
                start = amp + 1;
            }
            return form;
        }

        std::string parseTextBody() const {
            return std::string(body.begin(),body.end());
        }
        
        std::string urlDecode(const std::string& str) const {
            std::string result;
            result.reserve(str.size());
            for (size_t i = 0; i < str.size(); i++) {
                if (str[i] == '+') {
                    result += ' ';
                } else if (str[i] == '%' && i + 2 < str.size()) {
                    std::string hex = str.substr(i + 1, 2);
                    char ch = static_cast<char>(std::stoi(hex, nullptr, 16));
                    result += ch;
                    i += 2;
                } else {
                    result += str[i];
                }
            }
            return result;
        }

    private:
        void parseHeaders(const std::string& req);
        void parseQueryParams(const std::string& query);
        std::string getHeader(const std::string& req, const std::string& key);
};