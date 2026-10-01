#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <iostream>

using namespace ftxui;

int main() {
    for (int i=0; i<25; i++) {
        auto doc = spinner(i, 3);
        auto screen = Screen::Create(Dimension::Fixed(10), Dimension::Fixed(1));
        Render(screen, doc);
        std::cout << i << ": " << screen.ToString() << "\n";
    }
    return 0;
}