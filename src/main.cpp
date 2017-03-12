#include <dirent.h>
#include <libgen.h>
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
                                    "Configurer le WiFi" <<
                                Close() <<
                            Close() <<
                            Li() <<
                                A().Attr("href",
                                "https://cadre.vermeille.fr/oauth2authorize") <<
                                    "Autoriser Google Photos" <<
                                Close() <<
                            Close() <<
                            Li() <<
                                A().Attr("href",
                                "https://cadre.vermeille.fr/") <<
                                    "Autoriser Facebook" <<
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

std::string Quote(const std::string& str) {
    if (str.find(' ') == std::string::npos) {
        return str;
    }
    return "\"" + str + "\"";
}

std::string Unquote(const std::string& str) {
    if (str.front() == str.back() && str.front() == '"') {
        return str.substr(1, str.size() - 2);
    }
    return str;
}

class ConfigFile {
   public:
    static auto ParseFile(const std::string& path) {
        std::ifstream t(path.c_str());
        std::string line;
        std::string essid;
        std::string key;
        while (std::getline(t, line)) {
            std::string::size_type eq = line.find('=');
            if (eq == std::string::npos) {
                continue;
            }

            if (!strncmp(line.c_str(), "ESSID", 5)) {
                essid = line.substr(eq + 1);
            } else if (!strncmp(line.c_str(), "Key", 3)) {
                key = line.substr(eq + 1);
            }
        }

        if (essid.empty() || key.empty()) {
            throw std::runtime_error("Can't parse " + path);
        }
        return std::make_pair(essid, key);
    }

    static std::string ProfileName(const std::string& path) {
        std::vector<char> path_copy(path.c_str(),
                                    path.c_str() + path.size() + 1);
        char* base = basename(&path_copy[0]);
        return std::string(base);
    }

    ConfigFile(const std::string& path)
        : path_(path), profile_(ProfileName(path)) {
        auto res = ParseFile(path);
        ssid_ = Unquote(res.first);
        passwd_ = Unquote(res.second);
    }

    ConfigFile(const std::string& path,
               const std::string& ssid,
               const std::string password)
        : ssid_(ssid),
          passwd_(password),
          path_(path),
          profile_(ProfileName(path)) {}

    void Write() const {
        std::ofstream out(path_);

        out << "Description=" << profile_
            << "\nInterface=wlan0\nConnection=wireless\nSecurity=wpa\nIP="
               "dhcp\nESSID="
            << Quote(ssid_) << "\nKey=" << Quote(passwd_) << "\n";
    }

    const std::string& ssid() const { return ssid_; }
    const std::string& passwd() const { return passwd_; }
    const std::string& path() const { return path_; }
    const std::string& profile() const { return profile_; }

   private:
    std::string ssid_;
    std::string passwd_;
    std::string path_;
    std::string profile_;
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

std::string WifiEditForm(const std::string& btn_text,
                         const std::string& profile = "",
                         const std::string& ssid = "",
                         const std::string& passwd = "") {
    using namespace httpi::html;
    Html html;
    // clang-format off
    html <<
        Form("POST", "/").AddClass("form-horizontal") <<
            Div().AddClass("form-group") <<
                Label().AddClass("col-sm-2 control-label") <<
                    "Nom du profil" <<
                Close() <<
                Div().AddClass("col-sm-7") <<
                    Input()
                    .Name("profile")
                    .Attr("type", "text")
                    .AddClass("form-control")
                    .Attr("value", profile) <<
                Close() <<
            Close() <<
            Div().AddClass("form-group") <<
                Label().AddClass("col-sm-2 control-label") <<
                    "Nom du réseau" <<
                Close() <<
                Div().AddClass("col-sm-7") <<
                    Input()
                    .Name("ssid")
                    .Attr("type", "text")
                    .AddClass("form-control")
                    .Attr("value", ssid) <<
                Close() <<
            Close() <<
            Div().AddClass("form-group") <<
                Label().AddClass("col-sm-2 control-label") <<
                    "Mot de passe" <<
                Close() <<
                Div().AddClass("col-sm-7") <<
                    Input()
                    .Name("password")
                    .AddClass("form-control")
                    .Attr("value", passwd) <<
                Close() <<
            Close() <<
            Button()
            .Attr("type", "submit")
            .AddClass("btn btn-default") <<
                btn_text <<
            Close() <<
        Close();
    // clang-format on
    return html.Get();
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " /path/to/netctl/directory";
        return 1;
    }

    const std::string path = argv[1] + std::string("/");
    std::vector<ConfigFile> config_files = ReadConfs(path);

    HTTPServer server(80);

    server.RegisterUrl(
        "/",
        httpi::RestPageMaker(MakePage)
            .AddResource("GET",
                         httpi::RestResource(httpi::html::FormDescriptor<>{},
                                             []() { return 0; },
                                             [&](int) {
                                                 using namespace httpi::html;
                                                 Html html;
                                                 for (auto& c : config_files) {
                                                     html << WifiEditForm(
                                                         "Modifier",
                                                         Escape(c.profile()),
                                                         Escape(c.ssid()),
                                                         Escape(c.passwd()));
                                                 }
                                                 return html.Get();
                                             },
                                             [](int) { return "{}"; }))
            .AddResource("POST",
                         httpi::RestResource(
                             httpi::html::FormDescriptor<std::string,
                                                         std::string,
                                                         std::string>{
                                 "POST",
                                 "/",
                                 "Réseaux",
                                 "Modifier les réseaux",
                                 {{"profile", "text", "nom du profil"},
                                  {"ssid", "text", "nom du réseau"},
                                  {"password", "text", "mot de passe wifi"}}},
                             [&](auto profile, auto ssid, auto passwd) {
                                 auto found = std::find_if(
                                     config_files.begin(),
                                     config_files.end(),
                                     [&](const auto& x) {
                                         return x.profile() == profile;
                                     });
                                 if (found == config_files.end()) {
                                     config_files.emplace_back(
                                         path + profile, ssid, passwd);
                                 } else {
                                     *found = ConfigFile(
                                         path + profile, ssid, passwd);
                                 }
                                 for (auto& file : config_files) {
                                     file.Write();
                                 }
                                 return true;
                             },
                             [&](bool ok) {
                                 using namespace httpi::html;
                                 Html html;
                                 html << "OK";
                                 for (auto& c : config_files) {
                                     html << WifiEditForm("Modifier",
                                                          Escape(c.profile()),
                                                          Escape(c.ssid()),
                                                          Escape(c.passwd()));
                                 }
                                 return html.Get();
                             },
                             [](bool ok) { return "{}"; })));

    server.ServiceLoopForever();
    return 0;
}
