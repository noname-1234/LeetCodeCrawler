#include <curl/curl.h>
#include <curl/easy.h>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/value.h>

#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/lexical_cast/bad_lexical_cast.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#define LEETCODE_PROBLEMS_URL "https://leetcode.com/api/problems/all/"
#define LEETCODE_GRAPHQL_URL "https://leetcode.com/graphql"

namespace {
std::size_t callback(const char* in, std::size_t size, std::size_t num,
                     std::string* out) {
    const std::size_t totalBytes(size * num);
    out->append(in, totalBytes);
    return totalBytes;
}
}  // namespace

std::string read_from_file(std::string file_path) {
    std::string line;
    std::stringstream ss;

    std::ifstream rf(file_path);
    while (std::getline(rf, line)) {
        ss << line;
    }
    return ss.str();
}

bool get_code_snip(std::string slug, std::string tk_file_path,
                   std::string& code) {
    CURL* curl = curl_easy_init();

    if (curl) {
        long httpCode(0);
        struct curl_slist* sl;
        std::unique_ptr<std::string> httpData(new std::string());

        sl = NULL;
        sl = curl_slist_append(sl, "Content-Type: application/json");
        sl = curl_slist_append(sl, "Referer: https://leetcode.com");
        sl = curl_slist_append(sl, "Origin: https://leetcode.com");

        std::stringstream ss;
        ss << "x-csrftoken: " << read_from_file(tk_file_path);

        sl = curl_slist_append(sl, ss.str().c_str());

        curl_easy_setopt(curl, CURLOPT_URL, LEETCODE_GRAPHQL_URL);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);

        ss.str("");
        ss << "{ \"operationName\": \"questionData\", \"query\": \"query "
              "questionData { question(titleSlug: \\\""
           << slug
           << "\\\") { questionId title titleSlug codeSnippets { lang langSlug "
              "code } }}\" }";
        std::string body = ss.str();

        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, sl);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, httpData.get());

        curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        curl_easy_cleanup(curl);

        curl_slist_free_all(sl);

        if (httpCode != 200) {
            std::cerr << "Get Code Snippet Error";
            std::cerr << "HTTP Status Code: " << httpCode;
            std::cerr << *httpData.get();
            return false;
        }

        Json::Reader reader;
        Json::Value val;

        if (!reader.parse(*httpData.get(), val)) {
            std::cerr << "HTTP Response Parsing Error" << std::endl;
            return false;
        }

        if (!val["errors"].empty()) {
            std::cerr << "Error: " << val["errors"][0]["message"].asString()
                      << std::endl;
            return false;
        }

        Json::Value snippets = val["data"]["question"]["codeSnippets"];
        for (unsigned int i = 0; i < snippets.size(); i++) {
            if (snippets[i]["lang"].asString() == "C++") {
                code = snippets[i]["code"].asString();
            }
        }

        if (code.empty()) {
            std::cerr << "C++ Not Support For Problem " << slug << std::endl;
            return false;
        }
    }

    return true;
}

std::string get_question_slug(int q_id, int& d_lvl) {
    CURL* curl = curl_easy_init();

    std::string ret;

    if (curl) {
        long httpCode(0);
        std::unique_ptr<std::string> httpData(new std::string());

        curl_easy_setopt(curl, CURLOPT_URL, LEETCODE_PROBLEMS_URL);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, httpData.get());

        curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        curl_easy_cleanup(curl);

        if (httpCode != 200) {
            return ret;
        }

        std::cout << "Get slug title Successfully" << std::endl;

        Json::Reader reader;
        Json::Value val;

        if (reader.parse(*httpData.get(), val)) {
            Json::Value j = val["stat_status_pairs"];

            for (unsigned int i = 0; i < j.size(); i++) {
                if (j[i]["stat"]["frontend_question_id"].asInt() == q_id) {
                    d_lvl = j[i]["difficulty"]["level"].asInt();
                    std::cout << j[i]["stat"] << std::endl;
                    return j[i]["stat"]["question__title_slug"].asString();
                }
            }
        }
    }

    return ret;
}

bool create_folder_and_file(int d_lvl, int q_id, std::string slug,
                            std::string snippet) {
    std::stringstream ss;

    ss << ((d_lvl == 1)
               ? "EASY"
               : (d_lvl == 2) ? "MEDIUM" : (d_lvl == 3) ? "HARD" : "UNKNOWN");

    if (!boost::filesystem::exists(ss.str()) &&
        !boost::filesystem::create_directory(ss.str())) {
        return false;
    }

    ss << "/" << q_id << "." << slug;

    if (!boost::filesystem::create_directory(ss.str())) {
        return false;
    }

    ss << "/main.cpp";

    std::ofstream f;
    f.open(ss.str());
    f << "#include \"../../headers/leetcode.hpp\"" << std::endl;
    f << std::endl;
    f << snippet;

    f.close();
    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "A question id should be specified";
        return 1;
    }

    char* str_q_id = argv[1];
    int q_id = -1;
    try {
        q_id = boost::lexical_cast<int>(str_q_id);
    } catch (boost::bad_lexical_cast& e) {
        std::cerr << "Unable to identify question id";
        return 1;
    }

    std::string leetcode_token_path = getenv("HOME");
    leetcode_token_path += "/leetcode_token";

    if (!boost::filesystem::exists(leetcode_token_path)) {
        std::cerr << "Cookie file dose not exist";
        return 1;
    }

    int d_lvl = -1;
    std::cout << "Getting problem title slug..." << std::endl;
    std::string slug = get_question_slug(q_id, d_lvl);
    if (slug.empty()) {
        std::cerr << "Unable to find a problem with question id: " << q_id;
        return 1;
    }
    std::cout << "Title slug: " << slug << std::endl;

    std::cout << "Getting problem code snippet..." << std::endl;
    std::string snippet;
    if (!get_code_snip(slug, leetcode_token_path, snippet)) {
        std::cerr << "Get code snippet failed";
        return 1;
    }
    std::cout << "Code snippet retrieved" << std::endl;

    std::cout << "Creating the folder and code snippet file..." << std::endl;
    if (create_folder_and_file(d_lvl, q_id, slug, snippet)) {
        std::cout << "The folder and code have been created: " << q_id << "."
                  << slug;
        return 0;
    } else {
        std::cerr << "Create folder and write file failed";
        return 1;
    }
}
