#include <dirent.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include "parcxx.h"

#include <httpi/displayer.h>
#include <httpi/html/chart.h>
#include <httpi/html/form-gen.h>
#include <httpi/html/json.h>
#include <httpi/job.h>
#include <httpi/monitoring.h>
#include <httpi/rest-helpers.h>

std::string MakePage(const std::string& content) {
    // clang-format off
    using namespace httpi::html;
    return (Html() <<
        "<!DOCTYPE html>"
        "<html>"
           "<head>"
                R"(<meta charset="utf-8">)"
                R"(<meta http-equiv="X-UA-Compatible" content="IE=edge">)"
                R"(<meta name="viewport" content="width=device-width, initial-scale=1">)"
                R"(<link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.5/css/bootstrap.min.css">)"
                R"(<link rel="stylesheet" href="//cdn.jsdelivr.net/chartist.js/latest/chartist.min.css">)"
                R"(<script src="//cdn.jsdelivr.net/chartist.js/latest/chartist.min.js"></script>)"
            "</head>"
            "<body lang=\"en\">"
                "<div class=\"container\">"
                    "<h1>Vermicadre wifi configuration</h1>" <<
                    Div().Attr("class", "col-md-9") <<
                        content <<
                    Close() <<
                    Div().Attr("class", "col-md-3") <<
                        Ul() <<
                            Li() <<
                                A().Attr("href", "/") <<
                                    "How to / manual" <<
                                Close() <<
                            Close() <<
                            Li() <<
                                A().Attr("href", "/new") <<
                                    "Create a new solo game" <<
                                Close() <<
                            Close() <<
                            Li() <<
                                A().Attr("href", "/turn") <<
                                    "Play a turn in a solo game" <<
                                Close() <<
                            Close() <<
                            Li() <<
                                A().Attr("href", "/newvs") <<
                                    "Create/Join a Versus game" <<
                                Close() <<
                            Close() <<
                            Li() <<
                                A().Attr("href", "/turnvs") <<
                                    "Play a turn in a versus game" <<
                                Close() <<
                            Close() <<
                        Close() <<
                    Close() <<
                "</div>"
            "</body>"
        "</html>").Get();
    // clang-format on
}

auto ParseUntilEnd() {
    return parse_while1(
        parser_pred(parse_char(), [](auto c) { return c != '\n'; }),
        std::string(),
        [](std::string res, char c) {
            res.push_back(c);
            return res;
        });
}

class ConfigFile {
   public:
    ConfigFile(const std::string& path) : path_(path) {
        std::ifstream t(path.c_str());
        std::string content{std::istreambuf_iterator<char>(t),
                            std::istreambuf_iterator<char>()};

        static auto parser = (parse_word("ssid") >> parse_char('=') >>
                              ParseUntilEnd() << parse_char('\n')) &
                             (parse_word("passwd") >> parse_char('=') >>
                              ParseUntilEnd() << parse_char('\n'));
        auto res = parser(content.begin(), content.end());
        if (!res) {
            throw std::runtime_error("parse error");
        }

        ssid_ = res->first.first;
        passwd_ = res->first.second;
        path_ = path;
    }

    void Write() const {
        std::ofstream out(path_);

        out << "ssid=\"" << ssid_ << "\"\npasswd=" << passwd_ << "\n";
    }

    const std::string& ssid() const { return ssid_; }
    const std::string& passwd() const { return passwd_; }
    const std::string& path() const { return path_; }

   private:
    std::string ssid_;
    std::string passwd_;
    std::string path_;
};

std::vector<ConfigFile> ReadConfs(const std::string& path) {
    std::vector<ConfigFile> files;
    std::unique_ptr<DIR, std::function<int(DIR*)>> d(opendir(path.c_str()),
                                                     closedir);
    if (!d) {
        return files;
    }

    dirent* dir;
    while ((dir = readdir(d.get())) != nullptr) {
        if (dir->d_type != DT_REG) {
            continue;
        }

        std::string conf_path = path + "/" + std::string(dir->d_name);
        try {
            files.emplace_back(conf_path);
        } catch (std::exception&) {
            std::cerr << "Couldn't parse " << conf_path << "\n";
        }
    }

    return files;
}

std::string Escape(const std::string& str) {
    std::string res;
    for (char c : str) {
        switch (c) {
            case '"':
                res += "&quot;";
                break;
            case '\'':
                res += "&#39;";
                break;
            case '<':
                res += "&lt;";
                break;
            case '>':
                res += "&gt;";
                break;
            default:
                res.push_back(c);
                break;
        }
    }
    return res;
}

int main() {
    std::vector<ConfigFile> config_files = ReadConfs("./wifi");

    HTTPServer server(8080);

    server.RegisterUrl(
        "/",
        httpi::RestPageMaker(MakePage).AddResource(
            "GET",
            httpi::RestResource(
                httpi::html::FormDescriptor<>{},
                []() { return 0; },
                [&](int a) {
                    using namespace httpi::html;
                    Html html;
                    html << Ul();
                    for (auto& c : config_files) {
                        html << Form() << Div().Attr("class", "form-group row")
                             << Input()
                                    .Name("path")
                                    .Attr("type", "text")
                                    .Attr("class", "form-control")
                                    .Attr("value", Escape(c.path()))
                             << Close() << Div().Attr("class", "form-group row")
                             << Input()
                                    .Name("ssid")
                                    .Attr("type", "text")
                                    .Attr("class", "form-control")
                                    .Attr("value", Escape(c.ssid()))
                             << Close() << Div().Attr("class", "form-group row")
                             << Input()
                                    .Name("passwd")
                                    .Attr("class", "form-control")
                                    .Attr("value", Escape(c.passwd()))
                             << Close()
                             << Tag("button")
                                    .Attr("type", "submit")
                                    .Attr("class", "btn btn-default")
                             << "Edit" << Close() << Close();
                    }
                    html << Close();
                    return html.Get();
                },
                [](int a) {
                    return JsonBuilder().Append("result", a).Build();
                })));

    server.ServiceLoopForever();
    return 0;
}
