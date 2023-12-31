#include "utils.h"
#include "minilog.hh"
#include <iomanip>
#include <regex>
using enum LogLevel;
using namespace std;

unordered_map<string, string> analyze_request(const string &request)
{
    unordered_map<string, string> header;
    size_t sep{request.find("\r\n\r\n")};
    size_t pos{request.find("\r\n")};
    size_t next{pos};
    auto status_code = request.substr(0, pos);
    size_t spos{status_code.find(" ")}, snext{status_code.find(" ", spos + 1)};
    auto method{status_code.substr(0, spos)}, req_path{status_code.substr(spos + 1, snext - spos - 1)}, httpver{status_code.substr(snext + 1)};
    // 
    header["method"] = std::move(method);
    header["req_path"] = std::move(req_path);
    header["httpver"] = std::move(httpver);
    while (pos < sep)
    {
        next = request.find("\r\n", pos + 2);
        auto line = request.substr(pos + 2, next - pos - 2);
        pos = next;
        auto mid = line.find(": ");
        auto k{line.substr(0, mid)};
        auto v{line.substr(mid + 2)};
        header[k] = v;
        // 
    }
    header["body"] = request.substr(sep + 4);
    return header;
}

void send_large_file(const filesystem::path path, struct stat &st, const int cfd, int start, int end)
{
    filesystem::directory_entry dir{path};
    
    ifstream fin{path, ios::binary};
    if (!fin.is_open())
    {
        
        return;
    }
    string fname{path.filename()};
    size_t fsize = st.st_size;

    if (start > 0)
    {
        fin.seekg(start);
    }

    int sendsize(end - start);
    // 
    vector<char> buf(sendsize);
    int sentbytes = fin.readsome(buf.data(), sendsize - 1);
    string ret{"HTTP/1.1 206 Partial Content\r\n"};
    ret += format("Content-Type: application/octet-stream\r\n");
    ret += format("Content-Disposition: attachment\r\n");
    ret += format("Content-Length: {}\r\n", sendsize);
    ret += format("Accept-Ranges: bytes\r\n");
    ret += format("Content-Range: bytes {}-{}/{}\r\n", start, start + sentbytes, fsize);
    ret += "\r\n";
    ret += buf.data();
    //
    send(cfd, ret.c_str(), ret.size(), 0);
    //
}

string get_body(const filesystem::path &path, struct stat &st)
{
    filesystem::directory_entry dir{path};
    string ret;

    // cout<<"Okpage\n";
    ifstream fin{path, ios_base::binary};
    char ch;
    while ((ch = fin.get()) != -1)
    {
        ret += ch;
    }
    fin.close();
    return ret;
}

string get_header(unordered_map<string, string> &mp, struct stat &st)
{

    filesystem::path path{mp["req_path"]};
    filesystem::directory_entry dir{path};
    string ret{"HTTP/1.1 200 OK\r\n"};

    ret += format("Content-Length: {}\r\n", st.st_size);
    string postfix{path.extension()};
    string fname{path.filename()};
    
    if (postfix == ".html")
    {
        ret += format("Content-Type: text/html\r\n");
    }
    else
    {
        ret += format("Content-Type: application/octet-stream\r\n");
        ret += format("Content-Disposition: attachment\r\n");
    }

    ret += "\r\n";
    return ret;
}

string url_decode(const string &encoded)
{
    std::ostringstream decoded;
    std::istringstream input(encoded);

    char ch;
    while (input >> std::noskipws >> ch)
    {
        if (ch == '%')
        {
            char hex[3];
            if (input >> hex[0] >> hex[1])
            {
                hex[2] = '\0';
                int decodedChar = std::stoi(hex, nullptr, 16);
                decoded << static_cast<char>(decodedChar);
            }
            else
            {
                decoded << ch;
            }
        }
        else if (ch == '+')
        {
            decoded << ' ';
        }
        else
        {
            decoded << ch;
        }
    }

    return decoded.str();
}

void Service(const int cfd, const sockaddr_in &client)
{
    char buf[bufsz]{};
    while (true)
    {
        auto n = recv(cfd, buf, bufsz - 1, 0);
        if (n < 0)
        {
            
            exit(RECV_ERR);
        }
        else if (n == 0)
        {
            
            return;
        }
        else
        {
            buf[n] = 0;
            
        }
        string request{buf};
        bzero(buf, bufsz);
        auto header{analyze_request(request)};
        header["req_path"] = url_decode(header["req_path"]);
        if (header["req_path"].back() == '/')
            header["req_path"] += "index.html";
        header["req_path"] = webroot + header["req_path"];

        string ret;
        
        filesystem::path path{header["req_path"]};
        filesystem::directory_entry dir{path};
        if (dir.exists() && dir.is_regular_file())
        {
            
        }
        else
        {
            header["req_path"] = errorpath;
        }
        
        struct stat st;
        stat(header["req_path"].c_str(), &st);
        if (static_cast<size_t>(st.st_size) < bufsz)
        {
            ret += get_header(header, st);
            ret += get_body(header["req_path"], st);
            
            send(cfd, ret.c_str(), ret.size(), 0);
        }
        else
        {
            if (header.contains("Range"))
            {
                
                std::regex pattern("^bytes=(\\d*)-(\\d*)$");

                std::smatch matches;
                auto input{header["Range"]};
                if (std::regex_search(input, matches, pattern))
                {
                    
                    if (matches.size() == 3)
                    {
                        std::string start_str = matches[1].str();
                        std::string end_str = matches[2].str();

                        size_t start = std::stoull(start_str);
                        size_t end = end_str == "" ? st.st_size : stoull(end_str);
                        end = end > st.st_size ? st.st_size : end;
                        send_large_file(header["req_path"], st, cfd, start, end);
                    }
                }
                else
                {
                    
                }
            }
            else
            {
                
                string ret{"HTTP/1.1 200 OK\r\n"};
                filesystem::directory_entry dir{path};
                ifstream fin{path, ios::binary};
                if (!fin.is_open())
                {
                    
                    return;
                }
                string fname{path.filename()};
                int fsize = st.st_size;
                // 

                ret += format("Content-Type: application/octet-stream\r\n");
                ret += format("Content-Disposition: attachment\r\n");
                ret += format("Content-Length: {}\r\n", fsize);
                ret += "\r\n";
                // send(cfd, ret.c_str(), ret.size(), 0);
                char buf[bufsz];
                int start{0}, end{fsize};
                
                send(cfd, ret.c_str(), ret.size(), 0);
                ret.clear();
                while (end > start)
                {
                    int sentbytes = fin.readsome(buf, bufsz);

                    
                    send(cfd, buf, sentbytes, 0);
                    bzero(buf,bufsz);
                    end -= sentbytes;
                    
                }
            }
        }
    }
}