#include "httplib.h"
#include <iostream>
int main() {
    httplib::Server svr;
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("Hello", "text/plain");
    });
    std::cout << "Listening..." << std::endl;
    svr.listen("127.0.0.1", 18088);
}
