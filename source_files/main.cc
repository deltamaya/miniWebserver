#include "utils.h"
#include "use_counting.hh"
#include "zipping.hh"
#include "minilog.hh"
using namespace httplib;
using namespace std;
using enum LogLevel;
stat_manager manager(webroot);

void bad_request(const Request &rq, Response &res)
{
    ifstream fin{errorpath, ios::binary};
    string content;
    char ch;
    while (ch = fin.get(), ch != -1)
    {
        content.push_back(ch);
    }
    res.status = 404;
    res.reason = "Not Found";
    res.set_content(content, "text/html");
}

string get_etag(filesystem::path path)
{
    struct stat st;
    stat(path.c_str(), &st);
    return format("{}-{}", path.filename().c_str(), st.st_size);
}

void get_content(const filesystem::directory_entry &entry, Response &res)
{
    vector<char> buf(1024 * 256);
    ifstream file{entry.path(), ios::binary};
    while (!file.eof())
    {
        file.read(buf.data(), buf.size());
        std::streamsize bytesRead = file.gcount();
        if (bytesRead > 0)
        {
            res.body += buf.data();
        }
    }
}

void read_file(const Request &rq, Response &res)
{
    // cout << "read_file" << endl;
    filesystem::path fname{webroot + rq.path};
    if (fname.extension() == "")
        fname /= "index.html";
    filesystem::directory_entry entry{fname};
    if (entry.exists() && entry.is_regular_file())
    {
        if (fname.extension() == ".html")
        {
            stringstream content;
            ifstream fin{fname, ios::binary};
            content << fin.rdbuf();
            res.set_content(content.str(), "text/html");
        }
        else
        {

            res.set_chunked_content_provider("application/octet-stream", [&](size_t offset, DataSink &sink)
                                             {
                                                 ifstream file{fname, ios::binary};
                                                 vector<char> buf(1024 * 256);
                                                 while (!file.eof())
                                                 {
                                                     file.read(buf.data(), buf.size());
                                                     std::streamsize bytesRead = file.gcount();
                                                     if (bytesRead > 0)
                                                     {
                                                         sink.write(buf.data(), buf.size());
                                                     }
                                                 }
                                                 sink.done(); // No more data
                                                 return true; //
                                             });
        }
    }
    else
    {
        bad_request(rq, res);
    }
}

void post_file(const Request &rq, Response &res)
{
    // cerr << "post file" << endl;
    if (rq.has_file("file"))
    {
        const auto &file = rq.get_file_value("file");
        std::string filename = webroot + "/" + file.filename;
        std::string file_content = file.content;
        // cout << "filename " << filename << endl;
        std::ofstream outfile(filename, std::ofstream::binary);
        outfile.write(file_content.data(), file_content.length());
        outfile.close();

        res.set_content("File uploaded successfully", "text/plain");
        manager.update(filename);
    }
    else
    {
        res.status = 400;
        res.set_content("Bad Request: No file uploaded", "text/plain");
    }
    manager.update_index_html();
}

// bool handle_file_request(const Request &req, Response &res,
//                                         bool head) {
//   for (const auto &entry : base_dirs_) {
//     // Prefix match
//     if (!req.path.compare(0, entry.mount_point.size(), entry.mount_point)) {
//       std::string sub_path = "/" + req.path.substr(entry.mount_point.size());
//       if (detail::is_valid_path(sub_path)) {
//         auto path = entry.base_dir + sub_path;
//         if (path.back() == '/') { path += "index.html"; }

//         if (detail::is_file(path)) {
//           for (const auto &kv : entry.headers) {
//             res.set_header(kv.first.c_str(), kv.second);
//           }

//           auto mm = std::make_shared<detail::mmap>(path.c_str());
//           if (!mm->is_open()) { return false; }

//           res.set_content_provider(
//               mm->size(),
//               detail::find_content_type(path, file_extension_and_mimetype_map_,
//                                         default_file_mimetype_),
//               [mm](size_t offset, size_t length, DataSink &sink) -> bool {
//                 sink.write(mm->data() + offset, length);
//                 return true;
//               });

//           if (!head && file_request_handler_) {
//             file_request_handler_(req, res);
//           }

//           return true;
//         }
//       }
//     }
//   }
//   return false;
// }
using namespace std::string_literals;
static const unordered_map<string, string> extension_to_type_mapping = {
    pair{".html"s, "text/html"s},
    pair{".txt"s, "text/plain"s},
    // pair{".mp3"s, "audio/mp3"s},
    pair{".jpeg"s, "image/jpeg"s},
    pair{".jpg"s, "image/jpeg"s},
    pair{".png"s, "image/png"s},
    // pair{".mp4"s, "video/mp4"s},
    pair{".css"s, "ext/css"s},
};

bool read_file_handler(const Request &req, Response &res)
{

    filesystem::path path{webroot + req.path};
    manager.use(path);
    if (!path.has_extension())
        path /= "index.html";
    filesystem::directory_entry entry{path};
    if (entry.exists() && entry.is_regular_file())
    {
        ifstream file{path, ios::binary};
        file.seekg(0, ios::end);
        auto endpos{file.tellg()};
        file.seekg(0, ios::beg);
        // res.set_header("Content-Length", to_string(endpos));
        auto ext{path.extension()};
        string type;
        if (auto it{extension_to_type_mapping.find(ext)}; it != extension_to_type_mapping.end())
        {
            type = it->second;
        }
        else
        {
            type = "application/octet-stream";
        }
        // cout << endpos << endl;

        if (endpos < 1024 * 256)
        {
            vector<char> content(endpos);
            file.readsome(content.data(), endpos);
            res.set_content_provider(endpos, type,
                                     [content](size_t offset, size_t length, DataSink &sink) -> bool
                                     {
                                         // printf("offset: %d,length:%d\n",offset,length);
                                         sink.write(content.data() + offset, length);
                                         return true;
                                     });
        }
        else
        {
            file.close();
            res.set_chunked_content_provider(type,
                                             [entry](size_t offset, DataSink &sink) -> bool
                                             {
                                                 ifstream file{entry.path(), ios::binary};
                                                 vector<char> partial_content(1024 * 256);
                                                 streampos begin{file.tellg()};
                                                 file.seekg(0, ios::end);
                                                 streampos end{file.tellg()};
                                                 file.seekg(0, ios::beg);
                                                 while (begin < end)
                                                 {
                                                     auto readsize = file.readsome(partial_content.data(), 1024 * 256);
                                                     begin += readsize;
                                                     sink.write(partial_content.data(), readsize);
                                                 }
                                                 sink.done();
                                                 return true;
                                             });
        }
    }
    return false;
}

int main()
{
    set_logfile("./logfile");
    set_loglev(LogLevel::debug);
    log_debug("{} server inited",55369);
    manager.update_index_html();
    Server svr;
    // svr.set_mount_point("/", webroot);
    svr.Get(R"(.*)", read_file_handler);
    svr.Post("/update", post_file);
    svr.listen("0.0.0.0", default_port);
}
