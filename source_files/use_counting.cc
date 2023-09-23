#include "utils.h"
#include "use_counting.hh"
#include <zip.h>
using namespace std::chrono;
using namespace std;

void stat_manager::update_index_html()
{
    unique_lock<mutex> lk(mtx_);
    filesystem::path path{webroot};
invalid_iterator:
    filesystem::recursive_directory_iterator begin(path), end;
    for (auto iter{begin}; iter != end; ++iter)
    {
        auto name{iter->path()};
        // cout <<"checking  "<< name << endl;
        if (time(nullptr) - data_[name]["atime"] > 60 * 60 * 24 * 5)
        {
            lk.unlock();
            zipfile(name);
            goto invalid_iterator;
        }
    }
    ofstream file{path / "index.html"};
    file << R"(<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>资源云存储</title>
            <script src="https://code.jquery.com/jquery-3.6.0.min.js"></script>
    <script>
        $(document).ready(function() {
            $('#fileUploadLink').click(function(e) {
                e.preventDefault(); // 阻止默认的链接点击行为

                // 触发文件输入控件的点击事件
                $('#fileInput').click();
            });

            // 当用户选择文件后触发以下事件
            $('#fileInput').change(function() {
                var formData = new FormData();

                // 获取用户选择的文件输入
                var fileInput = $('#fileInput')[0];
                var file = fileInput.files[0];

                // 将文件添加到 FormData 对象中
                formData.append('file', file);

                // 发送POST请求到服务器
                $.ajax({
                    url: '/update',
                    type: 'POST',
                    data: formData,
                    contentType: false,
                    processData: false,
                    success: function(response) {
                        // 处理服务器的响应
                        $('#result').text(response);
                    },
                    error: function() {
                        console.log('发送数据到服务器时发生错误');
                    }
                });
            });
        });
    </script>
    </head>
    <h1>文件下载</h1>
    )";
filesystem::recursive_directory_iterator genbegin(path), genend;
    for (auto iter{genbegin}; iter != genend; ++iter)
    {
        auto name{iter->path()};
        // cout <<"printing "<< name << endl;
        file << "<a href="
             << "./" / name.filename() << ">" << name.filename() << "</a>\n";
        if (data_[name]["zipped"] != 0)
            file << " (已压缩) ";
        file << "last modified in [" << ctime(&(data_[name]["mtime"]));
        file << "] accessed " << data_[name]["count"] << " times";
        file << "<br></br>";
    }
    file << R"(
<body>
    <h1>文件上传</h1>

    <!-- 文件上传控件 -->
    <form action="/update" method="post" enctype="multipart/form-data">
        <input type="file" name="file" id="fileInput" style="display: none;">
        <a href="#" id="fileUploadLink">点击这里选择文件上传</a>
    </form>

    <!-- 用于显示服务器响应消息 -->
    <div id="result"></div>
</body>
</html>)";
    // print();
}

void stat_manager::zipfile(const filesystem::path &path)
{
    unique_lock<mutex> lk(mtx_);
    auto archivepath = path.filename().replace_extension(".zip");
    // cout << "archive path :" << archivepath << endl;
    int error = 0;
    archivepath = webroot / archivepath;
    // cout << archivepath.c_str() << endl;
    zip_t *archive = zip_open(archivepath.c_str(), ZIP_CREATE, &error);
    data_[archivepath.filename()] = std::move(data_[path]);
    data_.erase(path);
    if (!archive)
    {
        cout << "can not create file\n";
        exit(1000);
    }
    // cout << "zip open ok\n";
    if (error)
    {
        printf("could not open or create archive\n");
        exit(999);
    }

    zip_source_t *source = zip_source_file(archive, path.c_str(), 0, -1);
    if (source == NULL)
    {
        printf("failed to create source buffer. %s\n", zip_strerror(archive));
        exit(777);
    }
    // cout << "ifstream close ok\n";
    int index = (int)zip_file_add(archive, path.filename().c_str(), source, ZIP_FL_OVERWRITE);
    if (index < 0)
    {
        printf("failed to add file to archive: %s\n", zip_strerror(archive));
        exit(666);
    }
    // cout << "zip file add ok\n";
    zip_close(archive);
    // cout << "zip close ok\n";
    data_[path]["zipped"] = 1;
    filesystem::remove(path);
    lk.unlock();
    update(archivepath);
}
